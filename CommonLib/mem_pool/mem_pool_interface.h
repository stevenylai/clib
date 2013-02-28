#ifndef __MEM_POOL_INTERFACE_H__
#define __MEM_POOL_INTERFACE_H__
#include "error/error.h"
typedef enum _MEM_TYPE {
	MEM_HEAP,
	MEM_ANON_FILE,
	MEM_FILE,
	MEM_SUB_BLOCK,
	MEM_EXTERN
} MEM_TYPE;
typedef struct _MEM_PTR {
	const char * pID;
	size_t size;
	size_t offset;
	void * pData;
	int err;
} MEM_PTR;
// General
void * __mem_pool_create (void * pParentPool, const char * pcID, enum _MEM_TYPE enType, void ** pPool, struct _ERROR_HANDLER * pstError);
void __mem_pool_destroy (void * pPool);
void * __mem_pool_malloc(void * pPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr);
void * __mem_pool_realloc(void * pPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr);
void __mem_pool_free(void * pPool, struct _MEM_PTR * pstPtr);
void __mem_pool_gc (void * pPool);
void * __mem_pool_open(void * pPool, struct _MEM_PTR * pstPtr);
void __mem_pool_close(void * pPool, struct _MEM_PTR * pstPtr);
#endif