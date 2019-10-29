#ifndef __BUFFERMANAGER_H_
#define __BUFFERMANAGER_H_

#include "bpt.h"
#include "filemanager.h"

typedef struct
{
	page_t page;
	int table_id;
	pagenum_t page_num;
	uint32_t is_dirty;
	uint32_t is_pinned;
	uint32_t is_refered;
	uint32_t is_alloc;
} bufferFrame;

typedef struct hashBucket hashBucket;

struct hashBucket{
	int idx;
	hashBucket* next;
};

extern bufferFrame* buffer;
extern page_t headers[MAX_TABLE + 5];
extern unsigned int buffer_size;
extern unsigned int bufffer_capacity;
extern hashBucket** hash_table;

extern int clock_hand;

// Initialize buffer pool
int buffer_init(int buf_num);

// Hashing table_id and page number use rabin fingerprint
uint64_t buffer_hashing(int table_id, pagenum_t pagenum);

// Evict unpinned page from buffer, only buffer manager Internal Use
void buffer_evict_page(int idx);

// Allocate an on-disk page from the free page list
pagenum_t buffer_alloc_page(int table_id);

// Free an on-disk page to the free page list
void buffer_free_page(int table_id, pagenum_t pagenum);

// Find page in buffer, only buffer manager Internal Use
int buffer_find_page(int table_id, pagenum_t pagenum);

// Read an on-disk page into the in-memory page, count 1 pin of page
page_t* buffer_read_page(int table_id, pagenum_t pagenum);

// Discount 1 pin of page
void buffer_unpin_page(int table_id, pagenum_t pagenum);

// Set dirty of page
void buffer_set_dirty_page(int table_id, pagenum_t pagenum);

// Open an on-disk table File.
// It is just wrapper function of file_open_table, to protect layered architecture
int buffer_open_table(const char* pathname);

// Close an on-disk table File.
// Write all page of this table from buffer and close
int buffer_close_table(int table_id);

// Write all buffer and Destroy buffer
int buffer_destory();

// Wrapping function of file_is_table_opened
int buffer_is_table_opened(int table_id);

#endif