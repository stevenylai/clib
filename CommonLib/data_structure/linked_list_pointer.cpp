#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
static void * linked_list_default_allocate(void * pAllocator, size_t size) {
	return malloc(size);
}
static void * linked_list_default_reallocate(void * pAllocator, void * pOld, size_t size) {
	return realloc(pOld, size);
}
static void linked_list_default_free(void * pAllocator, void * pData) {
	free(pData);
}
static struct _ALLOCATOR g_stDefaultAllocator = {NULL, 1, 2, 1, linked_list_default_allocate, linked_list_default_reallocate, linked_list_default_free};
static void * linked_list_create (struct _LIST *pstList, struct _ERROR_HANDLER * pstError, struct _ALLOCATOR * pstAllocator) {
	struct _LINKED_LIST_HEAD * pstInfo = NULL;
	pstList->pstError = pstError;
	pstList->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;
	pstInfo = (struct _LINKED_LIST_HEAD *)(pstList->pList = pstList->pstAllocator->allocate(pstList->pstAllocator->pAllocator, sizeof(struct _LINKED_LIST_HEAD)));
	if (pstInfo) {
		pstInfo->pstFirst = NULL, pstInfo->count = 0;
		pstInfo->pstPrevious = pstInfo->pstCurrent = NULL, pstInfo->iteratorIndex = 0;
		return pstList;
	} else {
		pstList->pstError->handle_error(pstList->pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory to create list");
		return NULL;
	}
}
static void linked_list_destroy (struct _LIST * pstList) {
	struct _LINKED_LIST_HEAD * pstInfo = (struct _LINKED_LIST_HEAD *)(pstList->pList);
	if (pstInfo) {
		struct _LINKED_LIST_NODE * pstNext = pstInfo->pstFirst;
		while (pstNext) {
			struct _LINKED_LIST_NODE * pstFree = pstNext;
			pstNext = pstNext->pstNext;
			pstList->pstAllocator->memfree(pstList->pstAllocator->pAllocator, pstFree);
		}
		pstList->pstAllocator->memfree(pstList->pstAllocator->pAllocator, pstInfo);
	}
}
static unsigned int linked_list_count (struct _LIST * pstList) {
	return ((struct _LINKED_LIST_HEAD *)(pstList->pList))->count;
}
static struct _LINKED_LIST_NODE * linked_list_get_node (struct _LIST * pstList, unsigned int index) {
	struct _LINKED_LIST_HEAD * pstListInfo = (struct _LINKED_LIST_HEAD *)pstList->pList;
	struct _LINKED_LIST_NODE * pstCurrent = pstListInfo->pstFirst;
	for (unsigned int i = 0; i < index; i++) {
		pstCurrent = pstCurrent->pstNext;
	}
	return pstCurrent;
}
static void * linked_list_get (struct _LIST * pstList, unsigned int index, size_t * pSize) {
	if (index >= linked_list_count(pstList)) return NULL;
	else {
		struct _LINKED_LIST_NODE * pstNode = linked_list_get_node (pstList, index);
		*pSize = pstNode->size;
		return (char *)pstNode + sizeof(struct _LINKED_LIST_NODE);
	}
}
static void * linked_list_insert (struct _LIST * pstList, unsigned int index, size_t size, const void * pEntry) {
	if (index > linked_list_count(pstList)) {
		return NULL;
	}
	size_t capacity = (sizeof(struct _LINKED_LIST_NODE) + size / pstList->pstAllocator->reallocSize + 1) * pstList->pstAllocator->reallocSize;
	struct _LINKED_LIST_NODE * pstNewNode = (struct _LINKED_LIST_NODE *)pstList->pstAllocator->allocate(pstList->pstAllocator->pAllocator, capacity);
	if (!pstNewNode) {
		pstList->pstError->handle_error(pstList->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory to create a new node in the list");
		return pstNewNode;
	}
	pstNewNode->size = size, pstNewNode->capacity = capacity, memcpy((char *)pstNewNode + sizeof(struct _LINKED_LIST_NODE), pEntry, size);
	pstNewNode->pstNext = NULL;
	if (index) {
		struct _LINKED_LIST_NODE * pstPrevNode = linked_list_get_node(pstList, index - 1);
		pstNewNode->pstNext = pstPrevNode->pstNext, pstPrevNode->pstNext = pstNewNode;
	} else {
		pstNewNode->pstNext = ((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst;
		((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst = pstNewNode;
	}
	((struct _LINKED_LIST_HEAD *)(pstList->pList))->count++;
	return (char *)pstNewNode + sizeof(struct _LINKED_LIST_NODE);
}
static void linked_list_remove (struct _LIST * pstList, unsigned int index) {
	if (index + 1 > linked_list_count(pstList)) {
		return;
	}
	if (index) {
		struct _LINKED_LIST_NODE * pstPrevNode = linked_list_get_node(pstList, index - 1);
		struct _LINKED_LIST_NODE * pstCurrentNode = pstPrevNode->pstNext;
		pstPrevNode->pstNext = pstCurrentNode->pstNext;
		pstList->pstAllocator->memfree(pstList->pstAllocator->pAllocator, pstCurrentNode);
	} else {
		struct _LINKED_LIST_NODE * pstCurrentNode = ((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst;
		((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst = pstCurrentNode->pstNext;
		pstList->pstAllocator->memfree(pstList->pstAllocator->pAllocator, pstCurrentNode);
	}
	((struct _LINKED_LIST_HEAD *)(pstList->pList))->count--;
}
static void linked_list_copy_payload (struct _LINKED_LIST_NODE * pstNode, size_t size, const void * pEntry) {
	memcpy((char *)pstNode + sizeof(struct _LINKED_LIST_NODE), pEntry, size);
	pstNode->size = size;
}
static void * linked_list_update (struct _LIST * pstList, unsigned int index, size_t size, const void * pEntry) {
	if (index >= linked_list_count(pstList)) {
		return NULL;
	}
	size_t capacity = (sizeof(struct _LINKED_LIST_NODE) + size / pstList->pstAllocator->reallocSize + 1) * pstList->pstAllocator->reallocSize;
	if (index) {
		struct _LINKED_LIST_NODE * pstPrevNode = linked_list_get_node(pstList, index - 1);
		struct _LINKED_LIST_NODE * pstCurrentNode = pstPrevNode->pstNext;
		if (pstCurrentNode->capacity >= sizeof(struct _LINKED_LIST_NODE) + size) { // Still enough capacity
			linked_list_copy_payload(pstCurrentNode, size, pEntry);
			return (char *)pstCurrentNode + sizeof(struct _LINKED_LIST_NODE);
		} else { // Reallocate
			struct _LINKED_LIST_NODE * pstNewNode = (struct _LINKED_LIST_NODE *)pstList->pstAllocator->reallocate (pstList->pstAllocator->pAllocator, pstCurrentNode, capacity);
			if (!pstNewNode) {
				pstList->pstError->handle_error(pstList->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory to expand a node in the list");
				return pstNewNode;
			} else {
				pstNewNode->capacity = capacity;
				linked_list_copy_payload(pstNewNode, size, pEntry);
				pstPrevNode->pstNext = pstNewNode;
				return (char *)pstNewNode + sizeof(struct _LINKED_LIST_NODE);
			}
		}
	} else {
		struct _LINKED_LIST_NODE * pstCurrentNode = ((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst;
		if (pstCurrentNode->capacity >= sizeof(struct _LINKED_LIST_NODE) + size) {
			linked_list_copy_payload(pstCurrentNode, size, pEntry);
			return (char *)pstCurrentNode + sizeof(struct _LINKED_LIST_NODE);
		} else {
			struct _LINKED_LIST_NODE * pstNewNode = (struct _LINKED_LIST_NODE *)pstList->pstAllocator->reallocate (pstList->pstAllocator->pAllocator, pstCurrentNode, capacity);
			if (!pstNewNode) {
				pstList->pstError->handle_error(pstList->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory to expand a node in the list");
				return pstNewNode;
			} else {
				pstNewNode->capacity = capacity;
				linked_list_copy_payload(pstNewNode, size, pEntry);
				((struct _LINKED_LIST_HEAD *)pstList->pList)->pstFirst = pstNewNode;
				return (char *)pstNewNode + sizeof(struct _LINKED_LIST_NODE);
			}
		}
	}
}
static void linked_list_start_iterator (struct _LIST * pstList, unsigned int startIndex) {
	struct _LINKED_LIST_HEAD * pstListInfo = (struct _LINKED_LIST_HEAD *)pstList->pList;
	pstListInfo->pstPrevious = NULL;
	pstListInfo->pstCurrent = pstListInfo->pstFirst;
	for (pstListInfo->iteratorIndex = 0; pstListInfo->iteratorIndex < linked_list_count(pstList) && pstListInfo->iteratorIndex < startIndex; pstListInfo->iteratorIndex++) {
		pstListInfo->pstPrevious = pstListInfo->pstCurrent;
		pstListInfo->pstCurrent = pstListInfo->pstCurrent->pstNext;
	}
}
static void * linked_list_next (struct _LIST * pstList, size_t * pSize, unsigned int * pIndex) {
	struct _LINKED_LIST_HEAD * pstListInfo = (struct _LINKED_LIST_HEAD *)pstList->pList;
	if (pstListInfo->pstCurrent) {
		void * pPayload = (char *)pstListInfo->pstCurrent + sizeof(struct _LINKED_LIST_NODE);
		*pIndex = pstListInfo->iteratorIndex, *pSize = pstListInfo->pstCurrent->size;

		pstListInfo->pstPrevious = pstListInfo->pstCurrent;
		pstListInfo->pstCurrent = pstListInfo->pstCurrent->pstNext;
		pstListInfo->iteratorIndex++;
		return pPayload;
	} else {
		return NULL;
	}
}
static bool linked_list_has_next (struct _LIST * pstList) {
	struct _LINKED_LIST_HEAD * pstListInfo = (struct _LINKED_LIST_HEAD *)pstList->pList;
	if (pstListInfo->pstCurrent) {
		return true;
	} return false;
}
void __linked_list_pointer_set_func_ptr (struct _LIST * pstList) {
	// List essential:
	pstList->list_create = linked_list_create;
	pstList->list_destroy = linked_list_destroy;
	pstList->list_get = linked_list_get;
	pstList->list_count = linked_list_count;
	pstList->list_insert = linked_list_insert;
	pstList->list_update = linked_list_update;
	pstList->list_remove = linked_list_remove;
	// Iterator:
	pstList->start_iterator = linked_list_start_iterator;
	pstList->next = linked_list_next;
	pstList->has_next = linked_list_has_next;
}