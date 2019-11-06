#include "buffermanager.h"


// Buffer
bufferFrame* buffer;
unsigned int bufffer_size = 0;
unsigned int buffer_capacity = 0;
int clock_hand = 0;
hashBucket** hash_table;

// Buffer for header
page_t headers[MAX_TABLE + 5];


/// <summary>
/// Initialize buffer pool with given number and buffer manager
/// </summary>
/// <param name="buf_num"> size of buffer </param>
/// <returns> If success 0, otherwise -1</returns>
int buffer_init(int buf_num)
{
	int i;

	// Invalid Buffer Size
	if(buf_num <= 0){
		return -1;
	}

	buffer_capacity = buf_num;
	bufffer_size = 0;

	buffer = malloc(sizeof(bufferFrame) * buffer_capacity);
	hash_table = malloc(sizeof(hashBucket*) * buffer_capacity);

	if (buffer == NULL || hash_table == NULL) {
#ifdef DEBUG
		printf("Error: Failed to malloc buffer.\n");
#endif
		return -1;
	}

	// Init
	for (i = 0; i < buffer_capacity; i++) {
		buffer[i].is_alloc = 0;
		buffer[i].is_dirty = 0;
		buffer[i].is_pinned = 0;
	}

	for (i = 0; i < buffer_capacity; i++) {
		hash_table[i] = NULL;
	}

	return 0;
}

/// <summary>
/// Hashing table_id and page number use rabin fingerprint
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
/// <returns></returns>
uint64_t buffer_hashing(int table_id, pagenum_t pagenum)
{
	uint64_t x = 13;
	uint64_t res = 0;

	while (table_id > 0) {
		res += (table_id % 10) * x;
		x *= 13;
		table_id /= 10;
	}

	while (pagenum > 0) {
		res += (pagenum % 10) * x;
		x *= 13;
		pagenum /= 10;
	}

	return res;
}

/// <summary>
/// Evict unpinned page from buffer, only buffer manager internal use
/// </summary>
void buffer_evict_page(int idx)
{
	int hash;
	hashBucket* bucket_ptr, * prv;

	if (idx < 0 || idx >= buffer_capacity) {
#ifdef DEBUG
		printf("Error: tried to evict invalid page.\n");
#endif
		exit(EXIT_FAILURE);
	}
	
	// if dirty, write page
	if(buffer[idx].is_dirty){
		file_write_page(buffer[idx].table_id, buffer[idx].page_num, &(buffer[idx].page));
	}

	free(buffer[idx].page.keys);
	free(buffer[idx].page.pointers);

	// delete from hash table
	hash = buffer_hashing(buffer[idx].table_id, buffer[idx].page_num) % buffer_capacity;
	bucket_ptr = hash_table[hash];
	prv = NULL;

	while (bucket_ptr != NULL) {
		if (bucket_ptr->idx == idx) {
			if (prv == NULL) {
				hash_table[hash] = bucket_ptr->next;
			}
			else {
				prv->next = bucket_ptr->next;
			}

			free(bucket_ptr);
			break;
		}

		prv = bucket_ptr;
		bucket_ptr = bucket_ptr->next;
	}

	buffer[idx].is_alloc = 0;
	buffer[idx].is_dirty = 0;
	buffer[idx].is_pinned = 0;
	buffer[idx].is_refered = 0;

	return;
}

/// <summary>
/// Allocate an on-disk page from the free page list
/// </summary>
/// <param name="table_id"> table_id </param>
/// <returns> if success, page number. </returns>
pagenum_t buffer_alloc_page(int table_id)
{
	int cnt = 0;
	page_t* freepage;
	page_t* header = &headers[table_id];
	pagenum_t freepage_num;

	// Invalid Table id
	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error! Tried to alloc page of not exist table (%d)\n", table_id);
#endif
		exit(EXIT_FAILURE);
	}

	// does not have free page
	if (header->freepage == 0) {
		freepage_num = header->num_page++;
	}
	else {
		freepage_num = header->freepage;
		freepage = buffer_read_page(table_id, freepage_num);
		header->freepage = freepage->parent;
		buffer_unpin_page(table_id, freepage_num);
	}

	// before 2 rotate
	while (cnt <= 2LL * buffer_capacity) {
		// happy case: got it!
		if (buffer[clock_hand].is_alloc == 0)
			break;

		// pinned page
		if (buffer[clock_hand].is_pinned) {
			// skip
			clock_hand = (clock_hand + 1) % buffer_capacity;
			cnt++;
			continue;
		}

		// refered page
		if (buffer[clock_hand].is_refered) {
			// unset refered
			buffer[clock_hand].is_refered = 0;
			clock_hand = (clock_hand + 1) % buffer_capacity;
			cnt++;
			continue;
		}

		// got victim
		buffer_evict_page(clock_hand);

		break;
	}

	// all page in buffer pinned
	if (cnt > 2LL * buffer_capacity) {
#ifdef DEBUG
		printf("Error: all page in buffer pinned!\n");
#endif
		exit(EXIT_FAILURE);
	}

	// Init buffer block of free page
	// load to buffer free page, and does not pin
	// because index layer need to call buffer_read_page to access this page
	buffer[clock_hand].is_alloc = 1;
	buffer[clock_hand].is_dirty = 0;
	buffer[clock_hand].is_pinned = 0;
	buffer[clock_hand].is_refered = 1;
	buffer[clock_hand].table_id = table_id;
	buffer[clock_hand].page_num = freepage_num;

	// Insert into hash table
	int hash = buffer_hashing(table_id, freepage_num) % buffer_capacity;
	hashBucket* newBucket = malloc(sizeof(hashBucket));

	if (newBucket == NULL) {
#ifdef DEBUG
		printf("Error: Failed to malloc hash Bucket.\n");
#endif
		exit(EXIT_FAILURE);
	}

	newBucket->idx = clock_hand;
	newBucket->next = hash_table[hash];

	hash_table[hash] = newBucket;

	// advance clock
	clock_hand = (clock_hand + 1) % buffer_capacity;

	return freepage_num;
}

