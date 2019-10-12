#include "bptservice.h"

// MAIN
int main( int argc, char ** argv ) {

	int ret, cnt = 0;
	char instruction[50];
	char value[4096];
    int64_t input, range2;

	printf("> ");
    while (scanf("%s", instruction) != EOF) {
		if (strcmp(instruction, "open") == 0) {
			scanf("%s", value);
			open_table(value);
		}
		else if (strcmp(instruction, "insert") == 0) {
#ifdef DEBUG
			scanf("%lld", &input);
			memset(value, 0, sizeof value);
			sprintf(value, "%lld", input);
#else
			scanf("%lld%*c%s", &input, value);
#endif

			db_insert(input, value);
		}
		else if (strcmp(instruction, "find") == 0) {
			scanf("%lld", &input);
			ret = db_find(input, &value);
		}
		else if (strcmp(instruction, "delete") == 0) {
			scanf("%lld", &input);
			db_delete(input);
		}
		else if (strcmp(instruction, "quit") == 0) {
			return EXIT_SUCCESS;
		}
    }
    printf("\n");

    return EXIT_SUCCESS;
}
