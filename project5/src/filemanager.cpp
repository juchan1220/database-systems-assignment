#include "filemanager.h"

char file_buffer[PAGE_SIZE];
int table_cnt = 0;
FILE* tables[MAX_TABLE + 5];

// File Manager API

/// <summary>
/// Read an on-disk page into the in-memory page structure (dest)
/// </summary>
/// <param name="pagenum"> page number want to read </param>
/// <param name="dest"> memory that will store on-disk page data.
/// Caution: keys and pointers in dest must be free before call this function. </param>
void file_read_page(int table_id, pagenum_t pagenum, page_t* dest)
{
	char* bufptr = file_buffer;
	int64_t* keys = NULL;
	pagenum_t* pagenums = NULL;
	record* records = NULL;
	FILE* table_ptr = tables[table_id];

	if (table_ptr == NULL) {
#ifdef DEBUG
		printf("Error: Tried to read not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}

	fseek(table_ptr, PAGE_SIZE * pagenum, SEEK_SET);
	fread(bufptr, PAGE_SIZE, 1, table_ptr);

	// Init
	dest->keys = NULL;
	dest->pointers = NULL;

	// Header page
	if (pagenum == 0) {
		dest->is_header = true;
		memcpy(&(dest->freepage), bufptr, sizeof(pagenum_t));
		bufptr += sizeof(pagenum_t);
		memcpy(&(dest->rootpage), bufptr, sizeof(pagenum_t));
		bufptr += sizeof(pagenum_t);
		memcpy(&(dest->num_page), bufptr, sizeof(pagenum_t));

		return;
	}

	dest->is_header = false;

	memcpy(&(dest->parent), bufptr, sizeof(pagenum_t));
	bufptr += sizeof(pagenum_t);
	memcpy(&(dest->is_leaf), bufptr, sizeof(int32_t));
	bufptr += sizeof(int32_t);
	memcpy(&(dest->num_keys), bufptr, sizeof(int32_t));
	bufptr += sizeof(int32_t) + RESERVED_SIZE_IN_PAGE_HEADER;
	memcpy(&(dest->another_page), bufptr, sizeof(pagenum_t));
	bufptr += sizeof(pagenum_t);

	// Free page
	if (dest->num_keys == 0) {
		return;
	}

	// Internal page
	if (dest->is_leaf == 0) {
		keys = (int64_t *)malloc(sizeof(int64_t) * INTERNAL_ORDER);
		pagenums = (pagenum_t *)malloc(sizeof(pagenum_t) * (INTERNAL_ORDER + 1));

		if (keys == NULL || pagenums == NULL) {
#ifdef DEBUG
			printf("Error: Failed to alloc keys or page nums in file_read_page()\n");
#endif
			exit(EXIT_FAILURE);
		}

		// for convenience on implementation, store left most pagenums with other keys
		pagenums[0] = dest->another_page;

		for (int i = 0; i < dest->num_keys; i++) {
			memcpy(keys + i, bufptr, sizeof(int64_t));
			bufptr += sizeof(int64_t);

			memcpy(pagenums + i + 1, bufptr, sizeof(pagenum_t));
			bufptr += sizeof(pagenum_t);
		}

		dest->keys = keys;
		dest->pointers = pagenums;

		return;
	}

	// Leaf page
	keys = (int64_t *)malloc(sizeof(int64_t) * LEAF_ORDER);
	records = (record *)malloc(sizeof(record) * LEAF_ORDER);

	if (keys == NULL || records == NULL) {
#ifdef DEBUG
		printf("Error: Failed to alloc keys or values in file_read_page()\n");
#endif		
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < dest->num_keys; i++) {
		memcpy(keys + i, bufptr, sizeof(int64_t));
		bufptr += sizeof(int64_t);

		memcpy(&(records[i].value), bufptr, sizeof(char) * VALUE_SIZE);
		bufptr += sizeof(char) * VALUE_SIZE;
	}

	dest->keys = keys;
	dest->pointers = records;

	return;
}


/// <summary>
/// Write an in-memory page(src) to the on-disk page
/// </summary>
/// <param name="pagenum"> page number want to write </param>
/// <param name="src"> page struct that store data will be write </param>
void file_write_page(int table_id, pagenum_t pagenum, const page_t* src)
{
	char* bufptr = file_buffer;
	int64_t* keys = NULL;
	pagenum_t* pagenums = NULL;
	record* records = NULL;
	FILE* table_ptr = tables[table_id];

	if (table_ptr == NULL) {
#ifdef DEBUG
		printf("Error: Tried to write not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}

	// Init by 0
	memset(bufptr, 0, PAGE_SIZE);

	// Header page
	if (src->is_header) {
#ifdef DEBUG
		if (pagenum != 0) {
			printf("Warning: tried to write header page into not header position! (pagenum : %lld)\n", pagenum);
		}
#endif

		// Copy to buffer
		memcpy(bufptr, &(src->freepage), sizeof(pagenum_t));
		bufptr += sizeof(pagenum_t);
		memcpy(bufptr, &(src->rootpage), sizeof(pagenum_t));
		bufptr += sizeof(pagenum_t);
		memcpy(bufptr, &(src->num_page), sizeof(pagenum_t));

		bufptr = file_buffer;

		// Write to file
		fseek(table_ptr, PAGE_SIZE * pagenum, SEEK_SET);
		fwrite(bufptr, PAGE_SIZE, 1, table_ptr);
		fflush(table_ptr);

		return;
	}

	// Copy page header to buffer
	memcpy(bufptr, &(src->parent), sizeof(pagenum_t));
	bufptr += sizeof(pagenum_t);
	memcpy(bufptr, &(src->is_leaf), sizeof(int32_t));
	bufptr += sizeof(int32_t);
	memcpy(bufptr, &(src->num_keys), sizeof(int32_t));
	bufptr += sizeof(int32_t) + RESERVED_SIZE_IN_PAGE_HEADER;



	// Leaf page
	if (src->is_leaf) {
		// Copy keys and values
		keys = src->keys;
		records = (record *)src->pointers;

		// free page has NULL in keys and pointers
		if (src->num_keys != 0) {
			memcpy(bufptr, &(src->another_page), sizeof(pagenum_t));
			bufptr += sizeof(pagenum_t);
		}

		for (int i = 0; i < src->num_keys; i++) {
			memcpy(bufptr, &(keys[i]), sizeof(int64_t));
			bufptr += sizeof(int64_t);
			memcpy(bufptr, &(records[i].value), sizeof(char) * VALUE_SIZE);
			bufptr += sizeof(char) * VALUE_SIZE;
		}
	}
	// Internal page
	else {
		// Copy keys and pagenums
		keys = src->keys;
		pagenums = (pagenum_t *)src->pointers;
		
		// free page has NULL in keys and pointers
		if (src->num_keys != 0) {
			memcpy(bufptr, &pagenums[0], sizeof(pagenum_t));
			bufptr += sizeof(pagenum_t);
		}

		for (int i = 0; i < src->num_keys; i++) {
			memcpy(bufptr, &(keys[i]), sizeof(int64_t));
			bufptr += sizeof(int64_t);
			memcpy(bufptr, &(pagenums[i + 1]), sizeof(pagenum_t));
			bufptr += sizeof(pagenum_t);
		}
	}

	// Write to file
	bufptr = file_buffer;

	fseek(table_ptr, pagenum * PAGE_SIZE, SEEK_SET);
	fwrite(bufptr, PAGE_SIZE, 1, table_ptr);
	fflush(table_ptr);

	return;
}


/// <summary>
/// Open an on-disk table file
/// </summary>
/// <param name="pathname"> path to table file </param>
/// <returns> If success, unique table id, Otherwise -1</returns>
int file_open_table(const char* pathname) {
	FILE* tmp_ptr;
	page_t tmp_page;

	tmp_ptr = fopen(pathname, "rb+");

	if (tmp_ptr == NULL) {
#ifdef DEBUG
		printf("Warning: Failed to open table file %s\n", pathname);
		printf("Warning: Trying to make new table file %s\n", pathname);
#endif

		tmp_ptr = fopen(pathname, "wb+");

		if (tmp_ptr == NULL) {
#ifdef DEBUG
			printf("Error: Faile to create new table file %s\n", pathname);
#endif
			return -1;
		}

		table_cnt++;
		tables[table_cnt] = tmp_ptr;

		// if create new table file, write header page
		tmp_page.is_header = true;
		tmp_page.freepage = 0;
		tmp_page.rootpage = 0;
		tmp_page.num_page = 1;
		file_write_page(table_cnt, 0, &tmp_page);
	}
	else {
		table_cnt++;
		tables[table_cnt] = tmp_ptr;
	}

	return table_cnt;
}

/// <summary>
/// Close an on-disk table file
/// </summary>
/// <param name="table_id"> table id </param>
/// <returns> If success 0, otherwise -1 </returns>
int file_close_table(int table_id)
{
	// if table does not exist
	if (tables[table_id] == NULL) {
		return -1;
	}

	// Close
	if (fclose(tables[table_id]) == EOF) {
		return -1;
	}

	tables[table_id] = NULL;

	return 0;
}

/// <summary>
/// return whether table is opened
/// </summary>
/// <param name="table_id"> table id</param>
/// <returns> if opened 1, otherwise 0</returns>
int file_is_table_opened(int table_id)
{
	if (table_id <= 0 || table_id > MAX_TABLE)
		return 0;
	if (tables[table_id] == NULL)
		return 0;

	return 1;
}
