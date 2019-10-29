#include "bpt.h"
#include "buffermanager.h"


#ifdef DEBUG

int queue_front, queue_rear, queue_size;
pagenum_t* queue = NULL;
int* queue_lv = NULL;

#endif

int selected_table = 0;


//////////////////////////////////////////
/// UTILITY
//////////////////////////////////////////


int cut(int length) {
	if (length % 2 == 0)
		return length / 2;
	else
		return length / 2 + 1;
}

void select_table(int table_id)
{
	if (buffer_is_table_opened(table_id) == 0) {
#ifdef DEBUG
		printf("Error: Tried to select unopened table.\n");
#endif
		exit(EXIT_FAILURE);
	}

	selected_table = table_id;
	return;
}

/// <summary>
/// find leaf that may contain key
/// </summary>
/// <param name="key"> key </param>
/// <returns> page number of leaf if success, otherwise 0</returns>
pagenum_t find_leaf(int64_t key)
{
	int i = 0;
	page_t *cur;
	pagenum_t cur_num = 0, next_num = 0;
	pagenum_t* pagenums = NULL;

	cur = buffer_read_page(selected_table, 0);

	if (selected_table == 2 && key == 17413) {
		printf("BREAK");
	}

	// Empty Tree
	if (cur->rootpage == 0) {
#ifdef DEBUG
		printf("Empty Tree.\n");
#endif
		buffer_unpin_page(selected_table, 0);
		return 0;
	}

	cur_num = cur->rootpage;

	buffer_unpin_page(selected_table, 0);

	cur = buffer_read_page(selected_table, cur_num);

	while (!cur->is_leaf) {
		i = 0;
		while (i < cur->num_keys) {
			if (key >= cur->keys[i]) i++;
			else break;
		}

		pagenums = cur->pointers;
		next_num = pagenums[i];
		buffer_unpin_page(selected_table, cur_num);
		cur_num = next_num;
		cur = buffer_read_page(selected_table, cur_num);
	}

	buffer_unpin_page(selected_table, cur_num);

	if (selected_table == 2 && key == 17413) {
		cur = buffer_read_page(selected_table, cur_num);
		buffer_unpin_page(selected_table, cur_num);
	}

	return cur_num;
}

/// <summary>
/// find record match with key
/// </summary>
/// <param name="key"> key </param>
/// <returns> record pointer if success, NULL otherwise 
/// Caution! returned pointer should be free </returns>
record* find(int64_t key)
{
	int i;
	page_t *cur;
	pagenum_t leaf = find_leaf(key);
	record* records = NULL;
	record* cast = NULL;

	// find_leaf failed
	if (leaf == 0) {
		return NULL;
	}

	cur = buffer_read_page(selected_table, leaf);

	for (i = 0; i < cur->num_keys; i++) {
		if (cur->keys[i] == key)
			break;
	}

	// no same key
	if (i == cur->num_keys) {
		buffer_unpin_page(selected_table, leaf);
		return NULL;
	}

	records = malloc(sizeof(record));

	if (records == NULL) {
#ifdef DEBUG
		printf("Error: Failed to alloc temporary record\n");
#endif
		exit(EXIT_FAILURE);
	}

	cast = cur->pointers;
	memcpy(records, &(cast[i]), sizeof(record));

	buffer_unpin_page(selected_table, leaf);

	return records;
}

#ifdef DEBUG

