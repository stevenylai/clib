#include <stdlib.h>
#include <string.h>
#include "parser/parser_interface.h"
#include "platform/platform_interface.h"
static int test_asprintf() {
	char * pcName = __asprintf("TEST_%d_%s", 2, "TEST");
	int err = strcmp("TEST_2_TEST", pcName);
	free (pcName);
	return err;
}
int __test_parser () {
	int err = 0;
	if (err = test_asprintf()) {
		__write_to_console("Failed test case on test_printf\n");
	}
	return err;
}