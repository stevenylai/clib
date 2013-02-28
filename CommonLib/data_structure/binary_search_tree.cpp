#include "detect_mem_leak.h"
#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_structure_internal.h"
static bool g_bTraceDepth = 
									#ifdef _BINARY_TREE_DEBUG_TRAVERSE_
									true;
									#else
									false;
									#endif
static void bst_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, bst_default_error};
void * __bst_create (void ** pTree,  enum _DATA_STRUCTURE_TYPE enType, struct _COMPARATOR * pstComparator, struct _ERROR_HANDLER  * pstError, struct _ALLOCATOR * pstAllocator) {
	bool bErr = false;
	if (!pstError) pstError = &g_stDefaultError;
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)(*pTree = malloc(sizeof(struct _BINARY_SEARCH_TREE)));
	if (!pstTree) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory to create binary search tree");
		return *pTree;
	} else {
		memset(pstTree, 0, sizeof(struct _BINARY_SEARCH_TREE));
	}
	switch (pstTree->enType = enType) {
		case BST_NAIVE_POINTER:
			pstTree->stStorage.enType = BINARY_TREE_POINTER;
			__binary_tree_pointer_set_func_ptr (&pstTree->stStorage);
			break;
		case BST_NAIVE_ARRAY:
			pstTree->stStorage.enType = BINARY_TREE_ARRAY;
			__binary_tree_array_set_func_ptr (&pstTree->stStorage);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "This type of binary tree has no implmentation");
			bErr = true;
			goto error;
	}
	if (!pstTree->stStorage.storage_create(&pstTree->stStorage, pstError, pstAllocator)) {
		bErr = true;
		goto error;
	}
	pstTree->pstComparator = pstComparator;
	switch(pstTree->enType) {
		case BST_NAIVE_POINTER:
		case BST_NAIVE_ARRAY:
			__binary_search_tree_naive_set_func_ptr(pstTree);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "This type of binary search tree has no implmentation");
			bErr = true;
			goto error;
	}
	if (!pstTree->tree_create(pstTree)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr) {
		free(pstTree), *pTree = pstTree = NULL;
	}
	return *pTree;
}
void __bst_destroy (void * pTree) {
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)pTree;
	pstTree->stStorage.storage_destroy(&pstTree->stStorage);
	free(pstTree);
}
void * __bst_update (void * pTree, size_t size, void * pData) {
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)pTree;
	return pstTree->tree_update(pstTree, size, pData);
}
void * __bst_retrieve (void * pTree, void * pKey) {
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)pTree;
	return pstTree->tree_retrieve(pstTree, pKey);
}
void __bst_remove (void * pTree, void * pKey) {
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)pTree;
	pstTree->tree_remove(pstTree, pKey);
}
static void bst_traverse_node (struct _BINARY_TREE_STORAGE * pstStorage, struct _TRAVERSER * pstTraverser, enum _BST_TRAVERSE_ORDER enOrder, void * node, unsigned int depth) {
	if (!node) return;
	void * left = pstStorage->node_get(pstStorage, node, BINARY_TREE_LEFT);
	void * right = pstStorage->node_get(pstStorage, node, BINARY_TREE_RIGHT);
	switch (enOrder) {
		case BST_TRAVERSE_IN_ORDER:
			bst_traverse_node(pstStorage, pstTraverser, enOrder, left, depth + 1);
			for (unsigned int i = 0; g_bTraceDepth && i < depth; i++) {
				__data_structure_debug_printf(" ");
			}
			pstTraverser->traverse(pstTraverser->pTraverser, pstStorage->node_payload(pstStorage, node, NULL));
			bst_traverse_node(pstStorage, pstTraverser, enOrder, right, depth + 1);
			break;
		case BST_TRAVERSE_PRE_ORDER:
			bst_traverse_node(pstStorage, pstTraverser, enOrder, left, depth + 1);
			bst_traverse_node(pstStorage, pstTraverser, enOrder, right, depth + 1);
			for (unsigned int i = 0; g_bTraceDepth && i < depth; i++) {
				__data_structure_debug_printf(" ");
			}
			pstTraverser->traverse(pstTraverser->pTraverser, pstStorage->node_payload(pstStorage, node, NULL));
			break;
		case BST_TRAVERSE_POST_ORDER:
			for (unsigned int i = 0; g_bTraceDepth && i < depth; i++) {
				__data_structure_debug_printf(" ");
			}
			pstTraverser->traverse(pstTraverser->pTraverser, pstStorage->node_payload(pstStorage, node, NULL));
			bst_traverse_node(pstStorage, pstTraverser, enOrder, left, depth + 1);
			bst_traverse_node(pstStorage, pstTraverser, enOrder, right, depth + 1);
			break;
		default:
			return;
	}
}
void __bst_traverse(void * pTree, struct _TRAVERSER * pstTraverser, enum _BST_TRAVERSE_ORDER enOrder) {
	struct _BINARY_SEARCH_TREE * pstTree = (struct _BINARY_SEARCH_TREE *)pTree;
	void * root = pstTree->stStorage.node_get(&pstTree->stStorage, NULL, BINARY_TREE_PARENT);
	bst_traverse_node(&pstTree->stStorage, pstTraverser, enOrder, root, 0);

}