void print_tree(int table_id)
{
	int i, lv, prv_lv = 0;
	page_t *cur;
	pagenum_t pnum;
	pagenum_t* pagenums;
	if (queue == NULL) {
		queue = malloc(sizeof(int64_t) * 5000);
		queue_lv = malloc(sizeof(int) * 5000);
		queue_size = 5000;
	}

	if (queue == NULL || queue_lv == NULL) {
		printf("Error: Failed to malloc queue in print_tree\n");
		exit(EXIT_FAILURE);
	}

	queue_front = queue_rear = 0;

	cur = buffer_read_page(table_id, 0);

	if (cur->rootpage == 0) {
		printf("Empty Tree!\n");
		return;
	}

	queue[queue_rear] = cur->rootpage;
	queue_lv[queue_rear++] = 0;

	buffer_unpin_page(table_id, 0);

	while (queue_front != queue_rear) {
		pnum = queue[queue_front];
		lv = queue_lv[queue_front++];

		if (prv_lv != lv) {
			printf("\n");
		}

		queue_front %= queue_size;
		cur = buffer_read_page(table_id, pnum);

		for (i = 0; i < cur->num_keys; i++) {
			printf("%lld ", cur->keys[i]);
		}
		printf("| ");

		prv_lv = lv;

		pagenums = cur->pointers;
		for (i = 0; i <= cur->num_keys && cur->is_leaf == 0; i++) {
			queue[queue_rear] = pagenums[i];
			queue_lv[queue_rear++] = lv + 1;
			queue_rear %= queue_size;
		}

		buffer_unpin_page(table_id, pnum);
	}

	printf("\n");

	return;
}

#endif

//////////////////////////////////////////
/// INSERTION
//////////////////////////////////////////

/// <summary>
/// make new node
/// </summary>
/// <param name="isLeaf"> set new node into leaf </param>
/// <returns> new node's pointer </returns>
void init_page(page_t* new_page, bool isLeaf)
{
	if (isLeaf) {
		new_page->is_leaf = 1;
		new_page->keys = malloc(sizeof(int64_t) * LEAF_ORDER);
		new_page->pointers = malloc(sizeof(record) * LEAF_ORDER);
	}
	else {
		new_page->is_leaf = 0;
		new_page->keys = malloc(sizeof(int64_t) * INTERNAL_ORDER);
		new_page->pointers = malloc(sizeof(pagenum_t) * (INTERNAL_ORDER + 1));
	}

	if (new_page->keys == NULL || new_page->pointers == NULL) {
#ifdef DEBUG
		printf("Error: Failed to alloc keys and pointers in make_page()\n");
#endif
		exit(EXIT_FAILURE);
	}

	new_page->is_header = false;
	new_page->num_keys = 0;
	new_page->parent = 0;
	new_page->another_page = 0;
}

/// <summary>
/// return index of child in parent
/// </summary>
/// <param name="parent"> parent </param>
/// <param name="leftnum"> page number of child </param>
/// <returns></returns>
int get_left_index(page_t *parent, pagenum_t leftnum)
{
	pagenum_t* pagenums;
	int left_index = 0;

	pagenums = parent->pointers;

	while (left_index <= parent->num_keys &&
		pagenums[left_index] != leftnum)
		left_index++;

	return left_index;
}

/// <summary>
/// start new tree
/// </summary>
/// <param name="key"> key </param>
/// <param name="value"> value </param>
void start_new_tree(int64_t key, const char* value)
{
	page_t* header;
	page_t* root;
	record* records;
	pagenum_t rootnum;

	// Read header, Alloc new root, Set root page number
	rootnum = buffer_alloc_page(selected_table);
	root = buffer_read_page(selected_table, rootnum);
	init_page(root, true);

	header = buffer_read_page(selected_table, 0);
	header->rootpage = rootnum;
	header->num_page++;

	buffer_set_dirty_page(selected_table, 0);
	buffer_unpin_page(selected_table, 0);


	// Set root

	root->parent = 0;
	root->another_page = 0;
	root->keys[0] = key;

	records = root->pointers;
	memcpy(&(records[0].value), value, VALUE_SIZE);

	root->num_keys++;

	buffer_set_dirty_page(selected_table, rootnum);
	buffer_unpin_page(selected_table, rootnum);

	return;
}


