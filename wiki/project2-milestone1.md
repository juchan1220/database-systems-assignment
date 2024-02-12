# Project 2: Analyze B+ Tree Code
2018008277 CSE 노주찬<br><br>

## 1. Possible call path of the insert / delete operation
### 1.1. Insert operation
모든 Insert Operation은 `Insert(node *root, int key, int value)` 를 통해서 일어나며, key는 value를 그대로 사용한다. Insert Operation 중 root가 변경될 수 있기 때문에 Insert Operation을 처리하는 함수는 Operation 이후 변경된 root를 반환한다.

#### 1.1.1. Case 0. Duplicate key
주어진 B+ Tree 에서는 key가 중복되는 것을 허용하지 않는다. 그러므로 `Insert` 의 첫 부분에서는 `find( node * root, int key, bool verbose )` 를 통해서 입력으로 주어진 key가 이미 B+ Tree 내부에 존재하는지 확인한다. key가 이미 존재하는 경우에는 Insert Operation 을 수행하지 않고 root를 반환한다. Key가 존재하지 않는 경우 Tree 에 삽입할 record를 `make_record(int value)` 를 사용하여 만든다. `make_record`는 record를 동적할당하여 value를 담고 그 pointer를 반환한다.

#### 1.1.2. Case 1. Empty tree
만약 root가 NULL 이라면 아직 tree가 만들어지지 않은 것이므로 `start_new_tree(int key, record * pointer)` 를 통해 root를 새로 만들고 이를 반환한다.  `make_node( void )` 는  order-1 개의 key를 저장할 공간, order 개의 pointer를 저장할 공간을 지니는 새 node를 할당하여 반환하고, `make_leaf( void )` 에서는 `make_node` 를 호출하여 반환된 node를 leaf로 설정하고 반환한다. `start_new_tree` 안에서는 `make_leaf` 를 호출하여 반환된 leaf 안에 key와 record의 pointer를 insert한다.

#### 1.1.3. Case 2. Leaf has room for key and pointer
이미 tree가 존재하는 경우에는 그 안에 새로운 record를 삽입해야하므로 `find_leaf( node * root, int key, bool verbose )` 를 통해 record가 insert 되어야 할 적절한 leaf를 찾는다. `find_leaf` 는 node를 선형탐색하면서 key보다 작거나 같은 keys 중에서 가장 큰 쪽이 가르키는 pointer를 타고 가면서 leaf 에 도착할 때까지 내려간다. 삽입될 leaf를 찾은 다음에 그 leaf에 key와 pointer를 삽입할 자리가 남아있는지 (node에 있는 key의 갯수가 Tree의 order - 1보다 작은지) 확인한다. 만약 삽입될 자리가 남아있는 경우 `insert_into_leaf( node * leaf, int key, record * pointer)` 를 호출하여 insert 한다. `insert_into_leaf`에서는 `find_leaf`와 비슷하게 선형탐색으로 insertion point를 찾은 다음에 그 뒤에 있는 key와 pointer를 한 칸씩 뒤로 밀고 새로운 key와 record의 pointer를 insert 한다.

