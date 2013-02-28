#include "detect_mem_leak.h"
#include <string.h>
#include "platform/platform_interface.h"
#include "error/error.h"
#include "data_glue_internal.h"
#include "mem_pool/mem_pool_internal.h"
#include "data_structure/data_structure_interface.h"
#include "parser/parser_interface.h"
#include "data_converter/data_converter_interface.h"
static void lock_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, lock_default_error};
static struct _LOCK_FOR_POOL * locked_pool_get_glue (struct _MEM_POOL * pstPool) {
	void ** pGlues = (void **)((char *)pstPool->pGlue + sizeof(struct _GLUE_HEAD));
	if (!pstPool->pGlue) return NULL;
	else {
		return (struct _LOCK_FOR_POOL *)pGlues[GLUE_LOCK_FOR_POOL];
	}
}
static void locked_pool_remove_glue (struct _MEM_POOL * pstPool) {
	void ** pGlues = (void **)((char *)pstPool->pGlue + sizeof(struct _GLUE_HEAD));
	if (pstPool->pGlue && pGlues[GLUE_LOCK_FOR_POOL]) {
		((struct _GLUE_HEAD *)pstPool->pGlue)->count--;
		pGlues[GLUE_LOCK_FOR_POOL] = NULL;
		if (!((struct _GLUE_HEAD *)pstPool->pGlue)->count) {
			free(pstPool->pGlue), pstPool->pGlue = NULL;
		}
	}
}
static struct _LOCK_FOR_POOL * locked_pool_add_glue (struct _MEM_POOL * pstPool, struct _LOCK_FOR_POOL * pstGlue) {
	if (!pstPool->pGlue) {
		if (pstPool->pGlue = malloc(sizeof(void *) * TOTAL_GLUE_TYPES + sizeof(struct _GLUE_HEAD))) {
			memset(pstPool->pGlue, 0, sizeof(void *) * TOTAL_GLUE_TYPES + sizeof(struct _GLUE_HEAD));
		} else {
			return NULL;
		}
	}
	void ** pGlues = (void **)((char *)pstPool->pGlue + sizeof(struct _GLUE_HEAD));
	if (pGlues[GLUE_LOCK_FOR_POOL]) return NULL; // Already exists
	((struct _GLUE_HEAD *)pstPool->pGlue)->count++;
	return (struct _LOCK_FOR_POOL *)(pGlues[GLUE_LOCK_FOR_POOL] = pstGlue);
}
static int lock_mem_pool (struct _LOCK_FOR_POOL * pstGlue) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pstGlue->pPool;
	char * pcLockName = NULL;
	int err = SUCCESS;
	if (!pstGlue->poolLock) {
		if (pstPool->pcID) {
			if (!(pcLockName = __asprintf("%s%s", pstPool->pcID, SHARED_MEM_LOCK_SUFFIX))) {
				err = MEM_POOL_LOCK_ERROR;
				goto error;
			}
		}
		if (!__semaphore_create(&pstGlue->poolLock, pcLockName, 1)) {
			err = MEM_POOL_LOCK_ERROR;
			goto error;
		}
	}
	if (err = __semaphore_acquire(pstGlue->poolLock, SHARED_MEM_LOCK_TIMEOUT)) {
		goto error;
	}
