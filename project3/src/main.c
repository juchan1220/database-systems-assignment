#include "bptservice.h"

// MAIN
int main( int argc, char ** argv ) {

	int ret, cnt = 0, table_num;
	int instruction;
	char value[4096];
    int64_t input;

	init_db(512);

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
				printf("Insert: Success!\n");
			}
			else {
				printf("Insert: Failed.\n");
			}
		}
		else if (instruction == 2) {
			scanf("%d%lld", &table_num, &input);
			ret = db_find(table_num, input, value);
			
			if (ret == 0)
				printf("Find: Success!\n");
			else
				printf("Find: Failed!\n");
		}
		else if (instruction == 3) {
			scanf("%d%lld",&table_num, &input);
			ret = db_delete(table_num, input);
			if (ret == 0) {
				printf("Delete: Success!\n");
			}
			else {
				printf("Delete: Failed.\n");
			}
		}
		else if (instruction == 4) {
			scanf("%d", &table_num);
			close_table(table_num);
		}
    }
    

	shutdown_db();

    return EXIT_SUCCESS;
}
