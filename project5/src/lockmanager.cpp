#include "lockmanager.h"

int trx_cnt = 0;
std::mutex trx_mtx;

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

	int ret = 0;

	if (trx_map.find(tid) != trx_map.end())
		trx_map.erase(tid);
	else
		ret = -1;

	trx_mtx.unlock();

	return 0;
}