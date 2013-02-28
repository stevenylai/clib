#ifndef __DATA_GLUE_INTERFACE_H__
#define __DATA_GLUE_INTERFACE_H__
#include "error/error.h"
typedef enum _DATA_GLUE_TYPES {
	GLUE_PACKER_FOR_LIST,
	GLUE_PACKER_FOR_LIST_AUTO_FREE,
	GLUE_PACKER_FOR_BST,
	GLUE_LOCK_FOR_POOL,
	TOTAL_GLUE_TYPES
} DATA_GLUE_TYPES;
/* Packer for data structures */
void * __glue_create_packer_for_data_structure (void ** pGlue, void * pPacker, enum _DATA_GLUE_TYPES enType, void * pDataStructure, struct _ERROR_HANDLER * pstError);
void __glue_destroy_packer_for_data_structure (void * pGlue);
void * __glue_create_lock_for_pool (void ** pGlue, enum _DATA_GLUE_TYPES enType, void * pPool, struct _ERROR_HANDLER * pstError);
void __glue_destroy_lock_for_pool (void * pGlue);
#endif