/// <summary>
/// Free an on-disk page to the free page list
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
void buffer_free_page(int table_id, pagenum_t pagenum)
{
	page_t* cur;
	page_t* header = &headers[table_id];
	pagenum_t freepage_num;

	// Invalid table id
	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error! Tried to free page of not exist table! : %d\n", table_id);
#endif
		exit(EXIT_FAILURE);
	}

	// Set first free page into page number of new free page
	freepage_num = header->freepage;
	header->freepage = pagenum;

	cur = buffer_read_page(table_id, pagenum);

	cur->num_keys = 0;
	cur->parent = freepage_num;
	free(cur->keys);
	free(cur->pointers);
	cur->keys = NULL;
	cur->pointers = NULL;

	buffer_set_dirty_page(table_id, pagenum);
	buffer_unpin_page(table_id, pagenum);

	return;
}

/// <summary>
/// Find page in buffer, only buffer manager Internal Use
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
/// <returns> if success, index of page in buffer. otherwise -1</returns>
int buffer_find_page(int table_id, pagenum_t pagenum)
{
	int hash = buffer_hashing(table_id, pagenum) % buffer_capacity;
	hashBucket* bucket = hash_table[hash];

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: tried to find page of not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}

	while (bucket != NULL) {
		if (buffer[bucket->idx].table_id == table_id
			&& buffer[bucket->idx].page_num == pagenum)
			return bucket->idx;
		bucket = bucket->next;
	}
	
	return -1;
}

/// <summary>
/// Read an on-disk page into ther in-memory page, count 1 pin of page
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
/// <returns> page pointer </returns>
page_t* buffer_read_page(int table_id, pagenum_t pagenum)
{
	uint64_t cnt = 0;

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: tried to read page of not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}
	// Special Case: Header
	if (pagenum == 0) {
		return &(headers[table_id]);
	}

	int idx = buffer_find_page(table_id, pagenum);

	// Already exists in buffer
	if (idx != -1) {
		// count 1 more pin
		buffer[idx].is_pinned++;
		buffer[idx].is_refered = 1;
		return &(buffer[idx].page);
	}

	// have to read page from file

	// before 2 rotate
	while (cnt <= 2LL * buffer_capacity) {
		// happy case: got it!
		if (buffer[clock_hand].is_alloc == 0)
			break;
		
		// pinned page
		if (buffer[clock_hand].is_pinned) {
			// skip
			clock_hand = (clock_hand + 1) % buffer_capacity;
			cnt++;
			continue;
		}

		// refered page
		if (buffer[clock_hand].is_refered) {
			// unset refered
			buffer[clock_hand].is_refered = 0;
			clock_hand = (clock_hand + 1) % buffer_capacity;
			cnt++;
			continue;
		}

		// got victim
		buffer_evict_page(clock_hand);
		
		break;
	}

	// all page in buffer pinned
	if (cnt > 2LL * buffer_capacity) {
#ifdef DEBUG
		printf("Error: all page in buffer pinned!\n");
#endif
		exit(EXIT_FAILURE);
	}

	// Read and Init
	file_read_page(table_id, pagenum, &(buffer[clock_hand].page));
	buffer[clock_hand].table_id = table_id;
	buffer[clock_hand].page_num = pagenum;
	buffer[clock_hand].is_dirty = 0;
	buffer[clock_hand].is_pinned = 1;
	buffer[clock_hand].is_refered = 1;
	buffer[clock_hand].is_alloc = 1;

	// Insert into hash table
	int hash = buffer_hashing(table_id, pagenum) % buffer_capacity;
	hashBucket* newBucket = malloc(sizeof(hashBucket));

	if (newBucket == NULL) {
#ifdef DEBUG
		printf("Error: Failed to malloc hash Bucket.\n");
#endif
		exit(EXIT_FAILURE);
	}
	
	newBucket->idx = clock_hand;
	newBucket->next = hash_table[hash];

	hash_table[hash] = newBucket;

	int prv_hand = clock_hand;
	clock_hand = (clock_hand + 1) % buffer_capacity;

	return &(buffer[prv_hand].page);
}

