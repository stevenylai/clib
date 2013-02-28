#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
static void * pointer_default_allocate(void * pAllocator, size_t size) {
	return malloc(size);
}
static void * pointer_default_reallocate(void * pAllocator, void * pOld, size_t size) {
	return realloc(pOld, size);
}
static void pointer_default_free(void * pAllocator, void * pData) {
	free(pData);
}
static struct _ALLOCATOR g_stDefaultAllocator = {NULL, 1, 2, 1, pointer_default_allocate, pointer_default_reallocate, pointer_default_free};
static void * pointer_node_payload (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, size_t * pSize) {
	struct _BINARY_TREE_POINTER_NODE * pstNode = (struct _BINARY_TREE_POINTER_NODE *)pNode;
	if (pSize)
		*pSize = pstNode->size;
	return (char *)pNode + sizeof(struct _BINARY_TREE_POINTER_NODE);
}
static void pointer_node_remove (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode) {
	free(pNode);
	if (pstStorage->pStorage == pNode)
		pstStorage->pStorage = NULL;
}
static void * pointer_node_update (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, void * pPayload, size_t size) {
	struct _BINARY_TREE_POINTER_NODE * pstNode = (struct _BINARY_TREE_POINTER_NODE *)pNode;
	size_t minimum = size + sizeof(struct _BINARY_TREE_POINTER_NODE);
	if (minimum > pstNode->capacity) { // Need to expand the node
		size_t capacity = (minimum / pstStorage->pstAllocator->reallocSize + (minimum % pstStorage->pstAllocator->reallocSize ? 1 : 0)) * pstStorage->pstAllocator->reallocSize;
		struct _BINARY_TREE_POINTER_NODE * pstExpandedNode = (struct _BINARY_TREE_POINTER_NODE *)pstStorage->pstAllocator->reallocate(pstStorage->pstAllocator->pAllocator, pNode, capacity);
		if (!pstExpandedNode) {
			pstStorage->pstError->handle_error(pstStorage->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory to expand the tree node");
			return pstExpandedNode;
		}
		pstExpandedNode->capacity = capacity;
		if (pstExpandedNode->pstParent && pstExpandedNode->pstParent->pstLeft == pstNode) { // Notify the parent
			pstExpandedNode->pstParent->pstLeft = pstExpandedNode;
		} else if (pstExpandedNode->pstParent) {
			pstExpandedNode->pstParent->pstRight = pstExpandedNode;
		} else {
			pstStorage->pStorage = pstExpandedNode;
		} // Notify the children
		if (pstExpandedNode->pstLeft)
			pstExpandedNode->pstLeft->pstParent = pstExpandedNode;
		if (pstExpandedNode->pstRight)
			pstExpandedNode->pstRight->pstParent = pstExpandedNode;
		pstNode = pstExpandedNode;
	}
	memcpy((char *)pstNode + sizeof(struct _BINARY_TREE_POINTER_NODE), pPayload, pstNode->size = size);
	return pstNode;
}
static void pointer_node_swap(struct _BINARY_TREE_STORAGE * pstStorage, void ** pNode1, void ** pNode2) {
	struct _BINARY_TREE_POINTER_NODE *pstNode1 = (struct _BINARY_TREE_POINTER_NODE *)*pNode1, *pstNode2 = (struct _BINARY_TREE_POINTER_NODE *)*pNode2, *pstTemp = NULL;
	if (pstNode1->pstLeft && pstNode1->pstLeft != pstNode2) pstNode1->pstLeft->pstParent = pstNode2; // Fix external pointers for node1
	if (pstNode1->pstRight && pstNode1->pstRight != pstNode2) pstNode1->pstRight->pstParent = pstNode2;
	if (pstNode1->pstParent && pstNode1->pstParent != pstNode2) {
		if (pstNode1->pstParent->pstLeft == pstNode1)
			pstNode1->pstParent->pstLeft = pstNode2;
		else if (pstNode1->pstParent->pstRight == pstNode1)
			pstNode1->pstParent->pstRight = pstNode2;
	} else if (!pstNode1->pstParent) {
		pstStorage->pStorage = pstNode2;
	}
	if (pstNode2->pstLeft && pstNode2->pstLeft != pstNode1) pstNode2->pstLeft->pstParent = pstNode1; // Fix external pointers for node2
	if (pstNode2->pstRight && pstNode2->pstRight != pstNode1) pstNode2->pstRight->pstParent = pstNode1;
	if (pstNode2->pstParent && pstNode2->pstParent != pstNode1) {
		if (pstNode2->pstParent->pstLeft == pstNode2)
			pstNode2->pstParent->pstLeft = pstNode1;
		else if (pstNode2->pstParent->pstRight == pstNode2)
			pstNode2->pstParent->pstRight = pstNode1;
	} else if (!pstNode2->pstParent) {
		pstStorage->pStorage = pstNode1;
	}
	enum _BINARY_TREE_NODE_RELATION enRelation = BINARY_TREE_PARENT; // Swap
	if (pstNode1->pstParent == pstNode2) {
		if (pstNode2->pstLeft == pstNode1) enRelation = BINARY_TREE_LEFT;
		else enRelation = BINARY_TREE_RIGHT;

		pstNode1->pstParent = pstNode2->pstParent, pstNode2->pstParent = pstNode1;
		if (enRelation == BINARY_TREE_LEFT) {
			pstNode2->pstLeft = pstNode1->pstLeft, pstNode1->pstLeft = pstNode2;
			pstTemp = pstNode2->pstRight, pstNode2->pstRight = pstNode1->pstRight, pstNode1->pstRight = pstTemp;
		} else {
			pstNode2->pstRight = pstNode1->pstRight, pstNode1->pstRight = pstNode2;
			pstTemp = pstNode2->pstLeft, pstNode2->pstLeft = pstNode1->pstLeft, pstNode1->pstLeft = pstTemp;
		}
	} else if (pstNode2->pstParent == pstNode1) {
		if (pstNode1->pstLeft == pstNode2) enRelation = BINARY_TREE_LEFT;
		else enRelation = BINARY_TREE_RIGHT;
		
		pstNode2->pstParent = pstNode1->pstParent, pstNode1->pstParent = pstNode2;
		if (enRelation == BINARY_TREE_LEFT) {
			pstNode1->pstLeft = pstNode2->pstLeft, pstNode2->pstLeft = pstNode1;
			pstTemp = pstNode2->pstRight, pstNode2->pstRight = pstNode1->pstRight, pstNode1->pstRight = pstTemp;
		} else {
			pstNode1->pstRight = pstNode2->pstRight, pstNode2->pstRight = pstNode1;
			pstTemp = pstNode2->pstLeft, pstNode2->pstLeft = pstNode1->pstLeft, pstNode1->pstLeft = pstTemp;
		}
	} else {
		pstTemp = pstNode2->pstParent, pstNode2->pstParent = pstNode1->pstParent, pstNode1->pstParent = pstTemp;
		pstTemp = pstNode2->pstRight, pstNode2->pstRight = pstNode1->pstRight, pstNode1->pstRight = pstTemp;
		pstTemp = pstNode2->pstLeft, pstNode2->pstLeft = pstNode1->pstLeft, pstNode1->pstLeft = pstTemp;
	}
	*pNode1 = pstNode2, *pNode2 = pstNode1;
}
static void * pointer_node_create (struct _BINARY_TREE_STORAGE * pstStorage, void * pPayload, size_t size) {
	size_t minimum = size + sizeof(struct _BINARY_TREE_POINTER_NODE);
	size_t capacity = (minimum / pstStorage->pstAllocator->reallocSize + (minimum % pstStorage->pstAllocator->reallocSize ? 1 : 0)) * pstStorage->pstAllocator->reallocSize;
	struct _BINARY_TREE_POINTER_NODE * pstNode = (struct _BINARY_TREE_POINTER_NODE *)pstStorage->pstAllocator->allocate(pstStorage->pstAllocator->pAllocator, capacity);
	if (!pstNode) {
		pstStorage->pstError->handle_error(pstStorage->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory to create binary tree node");
		return pstNode;
	}
	pstNode->capacity = capacity, pstNode->pstParent = pstNode->pstLeft = pstNode->pstRight = NULL;
	memcpy((char *)pstNode + sizeof(struct _BINARY_TREE_POINTER_NODE), pPayload, pstNode->size = size);
	return pstNode;
}
static void * pointer_node_get (struct _BINARY_TREE_STORAGE * pstStorage, void * pNode, enum _BINARY_TREE_NODE_RELATION enRelation) {
	struct _BINARY_TREE_POINTER_NODE * pstNode = (struct _BINARY_TREE_POINTER_NODE *)pNode;
	if (!pstNode) return pstStorage->pStorage; // root
	switch (enRelation) {
		case BINARY_TREE_PARENT:
			return pstNode->pstParent;
		case BINARY_TREE_LEFT:
			return pstNode->pstLeft;
		case BINARY_TREE_RIGHT:
			return pstNode->pstRight;
		default:
			return NULL;
	}
}
static void pointer_node_set (struct _BINARY_TREE_STORAGE * pstStorage, void * pNodeSet, enum _BINARY_TREE_NODE_RELATION enRelation, const void * pNodeGet) {
	struct _BINARY_TREE_POINTER_NODE * pstNodeSet = (struct _BINARY_TREE_POINTER_NODE *)pNodeSet;
	struct _BINARY_TREE_POINTER_NODE * pstNodeGet = (struct _BINARY_TREE_POINTER_NODE *)pNodeGet;
	if (!pNodeSet) {
		pstStorage->pStorage = pstNodeGet;
		return;
	}
	switch (enRelation) {
		case BINARY_TREE_PARENT:
			pstNodeSet->pstParent = pstNodeGet;
			break;
		case BINARY_TREE_LEFT:
			pstNodeSet->pstLeft = pstNodeGet;
			break;
		case BINARY_TREE_RIGHT:
			pstNodeSet->pstRight = pstNodeGet;
			break;
	}
}
static void * pointer_storage_create (struct _BINARY_TREE_STORAGE * pstStorage, struct _ERROR_HANDLER * pstError, struct _ALLOCATOR * pstAllocator) {
	pstStorage->pstError = pstError;
	pstStorage->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;
	pstStorage->pStorage = NULL;
	return pstStorage;
}
static void pointer_storage_destroy_subtree (struct _BINARY_TREE_STORAGE * pstStorage, struct _BINARY_TREE_POINTER_NODE * pstNode) {
	if (pstNode) {
		if (pstNode->pstLeft)
			pointer_storage_destroy_subtree(pstStorage, pstNode->pstLeft), pstNode->pstLeft = NULL;
		if (pstNode->pstRight)
			pointer_storage_destroy_subtree(pstStorage, pstNode->pstRight), pstNode->pstRight = NULL;
		pointer_node_remove(pstStorage, pstNode);
	}
}
static void pointer_storage_destroy (struct _BINARY_TREE_STORAGE * pstStorage) {
	pointer_storage_destroy_subtree(pstStorage, (struct _BINARY_TREE_POINTER_NODE *)pstStorage->pStorage);
}
void __binary_tree_pointer_set_func_ptr (struct _BINARY_TREE_STORAGE * pstStorage) {
	pstStorage->storage_create = pointer_storage_create;
	pstStorage->storage_destroy = pointer_storage_destroy;
	pstStorage->node_remove = pointer_node_remove;
	pstStorage->node_update = pointer_node_update;
	pstStorage->node_swap_payload = pointer_node_swap;
	pstStorage->node_create = pointer_node_create;
	pstStorage->node_get = pointer_node_get;
	pstStorage->node_set = pointer_node_set;
	pstStorage->node_payload = pointer_node_payload;
}