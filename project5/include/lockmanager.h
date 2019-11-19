#ifndef __LOCKMANAGER_H__
#define __LOCKMANAGER_H__

#include "bpt.h"
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <condition_variable>


// table id, page number pair
typedef std::pair<int, pagenum_t> ptp;

struct ptpHasher
{
	std::size_t operator()(const ptp& k) const {
		size_t ret = 23;
		ret = ret * 37 + std::hash<int>()(k.first);
		ret = ret * 37 + std::hash<pagenum_t>()(k.second);
		return ret;
	}
};

enum lock_mode
{
	MODE_SHARED,
	MODE_EXCLUSIVE
};

enum trx_state
{
	STATE_IDLE ,
	STATE_RUNNING,
	STATE_WAITING
};

struct lock_t
{
	int table_id;
	pagenum_t page_num;
	int64_t key;

	lock_mode mode;

	// same page prev lock
	lock_t* page_prev;

	// same page next lock
	lock_t* page_next;

	// same trx next lock
	lock_t* trx_next;

	// transaction id
	int trx_id;

	std::condition_variable lock_cv;

	lock_t() : table_id(-1), key(-1), mode(MODE_SHARED), page_prev(nullptr), page_next(nullptr), trx_next(nullptr), trx_id(-1) {};
};

struct trx_t
{
	int trx_id;
	trx_state state;
	lock_t* trx_locks;
	lock_t* wait_lock;

	trx_t() : trx_id(-1), state(STATE_RUNNING), trx_locks(nullptr), wait_lock(nullptr) {};
};


extern int trx_cnt;
extern std::mutex trx_mtx;
extern std::unordered_map<int, trx_t> trx_map;
extern std::condition_variable lock_cv;

// first lock_t is head, second lock_t is tail
extern std::unordered_map<ptp, std::pair<lock_t*, lock_t*>, ptpHasher> lock_map;


/// <summary>
/// Allocate transaction structure and initialize it
/// </summary>
int alloc_trx();

/// <summary>
/// Clean up the transaction with given tid and its related information
/// </summary>
int delete_trx(int tid);

#endif