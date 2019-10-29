#ifndef __FILEMANAGER_H__
#define __FILEMANAGER_H__

#include "bpt.h"

#define PAGE_SIZE 4096
#define RESERVED_SIZE_IN_PAGE_HEADER 104
#define MAX_TABLE 10


// File Manager API

extern char file_buffer[PAGE_SIZE];
extern FILE* tables[MAX_TABLE + 5];
extern int table_cnt;

// Read an on-disk page into the in-memory page structure (dest)
void file_read_page(int table_id, pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page
void file_write_page(int table_id, pagenum_t pagenum, const page_t* src);

// Open an on-disk table File.
int file_open_table(const char* pathname);

// Close an on-disk table File.
int file_close_table(int table_id);

// return whether table is opened
int file_is_table_opened(int table_id);

#endif