#include "bptservice.h"

// Use for unique table id, in API Service
int table_id = 0;

// API Service for External Use

/// <summary>
/// Open exisiting data file using 'pathname' or create one if not existed.
/// </summary>
/// <param name="pathname"> table file path </param>
/// <returns> if success, unique table id. otherwise return negative value </returns>
int open_table(char* pathname)
{
	page_t tmp;

#ifdef DEBUG
	if (table_id != 0) {
		printf("table (id : %d) close\n", table_id);
	}
#endif

	// First, open a table file
	int ret = file_open_table(pathname);

	// Failed to open table file
	if (ret < 0) {
		return -1;
	}

	table_id++;

#ifdef DEBUG
	printf("table (id : %d) open\n", table_id);
#endif

	if (ret == 1) {
		tmp.is_header = true;
		tmp.freepage = 0;
		tmp.rootpage = 0;
		tmp.num_page = 1;
		file_write_page(0, &tmp);
	}

	return table_id;
}

/// <summary>
/// Insert key, value pair into B+ Tree
/// </summary>
/// <param name="key"> key </param>
/// <param name="value"> value </param>
/// <returns> return 0 if success. Non-zero value otherwise </returns>
int db_insert(int64_t key, char* value)
{
	record* find_ret;
	page_t cur;
	pagenum_t leaf_num;


	find_ret = find(key);
	// Already exist key
	if (find_ret != NULL) {
#ifdef DEBUG
		printf("Already Exists!\n");
#endif
		free(find_ret);
		return -1;
	}

	free(find_ret);

	// Read header
	file_read_page(0, &cur);

	// Case 0. Empty tree
	if (cur.rootpage == 0) {
#ifdef DEBUG
		printf("start new tree\n");
#endif
		start_new_tree(key, value);
#ifdef DEBUG
		print_tree();
#endif
		return 0;
	}

	leaf_num = find_leaf(key);

	// Read leaf
	file_read_page(leaf_num, &cur);
	
	if (cur.num_keys < LEAF_ORDER - 1) {
		insert_into_leaf(leaf_num, key, value);
	}
	else {
		insert_into_leaf_after_splitting(leaf_num, key, value);
	}

	free(cur.keys);
	free(cur.pointers);

	return 0;
}


/// <summary>
/// Find the record containing input 'key'
/// </summary>
/// <param name="key"> key </param>
/// <param name="ret_val"> value </param>
/// <returns> if found, 0. otherwise non-zero </returns>
int db_find(int64_t key, char* ret_val)
{
	record* ret = NULL;

	ret = find(key);

	// does not exist key
	if (ret == NULL) {
		return -1;
	}

	memcpy(ret_val, ret, VALUE_SIZE);
	free(ret);
	
	return 0;
}


/// <summary>
/// Find the matching record and delete it if found.
/// </summary>
/// <param name="key"> key </param>
/// <returns> If success, return 0. Otherwise, return non-zero value. </returns>
int db_delete(int64_t key)
{
	record* find_ret;
	pagenum_t find_leaf_ret;

	find_ret = find(key);
	find_leaf_ret = find_leaf(key);

	// key does not exist
	if (find_ret == NULL || find_leaf_ret < 0) {
#ifdef DEBUG
		printf("db_delete: key does not exist!\n");
#endif
		free(find_ret);
		return -1;
	}

	free(find_ret);

	delete_entry(find_leaf_ret, key, &key);

	return 0;
}
