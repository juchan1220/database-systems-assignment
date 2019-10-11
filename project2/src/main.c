#include "bptservice.h"

// MAIN
int main( int argc, char ** argv ) {

	int ret, cnt = 0;
	char input_file[50];
	char value[125];
    int64_t input, range2;
    char instruction;

    //printf("table file name: ");
	//scanf("%s", input_file);

	open_table("test.db");

	printf("> ");
    while (scanf("%c", &instruction) != EOF) {
        switch (instruction) {
        case 'd':
            scanf("%lld", &input);
			db_delete(input);
            //print_tree(root);
            break;
        case 'i':
#ifdef DEBUG
            scanf("%lld", &input);
			memset(value, 0, sizeof value);
			sprintf(value, "%lld", input);
#else
			scanf("%lld%*c%s", &input, value);
#endif

			db_insert(input, value);
            break;
        case 'f':
			scanf("%lld", &input);
			ret = db_find(input, &value);

#ifdef DEBUG
			if (ret != 0) {
				printf("key %lld not found\n", input);
			}
			else {
				printf("key %lld: %s\n", input, value);
			}
#endif

			break;
        case 'p':
#ifdef DEBUG
			print_tree();
#endif
            break;
        case 'r':
            //scanf("%d %d", &input, &range2);
            //if (input > range2) {
            //    int tmp = range2;
            //    range2 = input;
            //    input = tmp;
            //}
            //find_and_print_range(root, input, range2, instruction == 'p');
            break;
        case 'l':
            //print_leaves(root);
            break;
        case 'q':
            while (getchar() != (int)'\n');
            return EXIT_SUCCESS;
            break;
        case 't':
            //print_tree(root);
            break;
        case 'v':
            //verbose_output = !verbose_output;
            break;
        case 'x':
            //if (root)
            //    root = destroy_tree(root);
            //print_tree(root);
            break;
        default:
            break;
        }

		cnt++;
		if (cnt % 10000 == 0) {
			printf("instruction %d passed!\n", cnt);
			fflush(stdout);
		}

        while (getchar() != (int)'\n');
        //printf("> ");
    }
    printf("\n");

    return EXIT_SUCCESS;
}
