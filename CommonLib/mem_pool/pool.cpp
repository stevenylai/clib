#include "detect_mem_leak.h"
#include <string.h>
#include "mem_pool_internal.h"
#include "error/error.h"
static void pool_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, pool_default_error};
void * __mem_pool_create (void * pParentPool, const char * pcID, enum _MEM_TYPE enType, void ** pPool, struct _ERROR_HANDLER * pstError) {
	int err = SUCCESS;
	if (!pstError) pstError = &g_stDefaultError;
	struct _MEM_POOL *pstParentPool = (struct _MEM_POOL *)pParentPool, *pstPool = (struct _MEM_POOL *)(*pPool = malloc(sizeof(struct _MEM_POOL)));
	if (!pstPool) {
		pstError->handle_error(pstError->pHandler, err = FATAL_CREATION_ERROR, "Failed to allocate memory for constructing the pool");
		goto error;
	} else {
		memset(pstPool, 0, sizeof(struct _MEM_POOL));
	}
	switch (pstPool->enType = enType) {
	case MEM_ANON_FILE:
		__mem_pool_set_func_ptr_anon_file(pstPool);
		break;
	default:
		pstError->handle_error(pstError->pHandler, err = FATAL_CREATION_ERROR, "There is no implementation for this type of memory pool");
		break;
	}
	if (!pstPool->mem_pool_create(pstParentPool, pcID, pstPool, pstError)) {
		err = FATAL_CREATION_ERROR;
		goto error;
	}
error:
	if (err) free(pstPool), *pPool = pstPool = NULL;
	return *pPool;
}
void __mem_pool_destroy (void * pPool) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	pstPool->mem_pool_destroy(pstPool);
	free (pstPool);
}
void __mem_pool_gc (void * pPool) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	pstPool->mem_pool_gc(pstPool);
}
void * __mem_pool_malloc(void * pPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	return pstPool->mem_pool_malloc(pstPool, pcKey, size, pstPtr);
}
void * __mem_pool_realloc(void * pPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	return pstPool->mem_pool_realloc(pstPool, pcKey, size, pstPtr);
}
void __mem_pool_free(void * pPool, struct _MEM_PTR * pstPtr) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	pstPool->mem_pool_free(pstPool, pstPtr);
}
void * __mem_pool_open(void * pPool, struct _MEM_PTR * pstPtr) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	return pstPool->mem_pool_open(pstPool, pstPtr);
}
void __mem_pool_close(void * pPool, struct _MEM_PTR * pstPtr) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pPool;
	pstPool->mem_pool_close(pstPool, pstPtr);
}
