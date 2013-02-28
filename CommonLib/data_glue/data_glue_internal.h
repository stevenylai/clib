#ifndef __DATA_GLUE_INTERNAL_H__
#define __DATA_GLUE_INTERNAL_H__
#include "error/error.h"
#include "data_glue_interface.h"
#define SHARED_MEM_LOCK_TIMEOUT 128
#define SHARED_MEM_LOCK_SUFFIX "_LOCK"
/* Packer glued with other */
typedef struct _GLUE_HEAD {
	size_t count;
} GLUE_HEAD;
typedef struct _PACKER_FOR_DATA_STRUCTURE {
	enum _DATA_GLUE_TYPES enType;
	struct _ERROR_HANDLER * pstError;
	void * pPacker;
	void * pDataStructure;
	void * pGlue;
	void * (*create)(struct _PACKER_FOR_DATA_STRUCTURE *, struct _ERROR_HANDLER *);
	void (*destroy) (struct _PACKER_FOR_DATA_STRUCTURE *);
} PACKER_FOR_DATA_STRUCTURE;
typedef struct _PACKER_FOR_LIST {
	void * pIteratorPackedData;
	void * pIteratorUnpackedData;
	void * (*list_get) (struct _LIST *, unsigned int, size_t *);
	void * (*list_insert) (struct _LIST *, unsigned int, size_t, const void *);
	void * (*list_update) (struct _LIST *, unsigned int, size_t, const void *);
	void (*start_iterator) (struct _LIST *, unsigned int);
	void * (*next) (struct _LIST *, size_t *, unsigned int *);
	bool (*has_next) (struct _LIST *);
} PACKER_FOR_LIST;
typedef struct _MEM_LOCK {
	char * pcName;
	void * pLockHandle;
} MEM_LOCK;
typedef struct _MEM_LOCK_DATA_STRUCTURE {
	void * pData;
	void * pPacker;
	void * pGlue;
} MEM_LOCK_DATA_STRUCTURE;
typedef struct _LOCK_FOR_POOL {
	enum _DATA_GLUE_TYPES enType;
	void * pPool;
	struct _ERROR_HANDLER * pstError;
	void * poolLock;
	struct _MEM_LOCK_DATA_STRUCTURE stLockList; // FIXME: use a binary search tree

	void * (*create)(struct _LOCK_FOR_POOL *, struct _ERROR_HANDLER *);
	void (*destroy) (struct _LOCK_FOR_POOL *);
	void (*mem_pool_gc) (struct _MEM_POOL *);
	void * (*mem_pool_malloc)(struct _MEM_POOL *, const char *, size_t, struct _MEM_PTR *);
	void * (*mem_pool_realloc)(struct _MEM_POOL *, const char *, size_t, struct _MEM_PTR *);
	void (*mem_pool_free)(struct _MEM_POOL *, struct _MEM_PTR *);
	void * (*mem_pool_open)(struct _MEM_POOL *, struct _MEM_PTR *);
	void (*mem_pool_close)(struct _MEM_POOL *, struct _MEM_PTR *);
} LOCK_FOR_POOL;
void __packer_glue_list_set_func_ptr (struct _PACKER_FOR_DATA_STRUCTURE * pstGlue);
#endif