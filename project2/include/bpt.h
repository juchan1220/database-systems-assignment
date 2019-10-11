//////////////////////////////////////////////
// you must comment below line before submit!
//////////////////////////////////////////////
// #define DEBUG

#ifndef __BPT_H__
#define __BPT_H__


// Uncomment the line below if you are compiling on Windows.
// #define WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef WINDOWS
#define bool char
#define false 0
#define true 1
#endif

// Default order is 4.
#define DEFAULT_ORDER 4

// Minimum order is necessarily 3.  We set the maximum
// order arbitrarily.  You may change the maximum order.
#define MIN_ORDER 3
#define MAX_ORDER 20


#define LEAF_ORDER 32
#define INTERNAL_ORDER 249

#define VALUE_SIZE 120

// TYPES.

#define EXIT_FAILURE 1

/* Type representing the record
 * to which a given key refers.
 * In a real B+ tree system, the
 * record would hold data (in a database)
 * or a file (in an operating system)
 * or some other information.
 * Users can rewrite this part of the code
 * to change the type and content
 * of the value field.
 */
typedef struct record {
	char value[VALUE_SIZE];
} record;

/* Type representing a node in the B+ tree.
 * This type is general enough to serve for both
 * the leaf and the internal node.
 * The heart of the node is the array
 * of keys and the array of corresponding
 * pointers.  The relation between keys
 * and pointers differs between leaves and
 * internal nodes.  In a leaf, the index
 * of each key equals the index of its corresponding
 * pointer, with a maximum of order - 1 key-pointer
 * pairs.  The last pointer points to the
 * leaf to the right (or NULL in the case
 * of the rightmost leaf).
 * In an internal node, the first pointer
 * refers to lower nodes with keys less than
 * the smallest key in the keys array.  Then,
 * with indices i starting at 0, the pointer
 * at i + 1 points to the subtree with keys
 * greater than or equal to the key in this
 * node at index i.
 * The num_keys field is used to keep
 * track of the number of valid keys.
 * In an internal node, the number of valid
 * pointers is always num_keys + 1.
 * In a leaf, the number of valid pointers
 * to data is always num_keys.  The
 * last leaf pointer points to the next leaf.
 */
typedef struct node {
    void ** pointers;
    int * keys;
    struct node * parent;
    bool is_leaf;
    int num_keys;
    struct node * next; // Used for queue.
} node;

typedef uint64_t pagenum_t;

typedef struct {
	pagenum_t parent;
	int32_t is_leaf;
	int32_t num_keys;
	/// <summary>
	/// right sibling page number for leaf page
	/// </summary>
	pagenum_t another_page;
	/// <summary>
	/// In leaf, key and pointer(values) is one-to-one match.
	/// In internal, pointer(pagenums) has (num of keys + 1) values.
	/// </summary>
	int64_t* keys;
	void* pointers;

	// for Header

	/// <summary>
	/// true if this page is header, false otherwise. Only Internal Use
	/// </summary>
	bool is_header;
	pagenum_t freepage;
	pagenum_t rootpage;
	pagenum_t num_page;
} page_t;

// GLOBALS.

/* The order determines the maximum and minimum
 * number of entries (keys and pointers) in any
 * node.  Every node has at most order - 1 keys and
 * at least (roughly speaking) half that number.
 * Every leaf has as many pointers to data as keys,
 * and every internal node has one more pointer
 * to a subtree than the number of keys.
 * This global variable is initialized to the
 * default value.
 */

#ifdef DEBUG

extern int queue_front, queue_rear, queue_size;
extern pagenum_t* queue;
extern int* queue_lv;

#endif

// On-disk B+ Tree Utility

pagenum_t find_leaf(int64_t key);
record* find(int64_t key);
int cut(int length);
void print_tree(void);

// Insertion

page_t* make_page(bool isLeaf);
int get_left_index(page_t* parent, pagenum_t leftnum);
void start_new_tree(int64_t key, const char* value);
void insert_into_leaf(pagenum_t leafnum, int64_t key, const char* value);
void insert_into_leaf_after_splitting(pagenum_t leafnum, int64_t key, const char * value);
void insert_into_node(page_t* parent, pagenum_t parentnum, int left_index, int64_t key, pagenum_t rightnum);
void insert_into_node_after_splitting(page_t* parent, pagenum_t parentnum, int left_index, int64_t key, pagenum_t rightnum);
void insert_into_parent(page_t* left, pagenum_t leftnum, int64_t key, page_t* right, pagenum_t rightnum);
void insert_into_new_root(page_t* left, pagenum_t leftnum, int64_t key, page_t* right, pagenum_t rightnum);


// Deletion

int get_neighbor_index(pagenum_t node_num, page_t* parent);
void adjust_root(page_t* root, pagenum_t rootnum);
void merge_nodes(page_t* node, pagenum_t node_num, page_t* neighbor, pagenum_t neighbor_num, int neighbor_index, int64_t k_prime);
void redistribute_nodes(page_t* node, pagenum_t node_num, page_t* neighbor, pagenum_t neighbor_num, int neighbor_index, int k_prime_index, int64_t k_prime);
void remove_entry_from_node(page_t *node, int64_t key, void* pointers);
void delete_entry(pagenum_t leafnum, int64_t key, void* pointers);


#endif /* __BPT_H__*/