error:
	free(pcLockName), pcLockName = NULL;
	return err;
}
static void unlock_mem_pool (struct _LOCK_FOR_POOL * pstGlue, bool destroy) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pstGlue->pPool;
	if (pstGlue->poolLock) {
		__semaphore_release(pstGlue->poolLock);
		if (destroy)
			__semaphore_destroy(pstGlue->poolLock), pstGlue->poolLock = NULL;
	}
}
static int lock_mem_block (struct _LOCK_FOR_POOL * pstGlue, const char * pcName, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	char * pcLockName = NULL;
	struct _MEM_LOCK stNewLock, *pstExistingLock = NULL;
	if (pcName) {
		for (__list_start_iterator(pstGlue->stLockList.pData, 0); __list_has_next(pstGlue->stLockList.pData); pstExistingLock = NULL) {
			pstExistingLock = (struct _MEM_LOCK *)__list_next(pstGlue->stLockList.pData, NULL, NULL);
			if (!strcmp(pstExistingLock->pcName, pcName)) {
				break;
			}
		}
	} else {
		pstExistingLock = (struct _MEM_LOCK *)__list_get(pstGlue->stLockList.pData, 0, NULL);
	}
	if (!pstExistingLock) {
		if (pcName) {
			if (!(pcLockName = __asprintf("%s%s", pcName, SHARED_MEM_LOCK_SUFFIX))) {
				err = MEM_POOL_LOCK_ERROR;
				goto error;
			}
		} // Create the lock first then add to the list / tree
		if (!__semaphore_create(&stNewLock.pLockHandle, pcLockName, 1)) {
			err = MEM_POOL_LOCK_ERROR;
			goto error;
		}
		if (err = __semaphore_acquire(stNewLock.pLockHandle, SHARED_MEM_LOCK_TIMEOUT)) {
			__semaphore_destroy(stNewLock.pLockHandle);
			goto error;
		}
		stNewLock.pcName = (pcName ? pcName : "");
		if (!__list_insert(pstGlue->stLockList.pData, 0, 0, &stNewLock)) {
			err = MEM_POOL_LOCK_ERROR;
			__semaphore_release(stNewLock.pLockHandle);
			__semaphore_destroy(stNewLock.pLockHandle);
		}
	} else {
		if (err = __semaphore_acquire(pstExistingLock->pLockHandle, SHARED_MEM_LOCK_TIMEOUT)) {
			goto error;
		}
	}
error:
	free(pstExistingLock);
	free(pcLockName);
	return err;
}
static void unlock_mem_block (struct _LOCK_FOR_POOL * pstGlue, const char * pcName, struct _MEM_PTR * pstPtr, bool destroy) {
	struct _MEM_LOCK * pstLock = NULL;
	unsigned int index = 0;
	for (__list_start_iterator(pstGlue->stLockList.pData, 0); __list_has_next(pstGlue->stLockList.pData); pstLock = NULL) {
		pstLock = (struct _MEM_LOCK *)__list_next(pstGlue->stLockList.pData, NULL, &index);
		if (!strcmp(pstLock->pcName, pcName)) {
			break;
		}
	} // Remove the entry from the list first. Then close the handle.
	if (pstLock) {
		__semaphore_release(pstLock->pLockHandle);
		if (destroy) {
			__semaphore_destroy(pstLock->pLockHandle);
			__list_remove(pstGlue->stLockList.pData, index);
		}
		free(pstLock);
	}
}
static void * locked_malloc (struct _MEM_POOL * pstPool, const char * pcName, size_t size, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	bool locked = false;
	void * mallocd = NULL;
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return NULL;
	}
	if (err = lock_mem_pool(pstGlue)) {
		goto error;
	}
	if (pcName || pstPool->enType == MEM_HEAP) { // Need to distinguish if this is an malloc without name or is the malloc taking place on the heap (in which case malloc never needs a name)
		if (err = lock_mem_block(pstGlue, pcName, pstPtr)) {
			unlock_mem_pool(pstGlue, false);
			goto error;
		} else {
			locked = true;
		}
	}
	if (!(mallocd = pstGlue->mem_pool_malloc(pstPool, pcName, size, pstPtr))) {
		if (locked)
			unlock_mem_block(pstGlue, pcName, pstPtr, false);
	}
	if (!locked && pstPtr->pID) { // A new key for locking
		lock_mem_block(pstGlue, pstPtr->pID, pstPtr);
	}
	unlock_mem_pool(pstGlue, false);
error:
	return mallocd;
}
static void * locked_realloc (struct _MEM_POOL * pstPool, const char * pcName, size_t size, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	void * reallocd = NULL;
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return NULL;
	}
	if (err = lock_mem_pool(pstGlue)) {
		goto error;
	}
	if (err = lock_mem_block(pstGlue, pcName, pstPtr)) {
		unlock_mem_pool(pstGlue, false);
		goto error;
	}
	if (!(reallocd = pstGlue->mem_pool_realloc(pstPool, pcName, size, pstPtr))) {
		unlock_mem_block(pstGlue, pcName, pstPtr, false);
	}
	unlock_mem_pool(pstGlue, false);
