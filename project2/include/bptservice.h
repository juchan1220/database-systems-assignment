#ifndef __BPTSERVICE_H__
#define __BPTSERVICE_H__

#include "bpt.h"
#include "filemanager.h"

// API Service

extern int table_id;

/// <summary>
/// Open exisiting data file using 'pathname' or create one if not existed.
/// </summary>
int open_table(char* pathname);

/// <summary>
/// Insert input 'key/value' (record) to data file at the right place.
/// </summary>
int db_insert(int64_t key, char* value);

/// <summary>
/// Find the record containing input 'key'.
/// </summary>
int db_find(int64_t key, char* ret_val);


/// <summary>
/// Find the matching record and delete it if found.
/// </summary>
int db_delete(int64_t key);

#endif