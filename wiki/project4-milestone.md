2018008277 CSE 노주찬<br>

C++이 허용되었지만 아직은 C로도 충분하다고 생각하여 C를 사용하기로 결정했다. bptservice.c 에 `join_table()` 이 구현된 것을 제외하곤 수정이 없었다.
<br>
<br>
`join_table()` 의 로직은 다음과 같다.
- 1. 먼저 주어진 table이 열려있는지 확인한다. 열려있지 않으면 -1 을 반환한다.
- 2. 결과를 작성할 파일을 fopen을 통해 연다. 열리지 않으면 -1을 반환한다.
- 3. key의 타입이 `int64_t` 이므로 bpt.c 에서 제공하는 `find_leaf(LLONG_MIN)` 을 통해서 각 table의 leftmost leaf를 찾는다. 존재하지 않으면 -1을 반환한다.
- 4. 투 포인터 방식을 통해 Natural Join 을 수행한다. 어느 한 테이블을 기준으로 하여, 반대쪽 테이블의 키 값이 기준이 되는 키 값보다 크거나 같을 때까지 계속 right로 이동한다. 키 값이 같으면 Join 결과를 출력한다. [Piazza에서 이 출력이 Layer Architecture를 지킬 필요는 없다](https://piazza.com/class/k01kszf1uom49h?cid=122)고 했으므로 바로 fprintf 를 통해 출력한다. 그리고 기준이 되는 테이블의 키 값도 한칸 right으로 이동하여 증가시킨다. (현재 B+ Tree가 중복된 키 값을 지원하지 않기에 가능하다.)
- 4-1. right로 이동하다 페이지의 끝에 도달하면 (num_keys 에 도달하면) right_sibling (실제 코드에서는 another_page) 을 통해 다음 페이지로 이동한다. 다음 페이지가 0, 다시말해 현재 페이지가 끝이면 현재 보고있던 페이지를 Unpin 하고 Join을 종료한다. 
