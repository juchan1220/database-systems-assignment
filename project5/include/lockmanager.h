#ifndef __LOCKMANAGER_H__
#define __LOCKMANAGER_H__

#include "bpt.h"
#include "buffermanager.h"
#include <mutex>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <condition_variable>


// table id, page number pair
typedef std::pair<int, pagenum_t> ptp;


struct Lock;
struct Transaction;

struct ptpHasher
{
	std::size_t operator()(const ptp& k) const {
		size_t ret = 1009;
		ret = ret * 1009 + std::hash<int>()(k.first);
		ret = ret * 1009 + std::hash<pagenum_t>()(k.second);
		return ret;
	}
};

enum class LockMode
{
	MODE_SHARED,
	MODE_EXCLUSIVE
};

enum class TransactionState
{
	STATE_IDLE,
	STATE_RUNNING,
	STATE_WAITING
};

enum class LockResult
{
	SUCCESS,
	CONFLICT,
	DEADLOCK
};

struct Undo
{
	int table_id;
	int64_t key;
	record* oldValue;
};

struct Lock
{
	int table_id;
	pagenum_t page_num;
	int64_t key;

	Transaction* trx;

	LockMode mode;

	// same page prev lock
	Lock* page_prev;

	// same page next lock
	Lock* page_next;

	bool acquired;
};

struct Transaction
{
	int trx_id;
	
	TransactionState trx_state;
	std::list<Lock*> acquired_locks;

	std::condition_variable trx_cond;
	std::mutex trx_mtx;

	int wait_this_cnt;

	Lock* wait_lock;

	std::list<Undo> undo_list;
	
	Transaction(int tid) : trx_id(tid), trx_state(TransactionState::STATE_IDLE), wait_lock(nullptr), wait_this_cnt(0) {};
};

struct TransactionManager
{
	std::unordered_map<int, Transaction*> trx_table;
	int next_trx_id;
	std::mutex trx_mtx;
};

struct LockManager
{
	// first lock_t is head, second lock_t is tail
	std::unordered_map<ptp, std::pair<Lock*, Lock*>, ptpHasher> lockTable;

	std::mutex lock_mtx;
	std::mutex waitlock_refresh_mtx;
};

extern TransactionManager trxManager;
extern LockManager lockManager;



/// <summary>
/// Allocate transaction structure and initialize it
/// </summary>
int lock_alloc_trx();

/// <summary>
/// Clean up the transaction with given tid and its related information
/// </summary>
int lock_delete_trx(int tid);

bool lock_trx_exist(int tid);

LockResult lock_record_lock(int table_id, pagenum_t pagenum, int64_t key, LockMode mode, int tid);

pagenum_t lock_buffer_page_lock(int table_id, int64_t key);

bool lock_conflict_recheck(int table_id, pagenum_t pagenum, int64_t key, LockMode mode, int tid);

void lock_abort_trx(int tid);

#endif