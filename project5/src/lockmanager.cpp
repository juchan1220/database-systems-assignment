#include "lockmanager.h"

int trx_cnt = 0;
std::mutex trx_mtx;
std::unordered_map<int, trx_t> trx_map;

int alloc_trx()
{
	trx_mtx.lock();
	
	int ret = ++trx_cnt;
	trx_map[ret] = trx_t();
	
	trx_mtx.unlock();

	return ret;
}

int delete_trx(int tid)
{
    int ret = 0;
    
	trx_mtx.lock();

	if(trx_map.find(tid) != trx_map.end())
	    trx_map.erase(tid);
	else
	    ret = -1;
	
	trx_mtx.unlock();

	return 0;
}