/// <summary>
/// insert key, value pair into leaf
/// </summary>
/// <param name="leafnum"> page number of leaf </param>
/// <param name="key"> key </param>
/// <param name="value"> value </param>
void insert_into_leaf(pagenum_t leafnum, int64_t key, const char* value)
{
	int i, insertion_point;
	page_t *leaf;
	record* records;

	leaf = buffer_read_page(selected_table, leafnum);
	insertion_point = 0;

	while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key) {
		insertion_point++;
	}

	records = leaf->pointers;
	for (i = leaf->num_keys; i > insertion_point; i--) {
		leaf->keys[i] = leaf->keys[i - 1];
		records[i] = records[i - 1];
	}

	leaf->keys[insertion_point] = key;
	memcpy(&(records[i]), value, VALUE_SIZE);
	leaf->num_keys++;

	buffer_set_dirty_page(selected_table, leafnum);
	buffer_unpin_page(selected_table, leafnum);

	return;
}

/// <summary>
/// insert key, value pair into leaf after leaf split, only for leaf
/// </summary>
/// <param name="leafnum"> page number of leaf </param>
/// <param name="key"> key </param>
/// <param name="value"> value </param>
void insert_into_leaf_after_splitting(pagenum_t leafnum, int64_t key, const char* value)
{
	int i, j, split, insertion_index;
	int64_t new_key;
	page_t* cur;
	page_t* new_leaf;
	pagenum_t new_leaf_num;
	int64_t* temp_keys;
	record* temp_values, *records;

	
	cur = buffer_read_page(selected_table, leafnum);
	new_leaf_num = buffer_alloc_page(selected_table);
	new_leaf = buffer_read_page(selected_table, new_leaf_num);
	init_page(new_leaf, true);
	
	temp_keys = malloc(LEAF_ORDER * sizeof(int64_t));
	temp_values = malloc(LEAF_ORDER * sizeof(record));

	if (temp_keys == NULL || temp_values == NULL) {
#ifdef DEBUG
		printf("Error: Failed to alloc temporary keys and values in insert_into_leaf_after_splitting(for leaf)\n");
#endif
		exit(EXIT_FAILURE);
	}

	insertion_index = 0;
	while (insertion_index < LEAF_ORDER - 1 && cur->keys[insertion_index] < key) {
		insertion_index++;
	}

	records = cur->pointers;
	for (i = 0, j = 0; i < cur->num_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_keys[j] = cur->keys[i];
		temp_values[j] = records[i];
	}

	temp_keys[insertion_index] = key;
	memcpy(&(temp_values[insertion_index].value), value, VALUE_SIZE);

	cur->num_keys = 0;

	split = cut(LEAF_ORDER - 1);

	records = cur->pointers;
	for (i = 0; i < split; i++) {
		cur->keys[i] = temp_keys[i];
		records[i] = temp_values[i];
		cur->num_keys++;
	}

	records = new_leaf->pointers;
	for (i = split, j = 0; i < LEAF_ORDER; i++, j++) {
		new_leaf->keys[j] = temp_keys[i];
		records[j] = temp_values[i];
		new_leaf->num_keys++;
	}

	free(temp_keys);
	free(temp_values);

	new_leaf->another_page = cur->another_page;
	cur->another_page = new_leaf_num;

	new_leaf->parent = cur->parent;
	new_key = new_leaf->keys[0];

	buffer_set_dirty_page(selected_table, leafnum);
	buffer_set_dirty_page(selected_table, new_leaf_num);

	insert_into_parent(cur, leafnum, new_key, new_leaf, new_leaf_num);

	buffer_unpin_page(selected_table, leafnum);
	buffer_unpin_page(selected_table, new_leaf_num);

	return;
}

/// <summary>
/// insert key and rightnum into left_index of parent
/// </summary>
/// <param name="parent"> parent </param>
/// <param name="left_index"> insertion point </param>
/// <param name="key"> key </param>
/// <param name="rightnum"> right num</param>
void insert_into_node(page_t* parent, pagenum_t parentnum, int left_index, int64_t key, pagenum_t rightnum)
{
	int i;
	pagenum_t* pagenums = parent->pointers;
	
	for (i = parent->num_keys; i > left_index; i--) {
		pagenums[i + 1] = pagenums[i];
		parent->keys[i] = parent->keys[i - 1];
	}
	pagenums[left_index + 1] = rightnum;
	parent->keys[left_index] = key;
	parent->num_keys++;

	buffer_set_dirty_page(selected_table, parentnum);

	return;
}

