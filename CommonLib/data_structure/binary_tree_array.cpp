#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
#define INVALID_NODE_REF 0xFFFFFFFF
static void * array_default_allocate(void * pAllocator, size_t size) {
	return malloc(size);
}
static void * array_default_reallocate(void * pAllocator, void * pOld, size_t size) {
	return realloc(pOld, size);
}
static void array_default_free(void * pAllocator, void * pData) {
	free(pData);
}
static struct _ALLOCATOR g_stDefaultAllocator = {NULL, ARRAY_REALLOC_DEFAULT_SIZE, 2, 1, array_default_allocate, array_default_reallocate, array_default_free};
static void * array_storage_create (struct _BINARY_TREE_STORAGE * pstStorage, struct _ERROR_HANDLER * pstError, struct _ALLOCATOR * pstAllocator) {
	pstStorage->pstError = pstError;
	pstStorage->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;
	size_t initSize = pstStorage->pstAllocator->reallocSize;
	while (initSize < sizeof(struct _BINARY_TREE_ARRAY_HEAD))
		initSize = (initSize * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor <= initSize ? initSize * 2 : initSize * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor);
	if (!(pstStorage->pStorage = pstStorage->pstAllocator->allocate(pstStorage->pstAllocator->pAllocator, initSize))) {
		pstStorage->pstError->handle_error(pstStorage->pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory to create binary tree");
		return pstStorage->pStorage;
	}
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	pstHead->totalCount = 0, pstHead->capacity = initSize, pstHead->size = sizeof(struct _BINARY_TREE_ARRAY_HEAD), pstHead->root = INVALID_NODE_REF;
	return pstStorage;
}
static void array_storage_destroy (struct _BINARY_TREE_STORAGE * pstStorage) {
	pstStorage->pstAllocator->memfree(pstStorage->pstAllocator->pAllocator, pstStorage->pStorage), pstStorage->pStorage = NULL;
}
static void * array_node_payload (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, size_t * pSize) {
	struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)pNode;
	if (pSize) *pSize = pstNode->size;
	return (char *)pNode + sizeof(struct _BINARY_TREE_ARRAY_NODE);
}
static void * array_storage_expanded (struct _BINARY_TREE_STORAGE * pstStorage, size_t expandedSize) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	size_t reallocSize = (pstHead->capacity ? (pstHead->capacity * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor <= pstHead->capacity ? pstHead->capacity * 2 : pstHead->capacity * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor) : pstStorage->pstAllocator->reallocSize);
	while (reallocSize < expandedSize) reallocSize = (reallocSize * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor <= reallocSize ? reallocSize * 2 : reallocSize * pstStorage->pstAllocator->expansionSize / pstStorage->pstAllocator->expansionFactor);
	void * pNewSpace = pstStorage->pstAllocator->reallocate(pstStorage->pstAllocator->pAllocator, pstStorage->pStorage, reallocSize);
	if (!pNewSpace) {
		pstStorage->pstError->handle_error(pstStorage->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory to expand the binary tree");
		return pNewSpace;
	} else {
		((struct _BINARY_TREE_ARRAY_HEAD *)(pstStorage->pStorage = pNewSpace))->capacity = reallocSize;
		return pstStorage->pStorage;
	}
}
static void * array_node_create (struct _BINARY_TREE_STORAGE * pstStorage, void * pPayload, size_t size) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	if (pstHead->size + size + sizeof(struct _BINARY_TREE_ARRAY_NODE) > pstHead->capacity) {
		if (!array_storage_expanded (pstStorage, pstHead->size + size + sizeof(struct _BINARY_TREE_ARRAY_NODE))) return NULL;
		pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	}
	struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstStorage->pStorage + pstHead->size);
	pstNode->left = pstNode->right = pstNode->parent = INVALID_NODE_REF, pstNode->size = size;
	pstHead->totalCount++, pstHead->size += (size + sizeof(struct _BINARY_TREE_ARRAY_NODE));
	memcpy(array_node_payload(pstStorage, pstNode, NULL), pPayload, size);
	return pstNode;
}
static size_t array_get_ref (struct _BINARY_TREE_STORAGE * pstStorage, struct _BINARY_TREE_ARRAY_NODE * pstNode) {
	if (pstNode)
		return (char *)pstNode - (char *)pstStorage->pStorage;
	else
		return INVALID_NODE_REF;
}
static void array_update_ref (struct _BINARY_TREE_STORAGE * pstStorage, size_t startRef, size_t endRef, struct _BINARY_TREE_ARRAY_NODE * pstNode, size_t offset, bool bForward) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	size_t curRef = array_get_ref(pstStorage, pstNode);
	if (pstNode->left != INVALID_NODE_REF) {
		((struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + pstNode->left))->parent = (bForward ? curRef + offset : curRef - offset);
		if (pstNode->left > startRef && pstNode->left < endRef) pstNode->left = (bForward ? pstNode->left + offset : pstNode->left - offset);
	}
	if (pstNode->right != INVALID_NODE_REF) {
		((struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + pstNode->right))->parent = (bForward ? curRef+ offset : curRef - offset);
		if (pstNode->right > startRef && pstNode->right < endRef) pstNode->right = (bForward ? pstNode->right + offset : pstNode->right - offset);
	}
	if (pstNode->parent != INVALID_NODE_REF && (pstNode->parent < (bForward ? startRef : startRef - offset) || pstNode->parent > (bForward ? endRef + offset: endRef))) {
		struct _BINARY_TREE_ARRAY_NODE * pstParent = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + pstNode->parent);
		if (pstParent->left == curRef)
			pstParent->left = (bForward ? curRef + offset : curRef - offset);
		else if (pstParent->right == curRef)
			pstParent->right = (bForward ? curRef + offset : curRef - offset);
	}
}
static void array_shift_storage (struct _BINARY_TREE_STORAGE * pstStorage, size_t startRef, size_t offset, bool bForward) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	size_t nodeRef = startRef;
	while (nodeRef < pstHead->size) {
		struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + nodeRef);
		array_update_ref(pstStorage, startRef, pstHead->size, pstNode, offset, bForward);
		nodeRef += pstNode->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
	}
	if (bForward) {
		for (size_t index = pstHead->size - 1; index >= startRef; index--) {
			memcpy((char *)pstHead + index + offset, (char *)pstHead + index, sizeof(char));
		}
	} else {
		for (size_t index = startRef; index < pstHead->size; index++) {
			memcpy((char *)pstHead + index - offset, (char *)pstHead + index, sizeof(char));
		}
	}
}
static void array_node_swap(struct _BINARY_TREE_STORAGE * pstStorage, void ** pNode1, void ** pNode2) {
	struct _BINARY_TREE_ARRAY_HEAD *pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	struct _BINARY_TREE_ARRAY_NODE *pstNode1 = (struct _BINARY_TREE_ARRAY_NODE *)*pNode1, *pstNode2 = (struct _BINARY_TREE_ARRAY_NODE *)*pNode2;
	if (array_get_ref(pstStorage, pstNode1) == array_get_ref(pstStorage, pstNode2)) return;
	//printf("To swap node1: %d, node2: %d\n", *(unsigned int*)((char *)pstNode1 + sizeof(struct _BINARY_TREE_ARRAY_NODE)), *(unsigned int*)((char *)pstNode2 + sizeof(struct _BINARY_TREE_ARRAY_NODE)));

	bool bForward = false; // Calculate the parameters for 'array_update_ref'
	size_t startRef = 0, endRef = 0, offset = pstNode1->size > pstNode2->size ? pstNode1->size - pstNode2->size : pstNode2->size - pstNode1->size;

	if (array_get_ref(pstStorage, pstNode1) < array_get_ref(pstStorage, pstNode2)) {
		startRef = array_get_ref(pstStorage, pstNode1) + pstNode1->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
		endRef = array_get_ref(pstStorage, pstNode2) + pstNode2->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
		bForward = (pstNode1->size > pstNode2->size ? false : true);
	} else {
		startRef = array_get_ref(pstStorage, pstNode2) + pstNode2->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
		endRef = array_get_ref(pstStorage, pstNode1) + pstNode1->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
		bForward = (pstNode2->size > pstNode1->size ? false : true);
	}
	size_t node1Remain = pstNode1->size, node2Remain = pstNode2->size;
	
	size_t nodeRef = startRef; // Update the references
	while (nodeRef < endRef) {
		struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + nodeRef);
		//printf("Update ref for node: %d, direction %d\n", *(unsigned int *)array_node_payload(pstStorage, pstNode, NULL), bForward);
		array_update_ref(pstStorage, startRef, endRef, pstNode, offset, bForward);
		nodeRef += pstNode->size + sizeof(struct _BINARY_TREE_ARRAY_NODE);
	}
	size_t temp; // Copy the common sized data
	void * payload1 = (char *)array_node_payload(pstStorage, pstNode1, NULL) + (pstNode1->size - node1Remain), * payload2 = (char *)array_node_payload(pstStorage, pstNode2, NULL) + (pstNode2->size - node2Remain);
	while (node1Remain && node2Remain) {
		payload1 = (char *)array_node_payload(pstStorage, pstNode1, NULL) + (pstNode1->size - node1Remain);
		payload2 = (char *)array_node_payload(pstStorage, pstNode2, NULL) + (pstNode2->size - node2Remain);
		memcpy(&temp, payload1, sizeof(char));
		memcpy(payload1, payload2, sizeof(char));
		memcpy(payload2, &temp, sizeof(char));
		node1Remain--, node2Remain--;
	}
	char * pSrc = NULL, * pDest = NULL; // Copy the remaining data
	if (node1Remain) {
		if (bForward) { // [___payload2_____payload1___]
			pSrc = (char *)payload1, pDest = (char *)payload2;
		} else { // [___payload1_____payload2___]
			pSrc = (char *)payload1 + node1Remain, pDest = (char *)payload2 + node2Remain;
		}
	}
	while (node1Remain) {
		if (bForward) { // [___dest_____src___]
			memcpy(&temp, pSrc, sizeof(char));
			for (char * pCopy = pSrc; pCopy - 1 >= pDest; pCopy--) {
				memcpy(pCopy, pCopy - 1, sizeof(char));
			}
			memcpy(pDest, &temp, sizeof(char));
			pSrc++, pDest++;
		} else { // [___src_____dest___]
			memcpy(&temp, pSrc, sizeof(char));
			for (char * pCopy = pSrc; pCopy + 1 <= pDest; pCopy++) {
				memcpy(pCopy, pCopy + 1, sizeof(char));
			}
			memcpy(pDest, &temp, sizeof(char));
			pSrc--, pDest--;
		}
		node1Remain--;
	}
	if (node2Remain) {
		if (bForward) { // [___payload1_____payload2___]
			pSrc = (char *)payload2, pDest = (char *)payload1;
		} else { // [___payload2_____payload1___]
			pSrc = (char *)payload2 + node2Remain, pDest = (char *)payload1 + node1Remain;
		}
	}
	while (node2Remain) {
		if (bForward) { // [___dest_____src___]
			memcpy(&temp, pSrc, sizeof(char));
			for (char * pCopy = pSrc; pCopy - 1 >= pDest; pCopy--) {
				memcpy(pCopy, pCopy - 1, sizeof(char));
			}
			memcpy(pDest, &temp, sizeof(char));
			pSrc++, pDest++;
		} else { // [___src_____dest___]
			memcpy(&temp, pSrc, sizeof(char));
			for (char * pCopy = pSrc; pCopy + 1 <= pDest; pCopy++) {
				memcpy(pCopy, pCopy + 1, sizeof(char));
			}
			memcpy(pDest, &temp, sizeof(char));
			pSrc--, pDest--;
		}
		node2Remain--;
	}
	if (array_get_ref(pstStorage, pstNode1) < array_get_ref(pstStorage, pstNode2)) {
		pstNode2 = (struct _BINARY_TREE_ARRAY_NODE *)(bForward ? (char *)pstNode2 + offset : (char *)pstNode2 - offset);
	} else {
		pstNode1 = (struct _BINARY_TREE_ARRAY_NODE *)(bForward ? (char *)pstNode1 + offset : (char *)pstNode1 - offset);
	}
	temp = pstNode2->size;
	pstNode2->size = pstNode1->size;
	pstNode1->size = temp;
	*pNode1 = pstNode1, *pNode2 = pstNode2;
}
static void * array_node_update (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, void * pPayload, size_t size) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	size_t nodeOffset = (char *)pNode - (char *)pstHead;
	struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + nodeOffset);
	if (pstHead->size - pstNode->size + size > pstHead->capacity) {
		if (!array_storage_expanded (pstStorage, pstHead->size - pstNode->size + size))
			return NULL;
		pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
		pstNode = (struct _BINARY_TREE_ARRAY_NODE *)((char *)pstHead + nodeOffset);
	}
	if (size > pstNode->size)
		array_shift_storage(pstStorage, array_get_ref(pstStorage, pstNode) + pstNode->size + sizeof(struct _BINARY_TREE_ARRAY_NODE), size - pstNode->size, true);
	else if (size < pstNode->size)
		array_shift_storage(pstStorage, array_get_ref(pstStorage, pstNode) + pstNode->size + sizeof(struct _BINARY_TREE_ARRAY_NODE), pstNode->size - size, false);
	memcpy((char *)pstNode + sizeof(struct _BINARY_TREE_ARRAY_NODE), pPayload, size);
	pstHead->size = pstHead->size - pstNode->size + size, pstNode->size = size;
	return pstNode;
}
static void array_node_remove (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)pNode;
	size_t nodeSize = pstNode->size;
	array_shift_storage(pstStorage, array_get_ref(pstStorage, pstNode) + nodeSize + sizeof(struct _BINARY_TREE_ARRAY_NODE), nodeSize + sizeof(struct _BINARY_TREE_ARRAY_NODE), false);
	pstHead->totalCount--, pstHead->size -= (nodeSize + sizeof(struct _BINARY_TREE_ARRAY_NODE));
}
static void * array_node_get (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, enum _BINARY_TREE_NODE_RELATION enRelation) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	struct _BINARY_TREE_ARRAY_NODE * pstNode = (struct _BINARY_TREE_ARRAY_NODE *)pNode;
	char * pBase = (char *)pstStorage->pStorage;
	if (!pNode) {
		if (pstHead->root == INVALID_NODE_REF) return NULL; // No node at all, not even root
		else return (char *)pstHead + pstHead->root; // Root
	}
	switch (enRelation) {
		case BINARY_TREE_PARENT:
			return pstNode->parent == INVALID_NODE_REF ? NULL : (char *)pstHead + pstNode->parent;
		case BINARY_TREE_LEFT:
			return pstNode->left == INVALID_NODE_REF ? NULL : (char *)pstHead + pstNode->left;
		case BINARY_TREE_RIGHT:
			return pstNode->right == INVALID_NODE_REF ? NULL : (char *)pstHead + pstNode->right;
		default:
			return NULL;
	}
}
static void array_node_set (struct _BINARY_TREE_STORAGE * pstStorage, void * pNodeSet, enum _BINARY_TREE_NODE_RELATION enRelation, const void * pNodeGet) {
	struct _BINARY_TREE_ARRAY_HEAD * pstHead = (struct _BINARY_TREE_ARRAY_HEAD *)pstStorage->pStorage;
	struct _BINARY_TREE_ARRAY_NODE * pstNodeSet = (struct _BINARY_TREE_ARRAY_NODE *)pNodeSet;
	struct _BINARY_TREE_ARRAY_NODE * pstNodeGet = (struct _BINARY_TREE_ARRAY_NODE *)pNodeGet;
	if (!pNodeSet) {
		pstHead->root = array_get_ref(pstStorage, pstNodeGet);
		return;
	}
	switch (enRelation) {
		case BINARY_TREE_PARENT:
			pstNodeSet->parent = array_get_ref(pstStorage, pstNodeGet);
			break;
		case BINARY_TREE_LEFT:
			pstNodeSet->left = array_get_ref(pstStorage, pstNodeGet);
			break;
		case BINARY_TREE_RIGHT:
			pstNodeSet->right = array_get_ref(pstStorage, pstNodeGet);
			break;
	}
}
void __binary_tree_array_set_func_ptr (struct _BINARY_TREE_STORAGE * pstStorage) {
	pstStorage->storage_create = array_storage_create;
	pstStorage->storage_destroy = array_storage_destroy;
	pstStorage->node_remove = array_node_remove;
	pstStorage->node_update = array_node_update;
	pstStorage->node_swap_payload = array_node_swap;
	pstStorage->node_create = array_node_create;
	pstStorage->node_get = array_node_get;
	pstStorage->node_set = array_node_set;
	pstStorage->node_payload = array_node_payload;
}