#### 1.1.4. Case 3. Leaf must be split
Case 2 에서 찾은 leaf에서 만약 node가 Tree의 order - 1 개의 key를 가지고 있어서 더 이상 insert할 공간이 없을 경우 `insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer)` 를 호출하여 node를 split하고 insert 하게 된다. 자세한 내용은 [2.1 Detail flow of Split](/home#21-detail-flow-of-split) 에서 다루고 있다.

<br><br>
### 1.2. Delete operation 
모든 Delete Operation은 `delete(node * root, int key)` 를 통해서 일어난다. Delete Operation 중 root가 변경될 수 있기 때문에 Delete Operation을 처리하는 함수는 Operation 이후 변경된 root를 반환한다. 먼저, `find(root, key, false)` 와 `find_leaf(root, key, false)` 를 통해서 Delete 하고자 하는 key가 B+ Tree에 존재하는지 확인한다. 만약 존재하지 않으면 Delete Operation을 진행하지 않고, 존재한다면 `delete_entry( node * root, node * n, int key, void * pointer )` 를 호출하여 Delete Operation을 수행한다.

`delete_entry( node * root, node * n, int key, void * pointer )` 에서는 먼저 `remove_entry_from_node(node * n, int key, node * pointer)` 를 통해 지우고자 하는 record를 node에서 제거한다. 

#### 1.2.1. Case 1. root
root에서 제거가 일어난 경우, `adjust_root(node * root)` 를 통해서 root를 조정하고 Operation을 마친다. key를 제거한 이후에도 root에 key가 1개 이상 남아있다면 기존 root를 그대로 사용한다. 만약 key가 없는 empty root가 된 경우, root의 첫번째 자식을 new_root로 설정해준다. root에 자식이 없는 경우에는 비어있는 트리가 되기 때문에 NULL을 new_root로 설정한다. 작업 이후에는 empty root를 할당해제한다.

#### 1.2.2. Case 2. node stays at or above minimum
B+ Tree의 고른 분포를 위해서 각 node들은 order와 비례한 최소갯수의 key를 가지고 있어야한다. leaf의 경우에는 $` \lfloor \frac{order}{2} \rfloor `$ 개, internal node의 경우에는 $` \lfloor \frac{order - 1}{2} \rfloor `$ 개를 가지고 있어야한다. node에서 key를 제거한 이후에도 최소 갯수 이상의 key를 가지고 있다면 조정할 필요가 없으니 그대로 root를 반환하여 Operation을 마친다. 

#### 1.2.3. Case 3. node falls below minimum
node에서 key를 제거한 이후에 node가 최소 갯수의 key를 가지고 있지 못 할 경우 주변 node와 merge 하거나 redistribute를 해서 B+ Tree의 분포를 고르게 한다. 자세한 내용은 [2.2 Detail flow of Merge](/home#22-detail-flow-of-merge) 에서 다루고 있다.

<br><br><br>
## 2. Detail flow of the structure modification
### 2.1. Detail flow of split
record를 insert할 leaf에 이미 order - 1 개의 key가 있다면 1개의 node에서 저장할 수 있는 key의 갯수제한을 넘기 때문에 split 과정이 필요하다. Split이 일어나는 과정을 함수의 흐름과 Case에 따라서 정리하였다.

#### 2.1.1. insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer)
이 함수는 갯수 제한을 넘어선 leaf를 2개로 나누는 과정을 담당한다. 먼저 2개의 leaf로 나눠야하기 때문에 새로운 leaf인 new_leaf와 order 개의 key와 pointer를 담아둘 수 있는 공간인 temp_keys와 temp_pointers 를 할당한다. 그리고 원래 node가 가지고 있던 key와 pointer, 그리고 새로 insert할 key와 pointer를 합쳐서 temp_keys와 temp_pointers에 동적할당한다. 합치는 과정은 `insert_into_leaf` 와 동일하게 선형탐색으로 insertion point를 찾고 그 뒤의 것들을 뒤로 한칸 씩 밀어낸 다음 insert하는 과정을 거친다. 그 다음 `cut( int length )` 를 이용해 temp_keys와 temp_pointer에 있는 order 개의 자료들을 절반으로 나누어 앞의 절반은 leaf에, 뒤의 절반은 new_leaf에 저장한다. 이제 더 이상 사용할 일이 없는 temp_keys와 temp_pointer의 할당을 해제한다. leaf 끼리 이어주는 order번째 pointer를 prv -> leaf -> next 의 형태에서 prv -> leaf -> new_leaf -> next 형태로 이어지도록 한다. 마지막으로 new_leaf의 parent를 leaf의 parent로 설정하고, new_leaf의 첫번째 key를 new_key로 설정한 다음 `insert_into_parent(root, leaf, new_key, new_leaf)` 를 호출한다.

#### 2.1.2. insert_into_parent(node * root, node * left, int key, node * right)
이 함수는 원래 leaf (left)의 parent에 새롭게 생긴 leaf (right) 와 key를 적절하게 insert하는 과정을 담당한다. 

##### 2.1.2.1. Case 0. new root
원래 leaf의 parent가 NULL. 즉, 원래 leaf가 root 였던 경우에는 새로운 root를 만들어서 그 위치에 key를 insert한다. `insert_into_new_root(node * left, int key, node * right)` 를 통해서 새롭게 root를 만든 다음, left와 right를 insert하고 각각의 parent를 root로 설정한 다음 반환한다.

##### 2.1.2.2. Case 1. new key fits into the node
원래 leaf의 parent는 node일 것이고, 그 node가 가지고 있는 pointer 중 하나는 left를 가리키고 있을 것이다. 'get_left_index(node * parent, node * left)' 를 통해서 몇번째 포인터가 left를 가지고 있는지를 가져온다. 원래 leaf의 parent에 key를 넣을 수 있는 공간이 있는 경우 (key를 order - 1 개 미만으로 가지고 있는 경우), `insert_into_node(node * root, node * n, int left_index, int key, node * right)` 를 통해 key를 insert 한다. 새로운 key가 left의 원래 key와 그 다음 key 사이의 값임이 보장되므로 `insert_into_node` 에서는 left_index 뒤의 key를 전부 뒤로 한 칸씩 밀고 해당 자리에 새로운 key를 insert한다.

##### 2.1.2.3. Case 2. split a node
원래 leaf의 parent가 order - 1개의 key를 가지고 있어서 새로운 key를 넣을 수 없는 경우에는, `insert_into_node_after_splitting(node * root, node * old_node, int left_index, int key, node * right)` 를 통해 해당 node를 split 하고 새로운 key를 insert한다.

#### 2.1.3. insert_into_node_after_splitting(node * root, node * old_node, int left_index, int key, node * right)
이 함수는 상술한 같은 이름의 함수와 마찬가지로 갯수 제한을 넘어선 node를 2개로 나누는 과정을 담당한다. 임시로 key, pointer를 담아둘 공간 temp_keys와 temp_pointers를 할당받는다. 그리고 새로운 key, pointer (right) 를 기존 old_node가 가지고 있던 key, pointer를 합쳐 temp_keys, temp_pointers 에 저장해둔다. `make_node` 를 통해 새로운 node를 할당받는다. 그 다음 `cut( int length )` 를 이용해 temp_keys와 temp_pointer에 있는 order 개의 자료들을 절반으로 나누어 앞의 절반은 old_node에, 뒤의 절반은 new_node에 저장한다. 이 때, leaf의 경우와는 달리 가운데의 key 하나를 old_node, new_node 어느쪽에도 넣지 않고 parent에 insert하고 이 둘을 구분하는 key (k_prime) 으로서 사용한다. 그리고 new_node의 parent를 node의 parent로 설정해주고 new_node의 child가 된 node들의 parent를 new_node로 설정해준다. 더 이상 사용하지 않는 temp_keys와 temp_pointers를 해제한다. 마지막으로 이전에 정한 k_prime을 `insert_into_parent(root, old_node, k_prime, new_node)` 을 통해 old_node, new_node의 parent에 insert한다. 이렇게 B+ Tree의 특성을 만족할 때까지 (2.1.2의 Case 0, 또는 Case 1에 도달하기 전까지) Case 2는 2.1.3 과 2.1.2 과정을 반복하게 된다.

<br><br>
### 2.2. Detail flow of Merge
node에서 key를 제거한 이후에 node가 order에 비례하는 최소 갯수의 key를 가지고 있지 않다면 merge 또는 redistribution 을 통해서 B+ Tree를 고르게 만드는 과정이 필요하다. 이러한 과정을 함수의 흐름과 Case에 따라서 정리하였다.

먼저 `get_neighbor_index( node * n )` 과 몇가지 처리를 통해서 현재 node의 이웃 node, 그리고 parent에서 이 둘을 구분하는 key (k_prime) 를 구한다. node가 parent의 첫번째 child 라면 두번째 child가 이웃 노드, 그렇지 않다면 현재 node의 바로 전 node가 이웃 노드가 된다. 현재 node와 이웃 node를 하나의 node로 합칠 수 있는 경우 (두 node가 가지고 있는 key의 갯수가 leaf의 경우 order 개, 아닌 경우 order - 1 개보다 작은지) 에는 `coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime)` 를 호출하여 두 node를 하나로 합치는 과정을 진행하고, 그렇지 않은 경우에는 `redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime_index, int k_prime)`를 호출하여 두 node를 고르게 나누는 과정을 진행한다.
 
#### 2.2.1. coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime)
이 함수는 2개의 node를 하나로 합쳐주는 과정을 담당한다. 먼저 현재 node가 첫번째 child여서 순서가 바뀐 경우, 다시 순서를 바꾸어준다.

##### 2.2.1.1. Case 1. Internal node
합치고자 하는 2개의 node가 Internal node 인 경우에는 왼쪽인 node에다가 k_prime과 오른쪽 node의 key, 그리고 pointer를 append한다. 그 뒤 새롭게 생긴 child 의 parent를 왼쪽 node로 설정한다. 마지막으로 두 node의 parent에서 합쳐지고 더 이상 쓰지 않게 되는 node를 지우기 위해 `delete_entry(root, n->parent, k_prime, n)` 를 호출하고 할당을 해제한다. 

##### 2.2.1.2. Case 2. Leaf
합치고자 하는 2개의 node가 leaf인 경우에는 오른쪽 node의 key와 pointer만 append한다. (leaf에서는 항상 key와 pointer가 1대1 대응이므로) 그 뒤, 다음 leaf로 이어주었던 order - 1 번째 포인터를 설정한다. 마지막으로 두 node의 parent에서 합쳐지고 더 이상 쓰지 않게 되는 node를 지우기 위해 `delete_entry(root, n->parent, k_prime, n)` 를 호출하고 할당을 해제한다.
<br>
두 경우 모두 항상 재귀적으로 Deletion Operation 을 실행하게 되고 coalesce가 발생하지 않을 때까지 이를 반복한다.

#### 2.2.2. redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime_index, int k_prime)
이 함수는 현재 node가 최소 개수를 맞추기 위해 이웃 node에서 key, pointer 쌍을 하나 가져오는 과정을 담당한다.

##### 2.2.2.1. Case 1. n has a neighbor to the left
이웃 node가 현재 node의 왼쪽에 있는 경우에는 이웃 node의 가장 오른쪽 key, pointer를 사용하여 현재 node의 key, pointer를 뒤로 한 칸씩 밀고 첫번째에 key와 pointer를 추가한다. 두 node가 leaf가 아닌 경우에는 pointer는 num_keys+1 번째 포인터를, key는 이 둘을 분리하던 k_prime을 사용하며 원래 k_prime이 있던 자리에는 이웃 node의 마지막 key를 사용한다. 두 node가 leaf인 경우에는 이웃 node의 가장 오른쪽 key, pointer를 그대로 사용하며, k_prime이 있던 자리에도 이웃 node의 가장 오른쪽 key를 사용한다.

##### 2.2.2.2. Case 2. n is the leftmost child
이웃 node가 현재 node의 오른쪽에 있는 경우에는 이웃 node의 가장 왼쪽 key, pointer를 사용하여 현재 node의 가장 오른쪽에 key와 pointer를 추가한다. 두 node가 leaf가 아닌 경우에는 k_prime과 가장 왼쪽 pointer를 사용하며 원래 k_prime이 있던 자리에은 이웃 node의 가장 왼쪽 key를 사용한다. 그리고 이동된 pointer가 가르키는 node의 parent를 n으로 수정한다. 두 node가 leaf인 경우에는 가장 왼쪽 key, pointer를 그대로 사용하며, k_prime이 있던 자리에는 이웃 node의 왼쪽에서 2번째 key를 사용한다. 마지막으로 이웃 node의 key와 pointer를 앞으로 한 칸씩 당겨준다.
<br>

이렇게 redistribution을 통해서 현재 node가 최소 key 갯수를 만족하게 되면서 Delete Operation이 마무리 된다.

<br><br>

## 3. Naïve designs or required changes for building on-disk b+ tree

먼저 on-disk B+ Tree에서는 모든 node가 page으로 변경되므로, page의 구조에 맞게 `struct node`의 수정이 필요하다.

```cpp
typedef struct page_t{
    uint64_t parent;
    int32_t is_leaf;
    int32_t num_keys;
    uint64_t another_page;
    uint64_t *keys;
    void * pointers;
} node;
```

- node를 가르키는 pointer는 page number로 바뀌게 되므로 `node*` 을 주어진 명세에 맞게 8바이트를 차지하는 `uint64_t`로 변경한다.
- print에만 이용되는 `struct node* next` 를 삭제한다.
- isLeaf 가 명세에서 4바이트를 차지하니 `bool`에서 `int32_t`로 변경한다.
- 기존에 pointers 의 order번 째에 있던 포인터를 `uint64_t`의 별개 멤버로 추가한다.
- key가 8바이트이므로 keys의 형식을 `int*` 에서 `uint64_t*` 로 수정한다.
- keys와 pointers는 기존과 같이 동적할당하는 방식을 사용한다.
- Internal Node와 Leaf Node의 value 크기가 다르므로 상황에 맞게 동적할당하여 사용할 수 있도록 기존과 같이 `void *` 을 사용한다. Leaf node의 경우 `char **` 으로 사용하게 되고, Internal Node의 경우 `uint64_t *` 으로 사용하게 된다

<br>
또한 value가 저장하는 값 또한 변경되므로 `struct record` 도 수정이 필요하다.

```cpp
typedef struct record {
    char value[120];
} record;
```
- value가 최대 120바이트이므로 120바이트 짜리 char형 배열로 수정한다.

<br>
node* root를 반환하는 대부분의 함수를 int64_t root (root의 page number)를 반환하도록 변경하고, 매 Operation이 끝날 때 Header Page의 root page number를 해당 값으로 변경한다.

<br>
make_node 에서 node를 새로 할당받는 작업에서 먼저 ```pagenum_t file_alloc_page()``` 을 사용하여 Page Number를 할당받도록 한다.
node를 free하는 작업에서 해당 page를 free page로 설정하도록 변경한다. `file_free_page()` 를 사용한다.
다른 node를 읽는 부분의 작업을 `void file_read_page(pagenum_t pagenum, page_t* dest)` 를 사용하여 읽어오도록 수정한다.
insert, delete operation를 수행하는 도중 node가 변경되었다면 항상 ```void file_write_page(pagenum_t pagenum, const page_t* src)``` 를 사용하여 변경내용을 바로 파일에 저장하도록 한다.

<br>
```pagenum_t file_alloc_page()``` 에서는 Header의 Free Page Number 를 확인하고 유효한 Free Page가 있다면 그 Page Number를 반환하고 그렇지 않다면 Number of Pages + 1 값을 반환한다 <br>
```void file_free_page(pagenum_t pagenum)``` 에서는 header의 Free Page Number를 pagenum으로 설정하고,pagenum에 해당하는 page가 가지고 있는 Next Free Page Number를 header의 Free Page Number로 설정한다.
```void file_read_page(pagenum_t pagenum, page_t* dest)``` 에서는 page가 고정된 size를 가지고 있으므로 pagenum만 알고 있으면 File에서 해당 page의 위치로 바로 이동하여 읽어올 수 있다. 따라서 해당 부분의 page를 읽어서 key와 value를 갯수와 크기에 맞게 동적할당하여 dest에 채워준다.
```void file_write_page(pagenum_t pagenum, const page_t* src)``` 에서도 위와 마찬가지로 pagenum에 해당하는 파일의 page에 src에 담겨있는 내용을 명세에 맞게 작성한다.

<br>
2.2.1 에서 살펴봤듯이 Merge (coalesce) 는 항상 재귀적으로 Delete Operation을 일으켜 많은 양의 File I/O를 유발하고 이는 퍼포먼스에 악영향을 미치게 된다. 따라서 `delete_entry( node * root, node * n, int key, void * pointer )` 에서 모든 keys가 지워지기 전까지는 `coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime)` 를 호출하지 않도록 하여 Delayed Merge를 적용한다.