/// <summary>
/// insert key and child into parent 
/// </summary>
/// <param name="parent"> parent node </param>
/// <param name="parentnum"> page number of parent node </param>
/// <param name="left_index"> insertion point </param>
/// <param name="key"> key </param>
/// <param name="rightnum"> page number of child </param>
void insert_into_node_after_splitting(page_t* parent, pagenum_t parentnum, int left_index, int64_t key, pagenum_t rightnum)
{
	int i, j, split;
	int64_t k_prime;
	int64_t* temp_keys;
	page_t* new_node;
	page_t* child;
	pagenum_t new_node_num;
	pagenum_t* temp_pagenums, *pagenums;

	temp_pagenums = malloc((INTERNAL_ORDER + 1) * sizeof(pagenum_t));
	temp_keys = malloc(INTERNAL_ORDER * sizeof(int64_t));

	if (temp_keys == NULL || temp_pagenums == NULL) {
#ifdef DEBUG
		printf("Error: Failed to alloc temporary keys and pagenums in insert_into_node_after_splitting(for internal)\n");
#endif
		exit(EXIT_FAILURE);
	}

	pagenums = parent->pointers;
	for (i = 0, j = 0; i < parent->num_keys + 1; i++, j++) {
		if (j == left_index + 1) j++;
		temp_pagenums[j] = pagenums[i];
	}

	for (i = 0, j = 0; i < parent->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_keys[j] = parent->keys[i];
	}
	temp_pagenums[left_index + 1] = rightnum;
	temp_keys[left_index] = key;

	split = cut(INTERNAL_ORDER);

	new_node_num = buffer_alloc_page(selected_table);
	new_node = buffer_read_page(selected_table, new_node_num);
	init_page(new_node, false);

	parent->num_keys = 0;
	pagenums = parent->pointers;
	for (i = 0; i < split - 1; i++) {
		pagenums[i] = temp_pagenums[i];
		parent->keys[i] = temp_keys[i];
		parent->num_keys++;
	}
	pagenums[i] = temp_pagenums[i];
	k_prime = temp_keys[split - 1];

	pagenums = new_node->pointers;
	for (++i, j = 0; i < INTERNAL_ORDER; i++, j++) {
		pagenums[j] = temp_pagenums[i];
		new_node->keys[j] = temp_keys[i];
		new_node->num_keys++;
	}
	pagenums[j] = temp_pagenums[i];

	free(temp_pagenums);
	free(temp_keys);

	new_node->parent = parent->parent;
	pagenums = new_node->pointers;
	for (i = 0; i <= new_node->num_keys; i++) {
		child = buffer_read_page(selected_table, pagenums[i]);
		child->parent = new_node_num;
		buffer_set_dirty_page(selected_table, pagenums[i]);
		buffer_unpin_page(selected_table, pagenums[i]);
	}

	buffer_set_dirty_page(selected_table, parentnum);
	buffer_set_dirty_page(selected_table, new_node_num);

	insert_into_parent(parent, parentnum, k_prime, new_node, new_node_num);

	buffer_unpin_page(selected_table, new_node_num);

	return;
}

/// <summary>
/// Insert key into parent of left and right.
/// </summary>
/// <param name="left"> left child page </param>
/// <param name="leftnum"> page number of left </param>
/// <param name="key"> key will be insert into parent </param>
/// <param name="right"> right child page </param>
/// <param name="rightnum"> page number of right </param>
void insert_into_parent(page_t* left, pagenum_t leftnum, int64_t key, page_t* right, pagenum_t rightnum)
{
	int left_index;
	page_t* parent;
	pagenum_t parentnum;

	parentnum = left->parent;

	// Case: new root
	if (parentnum == 0) {
		insert_into_new_root(left, leftnum, key, right, rightnum);
		return;
	}

	parent = buffer_read_page(selected_table, parentnum);
	left_index = get_left_index(parent, leftnum);

	if (parent->num_keys < INTERNAL_ORDER - 1) {
		insert_into_node(parent, parentnum, left_index, key, rightnum);
	}
	else {
		insert_into_node_after_splitting(parent, parentnum, left_index, key, rightnum);
	}

	buffer_unpin_page(selected_table, parentnum);

	return;
}

