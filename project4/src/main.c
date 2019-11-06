#include "bptservice.h"

// MAIN
int main( int argc, char ** argv ) {

	int ret, cnt = 0, table_num, table_num2;
	int join_cnt = 0;
	int instruction;
	char value[4096];
    int64_t input;

	init_db(100000);

	// printf("> ");
    
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

		cnt++;

		if (cnt % 10000 == 0)
			printf("%d\n", cnt);
    }

	//system("pause");
    

	shutdown_db();

    return EXIT_SUCCESS;
}
