#include <stdlib.h>
#include <string.h>
#include "mem_pool/mem_pool_internal.h"
#include "data_glue/data_glue_interface.h"
#include "platform/platform_interface.h"
//void __test_mem_pool_swap(void * pool);
static char * g_shortText = "This is a test. This is a test. This is a test. This is a test.";
static char * g_longText = "A very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long very long test";
static int test_locked_mem_pool_single_thread(enum _MEM_TYPE type) {
	int err = 0;
	void * pool = NULL, * lock = NULL, * mem = NULL;
	struct _MEM_PTR memPtr = {NULL, 0, 0, NULL, 0};
	if (!__glue_create_lock_for_pool(&lock, GLUE_LOCK_FOR_POOL, __mem_pool_create(NULL, "MEM_POOL_SIMPLE", type, &pool, NULL), NULL)) {
		return err;
	}
	if (!(mem = __mem_pool_malloc(pool, "Master1", strlen(g_shortText) + 1, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		strcpy((char *)mem, g_shortText);
		__mem_pool_close(pool, &memPtr);
	}
	if (!(mem = __mem_pool_open(pool, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		if (strcmp((char *)mem, g_shortText)) {
			err = FATAL_UNEXPECTED_ERROR;
			goto error;
		}
		__mem_pool_close(pool, &memPtr);
	}

	if (!(mem = __mem_pool_malloc(pool, "Slave1", strlen(g_shortText) + 1, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		strcpy((char *)mem, g_shortText);
		__mem_pool_close(pool, &memPtr);
	}
	if (!(mem = __mem_pool_open(pool, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		if (strcmp((char *)mem, g_shortText)) {
			err = FATAL_UNEXPECTED_ERROR;
			goto error;
		}
		__mem_pool_close(pool, &memPtr);
	}

	memPtr.pID = "Master1";
	__mem_pool_free(pool, &memPtr);
	if (__mem_pool_open(pool, &memPtr)) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	}
	if (!(mem = __mem_pool_realloc(pool, memPtr.pID, strlen(g_longText) + 1, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		strcpy((char *)mem, g_longText);
		__mem_pool_close(pool, &memPtr);
	}
	if (!(mem = __mem_pool_open(pool, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		if (strcmp((char *)mem, g_longText)) {
			err = FATAL_UNEXPECTED_ERROR;
			goto error;
		}
		__mem_pool_close(pool, &memPtr);
	}

	memPtr.pID = "Slave1";
	if (!(mem = __mem_pool_realloc(pool, memPtr.pID, strlen(g_longText) + 1, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		strcpy((char *)mem, g_longText);
		__mem_pool_close(pool, &memPtr);
	}
	if (!(mem = __mem_pool_open(pool, &memPtr))) {
		err = FATAL_UNEXPECTED_ERROR;
		goto error;
	} else {
		if (strcmp((char *)mem, g_longText)) {
			err = FATAL_UNEXPECTED_ERROR;
			goto error;
		}
		__mem_pool_close(pool, &memPtr);
	}
error:
	__glue_destroy_lock_for_pool(lock);
	__mem_pool_destroy(pool);
	return err;
}
int __test_mem_pool() {
	int err = 0;
	if (err = test_locked_mem_pool_single_thread(MEM_ANON_FILE)) {
		__write_to_console("Failed test case on test_locked_mem_pool_single_thread, type: MEM_ANON_FILE\n");
		return err;
	}
	return err;
}