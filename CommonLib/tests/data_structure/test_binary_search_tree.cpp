#include <stdlib.h>
#include <string.h>
#include "data_structure/data_structure_internal.h"
#include "platform/platform_interface.h"
typedef struct _BST_VERIFIER_SIMPLE {
	unsigned int count;
	int err;
	void * pLastNode;
} BST_VERIFIER_SIMPLE;
static int compare_int (void * pKey, void * pData1, void * pData2) {
	return *(int *)pData1 - *(int *)pData2;
}
static void bst_verify_simple (void * verifier, void * node) {
	struct _BST_VERIFIER_SIMPLE * pstVerifier = (struct _BST_VERIFIER_SIMPLE *) verifier;
	pstVerifier->count++;
	if (pstVerifier->pLastNode) {
		if (compare_int(NULL, pstVerifier->pLastNode, node) <= 0) {
			pstVerifier->err = FATAL_UNEXPECTED_ERROR;
		}
	}
	pstVerifier->pLastNode = node;
}
static void print_int (void * pTraverser, void * pData) {
	__data_structure_debug_printf("%d\n", *(int *)pData);
}
static int verify_binary_search_tree (void * pTree, unsigned int expectedCount) {
	struct _BST_VERIFIER_SIMPLE stVerifier;
	struct _TRAVERSER stVerifyTraverser = {&stVerifier, bst_verify_simple};
	memset(&stVerifier, 0, sizeof(struct _BST_VERIFIER_SIMPLE));
	__bst_traverse(pTree, &stVerifyTraverser, BST_TRAVERSE_IN_ORDER);
	if (stVerifier.count != expectedCount) {
		return FATAL_UNEXPECTED_ERROR;
	} else {
		return stVerifier.err;
	}
}
static void print_binary_tree (void * pTree) {	
	struct _TRAVERSER stIntDebug = {NULL, print_int};
	__bst_traverse(pTree, &stIntDebug, BST_TRAVERSE_IN_ORDER); __data_structure_debug_printf("============\n");
}
static int test_binary_search_tree (enum _DATA_STRUCTURE_TYPE enType) {
	int err = SUCCESS;
	void * pBST = NULL;
	struct _COMPARATOR stIntComparator = {NULL, compare_int};
	unsigned int data = 0, *puiRetrieved = NULL, rgBig[64] = {0}, count = 0;
	if (!__bst_create(&pBST, enType, &stIntComparator, NULL, NULL)) {
		return err = FATAL_CREATION_ERROR;
	}
	rgBig[0] = 64; __bst_update(pBST, sizeof(rgBig) - 4, rgBig);		// Level 0
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 32; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 1
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 16; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 2
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 8; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 24; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 48; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 2
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 40; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 56; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 98; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 1
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 82; __bst_update(pBST, sizeof(rgBig), rgBig);			// Level 2
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 90; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 74; __bst_update(pBST, sizeof(unsigned int), rgBig);		// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 114; __bst_update(pBST, sizeof(unsigned int), rgBig);	// Level 2
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 122; __bst_update(pBST, sizeof(unsigned int), rgBig);	// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	rgBig[0] = 106; __bst_update(pBST, sizeof(unsigned int), rgBig);	// Level 3
	if (err = verify_binary_search_tree (pBST, ++count)) goto error;
	//print_binary_tree(pBST);
	data = 16; puiRetrieved = (unsigned int *)__bst_retrieve(pBST, &data);
	if (puiRetrieved) __data_structure_debug_printf("Retrieved: %d\n", *puiRetrieved);
	data = 40; __bst_remove(pBST, &data);							// No child
	if (err = verify_binary_search_tree (pBST, --count)) goto error;
	//print_binary_tree(pBST);
	data = 48; __bst_remove(pBST, &data);							// One child
	if (err = verify_binary_search_tree (pBST, --count)) goto error;
	//print_binary_tree(pBST);
	data = 82; __bst_remove(pBST, &data);							// Two children
	if (err = verify_binary_search_tree (pBST, --count)) goto error;
	//print_binary_tree(pBST);
	data = 64; __bst_remove(pBST, &data);							// Two children
	if (err = verify_binary_search_tree (pBST, --count)) goto error;
	//print_binary_tree(pBST);
error:
	__bst_destroy(pBST);
	return err;
}
int __test_binary_search_tree() {
	int err = SUCCESS;
	if (err = test_binary_search_tree(BST_NAIVE_ARRAY)) {
		__write_to_console("Failed test case on test_binary_search_tree, type: BST_NAIVE_ARRAY\n");
		return err;
	}
	if (err = test_binary_search_tree(BST_NAIVE_POINTER)) {
		__write_to_console("Failed test case on test_binary_search_tree, type: BST_NAIVE_POINTER\n");
		return err;
	}
	return err;
}