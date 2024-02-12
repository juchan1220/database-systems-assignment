# Project 5: Implement Lock Manager
2018008277 CSE 노주찬 <br><br>

* 모든 코드는 하나의 Transaction은 하나의 Thread에서만 이뤄진다는 가정 하에 작성되었다. 이 가정이 틀렸을 경우, 치명적인 오류를 낼 가능성이 매우 높다.
* 특별한 언급이 없는 경우, (상위 레이어용 lock, unlock 함수 등을 제외) 모든 함수에서 획득한 mutex lock은 함수를 종료할 때 unlock한다. 

milestone1 이후 제공된 supplement에서 많은 부분을 참조하였고, milestone1에서 작성하였던 코드에서 상당한 수정이 있었다.

## 1. BufferManager
버퍼에도 Latch가 필요했기에 기존 buffermanager 코드를 수정할 필요가 있었다.

### 1.1. Object
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
	std::mutex buffer_page_mtx;
} bufferFrame;
```
buffer page latch를 담당하는 buffer_page_mtx 를 추가하였다.<br>

```cpp
extern std::mutex buffer_pool_mtx;
extern std::mutex buffer_pin_mtx;
```
buffer pool latch를 담당하는 buffer_pool_mtx, 그리고 buffer pin latch를 담당하는 buffer_pin_mtx를 추가하였다. 각 buffer page는 pin count를 지니며, buffer pool latch를 가지고 leaf page를 찾는 상황에서, buffer page latch를 lock 하는 상황, unlock 하는 상황에서 수정될 수 있다. 앞의 두 상황에서는 buffer pool latch를 가지고 있으므로 buffer page latch를 획득하는 과정에서 race condition으로 page의 access가 동시에 발생하지는 않는다. 그러나 마지막 상황에서는 공통적으로 가지고 있는 Latch가 없기 때문에 race condition이 발생할 우려가 있다고 보았다. (ex: buffer pool latch를 가지고 find_leaf(), find() 함수로 리프 페이지를 읽고 unpin 하는 쓰레드 / 같은 page의 latch를 가지고 있고 이제 unlock하여 unpin 하려는 쓰레드) 따라서 pin count를 수정하는 모든 동작을 thread-safe 하기 위해 buffer_pin_mtx를 추가하였다.

### 1.2. Function
```cpp
void buffer_pool_lock();

void buffer_pool_unlock();
```

각각 buffer pool latch를 Lock, Unlock 하는 함수이다. Layer Architecture를 준수하기 위해서 존재한다.

```cpp
bool buffer_page_try_lock(int table_id, pagenum_t pagenum);

void buffer_page_unlock(int table_id, pagenum_t pagenum);
```
* buffer_page_try_unlock() 은 해당 페이지의 Latch를 Lock 시도하는 함수이다. 획득하게 되면 해당 버퍼 페이지의 pin count가 1 올라가고 (이 pin count가 Latch를 Unlock 하기 전까지 해당 버퍼의 evict를 방지해준다.), true를 반환한다. 획득에 실패한 경우, false를 반환한다.
* buffer_page_unlock() 은 해당 페이지의 Latch를 Unlock 하는 함수이다. 해당 페이지의 pin count를 1 감소 시킨다.
<br><br>

## 2. lockmanager Object
### 2.1 Lock
```cpp
struct Lock
{
	int table_id;
	pagenum_t page_num;
	int64_t key;

	Transaction* trx;

	LockMode mode;

	// same page prev lock
	Lock* page_prev;

	// same page next lock
	Lock* page_next;
};
```
Lock 객체는 table id, page number, key, 해당 락을 소유하고 있는 Transaction을 가리키는 trx, Lock Table의 Page 단위 리스트를 연결하는 page_prev, page_next를 가진다.
<br>

### 2.2. Transaction
```
struct Transaction
{
	int trx_id;
	
	TransactionState trx_state;
	std::list<Lock*> acquired_locks;

	std::condition_variable trx_cond;
	std::mutex trx_mtx;

	int wait_this_cnt;

	Lock* wait_lock;

	std::list<Undo> undo_list;
	
