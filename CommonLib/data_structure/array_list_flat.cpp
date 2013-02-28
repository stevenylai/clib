#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
static void * array_list_default_allocate(void * pAllocator, size_t size) {
	return malloc(size);
}
static void * array_list_default_reallocate(void * pAllocator, void * pOld, size_t size) {
	return realloc(pOld, size);
}
static void array_list_default_free(void * pAllocator, void * pData) {
	free(pData);
}
static struct _ALLOCATOR g_stDefaultAllocator = {NULL, ARRAY_REALLOC_DEFAULT_SIZE, 2, 1, array_list_default_allocate, array_list_default_reallocate, array_list_default_free};
static void * array_list_create (struct _LIST * pstList, struct _ERROR_HANDLER * pstError, struct _ALLOCATOR * pstAllocator) {
	pstList->pList = NULL;
	pstList->pstError = pstError;
	pstList->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;
	return pstList;
}
static void array_list_destroy (struct _LIST * pstList) {
	pstList->pstAllocator->memfree(pstList->pstAllocator->pAllocator, pstList->pList), pstList->pList = NULL;
}
static unsigned int array_list_count (struct _LIST * pstList) {
	if (pstList->pList == NULL) {
		pstList->pList = pstList->pstAllocator->allocate (pstList->pstAllocator->pAllocator, pstList->pstAllocator->reallocSize);
		if (pstList->pList) {
			struct _ARRAY_LIST_HEAD * pHead = (struct _ARRAY_LIST_HEAD *)pstList->pList;
			pHead->iteratorLocation = sizeof(struct _ARRAY_LIST_HEAD), pHead->iteratorIndex = 0;
			pHead->count = 0, pHead->capacity = pstList->pstAllocator->reallocSize, pHead->usedCapacity = sizeof(struct _ARRAY_LIST_HEAD);
		} else {
			pstList->pstError->handle_error(pstList->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory to create list");
		}
	}
	return pstList->pList == NULL ? 0 : ((struct _ARRAY_LIST_HEAD *)pstList->pList)->count;
}
static void * array_list_get_node (struct _LIST * pstList, unsigned int index) {
	struct _ARRAY_LIST_NODE * pCurrent = pstList->pList == NULL ? NULL : (struct _ARRAY_LIST_NODE * )((char *)pstList->pList + sizeof(struct _ARRAY_LIST_HEAD));
	for(unsigned int i = 0; i < array_list_count(pstList); i++) {
		if (i == index) {
			return pCurrent;
		} else if (pCurrent) {
			pCurrent = (struct _ARRAY_LIST_NODE *)((char *)pCurrent + pCurrent->size + sizeof(struct _ARRAY_LIST_NODE));
		} else {
			break;
		}
	}
	return NULL;
}
static void * array_list_get (struct _LIST * pstList, unsigned int index, size_t * pSize) {
	struct _ARRAY_LIST_NODE * pEntry = (struct _ARRAY_LIST_NODE *)array_list_get_node(pstList, index);
	if (pEntry) {
		*pSize = pEntry->size;
		return (char *)pEntry + sizeof(struct _ARRAY_LIST_NODE);
	} else {
		return NULL;
	}
}
static void array_list_shift(struct _LIST * pstList, unsigned int index, size_t shift, bool bForward) {
	char *pcStart = (char *)array_list_get_node(pstList, index), *pcEnd = (char *)pstList->pList + ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity - 1;
	if (pcStart && bForward) {
		for (char * pMove = pcEnd; pMove >= pcStart; pMove--) {
			memcpy(pMove + shift, pMove, sizeof(char));
		}
	} else if (pcStart) {
		for (char * pMove = pcStart; pMove <= pcEnd; pMove++) {
			memcpy(pMove - shift, pMove, sizeof(char));
		}
	}
}
static void * array_list_expanded (struct _LIST * pstList, size_t size) {
	size_t currentMaxSize = ((struct _ARRAY_LIST_HEAD *)pstList->pList)->capacity;
	size_t reallocSize = (currentMaxSize ? (currentMaxSize * pstList->pstAllocator->expansionSize / pstList->pstAllocator->expansionFactor <= currentMaxSize ? currentMaxSize * 2 : currentMaxSize * pstList->pstAllocator->expansionSize / pstList->pstAllocator->expansionFactor) : pstList->pstAllocator->reallocSize);
	while (reallocSize < size) reallocSize = (reallocSize * pstList->pstAllocator->expansionSize / pstList->pstAllocator->expansionFactor <= reallocSize ? reallocSize * 2 : reallocSize * pstList->pstAllocator->expansionSize / pstList->pstAllocator->expansionFactor);
	struct _ARRAY_LIST_HEAD * pNewHead = (struct _ARRAY_LIST_HEAD *)pstList->pstAllocator->reallocate (pstList->pstAllocator->pAllocator, pstList->pList, reallocSize); // (Pre-)allocate with enough space
	if (!pNewHead) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory to expand the list");
		return pNewHead;
	} else {
		pstList->pList = pNewHead, pNewHead->capacity = reallocSize;
		return pstList->pList;
	}
}
static void * array_list_insert (struct _LIST * pstList, unsigned int index, size_t size, const void * pEntry) {
	if (index > array_list_count(pstList) || pstList->pList == NULL) {
		return NULL;
	}
	if (((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity + size + sizeof(struct _ARRAY_LIST_NODE) > ((struct _ARRAY_LIST_HEAD *)pstList->pList)->capacity) {
		if (!array_list_expanded(pstList, ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity + size + sizeof(struct _ARRAY_LIST_NODE))) return NULL;
	}
	void * pInsert = index == 0 ? (char *)pstList->pList + sizeof(struct _ARRAY_LIST_HEAD) : array_list_get_node (pstList, index - 1);
	if (pInsert == NULL) { // Should never reach here
		return NULL;
	}
	if (index < array_list_count(pstList)) { // Need to shift
		array_list_shift(pstList, index, size + sizeof(struct _ARRAY_LIST_NODE), true);
	}
	if (index > 0) {
		pInsert = (char *)pInsert + ((struct _ARRAY_LIST_NODE *)pInsert)->size + sizeof(struct _ARRAY_LIST_NODE);
	}
	((struct _ARRAY_LIST_NODE *)pInsert)->size = size, memcpy((char *)pInsert + sizeof(struct _ARRAY_LIST_NODE), pEntry, size);
	((struct _ARRAY_LIST_HEAD *)pstList->pList)->count++, ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity += (size + sizeof(struct _ARRAY_LIST_NODE));
	return (char *)pInsert + sizeof(struct _ARRAY_LIST_NODE);
}
static void array_list_remove (struct _LIST * pstList, unsigned int index) {
	struct _ARRAY_LIST_NODE * pEntry = (struct _ARRAY_LIST_NODE *)array_list_get_node(pstList, index);
	if (pEntry) {
		size_t size = pEntry->size;
		array_list_shift(pstList, index + 1, pEntry->size + sizeof(struct _ARRAY_LIST_NODE), false);
		((struct _ARRAY_LIST_HEAD *)pstList->pList)->count--, ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity -= (size + sizeof(struct _ARRAY_LIST_NODE));
	}
}
static void * array_list_update (struct _LIST * pstList, unsigned int index, size_t size, const void * pEntry) {
	struct _ARRAY_LIST_NODE * pEntryOld = (struct _ARRAY_LIST_NODE *)array_list_get_node(pstList, index);
	if (pEntryOld == NULL) {
		return NULL;
	}
	if (size > pEntryOld->size && size - pEntryOld->size > ((struct _ARRAY_LIST_HEAD *)pstList->pList)->capacity - ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity) {
		if (!array_list_expanded(pstList, size - pEntryOld->size + ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity)) {
			return NULL;
		}
		if (!(pEntryOld = (struct _ARRAY_LIST_NODE *)array_list_get_node(pstList, index))) return NULL;
	}
	if (pEntryOld->size != size) {
		array_list_shift(pstList, index + 1, pEntryOld->size > size ? pEntryOld->size - size : size - pEntryOld->size, pEntryOld->size > size ? false : true);
	}
	((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity = ((struct _ARRAY_LIST_HEAD *)pstList->pList)->usedCapacity + size - pEntryOld->size;
	return pEntryOld->size = size, memcpy((char *)pEntryOld + sizeof(struct _ARRAY_LIST_NODE), pEntry, size);
}
static void array_list_start_iterator (struct _LIST * pstList, unsigned int startIndex) {
	if (pstList->pList == NULL) return;
	struct _ARRAY_LIST_HEAD * pHead = (struct _ARRAY_LIST_HEAD *)pstList->pList;
	pHead->iteratorLocation = sizeof(struct _ARRAY_LIST_HEAD);
	for(pHead->iteratorIndex = 0; pHead->iteratorIndex < startIndex && pHead->iteratorIndex < array_list_count(pstList); pHead->iteratorIndex++) {
		struct _ARRAY_LIST_NODE * pEntry = (struct _ARRAY_LIST_NODE *)((char *)pstList->pList + pHead->iteratorLocation);
		pHead->iteratorLocation += sizeof(struct _ARRAY_LIST_NODE) + pEntry->size;
	}
}
static void * array_list_next (struct _LIST * pstList, size_t * pSize, unsigned int * pIndex) {
	if (pstList->pList == NULL) return NULL;
	struct _ARRAY_LIST_HEAD * pHead = (struct _ARRAY_LIST_HEAD *)pstList->pList;
	if (pHead->iteratorIndex >= array_list_count(pstList)) return NULL; // Out of range
	struct _ARRAY_LIST_NODE * pEntry = (struct _ARRAY_LIST_NODE *)((char *)pstList->pList + pHead->iteratorLocation);
	
	void * ptrData = (char *)pEntry + sizeof(struct _ARRAY_LIST_NODE); // Payload pointer
	*pSize = pEntry->size, *pIndex = pHead->iteratorIndex;

	pHead->iteratorLocation += sizeof(struct _ARRAY_LIST_NODE) + pEntry->size;
	pHead->iteratorIndex++;
	return ptrData;
}
static bool array_list_has_next (struct _LIST * pstList) {
	if (pstList->pList == NULL) return false;
	struct _ARRAY_LIST_HEAD * pHead = (struct _ARRAY_LIST_HEAD *)pstList->pList;
	if (pHead->iteratorIndex >= array_list_count(pstList)) return false;
	else return true;
}
void __array_list_flat_set_func_ptr (struct _LIST * pstList) {
	// List essential:
	pstList->list_create = array_list_create;
	pstList->list_destroy = array_list_destroy;
	pstList->list_get = array_list_get;
	pstList->list_count = array_list_count;
	pstList->list_insert = array_list_insert;
	pstList->list_update = array_list_update;
	pstList->list_remove = array_list_remove;
	// Iterator:
	pstList->start_iterator = array_list_start_iterator;
	pstList->next = array_list_next;
	pstList->has_next = array_list_has_next;
}