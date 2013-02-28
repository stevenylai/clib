#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
static void list_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, list_default_error};
void * __list_create (void ** pList,  enum _DATA_STRUCTURE_TYPE enType, struct _ERROR_HANDLER * pstError, struct _ALLOCATOR * pstAllocator) {
	bool bErr = false;
	if (!pstError) pstError = &g_stDefaultError;
	struct _LIST * pstList = (struct _LIST *)(*pList = malloc(sizeof(struct _LIST)));
	if (pstList == NULL) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory to create list");
		return *pList;
	} else {
		memset(pstList, 0, sizeof(struct _LIST));
	}
	switch (pstList->enType = enType) {
		case ARRAY_LIST_FLAT:
			__array_list_flat_set_func_ptr(pstList);
			break;
		case LINKED_LIST_POINTER:
			__linked_list_pointer_set_func_ptr(pstList);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "This type of list is not implemented");
			bErr = true;
			goto error;
	}
	if (!pstList->list_create(pstList, pstError, pstAllocator)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr) {
		free (pstList), *pList = NULL;
	}
	return *pList;
}
void __list_destroy (void * pList)  {
	struct _LIST * pstList = (struct _LIST *)pList;
	pstList->list_destroy(pstList);
	free(pList);
}
unsigned int __list_count (void * pList) {
	struct _LIST * pstList = (struct _LIST *)pList;
	return pstList->list_count(pstList);
}
void * __list_get (void * pList, unsigned int index, size_t * pSize) {
	struct _LIST * pstList = (struct _LIST *)pList;
	size_t sizeTemp;
	return pstList->list_get(pstList, index, pSize ? pSize : &sizeTemp);
}
void * __list_insert (void * pList, unsigned int index, size_t size, const void * pEntry) {
	struct _LIST * pstList = (struct _LIST *)pList;
	return pstList->list_insert(pstList, index, size, pEntry);
}
void __list_remove (void * pList, unsigned int index) {
	struct _LIST * pstList = (struct _LIST *)pList;
	pstList->list_remove(pstList, index);
}
void * __list_update (void * pList, unsigned int index, size_t size, const void * pEntry) {
	struct _LIST * pstList = (struct _LIST *)pList;
	return pstList->list_update(pstList, index, size, pEntry);
}
void __list_start_iterator (void * pList, unsigned int startIndex) {
	struct _LIST * pstList = (struct _LIST *)pList;
	pstList->start_iterator(pstList, startIndex);
}
void * __list_next (void * pList, size_t *pSize, unsigned int *pIndex) {
	struct _LIST * pstList = (struct _LIST *)pList;
	size_t size;
	unsigned int index;
	return pstList->next(pstList, pSize ? pSize : &size, pIndex ? pIndex : &index);
}
bool __list_has_next (void * pList) {
	struct _LIST * pstList = (struct _LIST *)pList;
	return pstList->has_next(pstList);
}
void __list_traverse (void * pList, struct _TRAVERSER * pstTraverser) {
	for (__list_start_iterator(pList, 0); __list_has_next(pList); ) {
		pstTraverser->traverse(pstTraverser->pTraverser, __list_next(pList, NULL, NULL));
	}
}
