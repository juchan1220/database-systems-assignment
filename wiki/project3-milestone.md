2018008277 CSE 노주찬

Project2와 마찬가지로 기능 별로 헤더와 소스를 구분하기 위해 버퍼 매니저는 buffermanager.h buffermanager.c 에 구현하였다. 또한 버퍼 매니저를 구현, 사용하기 위해 거의 모든 레이어에서 수정이 있었다.

# 1. File Manager
## 1.1. Deleted Function
페이지의 alloc, free 를 더 이상 File Manager 레이어에서 처리하지 않고 상위 레이어인 Buffer Manager 에서 처리하기 때문에 `file_alloc_page()` `file_free_page()`는 삭제되어 더 이상 사용하지 않는다.

## 1.2. 복수의 테이블 관리
Piazza의 질문을 통해 한번 실행에 10개 이상의 table이 open되지 않고, table id가 휘발성이어도 괜찮다는 점을 확인하여, Naive 하게 테이블 파일이 열린 순서대로 table id를 1부터 증가하면서 부여하였고 각각의 열린 테이블 파일을 파일 포인터를 FILE* tables[] 에 저장하였다. 테이블이 열려있으면 파일 포인터, 그렇지 않으면 항상 NULL을 가지고 있도록 설계해 `int file_is_table_opened(int table_id)` 함수를 통해 테이블이 열려있는지 확인할 수 있다. 

# 2. Buffer Manager
버퍼는 Naive하게 포인터를 사용한 동적할당 받는 배열 `bufferFrame* buffer` 로 구현되어있고, evcit policy로는 Clock Policy를 사용하였다. 그러나 배열로는 접근이 느리기 때문에, 빠른 접근을 위해 해시를 추가로 사용하였다. table id와 page number를 기반으로 한 해시 값을 얻고, 이를 버퍼의 크기로 나눈 나머지를 해시 테이블의 index로 사용하여 그곳에 해당 페이지가 저장되어있는 버퍼의 index를 저장한다. Collision이 일어난 경우에는 Linked List 방식의 Bucket 을 사용하도록 하였고, 이 List에서는 선형탐색을 진행하도록 하였다.

```cpp
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
```
bufferFrame에서는 명세에서 주어진 기본 구조에서 LRU List와 관련된 멤버를 제거하고, Clock Policy에 사용하기 위한 ref bit를 is_refered로 추가하여 사용하였다.

```cpp
struct hashBucket{
	int idx;
	hashBucket* next;
};
```

hashBucket은 buffer의 index를 저장하는 idx, 그리고 collision 발생 시 사용하는 Linked list에 필요한 next로 이루어져있다.

`buffer_hashing(int table_id, pagenum_t pagenum)` 에서는 table id와 page number를 가지고 rabin fingerprint 방식으로 해싱을 진행한다.
`buffer_find_page(int table_id, pagenum_t pagenum)` 에서는 해시 값을 버퍼 크기 ( = 해시 테이블 크기) 로 나눈 나머지를 index로 하여 해당 해시 테이블이 가지고 있는 Linked List 에서 table id와 page number가 일치하는 bufferFrame을 선형탐색으로 찾는다.

`buffer_alloc_page(int table_id)` 이제 헤더로부터 free page number 를 얻어, 그 table id와 paeg number에 해당하는 page를 buffer에 추가하고 page number를 반환한다. 이렇게 alloc 된 page는 다른 페이지와 마찬가지로 `buffer_read_page(int table_id, pagenum_t pagenum)` 를 통해 읽어야 사용할 수 있다. 또한 실제 페이지 내부에는 아무런 초기화가 되어있지 않기 때문에 상위 레이어에서 초기화가 필요하다.

`buffer_free_page(int table_id, pagenum_t pagenum)` 에서는 해당 페이지를 free하고 unpin한다.

`buffer_set_dirty_page(int table_id, pagenum_t pagenum)`, `buffer_unpin_page(int table_id, pagenum_t pagenum)` 에서는 상위 레이어에서 해당 페이지를 unpin하거나 dirty하게 된 경우를 체크하기 위해 사용한다.

`buffer_read_page(int table_id, pagenum_t pagenum)` 에서는 버퍼에 요청된 페이지가 있는지 확인하여 존재하면 그 페이지를 가르키는 포인터를 반환하고, 그렇지 않은 경우 File Manager 를 통해 버퍼로 불러와 반환해준다.

버퍼의 크기가 모두 작아 버퍼에 페이지를 추가해야되는데 모두 pin된 경우 `exit(EXIT_FAILURE);` 를 실행하도록 되어있다.

한번 실행에 10개 이상의 테이블을 열지 않는다고 가정했기 때문에 각 table의 header page는 table을 open할 때 `page_t headers[]` 에 따로 저장하여 buffer를 사용하지 않도록 하였다. 다만 Piazza의 답변에서 테이블이 많아지는 경우의 대응이 곤란한 점을 알았기 때문에 이후 과제에서는 header도 buffer를 사용하도록 수정될 예정이다. 

# 3. bpt
이전에서는 B+ Tree 내부 함수에서 페이지를 접근하는 방법에 `page_t`와 `page_t *` 가 혼용되었으나, 이제는 `page_t` 는 무조건 버퍼 프레임에 있고 그 포인터를 얻어오는 방식으로 바뀌었기 때문에 `page_t*` 로 통합되었다. 그리고 어디서 읽어왔는지 상관없이 쓰기가 발생한 함수에서 page에 dirty를 설정하고, 어느 곳에까지 사용되는지에 상관없이 open을 한 함수에서 더 이상 쓰이지 않게 될 때 unpin 하도록 하였다.

구현의 편의를 위해 모든 bptservice Layer에서는 Insert, Delete, Find 연산을 실행하기 전에 연산할 table 을 선택하는 함수인 `select_table(int table_id)` 를 호출하도록 설계했다.

이전 과제에서 새로운 page memory의 할당을 담당했던 `make_page()`는 bufferFrame 내의 page를 가르키는 포인터로만 page를 접근하게 되었기 때문에 해당 포인터를 통해 page를 초기화하는 `init_page(page_t* page, bool isLeaf)` 로 변경하였다.