	Transaction(int tid) : trx_id(tid), trx_state(TransactionState::STATE_IDLE), wait_lock(nullptr), wait_this_cnt(0) {};
};
```
Transaction 객체는 트랜잭션 ID인 trx_id, 현재 트랜잭션의 상태를 나타내는 trx_state, 트랜잭션이 보유한 Lock을 List로 가지고 있는 acquired_locks, 해당 락과 Conflict가 발생한 락을 대기 시키기 위해 사용하는 trx_cond, trx_mtx와 이 트랜잭션이 commit 되기를 기다리는 트랜잭션(스레드) 의 개수를 나타내는 wait_this_cnt, 만약 이 트랜잭션이 다른 Lock과 conflict가 발생하여 기다리고 있을 때 기다리는 Lock을 가리키는 wait_lock, db_update() 쿼리로 인해 발생한 변경사항을 가지고 있는 undo_list를 가지고 있다. <br>

### 2.3. Manager
```cpp
struct TransactionManager
{
	std::unordered_map<int, Transaction*> trx_table;
	int next_trx_id;
	std::mutex trx_mtx;
};

struct LockManager
{
	// first lock_t is head, second lock_t is tail
	std::unordered_map<ptp, std::pair<Lock*, Lock*>, ptpHasher> lockTable;

	std::mutex lock_mtx;
	std::mutex waitlock_refresh_mtx;
};
```
TransactionManager, LockManager 객체가 각각 단 하나만 생성되어 트랜잭션 Table과 락 Table을 관리한다.<br>

### 2.4. etc

```cpp
enum class LockMode
{
	MODE_SHARED,
	MODE_EXCLUSIVE
};

enum class TransactionState
{
	STATE_IDLE,
	STATE_RUNNING,
	STATE_WAITING
};

