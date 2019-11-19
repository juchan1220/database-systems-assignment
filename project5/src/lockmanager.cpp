#include "lockmanager.h"

int trx_cnt = 0;
std::mutex trx_mtx;
std::mutex lock_mtx;

std::unordered_map<int, trx_t> trx_map;
std::unordered_map<ptp, std::pair<lock_t*, lock_t *>, ptpHasher> lock_map;

int alloc_trx()
{
	trx_mtx.lock();

	int ret;

	if (trx_map.size() == UINT_MAX) {
		trx_mtx.unlock();
		return 0;
	}

	 do{
		ret = ++trx_cnt;
	 } while (ret == 0 || trx_map.find(ret) != trx_map.end());

	trx_map[ret] = trx_t();

	trx_mtx.unlock();

	return ret;
}

int delete_trx(int tid)
{
	trx_mtx.lock();

	if (trx_map.find(tid) == trx_map.end()) {
		trx_mtx.unlock();
		return 0;
	}

	trx_map[tid].state = STATE_IDLE;
	lock_t* lock_ptr = trx_map[tid].trx_locks;

	lock_mtx.lock();

	while (lock_ptr != nullptr) {
		if (lock_ptr->page_prev != nullptr) {
			lock_ptr->page_prev->page_next = lock_ptr->page_next;
		}
		else {
			lock_map[{lock_ptr->table_id, lock_ptr->page_num}].second = lock_ptr->page_next;
		}
		
		if (lock_ptr->page_next != nullptr) {
			lock_ptr->page_next->page_prev = lock_ptr->page_prev;
		}
		else {
			lock_map[{lock_ptr->table_id, lock_ptr->page_num}].first = lock_ptr->page_prev;
		}

		lock_ptr->lock_cv.notify_all();

		lock_t* tmp = lock_ptr;
		lock_ptr = lock_ptr->trx_next;
		delete tmp;
	}

	lock_mtx.unlock();

	trx_map.erase(tid);
	trx_mtx.unlock();

	return tid;
}