#ifndef __BPTSERVICE_H__
#define __BPTSERVICE_H__

#include "bpt.h"
#include "buffermanager.h"

// API Service

/// <summary>
/// Initialize buffer pool with given number and buffer manager
/// </summary>
int init_db(int buf_num);

/// <summary>
/// Open exisiting data file using 'pathname' or create one if not existed.
/// </summary>
int open_table(char* pathname);

/// <summary>
/// Write the pages relating to this table to disk and close the table
/// </summary>
int close_table(int table_id);

/// <summary>
/// Insert input 'key/value' (record) to data file at the right place.
/// </summary>
int db_insert(int table_id, int64_t key, char* value);

/// <summary>
/// Find the record containing input 'key'.
/// </summary>
int db_find(int table_id, int64_t key, char* ret_val);

/// <summary>
/// Find the matching record and delete it if found.
/// </summary>
int db_delete(int table_id, int64_t key);

/// <summary>
/// Destory buffer manager
/// </summary>
int shutdown_db(void);

/// <summary>
/// Do natural join with given two tables
/// </summary>
int join_table(int table_id_1, int table_id_2, char* pathname);

#endif