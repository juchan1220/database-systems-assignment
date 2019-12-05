#include "bptservice.h"
#include <thread>
#include <condition_variable>
#include <mutex>

using namespace std;
int table_num;
std::mutex deadlock_mtx;
condition_variable cv;

volatile bool chk[10];
int func1(int thread_num)
{
	int tid = begin_trx();
	char* buffer = new char[VALUE_SIZE];

	int ret = 0;

	printf("Thread1: 1S lock trying...\n");
	ret = db_find(table_num, 1, buffer, tid);
	if (ret != 0) {
		printf("Thread1: db_find (trx) Failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("Thread1: 1S lock Ok : %s\n", buffer);
	fflush(stdout);

	chk[0] = true;
	while (chk[2] == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	sprintf(buffer, "NEWDATA_%04d", 500);
	printf("Thread1: 1X lock trying...\n");
	chk[3] = true;
	ret = db_update(table_num, 500, buffer, tid);
	if (ret != 0) {
		printf("Thread1: db_update (trx) Failed.\n");
		return 0;
		// exit(EXIT_FAILURE);
	}
	printf("Thread1: 1X lock Ok: %s\n", buffer);

	end_trx(tid);
	delete[] buffer;

	return 0;
}

int func2(int thread_num)
{
	int tid = begin_trx();
	char* buffer = new char[VALUE_SIZE];

	int ret;

	printf("Thread2: 2S lock trying...\n");
	ret = db_find(table_num, 500, buffer, tid);
	if (ret != 0) {
		printf("Thread2: db_find (trx) Failed.\n");
		// exit(EXIT_FAILURE);
	}
	printf("Thread2: 2S lock complete!: %s\n", buffer);
	chk[1] = true;
	while (chk[2] == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	
	sprintf(buffer, "NEWDATA_%04d", 1);
	printf("Thread2: 2X lock trying...\n");
	chk[4] = true;
	ret = db_update(table_num, 1, buffer, tid);
	if (ret != 0) {
		printf("Thread2: db_update (trx) Failed.\n");
		return 0;
		// exit(EXIT_FAILURE);
	}
	printf("Thread2: 2X lock Ok: %s\n", buffer);

	if(ret == 0) end_trx(tid);
	delete[] buffer;

	return 0;
}

int func3(int thread_num)
{
	int tid = begin_trx();
	char* buffer = new char[VALUE_SIZE];

	int ret;

	while (chk[0] == false || chk[1] == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	printf("Thread3: 3S lock trying...\n");
	ret = db_find(table_num, 500, buffer, tid);
	if (ret != 0) {
		printf("Thread2: db_find (trx) Failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("Thread3: 3S lock complete!: %s\n", buffer);

	printf("Thread3: 3S lock trying...\n");
	ret = db_find(table_num, 1, buffer, tid);
	if (ret != 0) {
		printf("Thread2: db_find (trx) Failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("Thread3: 3S lock complete!: %s\n", buffer);

	chk[2] = true;
	while (chk[3] == false || chk[4] == false) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	printf("Thread3: end_trx\n");
	fflush(stdout);
	end_trx(tid);
	delete[] buffer;

	return 0;
}


// MAIN
int main( int argc, char ** argv ) {

	int ret, cnt = 0, table_num2;
	int join_cnt = 0;
	int instruction;
	char value[4096];
    int64_t input;

	init_db(100000);

	// printf("> ");

	/*
	while (scanf("%d", &instruction) != EOF) {

		if (instruction == 0) {
			scanf("%s", value);
			open_table(value);
		}
		else if (instruction == 1) {
			scanf("%d%lld%*c%s", &table_num, &input, value);
			ret = db_insert(table_num, input, value);
			if (ret == 0) {
				//printf("Insert: Success!\n");
			}
			else {
				//printf("Insert: Failed.\n");
			}
		}
		else if (instruction == 2) {
			scanf("%d%lld", &table_num, &input);
			ret = db_find(table_num, input, value);
			
			if (ret == 0){}
				//printf("Find: Success!\n");
			else {}
				//printf("Find: Failed!\n");
		}
		else if (instruction == 3) {
			scanf("%d%lld",&table_num, &input);
			ret = db_delete(table_num, input);
			if (ret == 0) {
				//printf("Delete: Success!\n");
			}
			else {
				//printf("Delete: Failed.\n");
			}
		}
		else if (instruction == 4) {
			scanf("%d", &table_num);
			close_table(table_num);
		}
		else if (instruction == 5) {
			scanf("%d%d%s", &table_num, &table_num2,&value);
			ret = join_table(table_num, table_num2, value);

			if (ret == 0) {
				printf("Join: Success!\n");
			}
			else {
				printf("Join: Failed.\n");
			}
		}
    }
	*/

	sprintf(value, "test.db");
	table_num = open_table(value);

	for (int i = 1; i <= 1000; i++) {
		sprintf(value, "DATA_%04d", i);
		db_insert(table_num, i, value);
	}
	std::thread th2 = std::thread(func2, 2);
	// std::this_thread::sleep_for(std::chrono::milliseconds(100));
	std::thread th1 = std::thread(func1, 1);
	std::thread th3 = std::thread(func3, 3);

	th1.join();
	th2.join();
	th3.join();

	//system("pause");

	close_table(table_num);

	shutdown_db();

    return EXIT_SUCCESS;
}
