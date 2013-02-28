#include <stdlib.h>
#include <string.h>
#include "data_structure/data_structure_interface.h"
#include "data_converter/data_converter_interface.h"
#include "data_glue/data_glue_interface.h"
#include "platform/platform_interface.h"
typedef struct {
	char * studentName;
	int studentAge;
} STUDENT;
int test_packer_for_list(enum _DATA_STRUCTURE_TYPE listType) {
	int err = 0;
	STUDENT students [2] = {{"TOM", 15}, {"Mary", 15}}, *iter = NULL;

	void * list = NULL, * packer = NULL, *glue = NULL;
	__glue_create_packer_for_data_structure(&glue, __data_converter_create(&packer, MEMORY_TO_MEMORY, "struct{string;unsigned int;};", NULL), GLUE_PACKER_FOR_LIST_AUTO_FREE, __list_create(&list, listType, NULL, NULL), NULL);

	for (unsigned int i = 0; i < sizeof(students) / sizeof(STUDENT); i++) {
		__list_insert(list, i, 0, students + i);
	}

	for(__list_start_iterator(list, 0); __list_has_next(list);) {
		unsigned int index = 0;
		iter = (STUDENT *)__list_next(list, NULL, &index);
		if (err = strcmp(students[index].studentName, iter->studentName)) {
			free(iter);
			goto error;
		}
		if (err = (int)students[index].studentAge - (int)iter->studentAge) {
			free(iter);
			goto error;
		}
	}
error:
	__glue_destroy_packer_for_data_structure(glue);
	__data_converter_destroy(packer);
	__list_destroy(list);
	return err;
}
int __test_packer_for_list() {
	int err = 0;
	if (err = test_packer_for_list(ARRAY_LIST_FLAT)) {
		__write_to_console("Failed test case on test_packer_for_list, type: ARRAY_LIST_FLAT\n");
		return err;
	}
	if (err = test_packer_for_list(LINKED_LIST_POINTER)) {
		__write_to_console("Failed test case on test_packer_for_list, type: LINKED_LIST_POINTER\n");
		return err;
	}
	return err;
}