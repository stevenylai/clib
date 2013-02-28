#ifndef __MEM_POOL_INTERNAL_H__
#define __MEM_POOL_INTERNAL_H__

#include "error/error.h"
#include "mem_pool_interface.h"
#include "data_structure/data_structure_interface.h" // For allocator
#define MEM_POOL_DEFAULT_NAME "MEM_POOL"
#define MEM_POOL_REALLOC_SIZE_FILE 128
#define MEM_POOL_BLOCK_SIZE MEM_POOL_REALLOC_SIZE_FILE
#define MEM_POOL_MIN_FIT_SIZE (MEM_POOL_REALLOC_SIZE_FILE/2)
#define _SHARED_MEM_DEBUG_
#define _SHARED_MEM_DEBUG_DETAIL_
typedef struct _MEM_POOL_INFO {
	unsigned int poolCount;
	unsigned int refCount;
	unsigned int memRecListSize;
	unsigned int indexListSize;
	char * pcMemName;
	char * pcIndexName;
} MEM_POOL_INFO;
typedef struct _MMAP_RECORD {
	char * pcID;
	void * pMmap;
	void * pData;
	unsigned int offsetCount;
	size_t * offsets;
} MMAP_RECORD;
typedef struct _MEM_RECORD_BLOCK {
	size_t offset;
	size_t length;
} MEM_RECORD_BLOCK;
typedef struct _MEM_RECORD {
	char * pcID;
	char * pcMemName;
	enum _MEM_TYPE enType;

	unsigned int availCount;
	struct _MEM_RECORD_BLOCK * pAvailableBlocks;

	unsigned int usedCount;
	struct _MEM_RECORD_BLOCK * pUsedBlocks;
} MEM_RECORD;
typedef struct _MEM_POOL_DATA_STRUCTURE {
	void * pData;
	void * pPacker;
	void * pGlue;
} MEM_POOL_DATA_STRUCTURE;
typedef struct _MEM_POOL {
	enum _MEM_TYPE enType;
	struct _MEM_POOL * pstParent;
	char * pcID;
	struct _ERROR_HANDLER * pstError;
	struct _MEM_POOL_INFO * pstPoolInfo;			// Essential info that will be extracted only once during each operation
	struct _MEM_POOL_DATA_STRUCTURE stPoolInfo;		// Single data only: pGlue == NULL

	struct _MEM_POOL_DATA_STRUCTURE stMemRecords;
	struct _MEM_POOL_DATA_STRUCTURE stHandles;		// FIXME: use a tree
	//MEM_POOL_DATA_STRUCTURE stIndices;			// FIXME: use trees to speed up the lookup for pMemRecord

	void * pSubPools;
	void * pExtra;
	void * pGlue;
	// Essential pool operations
	void * (*mem_pool_create) (struct _MEM_POOL *, const char *, struct _MEM_POOL *, struct _ERROR_HANDLER *);
	void (*mem_pool_destroy) (struct _MEM_POOL *);
	void (*mem_pool_gc) (struct _MEM_POOL *);
	void * (*mem_pool_malloc)(struct _MEM_POOL *, const char *, size_t, struct _MEM_PTR *);
	void * (*mem_pool_realloc)(struct _MEM_POOL *, const char *, size_t, struct _MEM_PTR *);
	void (*mem_pool_free)(struct _MEM_POOL *, struct _MEM_PTR *);
	void * (*mem_pool_open)(struct _MEM_POOL *, struct _MEM_PTR *);
	void (*mem_pool_close)(struct _MEM_POOL *, struct _MEM_PTR *);
} MEM_POOL;
/* Shared memory specific structures */
typedef struct _SHARED_MEM_EXTRA {
	struct _ALLOCATOR stMemRecordAllocator;
} SHARED_MEM_EXTRA;
/* Function pointer setters */
void __mem_pool_set_func_ptr_anon_file (struct _MEM_POOL *pstPool);
/* Auxiliary */
void __mem_pool_debug_printf (char * pcFormat, ...);
void __trace_shared_mem_pool (struct _MEM_POOL * pstPool);
#endif