struct Undo
{
	int table_id;
	int64_t key;
	record* oldValue;
};
```
각각 Lock의 mode, 트랜잭션의 현재 상황, db_update() 로 인해 발생한 변경사항을 저장하는 객체들이다.
<br><br>

## 3. lockmanager Function (Implementation)

```cpp
int lock_alloc_trx();
```
새로운 Transaction 객체를 생성하고 그 ID를 발급한다. Table이 허용하는 한 (size가 UINT_MAX 이하인 한) unique한 ID를 생성함이 보장된다.

* 1. Transaction Manager의 trx_mtx lock을 획득한다.
* 2. unique한 Transaction ID를 발급한다.
* 3. 새로운 Transaction 객체를 생성하여 Transaction Manager의 Table에 저장한다.
* 4. ID를 반환한다.

```cpp
int lock_delete_trx(int tid);
```
해당 tid를 가지는 Transaction을 Commit한다.

* 1. 해당 tid를 가지는 Transaction이 존재하는지 확인한다.
 * 없는 경우 0을 반환한다.
* 2. Lock Manager의 waitlock_refresh_mtx, lock_mtx lock을 획득한다.
 * 각각 wait_lock이 수정되는 동안 Deadlock Detection으로 접근하여 엑세스 위반이 일어나는 것을 방지, Lock Manager의 수정이 동시에 일어나는 것을 방지하는 역할을 한다.
* 3. 해당 Transaction 객체의 acquired_locks를 순회하면서 Lock을 LockManager의 Table로부터 제거하고 LockTable의 List를 재조정한다.
* 4. Lock Manager의 lock_mtx lock lcok을 해제한다.
* 5.현재 트랜잭션을 기다리고 있는 다른 Transaction이 1개 이상 있을 경우, 그 스레드가 전부 깨어나서 다른 락과의 Conflict 혹은 Success로 Lock이 처리될 때까지 해당 Transaction의 mutex와 conditional variable을 가지고 기다린다.
 * 이 과정을 없애고 바로 Transaction과 Lock을 Delete 하는 경우, 이 Transaction을 기다리고 있던 Transaction의 wait_lock이 Delete된 Lock 객체를 가리키고 있는 상황에서 Deadlock Detection을 하던 중 해당 값을 참조, 액세스 위반이 발생할 가능성이 있다.
* 6. acquired_locks의 모든 Lock을 할당해제한다. Transaction 객체 또한 할당해제하고 Table로부터 제거한다.

```cpp
bool lock_trx_exist(int tid);
```
해당 tid를 가지는 Transaction이 존재하면 true, 아니면 false를 반환한다. 레이어 아키텍쳐를 지키기 위한 상위 레이어 및 레이어 내부 이용 함수

```cpp
LockResult lock_record_lock(int table_id, pagenum_t pagenum, int64_t key, LockMode mode, int tid);
```
Lock Manager의 Lock Table에 해당하는 레코드의 Lock 생성을 시도한다.
* 1. Lock Manager의 waitlock_refresh_mtx, lock_mtx lock을 획득한다.
* 2. 해당 페이지 Lock List의 tail에서부터 같은 Transaction의 Lock이 있는지 탐색한다.
 * MODE_SHARED의 경우 MODE_EXCLUSIVE, MODE_SHARED의 Lock, MODE_EXCLUSIVE의 경우는 MODE_EXCLUSIVE Lock이 상위 혹은 동급의 Lock이므로 새로운 Lock을 생성할 필요가 없다.
 * 있는 경우, 1에서 획득했던 mtx을 unlock 하고 SUCCESS를 반환한다.
* 3. 다시 한번 Lock List의 tail에서부터 Deadlock, Conflict 검사를 시작한다.
 * 무시해도 되는 Lock (MODE_SHARED일 때 MODE_SHARED Lock, 같은 Transaction의 Lock) 은 지나친다.
 * 무시할 수 없는 Lock = Conflict. 일단 Conflict가 발생했다고 체크만 하고 계속해서 Deadlock Detect를 진행한다.
 * 현재 보고 있는 Transaction이 기다리고 있는  lock의 Transaction이 현재 처리중인 Transaction인지 확인한다. 일치하면 Deadlock (Cycle) 이므로 DEADLOCK을 반환한다. 아니라면 그 Transaction에 대해 해당 Lock의 소유 Transaction이 WAITING이 아닐 때까지 이 작업을 반복한다.
* 4. Conflict 발생 여부에 따라 적절하게 Lock 객체를 생성, 초기화하여 Lock List의 tail에 삽입하고 결과를 반환한다. Conflict가 발생했다면 Conflict가 발생한 가장 가까운 Lock을 wait_lock으로 설정하고 wait_lock을 소유하고 있는 Transaction의 wait_this_cnt를 증가시킨다.

```cpp
pagenum_t lock_buffer_page_lock(int table_id, int64_t key);
```
Buffer Page Latch를 획득하는 함수이다. 획득한 Latch의 lock은 함수 종료 시에도 유지되며 buffermanager의 buffer_page_unlock() 으로 해제해야한다. Latch를 획득한 page의 page number를 반환한다.
* 1. Buffer Pool Latch를 획득한다.
* 2. 주어진 key를 가지는 leaf page를 찾는다.
* 3. Buffer Page Latch 획득을 시도한다.
 * 실패하였다면 Buffer Pool Latch를 반환하고 1로 돌아간다.
* 4. Buffer Pool Latch를 반환하고 page number를 반환한다.

```cpp
bool lock_deadlock_check(int table_id, pagenum_t pagenum, int64_t key, Lock* start, int tid);
```
한번 Conflict가 발생하여 WAITING 상태였던 Lock이 Awake 되었을 때 Deadlock을 탐지하는 함수이다.
Deadlock의 탐지 로직은 lock_record_lock() 와 동일하다.

```cpp
void lock_abort_trx(int tid)
```
tid를 가지는 Transaction을 Abort 하는 함수이다.

* 1. 해당 Transaction의 undo_list를 순회하면서 Rollback을 진행한다.
 * 1-1. 해당 페이지의 buffer page latch를 획득한다.
 * 1-2. undo에 있는 이전 값으로 값을 되돌린다.
 * 1-3. buffer page latch를 반환한다.
* 2. 해당 Transaction을 끝낸다. (lock_delete_trx() 함수 호출)
<br><br>

## 4. bptservice Function (Implementation)
```cpp
int db_find(int table_id, int64_t key, char* ret_val, int trx_id);
int db_update(int table_id, int64_t key, char* values, int trx_id);