/// <summary>
/// Create new root and insert left, key, right
/// </summary>
/// <param name="left"> left child </param>
/// <param name="leftnum"> page number of left child </param>
/// <param name="key"> key </param>
/// <param name="right"> right child </param>
/// <param name="rightnum"> page number of right child</param>
void insert_into_new_root(page_t* left, pagenum_t leftnum, int64_t key, page_t* right, pagenum_t rightnum)
{
	page_t* header;
	pagenum_t root_num = buffer_alloc_page(selected_table);
	page_t* root = buffer_read_page(selected_table, root_num);
	int64_t* pagenums;

	init_page(root, false);

	pagenums = root->pointers;

	root->keys[0] = key;
	pagenums[0] = leftnum;
	pagenums[1] = rightnum;
	root->num_keys++;
	root->parent = 0;
	left->parent = root_num;
	right->parent = root_num;

	buffer_set_dirty_page(selected_table, leftnum);
	buffer_set_dirty_page(selected_table, rightnum);
	buffer_set_dirty_page(selected_table, root_num);
	buffer_unpin_page(selected_table, root_num);

	header = buffer_read_page(selected_table, 0);
	header->rootpage = root_num;

	buffer_unpin_page(selected_table, 0);

	return;
}


//////////////////////////////////////////
/// DELETION
//////////////////////////////////////////

int get_neighbor_index(pagenum_t node_num, page_t* parent)
{
	int i;
	pagenum_t* pagenums;

	pagenums = parent->pointers;
	for (i = 0; i <= parent->num_keys; i++) {
		if (pagenums[i] == node_num)
			return i - 1;
	}

#ifdef DEBUG
	printf("Error: Failed to find node in its parent.\n");
#endif
	exit(EXIT_FAILURE);
}

void adjust_root(page_t* root, pagenum_t rootnum)
{

	page_t* header, *newroot;
	pagenum_t newrootnum;
	pagenum_t* pagenums;

	// Ok.
	if (root->num_keys > 0) {
		return;
	}

	// Back to Empty Tree
	if (root->is_leaf == 1) {
		buffer_free_page(selected_table, rootnum);
		header = buffer_read_page(selected_table, 0);
		header->rootpage = 0;
		buffer_set_dirty_page(selected_table, 0);
		buffer_unpin_page(selected_table, 0);
		return;
	}
	// make first child root
	else {
		pagenums = root->pointers;
		newrootnum = pagenums[0];
		header = buffer_read_page(selected_table, 0);
		newroot = buffer_read_page(selected_table, newrootnum);
		header->rootpage = newrootnum;
		newroot->parent = 0;


		buffer_set_dirty_page(selected_table, 0);
		buffer_set_dirty_page(selected_table, newrootnum);
		buffer_free_page(selected_table, rootnum);
	}
}

void merge_nodes(page_t* node, pagenum_t node_num, page_t* neighbor, pagenum_t neighbor_num, int neighbor_index, int64_t k_prime)
{
	int i, j, neighbor_insection_index, n_end;
	page_t* tmp_page;
	page_t* child;
	pagenum_t tmp_pagenum;
	pagenum_t* pagenums1, * pagenums2;
	record* records1, * records2;


	if (neighbor_index == -1) {
		tmp_page = node;
		node = neighbor;
		neighbor = tmp_page;

		tmp_pagenum = node_num;
		node_num = neighbor_num;
		neighbor_num = tmp_pagenum;
	}

	neighbor_insection_index = neighbor->num_keys;

	// Case: Internal node
	if (node->is_leaf == 0) {
		neighbor->keys[neighbor_insection_index] = k_prime;
		neighbor->num_keys++;

		n_end = node->num_keys;

		pagenums1 = neighbor->pointers;
		pagenums2 = node->pointers;
		for (i = neighbor_insection_index + 1, j = 0; j < n_end; i++, j++) {
			neighbor->keys[i] = node->keys[j];
			pagenums1[i] = pagenums2[j];
			neighbor->num_keys++;
			node->num_keys--;
		}

		pagenums1[i] = pagenums2[j];

		for (i = 0; i < neighbor->num_keys + 1; i++) {
			child = buffer_read_page(selected_table, pagenums1[i]);
			child->parent = neighbor_num;
			buffer_set_dirty_page(selected_table, pagenums1[i]);
			buffer_unpin_page(selected_table, pagenums1[i]);
		}

	}

	// Case : Leaf node
	else {

		records1 = neighbor->pointers;
		records2 = node->pointers;
		for (i = neighbor_insection_index, j = 0; j < node->num_keys; i++, j++) {
			neighbor->keys[i] = node->keys[j];
			records1[i] = records2[j];
			neighbor->num_keys++;
		}

		neighbor->another_page = node->another_page;
	}

	buffer_set_dirty_page(selected_table, neighbor_num);
	buffer_free_page(selected_table, node_num);

	delete_entry(neighbor->parent, k_prime, &node_num);

	return;
}