error:
	return reallocd;
}
static void locked_free (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return;
	}
	if (err = lock_mem_pool(pstGlue)) {
		goto error;
	}
	if (err = lock_mem_block(pstGlue, pstPtr->pID, pstPtr)) {
		unlock_mem_pool(pstGlue, false);
		goto error;
	}
	pstGlue->mem_pool_free(pstPool, pstPtr);
	unlock_mem_block(pstGlue, pstPtr->pID, pstPtr, true);
	unlock_mem_pool(pstGlue, false);
error:
	return;
}
static void locked_gc (struct _MEM_POOL * pstPool) {
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return;
	}
	if (lock_mem_pool(pstGlue)) {
		pstGlue->mem_pool_gc(pstPool);
		unlock_mem_pool(pstGlue, false);
	}
}
static void * locked_open (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	void * opened = NULL;
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return NULL;
	}
	if (err = lock_mem_pool(pstGlue)) { // Acquire the lock for the entire pool to safely access the lock list
		goto error;
	}
	if (err = lock_mem_block(pstGlue, pstPtr->pID, pstPtr)) {
		unlock_mem_pool(pstGlue, false);
		goto error;
	}
	if (!(opened = pstGlue->mem_pool_open(pstPool, pstPtr))) {
		unlock_mem_block(pstGlue, pstPtr->pID, pstPtr, false);
	}
	unlock_mem_pool(pstGlue, false);