/// <summary>
/// Discoutn 1 pin of page in buffer
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
void buffer_unpin_page(int table_id, pagenum_t pagenum)
{
	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: tried to unpin page of not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}

	// Special Case: Header
	if (pagenum == 0) {
		return;
	}

	int idx = buffer_find_page(table_id, pagenum);

	if (idx < 0) {
#ifdef DEBUG
		printf("Error: tried to unpin not exist page\n");
#endif
		exit(EXIT_FAILURE);
	}

	buffer[idx].is_pinned--;

	return;
}

/// <summary>
/// Set dirty of page
/// </summary>
/// <param name="table_id"> table id </param>
/// <param name="pagenum"> page number </param>
void buffer_set_dirty_page(int table_id, pagenum_t pagenum)
{
	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: tried to set dirty on page of not exist table.\n");
#endif
		exit(EXIT_FAILURE);
	}
	// Special Case: Header
	if (pagenum == 0) {
		return;
	}

	int idx = buffer_find_page(table_id, pagenum);

	if (idx < 0) {
#ifdef DEBUG
		printf("Error: tried to set dirty on not exist page\n");
#endif
		exit(EXIT_FAILURE);
	}

	buffer[idx].is_dirty = 1;

	return;
}

/// <summary>
/// open table and read header page into header buffer
/// </summary>
/// <param name="pathname">path to table file</param>
/// <returns> if success, unique table id, otherwise -1</returns>
int buffer_open_table(const char* pathname)
{
	int table_id = file_open_table(pathname);

	// Failed to open table file
	if (table_id < 0) {
		return -1;
	}

	// Read header into buffer
	file_read_page(table_id, 0, &headers[table_id]);

	return table_id;
}

/// <summary>
/// Write all page of this table from buffer and Close this table
/// </summary>
/// <param name="table_id"> table_id </param>
/// <returns> if success 0, otherwise -1</returns>
int buffer_close_table(int table_id)
{
	hashBucket* bucket_ptr = NULL, * prv, * tmp;

	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: Tried to close already closed table.\n");
#endif
		return -1;
	}

	for (int i = 0; i < buffer_capacity; i++) {
		bucket_ptr = hash_table[i];
		prv = NULL;

		while (bucket_ptr != NULL) {
			if (buffer[bucket_ptr->idx].table_id == table_id) {
				// if dirty, write it
				if (buffer[bucket_ptr->idx].is_dirty) {
					file_write_page(buffer[bucket_ptr->idx].table_id, buffer[bucket_ptr->idx].page_num, &(buffer[bucket_ptr->idx].page));
				}

				// free page content
				free(buffer[bucket_ptr->idx].page.keys);
				free(buffer[bucket_ptr->idx].page.pointers);

				buffer[bucket_ptr->idx].is_dirty = 0;
				buffer[bucket_ptr->idx].is_pinned = 0;
				buffer[bucket_ptr->idx].is_alloc = 0;
				buffer[bucket_ptr->idx].is_refered = 0;

				tmp = bucket_ptr;
				bucket_ptr = bucket_ptr->next;

				if (prv == NULL) {
					hash_table[i] = NULL;
				}
				else {
					prv->next = NULL;
				}

				// free hash bucket
				free(tmp);
			}
			else {
				// linking
				if (prv == NULL) {
					hash_table[i] = bucket_ptr;
				}
				else {
					prv->next = bucket_ptr;
				}

				prv = bucket_ptr;
				bucket_ptr = bucket_ptr->next;
			}
		}
	}

	return 0;
}

/// <summary>
/// flush all data from buffer and destory buffer
/// </summary>
/// <returns>if success 0, otherwise -1 </returns>
int buffer_destory()
{
	int i;
	hashBucket* bucket_ptr, * tmp;

	// flush all page in buffer
	for (i = 0; i < buffer_capacity; i++) {
		if (buffer[i].is_alloc == 0)
			continue;

		if (buffer[i].is_dirty) {
			file_write_page(buffer[i].table_id, buffer[i].page_num, &(buffer[i].page));
		}

		// free page content
		free(buffer[i].page.keys);
		free(buffer[i].page.pointers);
	}

	// free hash table
	for (i = 0; i < buffer_capacity; i++) {
		bucket_ptr = hash_table[i];
		tmp = NULL;

		while (bucket_ptr != NULL) {
			tmp = bucket_ptr->next;
			free(bucket_ptr);
			bucket_ptr = tmp;
		}
	}

	free(buffer);

	return 0;
}

/// <summary>
/// Wrapping function of file_is_table_opened
/// </summary>
/// <param name="table_id"> table id </param>
/// <returns> if opened 1, otherwise 0 </returns>
int buffer_is_table_opened(int table_id)
{
	return file_is_table_opened(table_id);
}

