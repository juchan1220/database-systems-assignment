2018008277 CSE 노주찬<br><br>

먼저 기능과 레이어에 따라 헤더, 소스 파일을 분리하였다.<br>

1. Library를 통해 외부에서 사용할 open_table db_insert, db_delete, db_find 이 구현되어있는 bptservice
2. bptservice 에서 명령어를 처리하기 위해 필요한 B+ Tree 함수들이 구현되어있는 bpt
3. bptservice와 bpt에서 on-disk의 table과 그 안의 page를 불러오기 위한 File Manager API가 구현되어있는 filemanager


# 1. bptservice
## 1.1. open_table(char* pathname)
File Manager 에서 제공하는 `file_open_table(const char* pathname)` 를 사용하여 pathname의 디렉토리에 있는 테이블 파일을 읽거나, 없는 경우 새로 생성한다. `table_id` 의 지정도 여기서 이루어진다. 현재는 table 파일이 바뀔 때마다 table_id가 증가하도록 설정해두었다.

## 1.2. db_insert(int64_t key, char* value)
기존 In-memory B+ Tree 코드의 `Insert(node *root, int key, int value)` 함수와 동일한 역할을 담당한다.

## 1.3. db_delete(int64_t key)
기존 In-memory B+ Tree 코드의 `delete(node * root, int key)` 함수와 동일한 역할을 담당한다.

## 1.4 db_find(int64_t key, char* ret_val)
bpt의 `find(int64_t key)` 를 통해 key에 해당하는 record를 찾아 ret_val로 복사해주는 역할을 담당한다.
<br><br>

# 2. bpt
Delayed Merge를 제외한 거의 모든 로직은 기존의 In-memory B+ Tree 코드와 동일하다. 따라서 기존 코드와의 차이점에 초점을 두었다. 전반적으로 root를 인자로 넘겨줄 필요가 없어졌기 때문에 인자에서 `node *root`를 삭제하였다. 현재 node를 `node *` 를 인자로 사용하던 함수들은 `pagenum_t` 만을 넘겨주어 함수 안에서 읽게 하거나, caller에서 이미 읽은 경우에는 `page_t *` 도 같이 넘겨주어 사용하도록 하게 했다. 이미 Caller에서 읽은 페이지를 읽는 용도로만 사용할 때는 `page_t *`만을 넘겨주기도 한다.

## 2.1. struct
```cpp
typedef uint64_t pagenum_t;

typedef struct {
	pagenum_t parent;
	int32_t is_leaf;
	int32_t num_keys;
	pagenum_t another_page;
	int64_t* keys;
	void* pointers;

	// for Header

	bool is_header;
	pagenum_t freepage;
	pagenum_t rootpage;
	pagenum_t num_page;
} page_t;
```

page의 경우 Header, Internal, Leaf, Free 여부와 상관없이 모두 동일한 구조체를 사용한다. 먼저 특수한 경우인 Header Page는 맨 밑 4가지의 멤버만을 사용한다. `parent`는 page가 Leaf, Internal 일 때는 자신의 parent page 번호를 저장한다. 만약 현재 page가 root일 경우는 0을 저장한다. `another_page`는 Leaf일 경우에만 사용되며, 자신의 Right Sibling을 저장하는데 사용된다. 'pointers'는 Leaf일 경우에는 `file_read_page()`  또는 `make_page()` 에 의해 할당된 (LEAF_ORDER) 개의 record를 저장할 수 있는 `record*` 을 가지고 있고, Internal일 경우에는 `file_read_page()` 또는 `make_page()` 에 의해 할당된 (INTERNAL ORDER + 1) 개의 pagenum_t 을 저장할 수 있는 'pagenum_t *`를 가지고 있다. 특히 Leaf와는 달리 left most child를 가르키는 page 번호가 이곳에 같이 저장되기 때문에 INTERNAL ORDER + 1개를 할당받는다.

```cpp
typedef struct record {
	char value[VALUE_SIZE];
} record;
```
record의 경우에는 VALUE_SIZE를 120으로 define 하고 char [VALUE_SIZE]를 사용하여 정의했다. 

## 2.2. make_page(bool isLeaf)

기존 In-memory B+ Tree 코드에서 `make_node()`와 `make_leaf()`를 합친 함수이다. isLeaf 가 true인 경우 Leaf Node에 맞게 keys, pointer 를 할당한 page_t의 포인터를 반환하고, false인 경우 Internal Node에 맞게 keys, pointer를 할당한 page_t의 포인터를 반환한다. pagenum_t 와 page_t* 를 pair로 반환하기 어려웠기 때문에 구현의 편의를 위해 In-memory에서 사용할 page_t* 만을 반환한다. 따라서 이것으로 할당받은 page를 파일에 쓰기 위해서는 Caller에서 별도로 file_alloc_page() 를 호출하여 page number를 할당 받고, 그 page number를 사용해야한다.