error:
	return opened;
}
static void locked_close (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	struct _LOCK_FOR_POOL * pstGlue = locked_pool_get_glue(pstPool);
	if (!pstGlue) {
		pstPool->pstError->handle_error(pstPool->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the pool's glue");
		return;
	}
	if (err = lock_mem_pool(pstGlue)) { // Acquire the lock for the entire pool to safely access the lock list
		goto error;
	}
	pstGlue->mem_pool_close(pstPool, pstPtr);
	unlock_mem_block(pstGlue, pstPtr->pID, pstPtr, false);
	unlock_mem_pool(pstGlue, false);
error:
	return;
}
static void * lock_for_pool_create (struct _LOCK_FOR_POOL * pstGlue, struct _ERROR_HANDLER * pstError) {
	int err = SUCCESS;
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pstGlue->pPool;
	pstGlue->poolLock = NULL;
	pstGlue->stLockList.pData = pstGlue->stLockList.pPacker = pstGlue->stLockList.pGlue = NULL;
	if (!__list_create(&pstGlue->stLockList.pData, LINKED_LIST_POINTER, NULL, NULL)) {
		pstError->handle_error (pstError->pHandler, err = FATAL_CREATION_ERROR, "Fatal error in creating list for locks");
		goto error;
	}
	if (!__data_converter_create(&pstGlue->stLockList.pPacker, MEMORY_TO_MEMORY, "struct{string; void *;};", NULL)) {
		pstError->handle_error (pstError->pHandler, err = FATAL_CREATION_ERROR, "Fatal error in creating packer for locks");
		goto error;
	}
	if (!__glue_create_packer_for_data_structure(&pstGlue->stLockList.pGlue, pstGlue->stLockList.pPacker, GLUE_PACKER_FOR_LIST_AUTO_FREE, pstGlue->stLockList.pData, NULL)) {
		pstError->handle_error (pstError->pHandler, err = FATAL_CREATION_ERROR, "Fatal error in creating glue for locks");
		goto error;
	}
	if (!locked_pool_add_glue (pstPool, pstGlue)) {
		pstError->handle_error(pstError->pHandler, err = FATAL_CREATION_ERROR, "Failed to add lock glue for pool");
		goto error;
	}
	pstGlue->mem_pool_malloc = pstPool->mem_pool_malloc;
	pstGlue->mem_pool_realloc = pstPool->mem_pool_realloc;
	pstGlue->mem_pool_free = pstPool->mem_pool_free;
	pstGlue->mem_pool_open = pstPool->mem_pool_open;
	pstGlue->mem_pool_close = pstPool->mem_pool_close;

	pstPool->mem_pool_malloc = locked_malloc;
	pstPool->mem_pool_realloc = locked_realloc;
	pstPool->mem_pool_free = locked_free;
	pstPool->mem_pool_open = locked_open;
	pstPool->mem_pool_close = locked_close;
error:
	if (err) {
		if (pstGlue->stLockList.pGlue) __glue_destroy_packer_for_data_structure(pstGlue->stLockList.pGlue), pstGlue->stLockList.pGlue = NULL;
		if (pstGlue->stLockList.pData) __list_destroy(pstGlue->stLockList.pData), pstGlue->stLockList.pData = NULL;
		if (pstGlue->stLockList.pPacker) __data_converter_destroy(pstGlue->stLockList.pPacker), pstGlue->stLockList.pPacker = NULL;
		return NULL;
	} else {
		return pstGlue;
	}
}
static void lock_for_pool_destroy (struct _LOCK_FOR_POOL * pstGlue) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pstGlue->pPool;
	// Clean up
	for (__list_start_iterator(pstGlue->stLockList.pData, 0); __list_has_next(pstGlue->stLockList.pData);) {
		struct _MEM_LOCK * pstLock = (struct _MEM_LOCK *)__list_next(pstGlue->stLockList.pData, NULL, NULL);
		__semaphore_release(pstLock->pLockHandle);
		__semaphore_destroy(pstLock->pLockHandle);
	}
	unlock_mem_pool(pstGlue, true);
	// Restore the function pointers
	pstPool->mem_pool_malloc = pstGlue->mem_pool_malloc;
	pstPool->mem_pool_realloc = pstGlue->mem_pool_realloc;
	pstPool->mem_pool_free = pstGlue->mem_pool_free;
	pstPool->mem_pool_open = pstGlue->mem_pool_open;
	pstPool->mem_pool_close = pstGlue->mem_pool_close;
	locked_pool_remove_glue (pstPool);
	__glue_destroy_packer_for_data_structure(pstGlue->stLockList.pGlue), pstGlue->stLockList.pGlue = NULL;
	__list_destroy(pstGlue->stLockList.pData), pstGlue->stLockList.pData = NULL;
	__data_converter_destroy(pstGlue->stLockList.pPacker), pstGlue->stLockList.pPacker = NULL;
}
static void lock_for_pool_set_func_ptr (struct _LOCK_FOR_POOL * pstGlue) { // static function because seems there will be only one implementation
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pstGlue->pPool;
	pstGlue->create = lock_for_pool_create;
	pstGlue->destroy = lock_for_pool_destroy;
}

void * __glue_create_lock_for_pool (void ** pGlue, enum _DATA_GLUE_TYPES enType, void * pPool, struct _ERROR_HANDLER * pstError) {
	bool bErr = false;
	if (!pstError) pstError = &g_stDefaultError;
	struct _LOCK_FOR_POOL * pstGlue = (struct _LOCK_FOR_POOL *)(*pGlue = malloc(sizeof(struct _LOCK_FOR_POOL)));
	if (!pstGlue) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory for lock for pool");
		bErr = true;
		goto error;
	} else {
		memset(pstGlue, 0, sizeof(struct _LOCK_FOR_POOL));
	}
	pstGlue->pPool = pPool, pstGlue->pstError = pstError;
	switch(pstGlue->enType = enType) {
		case GLUE_LOCK_FOR_POOL:
			lock_for_pool_set_func_ptr(pstGlue);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "This type of lock glue is not implemented");
			bErr = true;
			goto error;
	}
	if (!pstGlue->create(pstGlue, pstError)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr)
		return NULL;
	else
		return pPool;
}
void __glue_destroy_lock_for_pool (void * pGlue) {
	struct _LOCK_FOR_POOL * pstGlue = (struct _LOCK_FOR_POOL *)pGlue;
	pstGlue->destroy(pstGlue);
	free(pGlue);
}