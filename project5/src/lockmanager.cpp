#include "lockmanager.h"


TransactionManager trxManager;
LockManager lockManager;

int lock_alloc_trx()
{
	trxManager.trx_mtx.lock();

	int ret;

	// Transaction Table is Full
	if (trxManager.trx_table.size() == UINT_MAX) {
		trxManager.trx_mtx.unlock();
		return 0;
	}

	// Get unique Trx id
	do{
		ret = ++trxManager.next_trx_id;
	} while (ret == 0 || trxManager.trx_table.find(ret) != trxManager.trx_table.end());
	
	// Add Transaction into table
	trxManager.trx_table[ret] = new Transaction(ret);
	trxManager.trx_mtx.unlock();

    return ret;
}

int lock_delete_trx(int tid)
{
	// Not exist transaction
	if (trxManager.trx_table.find(tid) == trxManager.trx_table.end()) {
		return 0;
	}

	lockManager.lock_mtx.lock();

	trxManager.trx_table[tid]->trx_state = TransactionState::STATE_IDLE;

	for (auto lock_ptr: trxManager.trx_table[tid]->acquired_locks) {
		if (lock_ptr->page_prev != nullptr) {
			lock_ptr->page_prev->page_next = lock_ptr->page_next;
		}
		else {
			lockManager.lockTable[{lock_ptr->table_id, lock_ptr->page_num}].second = lock_ptr->page_next;
		}
		
		if (lock_ptr->page_next != nullptr) {
			lock_ptr->page_next->page_prev = lock_ptr->page_prev;
		}
		else {
			lockManager.lockTable[{lock_ptr->table_id, lock_ptr->page_num}].first = lock_ptr->page_prev;
		}
	}

	lockManager.lock_mtx.unlock();

	trxManager.trx_mtx.lock();

	trxManager.trx_table[tid]->trx_cond.notify_all();
	if (trxManager.trx_table[tid]->wait_this_cnt > 0) {
		std::unique_lock<std::mutex> lck(trxManager.trx_table[tid]->trx_mtx);
		trxManager.trx_table[tid]->trx_cond.wait(lck);
	}

	for (auto lock_ptr : trxManager.trx_table[tid]->acquired_locks)
		delete lock_ptr;
	delete trxManager.trx_table[tid];
	trxManager.trx_table.erase(tid);

	trxManager.trx_mtx.unlock();

	return tid;
}

bool lock_trx_exist(int tid)
{
	if (trxManager.trx_table.find(tid) == trxManager.trx_table.end())
		return false;
	return true;
}

LockResult lock_record_lock(int table_id, pagenum_t pagenum, int64_t key, LockMode mode, int tid)
{
	Lock *tail, *lock_ptr;

	lockManager.lock_mtx.lock();

	// existence check
	if (lockManager.lockTable.find({ table_id, pagenum }) == lockManager.lockTable.end()) {
		lockManager.lockTable[{table_id, pagenum}] = { nullptr, nullptr };
	}

    lock_ptr = tail = lockManager.lockTable[{table_id, pagenum}].second;
	
	bool hasLock = false;

	// Find Same Transaction's lock
	while (lock_ptr != nullptr) {
		if (lock_ptr->key != key) {
			lock_ptr = lock_ptr->page_next;
			continue;
		}

		if (mode == LockMode::MODE_SHARED && lock_ptr->trx->trx_id == tid) {
			hasLock = true;
			break;
		}
			
		if (mode == LockMode::MODE_EXCLUSIVE && lock_ptr->mode == LockMode::MODE_EXCLUSIVE && lock_ptr->trx->trx_id == tid) {
			hasLock = true;
			break;
		}

		lock_ptr = lock_ptr->page_next;
	}

	// 이미 같은 트랜잭션에서 Lock을 가지고 있는 경우, 새로 걸지 않고 진행
	if (hasLock) {
		lockManager.lock_mtx.unlock();
		return LockResult::SUCCESS;
	}


	// Deadlock & conflict Detect
	bool conflictFlag = false;
	Lock* conflictLock = nullptr;
	lock_ptr = tail;
	while (lock_ptr != nullptr) {
		if ((lock_ptr->key != key) ||
			(mode == LockMode::MODE_SHARED && lock_ptr->mode == LockMode::MODE_SHARED) ||
			(lock_ptr->trx->trx_id == tid)) {
			lock_ptr = lock_ptr->page_next;
			continue;
		}

		conflictFlag = true;
		conflictLock = lock_ptr;

		Transaction* trx_ptr = lock_ptr->trx;
		while (true) {
			// DEADLOCK Detected
			if (trx_ptr->trx_id == tid) {
				lockManager.lock_mtx.unlock();
				return LockResult::DEADLOCK;
			}

			if (trx_ptr->trx_state != TransactionState::STATE_WAITING) break;

			trx_ptr = trx_ptr->wait_lock->trx;
		}
		lock_ptr = lock_ptr->page_next;
	}

	// No Conflict, No Deadlock
	if (!conflictFlag) {
		// Create Lock
		Lock* tmp = new Lock();

		tmp->table_id = table_id;
		tmp->page_num = pagenum;
		tmp->key = key;
		tmp->acquired = true;
		tmp->trx = trxManager.trx_table[tid];
		tmp->mode = mode;
		tmp->page_prev = nullptr;
		tmp->page_next = tail;
		if(tail != nullptr) tail->page_prev = tmp;

		if (tail == nullptr) {
			lockManager.lockTable[{table_id, pagenum}] = { tmp, tmp };
		}
		else {
			lockManager.lockTable[{table_id, pagenum}].second = tmp;
		}

		tmp->trx->acquired_locks.push_back(tmp);

		lockManager.lock_mtx.unlock();
		return LockResult::SUCCESS;
	}

	// Conflict
	Lock* tmp = new Lock();

	tmp->table_id = table_id;
	tmp->page_num = pagenum;
	tmp->key = key;
	tmp->acquired = false;
	tmp->trx = trxManager.trx_table[tid];
	tmp->mode = mode;
	tmp->page_prev = nullptr;
	tmp->page_next = tail;
	if(tail != nullptr) tail->page_prev = tmp;

	trxManager.trx_table[tid]->wait_lock = conflictLock;
	trxManager.trx_table[tid]->trx_state = TransactionState::STATE_WAITING;
	tmp->trx->acquired_locks.push_back(tmp);
	conflictLock->trx->wait_this_cnt++;

	lockManager.lockTable[{table_id, pagenum}].second = tmp;

	lockManager.lock_mtx.unlock();

	return LockResult::CONFLICT;
}

