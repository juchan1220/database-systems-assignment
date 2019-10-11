#ifndef __FILEMANAGER_H__
#define __FILEMANAGER_H__

#include "bpt.h"

#define PAGE_SIZE 4096
#define RESERVED_SIZE_IN_PAGE_HEADER 104


// File Manager API

extern char buffer[PAGE_SIZE];
extern FILE* table_ptr;

// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page();

// Free an on-disk page to the free page list
void file_free_page(pagenum_t pagenum);

// Read an on-disk page into the in-memory page structure (dest)
void file_read_page(pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page
void file_write_page(pagenum_t pagenum, const page_t* src);

// Open an on-disk table File.
int file_open_table(const char* pathname);

#endif