```
* 1. 해당 트랜잭션이 실제로 존재하는지 확인한다.
 * 없으면 1을 반환한다.
* 2. 해당 테이블이 열려있는지 확인한다.
 * 아니면 2를 반환한다.
* 3. Buffer Pool Latch를 가지고 key를 가지는 record가 있는지 확인한다.
 * 없으면 3을 반환한다.
* 4. Buffer Page Latch를 가지고 Buffer Pool Latch를 해제한다.
* 5. Record Lock 생성을 시도한다.
 * Deadlock으로 인해 실패한 경우, Buffer Page Latch를 해제하고 lock_abort_trx() 으로 트랜잭션을 abort한다.
 * Conflict로 인해 실패한 경우, Conflict난 Transaction의 mutex, condition_variable를 가지고 Sleep 한다.
  * 다시 깨어났을 때 Buffer Pool Latch를 가지고 Buffer Page Latch를 가진 다음 Buffer Pool Latch를 해제한다.
  * wait_lock을 Conflict 났던 Lock의 바로 다음 Lock으로 변경하고 Deadlock Detection을 진행한다.
  * 5의 결과를 처리하는 것과 동일하게 처리한다.
* 6. 실제 쿼리 작업을 처리한다. (update의 경우, Undo를 추가한다.)
* 7. Buffer Page Latch를 해제하고 작업을 마친다. 
<br><br>

## 5. Deadlock Detection 관련
* 생성 시점에 가장 가까운 Conflict Lock으로 인해 발생하는 deadlock은 Lock 생성 시에 탐지가 가능하다.
  * 현재 Deadlock이 없고 나중에 Lock의 추가로 Deadlock이 발생한다고 가정해도, 그 경우에는 그 Lock 생성 시에 탐지가 가능하다.

* Conflict Lock이 기다리는 Lock의 Conflict Lock과 엮여서 Deadlock이 발생하지만, 두번째 Conflict Lock이 가장 가깝지 않아서, 첫번째 Conflict Lock이 기다리는 Lock이 wait_lock이 없게 되고, 이로 인해 생성 시점에 Detect를 하지 못하는 경우는 탐지할 수 없다.
 * 나중에 중간에 있는 Lock이 Transaction의 커밋으로 사라지면서 그걸 기다리던 Lock이 사라진 Lock보다 앞에 있는 Lock을 기다리게 되고 이로 인해 두번째 Conflict Lock이 첫번째 Conflict Lock의 wait_lock이 되어 이 때 발생하는 Deadlock을 탐지하는 것. (정확히는 이미 Deadlock은 발생했다. wait_lock이 하나이기 때문에 탐지가 가능한 시점이 지금까지 늦춰졌을 뿐이다.)
 * 매번 Conflict가 해결되어 Awake된 Transaction에 대해 기다려야 하는 Lock에 대해 wait_lock을 재계산하여, 위의 Deadlock 탐지를 사용하면 Deadlock 탐지 시점이 늦더라도 이러한 경우의 Deadlock을 확실하게 잡아낼 수 있다.
 * 그런데 기다리고 있던 모든 Transaction의 wait_lock을 재계산, Deadlock Detection을 해야하나, Deadlock Detection 도중 아직 수정되지 않은 wait_lock으로 빠져 제대로 Deadlock Detection이 일어나지 않을 수 있다. 또한 매번 Conflict가 발생하면 해당 페이지의 모든 Lock List를 훑어야한다는 성능 상의 단점이 존재한다.

그러나 아래의 Theorem을 이용하면  빠르게 wait_lock을 재계산할 수 있다.

> 커밋되어 사라진 Lock을 기다리고 있던 Lock은 사라진 Lock의 **바로 다음 Lock**을 기다리게 된다.

* 커밋되어 사라진 Lock이 eXclusive Lock인 경우
 * 그 앞에는 락이 존재하지 않는다. (X락 앞에 락이 존재한다면 WAITING 상태이므로 커밋이 불가능하다. 커밋되어서 사라졌다는 가정에 모순.)
 * 따라서 이 eXclusive Lock을 기다리고 있는 Lock이면 반드시 SUCCESS임이 보장된다.

* 커밋되어 사라진 Lock이 Shared Lock인 경우
 * 그 앞에는 모두 Shared Lock 혹은 Lock이 존재하지 않는다. (Shared Lock 앞에 eXclusive mode 락이 존재한다면 커밋이 불가능하다. 커밋되어서 사라졌다는 가정에 모순.)
 * 또한 이 사라진 Lock을 기다리던 Lock이 Shared Lock이면 이 Shared Lock을 기다릴 필요가 없다. (기다리고 있다는 가정에 모순.)
 * 따라서 이 Shared 락을 기다리는 것은 항상 eXclusive Lock.
 * eXclusive Lock은 Shared Lock을 기다려야 하므로 (Shared Lock 혹은 없음이 보장된)  그 다음 락을 기다려야한다

이로 인해  커밋되어 사라진 Lock을 기다리고 있던 Lock이 새롭게 기다려야하는 Lock을 Lock List를 전부 돌지 않고도 빠르게 계산할 수 있다. 이렇게 새롭게 기다려야하는 Lock에 대해서 매번 Deadlock Check를 해주면 이러한 경우의 Deadlock를 탐지할 수 있다.