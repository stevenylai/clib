// SharedMemTester1.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

#include "detect_mem_leak.h"
#include "error/error.h"
#include "tests/test_interface.h"

int _tmain(int argc, char * argv[]) {
	int err = SUCCESS;
	if (err = __test_parser()) goto error;
	if (err = __test_list()) goto error;
	if (err = __test_binary_search_tree()) goto error;
	if (err = __test_data_converter()) goto error;
	if (err = __test_packer_for_list()) goto error;
	if (err = __test_mem_pool()) goto error;
error:
	_CrtDumpMemoryLeaks();
	return 0;
}

