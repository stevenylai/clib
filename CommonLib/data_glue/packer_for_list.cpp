#include "detect_mem_leak.h"
#include <string.h>
#include "error/error.h"
#include "data_glue_internal.h"
#include "data_converter/data_converter_internal.h"
#include "data_structure/data_structure_internal.h"
static struct _PACKER_FOR_DATA_STRUCTURE * packed_list_get_glue (struct _LIST * pstList) {
	void ** pGlues = (void **)((char *)pstList->pGlue + sizeof(struct _GLUE_HEAD));
	if (!pstList->pGlue) return NULL;
	else {
		return (struct _PACKER_FOR_DATA_STRUCTURE *)pGlues[GLUE_PACKER_FOR_LIST];
	}
}
static void packed_list_remove_glue (struct _LIST * pstList) {
	void ** pGlues = (void **)((char *)pstList->pGlue + sizeof(struct _GLUE_HEAD));
	if (pstList->pGlue && pGlues[GLUE_PACKER_FOR_LIST]) {
		((struct _GLUE_HEAD *)pstList->pGlue)->count--;
		pGlues[GLUE_PACKER_FOR_LIST] = NULL;
		if (!((struct _GLUE_HEAD *)pstList->pGlue)->count) {
			free(pstList->pGlue), pstList->pGlue = NULL;
		}
	}
}
static struct _PACKER_FOR_DATA_STRUCTURE * packed_list_add_glue (struct _LIST * pstList, struct _PACKER_FOR_DATA_STRUCTURE * pstGlue) {
	if (!pstList->pGlue) {
		if (pstList->pGlue = malloc(sizeof(void *) * TOTAL_GLUE_TYPES + sizeof(struct _GLUE_HEAD))) {
			memset(pstList->pGlue, 0, sizeof(void *) * TOTAL_GLUE_TYPES + sizeof(struct _GLUE_HEAD));
		} else {
			return NULL;
		}
	}
	void ** pGlues = (void **)((char *)pstList->pGlue + sizeof(struct _GLUE_HEAD));
	if (pGlues[GLUE_PACKER_FOR_LIST]) return NULL; // Already exists
	((struct _GLUE_HEAD *)pstList->pGlue)->count++;
	return (struct _PACKER_FOR_DATA_STRUCTURE *)(pGlues[GLUE_PACKER_FOR_LIST] = pstGlue);
}
static void * packed_list_get (struct _LIST * pstList, unsigned int index, size_t * pSize) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return NULL;
	}
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	void * pData = pstListGlue->list_get(pstList, index, pSize);
	if (!pData) {
		return pData;
	}
	struct _DATA_CONVERT_INFO stData = {NULL, (void *)pData, NULL, 0, 0};
	__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C, &stData, NULL);
	if (stData.iErr) {
		if (stData.pRight) {
			__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C_FREE, &stData, NULL);
			free (stData.pRight), stData.pRight = NULL;
		}
	} 
	return stData.pRight;

}
static void * packed_list_insert (struct _LIST * pstList, unsigned int index, size_t size, const void * pData) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return NULL;
	}
	void * pInserted = NULL;
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	struct _DATA_CONVERT_INFO stData = {NULL, (void *)pData, NULL, 0, 0};
	__data_converter_convert(pstGlue->pPacker, C_TO_PACKEDC, &stData, NULL);
	if (stData.pRight) {
		pInserted = pstListGlue->list_insert(pstList, index, stData.size, stData.pRight);
		free (stData.pRight), stData.pRight = NULL;
	}
	return pInserted;
}
static void * packed_list_update (struct _LIST * pstList, unsigned int index, size_t size, const void * pData) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return NULL;
	}
	void *pUpdated = NULL;
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	struct _DATA_CONVERT_INFO stData = {NULL, (void *)pData, NULL, 0, 0};
	__data_converter_convert(pstGlue->pPacker, C_TO_PACKEDC, &stData, NULL);
	if (stData.pRight) {
		pUpdated = pstListGlue->list_update(pstList, index, stData.size, stData.pRight);
		free (stData.pRight), stData.pRight = NULL;
	}
	return pUpdated;
}
static void * packed_list_next (struct _LIST * pstList, size_t * pSize, unsigned int * pIndex) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return NULL;
	}
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	struct _DATA_CONVERT_INFO stData = {NULL, NULL, NULL, 0, 0};
	if (pstListGlue->pIteratorUnpackedData && pstGlue->enType == GLUE_PACKER_FOR_LIST_AUTO_FREE) { // Free existing
		stData.pLeft =  pstListGlue->pIteratorPackedData, stData.pRight = pstListGlue->pIteratorUnpackedData;
		__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C_FREE, &stData, NULL);
		free (pstListGlue->pIteratorUnpackedData), pstListGlue->pIteratorUnpackedData = NULL;
	}
	stData.pRight = NULL, stData.pLeft = pstListGlue->pIteratorPackedData = pstListGlue->next(pstList, pSize, pIndex);
	if (!stData.pLeft) return NULL;
	__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C, &stData, NULL);
	if (stData.iErr) {
		if (stData.pRight) {
			__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C_FREE, &stData, NULL);
			free (stData.pRight), stData.pRight = NULL;
		}
	} 
	return pstListGlue->pIteratorUnpackedData = stData.pRight;
}
static bool packed_list_has_next (struct _LIST * pstList) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return NULL;
	}
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	struct _DATA_CONVERT_INFO stData = {NULL, NULL, NULL, 0, 0};
	if (pstListGlue->pIteratorUnpackedData && pstGlue->enType == GLUE_PACKER_FOR_LIST_AUTO_FREE) { // Free existing
		stData.pLeft = pstListGlue->pIteratorPackedData, stData.pRight = pstListGlue->pIteratorUnpackedData;
		__data_converter_convert(pstGlue->pPacker, PACKEDC_TO_C_FREE, &stData, NULL);
		free (pstListGlue->pIteratorUnpackedData), pstListGlue->pIteratorUnpackedData = NULL;
	}
	return pstListGlue->has_next(pstList);
}
static void packed_list_start_iterator (struct _LIST *pstList, unsigned int startIndex) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = packed_list_get_glue(pstList);
	if (!pstGlue) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_UNEXPECTED_ERROR, "Fatal error: unexpected NULL value found in the list's glue");
		return;
	}
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue);
	pstListGlue->pIteratorUnpackedData = NULL; // Reset this before every iteration
	return pstListGlue->start_iterator(pstList, startIndex);
}
static void * packer_create_glue_for_list (struct _PACKER_FOR_DATA_STRUCTURE * pstGlue, struct _ERROR_HANDLER * pstError) {
	bool bErr = false;
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *) (pstGlue->pGlue = malloc(sizeof(struct _PACKER_FOR_LIST)));
	if (!pstListGlue) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory for packer glue");
		bErr = true;
		goto error;
	}
	pstListGlue->pIteratorPackedData = pstListGlue->pIteratorUnpackedData = NULL;
	struct _LIST * pstList = (struct _LIST *)pstGlue->pDataStructure;
	// Save the original function pointers
	pstListGlue->list_get = pstList->list_get;
	pstListGlue->list_insert = pstList->list_insert;
	pstListGlue->list_update = pstList->list_update;
	pstListGlue->next = pstList->next;
	pstListGlue->has_next = pstList->has_next;
	pstListGlue->start_iterator = pstList->start_iterator;
	// Replace with the new ones
	pstList->list_get = packed_list_get;
	pstList->list_insert = packed_list_insert;
	pstList->list_update = packed_list_update;
	pstList->next = packed_list_next;
	pstList->has_next = packed_list_has_next;
	pstList->start_iterator = packed_list_start_iterator;
	// Need to save the pointer to implement the common list functions
	if (!packed_list_add_glue (pstList, pstGlue)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to add packer glue for list");
		bErr = true;
		goto error;
	}
error:
	if (bErr && pstGlue->pGlue) free (pstGlue->pGlue), pstGlue->pGlue = NULL;
	return pstGlue;
}
static void packer_destroy_glue_for_list (struct _PACKER_FOR_DATA_STRUCTURE * pstGlue) {
	struct _PACKER_FOR_LIST * pstListGlue = (struct _PACKER_FOR_LIST *)pstGlue->pGlue;
	struct _LIST * pstList = (struct _LIST *)pstGlue->pDataStructure;
	// Restore the function pointers
	pstList->list_get = pstListGlue->list_get;
	pstList->list_insert = pstListGlue->list_insert;
	pstList->list_update = pstListGlue->list_update;
	pstList->next = pstListGlue->next;
	pstList->has_next = pstListGlue->has_next;
	pstList->start_iterator = pstListGlue->start_iterator;
	// Remove the glue and free data
	packed_list_remove_glue (pstList);
	free (pstGlue->pGlue), pstGlue->pGlue = NULL;
}
void __packer_glue_list_set_func_ptr (struct _PACKER_FOR_DATA_STRUCTURE * pstGlue) {
	pstGlue->create = packer_create_glue_for_list;
	pstGlue->destroy = packer_destroy_glue_for_list;
}