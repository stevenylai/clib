#include <stdlib.h>
#include "error/error.h"
#include "data_structure_internal.h"
static void * bst_naive_create (struct _BINARY_SEARCH_TREE * pstTree) {
	pstTree->pTree = NULL;
	return pstTree;
}
static void bst_naive_destroy (struct _BINARY_SEARCH_TREE * pstTree) {
}
static void * bst_naive_update (struct _BINARY_SEARCH_TREE * pstTree, size_t size, void * pData) {
	enum _BINARY_TREE_NODE_RELATION enUpdateTo = BINARY_TREE_PARENT;
	void *pChild = NULL, *pParent = pstTree->stStorage.node_get(&pstTree->stStorage, NULL, BINARY_TREE_PARENT);
	while (true) {
		if (!pParent || pstTree->pstComparator->compare(pstTree->pstComparator->pKey, pstTree->stStorage.node_payload(&pstTree->stStorage, pParent, NULL), pData) == 0) {
			enUpdateTo = BINARY_TREE_PARENT;
			break;
		} else if (pstTree->pstComparator->compare(pstTree->pstComparator->pKey, pstTree->stStorage.node_payload(&pstTree->stStorage, pParent, NULL), pData) < 0) {
			if (!(pChild = pstTree->stStorage.node_get(&pstTree->stStorage, pParent, BINARY_TREE_LEFT))) {
				enUpdateTo = BINARY_TREE_LEFT;
				break;
			}
		} else {
			if (!(pChild = pstTree->stStorage.node_get(&pstTree->stStorage, pParent, BINARY_TREE_RIGHT))) {
				enUpdateTo = BINARY_TREE_RIGHT;
				break;
			}
		}
		pParent = pChild;
	}
	if (enUpdateTo == BINARY_TREE_PARENT) {
		if (!pParent) { // Root
			if (pChild = pstTree->stStorage.node_create(&pstTree->stStorage, pData, size)) {
				pstTree->stStorage.node_set(&pstTree->stStorage, NULL,  BINARY_TREE_PARENT, pChild);
			}
			return pChild ? pstTree->stStorage.node_payload(&pstTree->stStorage, pChild, NULL) : NULL;
		} else { // Existing parent
			pChild = pstTree->stStorage.node_update(&pstTree->stStorage, pParent, pData, size);
			return pChild ? pstTree->stStorage.node_payload(&pstTree->stStorage, pChild, NULL) : NULL;
		}
	} else { // New child
		if (pChild = pstTree->stStorage.node_create(&pstTree->stStorage, pData, size)) {
			pstTree->stStorage.node_set(&pstTree->stStorage, pParent, enUpdateTo, pChild);
			pstTree->stStorage.node_set(&pstTree->stStorage, pChild, BINARY_TREE_PARENT, pParent);
		}
		return pChild ? pstTree->stStorage.node_payload(&pstTree->stStorage, pChild, NULL) : NULL;
	}
}
static void * bst_naive_retrieve (struct _BINARY_SEARCH_TREE * pstTree, void * pKey) {
	void *payload = NULL, *pNode = pstTree->stStorage.node_get(&pstTree->stStorage, NULL, BINARY_TREE_PARENT);
	while (true) {
		if (!pNode) return NULL;
		payload = pstTree->stStorage.node_payload(&pstTree->stStorage, pNode, NULL);
		if (pstTree->pstComparator->compare(pstTree->pstComparator->pKey, payload, pKey) == 0) {
			return payload;
		} else if (pstTree->pstComparator->compare(pstTree->pstComparator->pKey, payload, pKey) < 0) {
			pNode = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT);
		} else {
			pNode = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_RIGHT);
		}
	}
}
static void bst_naive_remove_node (struct _BINARY_SEARCH_TREE * pstTree, void * pNode) {
	if (!pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT) && !pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_RIGHT)) {
		void * pParent = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_PARENT);
		enum _BINARY_TREE_NODE_RELATION enRelation = BINARY_TREE_PARENT;
		if (pParent && pstTree->stStorage.node_get(&pstTree->stStorage, pParent, BINARY_TREE_LEFT) == pNode)
			pstTree->stStorage.node_set(&pstTree->stStorage, pParent, BINARY_TREE_LEFT, NULL);
		else if (pParent)
			pstTree->stStorage.node_set(&pstTree->stStorage, pParent, BINARY_TREE_RIGHT, NULL);
		else
			pstTree->stStorage.node_set(&pstTree->stStorage, NULL, BINARY_TREE_PARENT, NULL);
		pstTree->stStorage.node_remove(&pstTree->stStorage, pNode);
		return;
	} // Case 1: no child
	if (!pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT) || !pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_RIGHT)) {
		void * pChild = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT) ? pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT) : pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_RIGHT);
		void * pParent = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_PARENT);
		enum _BINARY_TREE_NODE_RELATION enRelation = BINARY_TREE_PARENT;
		if (pParent) {
			if (pstTree->stStorage.node_get(&pstTree->stStorage, pParent, BINARY_TREE_LEFT) == pNode) enRelation = BINARY_TREE_LEFT;
			else enRelation = BINARY_TREE_RIGHT;
		}
		pstTree->stStorage.node_set(&pstTree->stStorage, pChild, BINARY_TREE_PARENT, pParent);
		pstTree->stStorage.node_set(&pstTree->stStorage, pParent, enRelation, pChild);
		pstTree->stStorage.node_remove(&pstTree->stStorage, pNode);
		return;
	} // Case 2: 1 child
	//printf("Case 3: Node: %d\n", *(unsigned int *)pstTree->stStorage.node_payload(&pstTree->stStorage, pNode, NULL));
	void * pRNode = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT); // Case 3: 2 children
	while (pstTree->stStorage.node_get(&pstTree->stStorage, pRNode, BINARY_TREE_RIGHT)) {
		pRNode = pstTree->stStorage.node_get(&pstTree->stStorage, pRNode, BINARY_TREE_RIGHT);
	}
	//printf("Final RNode: %d\n", *(unsigned int *)pstTree->stStorage.node_payload(&pstTree->stStorage, pRNode, NULL));
	pstTree->stStorage.node_swap_payload(&pstTree->stStorage, &pRNode, &pNode); // pRNode now hold the payload which is to be removed
	void * pRParent = pstTree->stStorage.node_get(&pstTree->stStorage, pRNode, BINARY_TREE_PARENT);
	void * pRChild = pstTree->stStorage.node_get(&pstTree->stStorage, pRNode, BINARY_TREE_LEFT);
	if (pRChild) pstTree->stStorage.node_set(&pstTree->stStorage, pRChild, BINARY_TREE_PARENT, pRParent);
	if (pRParent != pNode) {
		pstTree->stStorage.node_set(&pstTree->stStorage, pRParent, BINARY_TREE_RIGHT, pRChild);
	} else {
		pstTree->stStorage.node_set(&pstTree->stStorage, pRParent, BINARY_TREE_LEFT, pRChild);
	}
	pstTree->stStorage.node_remove(&pstTree->stStorage, pRNode);
}
static void bst_naive_remove (struct _BINARY_SEARCH_TREE * pstTree, void * pKey) {
	void *payload = NULL, *pNode = pstTree->stStorage.node_get(&pstTree->stStorage, NULL, BINARY_TREE_PARENT);
	while (true) {
		if (!pNode) return;
		payload = pstTree->stStorage.node_payload(&pstTree->stStorage, pNode, NULL);
		if (pstTree->pstComparator->compare(pstTree->pstComparator->pKey, payload, pKey) == 0) {
			bst_naive_remove_node(pstTree, pNode);
			return;
		} else if (pstTree->pstComparator->compare(pstTree->pstComparator->pKey, payload, pKey) < 0) {
			pNode = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_LEFT);
		} else {
			pNode = pstTree->stStorage.node_get(&pstTree->stStorage, pNode, BINARY_TREE_RIGHT);
		}
	}
}
void __binary_search_tree_naive_set_func_ptr (struct _BINARY_SEARCH_TREE * pstTree) {
	pstTree->tree_create = bst_naive_create;
	pstTree->tree_destroy = bst_naive_destroy;
	pstTree->tree_remove = bst_naive_remove;
	pstTree->tree_retrieve = bst_naive_retrieve;
	pstTree->tree_update = bst_naive_update;
}