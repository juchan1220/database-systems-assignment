# Design Lock Manager

2018008277 CSE 노주찬<br><br>

thread, mutex, 조건 변수 등을 사용하기 위해 언어를 C++ 로 바꾸었다.<br>
먼저 Lock Manager와 관련된 함수를 lockmanager.cpp, lockmanager.h 에 만들기로 계획하였다.
<br><br>
```cpp
typedef std::pair<int, pagenum_t> ptp;

struct ptpHasher
{
	std::size_t operator()(const ptp& k) const {
		size_t ret = 23;
		ret = ret * 37 + std::hash<int>()(k.first);
		ret = ret * 37 + std::hash<pagenum_t>()(k.second);
		return ret;
	}
};

std::unordered_map<int, trx_t> trx_map;
std::unordered_map<ptp, std::pair<lock_t*, lock_t *>, ptpHasher> lock_map;
```
트랜잭션 관리 테이블과 락 테이블은 C++에서 제공하는 해시 기반의 자료구조 unordered_map 을 사용하였다. lock table의 경우에는 key로 table id와 page number의 std::pair (ptp로 typedef 됨) 를 사용하기 때문에 별도의 해시함수 구조체 ptpHaher 를 만들어 지정해주었다.
<br><br>
```cpp
int trx_cnt = 0;
std::mutex trx_mtx;
```
`begin_trx()` 를 통해 발급되는 트랜잭션 ID는 C++의 std::mutex 를 이용하여 thread-safe하게 순차적으로 ID를 1부터 증가시키도록 했다. 트랜잭션 ID를 발급하고나서는 trx_map에 해당 ID를 가지고 새로운 트랜잭션 개체를 생성했다. `end_trx(int tid)` 에서는 trx_map에서 tid에 해당하는 트랜잭션이 존재하는지 확인하고 존재하지 않으면 -1, 존재하면 그 트랜잭션 객체를 erase 하고 0을 반환하도록 해두었다. begin_trx와 마찬가지로 thread-safe 하게 동작하도록 하였다.
<br><br>
```cpp
enum lock_mode
{
	MODE_SHARED,
	MODE_EXCLUSIVE
};

struct lock_t
{
	int table_id;
	pagenum_t page_num;
	int64_t key;
	lock_mode mode;
	lock_t* page_prev;
	lock_t* page_next;
	lock_t* trx_next;
	int trx_id;

	std::condition_variable lock_cv;
};
```

lock 객체는 lock_t 구조체로 정의하였다. 어느 레코드를 보고 있는지 확인하기 위한 table_id와 key, 현재 락의 상태를 나타내는 mode, 같은 페이지 끼리의 더블 링크드 리스트를 구현하기 위한 page_prev와 page_next, 같은 트랜잭션 내에서 다음 lock을 지칭하기 위한 trx_next, 이 락을 소유하고 있는 트랜잭션을 가르키기 위한 trx_id, 이 락이 해제되는 것을 기다리기 위한 조건변수 lock_cv 를 멤버변수로 가지고 있다.
<br><br>

```cpp
enum trx_state
{
	STATE_IDLE ,
	STATE_RUNNING,
	STATE_WAITING
};

struct trx_t
{
	int trx_id;
	enum trx_state state;
	lock_t* trx_locks;
	lock_t* wait_lock;
};
```
트랜잭션 객체는 trx_t 구조체로 정의하였다. 트랜잭션의 id인 trx_id, 트랜잭션의 상태를 알려주기 위한 state, 이 트랜잭션이 처음으로 건 lock 객체를 가르키는 trx_locks (lock 객체에서 trx_next로 이어지는 리스트가 된다), 만약 현재 트랜잭션이 다른 락이 풀리는 것을 기다리고 있을 때, 그 락을 가르키는 wait_lock으로 구성되어있다.
<br><br>

먼저 Lock을 thread-safe 하게 생성, 삭제하기 위해 std::mutex (Global Latch) 를 통해 mutex를 얻어야만 접근 가능하게 할 것이다.<br>
* 락을 추가하는 경우, 현재 Operation에 맞게 lock_t 를 생성한다.
* 그 다음 해당하는 table id, page number의 lock table이 가르키는 tail로 이동하여 head까지 순차적으로 같은 레코드에 접근하는 락이 있는지 없는지 확인한다.
  * 없는 경우 tail에 락 객체를 append 하고 tail을 append한 객체로 바꿔준다.
  * 만약 Shared Mode, Exclusive Mode 각각에 맞게 (같은 페이지에 접근하는 Exclusive lock, 같은 페이지에 접근하는 모든 lock) 같은 레코드에 접근하고 있어 기다려야할 락이 존재한다면 먼저 기다리는 lock의 트랜잭션 ID를 계속 확인하고 앞으로 나아가는 식으로 Deadlock Detection을 한다.
      * Deadlock이 탐지된다면 (즉, 기다리는 락의 트랜잭션 ID가 현재 Operation 하는 트랜잭션이라면) 해당 트랜잭션을 Abort 시키고 해당 트랜잭션 안에서 일어난 변경사항을 모두 rollback 한다.
      * Deadlock이 없다면 해당 락의 조건 변수를 이용하여 Global Latch에 해당하는 mutex의 lock을 풀어주고 해당 락이 풀릴 때까지 sleep 한다. 이후 notify를 통해 깨어나면 다시 Global Latch의 mutex를 lock 하고 더 이상 기다려야 할 락이 있는지 없는지 확인한다. 있는 경우는 이전과 마찬가지로 다시 대기하며, 없는 경우는 락을 걸고 해당 Operation을 수행한다.
* 트랜잭션을 종료하여 락을 해제하는 경우, 해당 trx의 trx_locks를 타고 순서대로 lock 을 해제한다. 해제할 때 링크드 리스트와 Head, Tail을 적절하게 조절하며, 해당 락을 기다리고 있을 스레드를 awake 하기 위해 notify_all()을 한다.