## 2.3. delete_entry(pagenum_t leafnum, int64_t key, void* pointers)
기존 In-memory B+ Tree 코드에서는 `void* pointers` 를 이용하여 `node.pointers`에 있는 주소값과 비교하여 child를 찾고 제거하는 부분이 있었다. 그러나 On-disk 환경에서는 항상 포인터를 사용할 수 없으므로 로직이 변경되었다. Leaf Node의 경우 key와 value가 항상 일대일로 대응하기 때문에 Key를 찾은 위치에서 value 도 같이 제거하도록 되어있다. Internal Node의 경우에는 `void* pointers`에 지워야 할 `pagenum_t` 의 주소값을 담게 하여 `page_t.pointers`가 담고 있는 pagenum_t와 비교하여 제거하도록 되어있다. 즉, 기본적으로 `delete_entry()` 를 호출할 때는 `void * pointers`에 지워야하는 child의 주소값을 담아서 호출하도록 하였고 실제로 사용되는 건 Internal Node의 경우 뿐이다.

## 2.4. Delayed Merge
`delete entry()` 에서 node의 최소 페이지 갯수를 지정하는 min_keys의 값을 order에 상관없이 1로 지정하였다. 이로서 key가 완전히 사라지기 전까지 재분배나 Merge가 일어나지 않도록했다. 그리고 Merge가 일어나는 조건을 `if (neighbor.num_keys + cur.num_keys < 2)` 으로 바꾸어 이웃과 현재 노드의 키 갯수가 1개 밑으로 내려갈 때만 Merge가 일어나게 하고, 그 전에는 최대한 재분배를 통해서 Merge를 미루도록 하였다.
<br><br>

# 3. File Manager 
모든 File I/O 작업은 C 표준 함수인 fread, fwrite, fseek, fflsuh 를 이용해서 처리하였다. 아직은 동시에 여러 파일을 다루지 않기 때문에 File Manager 전체적으로 `FILE *` 하나만을 가지고 File I/O를 처리하도록 설계했다. 바이트 단위의 자료를 조금씩 읽는 것보단 페이지 단위로 한번에 읽는 것이 빠를 것으로 생각해 페이지 1개 크기만큼의 `char buffer[]`를 가지고 있으며, 최종적인 fwrite와, 첫 fread는 이 버퍼를 대상으로 이루어진다. 또한 `file_read_page()`와 `file_write_page()`를 제외한 File Manager 내부에서도 페이지를 읽고 쓰는 작업은 File Manager API를 사용하도록 설계되었다. 

## 3.1. file_open_table(const char* pathname)
bptservice 에서 table을 새로 열 때, 파일 관련 시스템 콜을 직접 호출하지 않기 위해 추가된 File Manager API이다. 주어진 이름의 파일을 열거나, 없으면 새로 생성하도록 되어있다.

## 3.2. file_read_page(pagenum_t pagenum, page_t* dest)
먼저 fread를 통해 pagenum번에 존재하는 페이지를 파일로부터 buffer로 읽어들인다. 그리고 Header Page, Leaf Page, Internal Page, Free Page에 맞게 dest의 멤버를 설정하여 반환한다. buffer에서 memcpy를 통해 각 멤버들로 복사하도록 되어있다. Leaf Page, Internal Page의 경우 keys와 pointers의 동적할당이 여기서 이루어진다. 이 때 page에 keys를 추가하는 작업이 일어날 수 있기 때문에 num_keys에 맞춰 할당하지 않고 최대 order에 맞춰서 할당하도록 했다.  Free Page의 경우 항상 num_keys가 0이 되도록 하였기 때문에 이를 체크하여 keys와 pointers를 할당하지 않고 NULL로 설정했다. page가 사라질 때 keys와 pointers의 free가 자동으로 이뤄지지 않으므로 항상 page_t 가 사라질 때는 keys와 pointers를 free 하도록 코드를 작성했다.

## 3.3. file_write_page(pagenum_t pagenum, const page_t* src)
src로부터 정보를 불러들여 buffer에 작성한 다음, 그 buffer를 파일에 쓰도록 되어있다. Internal Node의 경우, Leaf와 달리 another page와 같은 별도의 멤버를 사용하지 않고 keys에 같이 저장하기 때문에 이에 대한 예외처리가 되어있다.

## 3.4. file_alloc_page()
먼저 header를 읽어서 free page의 번호를 확인한다. 만약 0인 경우에는 새로운 페이지를 append 하여 그것을 반환한다. 이 경우, 해당 위치에 미리 0을 채워놓고 page 번호를 반환한다. 따라서 반환된 페이지 번호를 사용하지 않는 경우 디스크에서 페이지 누수가 발생하게 되므로 반드시 사용하도록 했다.

## 3.5. file_free_page(pagenum_t pagenum)
먼저 header를 읽어서 현재 free page number를 불러들이고, free page number를 새로운 page number로 변경한 뒤 header를 write 한다. 그 다음, free할 page를 불러들여 num_keys를 0으로 바꾸고, parent (next free page) 를 아까 header에서 읽은 page number로 설정한 뒤 다시 write한다.









