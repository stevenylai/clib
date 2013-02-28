#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure/data_structure_internal.h"
#include "platform/platform_interface.h"
static int test_list (enum _DATA_STRUCTURE_TYPE listType) {
	int err = SUCCESS;
	void * pList = NULL;
	if (!__list_create (&pList,  listType, NULL, NULL)) {
		return err = FATAL_CREATION_ERROR;
	}
	char * testData[8] = {"egg", "tom", "test1", "test2", "apple", "pear", "a very long train", "door"};
	for (unsigned int i = 0; i < sizeof(testData) / sizeof(char*); i++) {
		if (!__list_insert (pList, i, strlen(testData[i]) + 1, testData[i])) {
			err = MALLOC_NO_MEM;
			goto error;
		}
	}
	if (__list_count (pList) != sizeof(testData) / sizeof(char*)) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	}
	if (__list_insert(pList, __list_count(pList) + 2, strlen(testData[0]) + 1, testData[0])) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	}
	testData[1] = "tom2 tom3 tom4";
	if (!__list_update(pList, 1, strlen(testData[1]) + 1, testData[1])) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	}
	for (__list_start_iterator(pList, 0); __list_has_next(pList);) {
		size_t size = 0;
		unsigned int index = 0;
		char * item = (char *)__list_next(pList, &size, &index);
		if (!item || index >= sizeof(testData) / sizeof(char*)) {
			err = FATAL_UNEXPECTED_ERROR;
			goto error;
		}
		if (err = strcmp(item, testData[index])) {
			goto error;
		}
	}
error:
	__list_destroy (pList);
	return err;
}
int __test_list() {
	int err = 0;
	if (err = test_list(ARRAY_LIST_FLAT)) {
		__write_to_console("Failed test case on test_list, type: ARRAY_LIST_FLAT\n");
		return err;
	}
	if (err = test_list(LINKED_LIST_POINTER)) {
		__write_to_console("Failed test case on test_list, type: LINKED_LIST_POINTER\n");
		return err;
	}
	return err;
}