void redistribute_nodes(page_t* node, pagenum_t node_num, page_t* neighbor, pagenum_t neighbor_num, int neighbor_index, int k_prime_index, int64_t k_prime)
{
	int i;
	page_t *child, *parent;
	pagenum_t* pagenums1, *pagenums2;
	record* records1, * records2;

	if (neighbor_index != -1) {
		if (node->is_leaf == 0) {
			pagenums1 = node->pointers;
			pagenums2 = neighbor->pointers;
			pagenums1[node->num_keys + 1] = pagenums1[node->num_keys];
		}
		else {
			records1 = node->pointers;
			records2 = neighbor->pointers;
		} 

		for (i = node->num_keys; i > 0; i--) {
			node->keys[i] = node->keys[i - 1];
			if (node->is_leaf == 0) pagenums1[i] = pagenums1[i - 1];
			else records1[i] = records1[i - 1];
		}

		if (node->is_leaf == 0) {
			pagenums1[0] = pagenums2[neighbor->num_keys];
			child = buffer_read_page(selected_table, pagenums1[0]);
			child->parent = node_num;
			buffer_set_dirty_page(selected_table, pagenums1[0]);
			buffer_unpin_page(selected_table, pagenums1[0]);

			node->keys[0] = k_prime;

			parent = buffer_read_page(selected_table, node->parent);
			parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
			buffer_set_dirty_page(selected_table, node->parent);
			buffer_unpin_page(selected_table, node->parent);
		}
		else {
			records1[0] = records2[neighbor->num_keys - 1];
			node->keys[0] = neighbor->keys[neighbor->num_keys - 1];

			parent = buffer_read_page(selected_table, node->parent);
			parent->keys[k_prime_index] = node->keys[0];
			buffer_set_dirty_page(selected_table, node->parent);
			buffer_unpin_page(selected_table, node->parent);
		}	
	}
	else {
		if (node->is_leaf == 1) {
			node->keys[node->num_keys] = neighbor->keys[0];
			records1 = node->pointers;
			records2 = neighbor->pointers;

			records1[node->num_keys] = records2[0];

			parent = buffer_read_page(selected_table, node->parent);
			parent->keys[k_prime_index] = neighbor->keys[1];
			buffer_set_dirty_page(selected_table, node->parent);
			buffer_unpin_page(selected_table, node->parent);
		}
		else {
			node->keys[node->num_keys] = k_prime;
			pagenums1 = node->pointers;
			pagenums2 = neighbor->pointers;

			pagenums1[node->num_keys + 1] = pagenums2[0];
			
			child = buffer_read_page(selected_table, pagenums1[node->num_keys + 1]);
			child->parent = node_num;
			buffer_set_dirty_page(selected_table, pagenums1[node->num_keys + 1]);
			buffer_unpin_page(selected_table, pagenums1[node->num_keys + 1]);

			parent = buffer_read_page(selected_table, node->parent);
			parent->keys[k_prime_index] = neighbor->keys[0];
			buffer_set_dirty_page(selected_table, node->parent);
			buffer_unpin_page(selected_table, node->parent);
		}

		for (i = 0; i < neighbor->num_keys - 1; i++) {
			neighbor->keys[i] = neighbor->keys[i + 1];
			if (node->is_leaf == 1) {
				records2[i] = records2[i + 1];
			}
			else {
				pagenums2[i] = pagenums2[i + 1];
			}
		}

		if (node->is_leaf == 0) {
			pagenums2[i] = pagenums2[i + 1];
		}
	}

	node->num_keys++;
	neighbor->num_keys--;
	
	buffer_set_dirty_page(selected_table, node_num);
	buffer_set_dirty_page(selected_table, neighbor_num);

	return;
}

