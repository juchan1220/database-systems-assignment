#include "filemanager.h"

char buffer[PAGE_SIZE];
FILE* table_ptr = NULL;

// File Manager API

/// <summary>
/// Allocate an on-disk page from the free page list
/// </summary>
/// <returns> if success, on-disk page number. Otherwise negative value </returns>
pagenum_t file_alloc_page()
{
	char* bufptr = buffer;
	page_t header, freepage;
	pagenum_t freepage_num;

	// read header
	file_read_page(0, &header);

	// does not have free page
	if (header.freepage == 0) {
		// append new page
		freepage_num = header.num_page++;

		memset(bufptr, 0, PAGE_SIZE);
		fseek(table_ptr, 0, SEEK_END);

		fwrite(bufptr, PAGE_SIZE, 1, table_ptr);
		file_write_page(0, &header);

		fflush(table_ptr);		
	}
	else {
		freepage_num = header.freepage;

		// read free page
		file_read_page(header.freepage, &freepage);

		// set header's free page into next free page
		header.freepage = freepage.parent;

		file_write_page(0, &header);
	}

	return freepage_num;
}

/// <summary>
/// Free an on-disk page to the free page list
/// </summary>
/// <param name="pagenum">page number want to free. </param>
void file_free_page(pagenum_t pagenum)
{
	page_t header, cur;
	pagenum_t freepage;

	// Read header
	file_read_page(0, &header);

	// Set first free page into pagenum
	freepage = header.freepage;
	header.freepage = pagenum;

	// Write Header
	file_write_page(0, &header);

	// Read pagenum page
	file_read_page(pagenum, &cur);

	// Set freepage
	cur.num_keys = 0;
	cur.parent = freepage;

	// Write
	file_write_page(pagenum, &cur);

	// Free
	free(cur.keys);
	free(cur.pointers);

	return;
}

/// <summary>
/// Read an on-disk page into the in-memory page structure (dest)
/// </summary>
/// <param name="pagenum"> page number want to read </param>
/// <param name="dest"> memory that will store on-disk page data.
/// Caution: keys and pointers in dest must be free before call this function. </param>
void file_read_page(pagenum_t pagenum, page_t* dest)
{
	char* bufptr = buffer;
	int64_t* keys = NULL;
	pagenum_t* pagenums = NULL;
	record* records = NULL;

	fseek(table_ptr, PAGE_SIZE * pagenum, SEEK_SET);
	fread(bufptr, PAGE_SIZE, 1, table_ptr);

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
		dest->keys = NULL;
		dest->pointers = NULL;
		return;
	}

	// Internal page
	if (dest->is_leaf == 0) {
		keys = malloc(sizeof(int64_t) * INTERNAL_ORDER);
		pagenums = malloc(sizeof(pagenum_t) * (INTERNAL_ORDER + 1));

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
	keys = malloc(sizeof(int64_t) * LEAF_ORDER);
	records = malloc(sizeof(record) * LEAF_ORDER);

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
void file_write_page(pagenum_t pagenum, const page_t* src)
{
	char* bufptr = buffer;
	int64_t* keys = NULL;
	pagenum_t* pagenums = NULL;
	record* records = NULL;


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

		bufptr = buffer;

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
		records = src->pointers;

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
		pagenums = src->pointers;
		
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
	bufptr = buffer;

	fseek(table_ptr, pagenum * PAGE_SIZE, SEEK_SET);
	fwrite(bufptr, PAGE_SIZE, 1, table_ptr);
	fflush(table_ptr);

	return;
}


/// <summary>
/// Open an on-disk table file
/// </summary>
/// <param name="pathname"> path to table file </param>
/// <returns> If success and file is table file, 0, else if success and created new table file, 1,  Otherwise -1</returns>
int file_open_table(const char* pathname) {
	if (table_ptr != NULL) {
		fclose(table_ptr);
	}

	table_ptr = fopen(pathname, "rb+");

	if (table_ptr == NULL) {
#ifdef DEBUG
		printf("Warning: Failed to open table file %s\n", pathname);
		printf("Warning: Trying to make new table file %s\n", pathname);
#endif

		table_ptr = fopen(pathname, "wb+");

		if (table_ptr == NULL) {
#ifdef DEBUG
			printf("Error: Faile to create new table file %s\n", pathname);
#endif
			return -1;
		}
		return 1;
	}

	return 0;
}