pagenum_t lock_buffer_page_lock(int table_id, int64_t key)
{
	pagenum_t pagenum;

	do {
		// Lock buffer pool
		// 버퍼 풀이 Lock 되어있는 동안 새로운 page의 alloc, read, evict는 일어나지 않는다
		buffer_pool_lock();

		pagenum = find_leaf(key);
		if (buffer_page_try_lock(table_id, pagenum)) {
			// Get page latch
			break;
		}

		// Unlock Buffer pool
		buffer_pool_unlock();
		// std::this_thread::sleep_for(std::chrono::milliseconds(10));
	} while (true);

	buffer_pool_unlock();

	return pagenum;
}

bool lock_conflict_recheck(int table_id, pagenum_t pagenum, int64_t key, LockMode mode, int tid)
{
	bool conflictFlag = false;
	Lock* conflictLock = nullptr, * lock_ptr, * tmp;

	lockManager.lock_mtx.lock();

	// Conflict가 일어났던 Transaction의 lock을 찾는다
	lock_ptr = lockManager.lockTable[{table_id, pagenum}].second;
	while (lock_ptr != nullptr) {
		if (lock_ptr->key == key && lock_ptr->trx->trx_id == tid) break;
		lock_ptr = lock_ptr->page_next;
	}

	// lock이 사라진 경우
	if (lock_ptr == nullptr) {
#ifdef DEBUG
		printf("Error: Lock is gone?!\n");
#endif
		exit(EXIT_FAILURE);
	}

	tmp = lock_ptr;
	while (lock_ptr != nullptr) {
		if ((lock_ptr->key != key) ||
			(mode == LockMode::MODE_SHARED && lock_ptr->mode == LockMode::MODE_SHARED) ||
			(lock_ptr->trx->trx_id == tid)) {
			lock_ptr = lock_ptr->page_next;
			continue;
		}

		conflictFlag = true;
		conflictLock = lock_ptr;
		break;
	}

	// No Conflict, No Deadlock
	if (!conflictFlag) {
		tmp->acquired = true;
		tmp->trx->wait_lock = nullptr;

		lockManager.lock_mtx.unlock();
		return true;
	}

	// Conflict
	tmp->acquired = false;

	trxManager.trx_table[tid]->wait_lock = conflictLock;
	trxManager.trx_table[tid]->trx_state = TransactionState::STATE_WAITING;
	conflictLock->trx->wait_this_cnt++;

	lockManager.lock_mtx.unlock();

	return false;
}

void lock_abort_trx(int tid)
{
	for (auto& undo :trxManager.trx_table[tid]->undo_list) {
		pagenum_t pagenum = lock_buffer_page_lock(undo.table_id, undo.key);

		page_t* page = buffer_read_page(undo.table_id, pagenum);

		for (int i = 0; i < page->num_keys; i++) {
			if (page->keys[i] == undo.key) {
				memcpy(((record*)(page->pointers)) + i, undo.oldValue, sizeof(record));
				break;
			}
		}

		buffer_unpin_page(undo.table_id, pagenum);
		buffer_page_unlock(undo.table_id, pagenum);
		delete undo.oldValue;
	}

	lock_delete_trx(tid);
}