/// <summary>
/// Delete key from leaf
/// </summary>
/// <param name="leafnum"> page number of leaf that contain key </param>
/// <param name="key"> key </param>
void remove_entry_from_node (page_t* node, pagenum_t node_num, int64_t key, void* pointers)
{
	int i;
	pagenum_t target;
	pagenum_t* pagenums;
	record* records;

	// delete key and value
	i = 0;
	while (node->keys[i] != key) {
		i++;
	}

	// Case: leaf node, key and record has same location
	if (node->is_leaf) {
		records = node->pointers;
		for (i++; i < node->num_keys; i++) {
			node->keys[i - 1] = node->keys[i];
			records[i - 1] = records[i];
		}
	}
	// Case : internal node, find child's pagenum and delete it
	else {
		for (i++; i < node->num_keys; i++) {
			node->keys[i - 1] = node->keys[i];
		}

		i = 0;
		target = *(pagenum_t*)pointers;
		pagenums = node->pointers;
		while (pagenums[i] != target)
			i++;
		for (i++; i < node->num_keys + 1; i++)
			pagenums[i - 1] = pagenums[i];
	}

	node->num_keys--;

	buffer_set_dirty_page(selected_table, node_num);
}

/// <summary>
/// Delete key and value from B+ Tree
/// </summary>
/// <param name="leafnum"> leaf that contain key want to delete </param>
/// <param name="key"> key want to delete </param>
void delete_entry(pagenum_t leafnum, int64_t key, void* pointers)
{
	int min_keys;
	int neighbor_index;
	pagenum_t neighbor_num;
	pagenum_t* pagenums;
	int k_prime_index;
	int64_t k_prime;
	page_t *cur, *parent, *header, *neighbor;
	int capacity;

	cur = buffer_read_page(selected_table, leafnum);
	remove_entry_from_node(cur, leafnum, key, pointers);


	header = buffer_read_page(selected_table, 0);
	if (header->rootpage == leafnum) {
		adjust_root(cur, leafnum);

		buffer_unpin_page(selected_table, leafnum);
		return;
	}

	// Delayed Merge
	min_keys = 1; // cur.is_leaf == 1 ? cut(order - 1) : (cut(order) - 1);

	// node satisfy minimum keys condition
	if (cur->num_keys >= min_keys) {
		buffer_unpin_page(selected_table, leafnum);

		return;
	}

	parent = buffer_read_page(selected_table, cur->parent);

	neighbor_index = get_neighbor_index(leafnum, parent);
	k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
	k_prime = parent->keys[k_prime_index];
	pagenums = parent->pointers;
	neighbor_num = neighbor_index == -1 ? pagenums[1] : pagenums[neighbor_index];
	capacity = cur->is_leaf == 1 ? LEAF_ORDER : (INTERNAL_ORDER - 1);

	buffer_unpin_page(selected_table, cur->parent);

	neighbor = buffer_read_page(selected_table, neighbor_num);

	if (neighbor->num_keys + cur->num_keys < 2) {
		merge_nodes(cur, leafnum, neighbor, neighbor_num, neighbor_index, k_prime);
	}
	else {
		redistribute_nodes(cur, leafnum, neighbor, neighbor_num, neighbor_index, k_prime_index, k_prime);
	}

	buffer_unpin_page(selected_table, leafnum);
	buffer_unpin_page(selected_table, neighbor_num);

	return;
}
