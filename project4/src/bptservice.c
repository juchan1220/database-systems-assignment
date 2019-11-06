#include "bptservice.h"

// API Service for External Use

/// <summary>
/// Initialize buffer pool with given number and buffer manager
/// </summary>
/// <param name="buf_num"> size of buffer </param>
/// <returns> If success 0, otherwise -1</returns>
int init_db(int buf_num)
{
	return buffer_init(buf_num);
}

/// <summary>
/// Open exisiting data file using 'pathname' or create one if not existed.
/// </summary>
/// <param name="pathname"> table file path </param>
/// <returns> if success, unique table id. otherwise return negative value </returns>
int open_table(char* pathname)
{
	// First, open a table file
	int table_id = buffer_open_table(pathname);

	// Failed to open table file
	if (table_id < 0) {
		return -1;
	}

#ifdef DEBUG
	printf("table (id : %d) open\n", table_id);
#endif

	return table_id;
}

/// <summary>
/// Write the pages relating to this table to disk and close the table
/// </summary>
/// <param name="table_id"> table id </param>
/// <returns> if success 0, otherwise -1</returns>
int close_table(int table_id)
{
	return buffer_close_table(table_id);
}

/// <summary>
/// Insert key, value pair into B+ Tree
/// </summary>
/// <param name="key"> key </param>
/// <param name="value"> value </param>
/// <returns> return 0 if success. Non-zero value otherwise </returns>
int db_insert(int table_id, int64_t key, char* value)
{
	record* find_ret;
	page_t* cur;
	pagenum_t leaf_num;

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: treid to insert into not opened table\n");
#endif
		return -1;
	}

	select_table(table_id);

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

	cur = buffer_read_page(table_id, 0);

	// Case 0. Empty tree
	if (cur->rootpage == 0) {
#ifdef DEBUG
		printf("start new tree\n");
#endif
		start_new_tree(key, value);

		buffer_unpin_page(selected_table, 0);
		return 0;
	}

	buffer_unpin_page(selected_table, 0);

	// header always pinned, so not need unpin
	// buffer_unpin_page(table_id, 0);

	leaf_num = find_leaf(key);

	// Read leaf
	cur = buffer_read_page(table_id, leaf_num);
	
	if (cur->num_keys < LEAF_ORDER - 1) {
		insert_into_leaf(leaf_num, key, value);
	}
	else {
		insert_into_leaf_after_splitting(leaf_num, key, value);
	}

	buffer_unpin_page(table_id, leaf_num);

	return 0;
}


/// <summary>
/// Find the record containing input 'key'
/// </summary>
/// <param name="key"> key </param>
/// <param name="ret_val"> value </param>
/// <returns> if found, 0. otherwise non-zero </returns>
int db_find(int table_id, int64_t key, char* ret_val)
{
	record* ret = NULL;

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: treid to find into not opened table\n");
#endif
		return -1;
	}

	select_table(table_id);

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
int db_delete(int table_id, int64_t key)
{
	record* find_ret;
	pagenum_t find_leaf_ret;

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: treid to delete into not opened table\n");
#endif
		return -1;
	}

	select_table(table_id);

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

/// <summary>
/// Flush all data from buffer and destroy buffer
/// </summary>
/// <returns> if succeess 0, otherwise non-zero</returns>
int shutdown_db(void)
{
	return buffer_destory();
}

/// <summary>
/// Do natural join with given two tables
/// </summary>
/// <param name="table_id_1"> table 1 id </param>
/// <param name="table_id_2"> table 2 id </param>
/// <param name="pathname"> result file </param>
/// <returns> if success 0, otherwise non-zero </returns>
int join_table(int table_id_1, int table_id_2, char* pathname)
{
	if (buffer_is_table_opened(table_id_1) == 0 || buffer_is_table_opened(table_id_2) == 0) {
#ifdef DEBUG
		printf("Error: Tried to join not opened table.\n");
#endif
		return -1;
	}

	FILE* res;
	pagenum_t table1_page_num, table2_page_num, tmp_num;
	page_t* table1_page, * table2_page;
	int x, y;

	res = fopen(pathname, "w");

	if (res == NULL)
	{
#ifdef DEBUG
		printf("Error: Failed to open result file in join_table()\n");
#endif
		return -1;
	}

	select_table(table_id_1);
	table1_page_num = find_leaf(LLONG_MIN);
	select_table(table_id_2);
	table2_page_num = find_leaf(LLONG_MIN);

	if (table1_page_num == 0 || table2_page_num == 0) {
#ifdef DEBUG
		printf("Error: Failed to find leftmost leaf in join_table()\n");
#endif
		return -1;
	}

	table1_page = buffer_read_page(table_id_1, table1_page_num);
	table2_page = buffer_read_page(table_id_2, table2_page_num);
	x = 0;
	y = 0;

	while (true) {
		if (table1_page_num == 0 || table2_page_num == 0) {
			buffer_unpin_page(table_id_1, table1_page_num);
			buffer_unpin_page(table_id_2, table2_page_num);
			break;
		}

#ifdef DEBUG
		if (table1_page->is_leaf == 0 || table2_page->is_leaf == 0) {
			printf("Something goes wrong... Is it not leaf?!\n");
			exit(EXIT_FAILURE);
		}
#endif


		while (x < table1_page->num_keys
			&& y < table2_page->num_keys) {
			while (table1_page->keys[x] > table2_page->keys[y]
				&& y < table2_page->num_keys)
				y++;
			
			if (y == table2_page->num_keys) {
				break;
			}

			if (table1_page->keys[x] == table2_page->keys[y]) {
				record* value1 = table1_page->pointers;
				record* value2 = table2_page->pointers;
				fprintf(res, "%lld,%s,%lld,%s\n", table1_page->keys[x], value1[x].value, table2_page->keys[y], value2[y].value);
			}

			x++;
		}

		if (x == table1_page->num_keys) {
			tmp_num = table1_page->another_page;
			buffer_unpin_page(table_id_1, table1_page_num);
			table1_page_num = tmp_num;

			x = 0;
			table1_page = buffer_read_page(table_id_1, table1_page_num);
		}

		if (y == table2_page->num_keys) {
			tmp_num = table2_page->another_page;
			buffer_unpin_page(table_id_2, table2_page_num);
			table2_page_num = tmp_num;

			y = 0;
			table2_page = buffer_read_page(table_id_2, table2_page_num);
		}
	}

	return 0;
}
