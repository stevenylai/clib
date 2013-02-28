#ifndef __DATA_STRUCTURE_INTERFACE_H__
#define __DATA_STRUCTURE_INTERFACE_H__

#include "error/error.h"
typedef struct _ALLOCATOR {
	void * pAllocator;
	size_t reallocSize;
	size_t expansionSize;
	size_t expansionFactor;
	void * (*allocate) (void *, size_t);			// void * allocate (void * pAllocator, size_t size);
	void * (*reallocate) (void *, void *, size_t);	// void * reallocate (void * pAllocator, void * pOld, size_t size);
	void (*memfree) (void *, void *);				// void free (void * pAllocator, void * pFree);
} C_ALLOCATOR;
typedef struct _COMPARATOR {
	void * pKey;
	int (*compare) (void *, void *, void *);	// int compare (void * pKey, void * pData1, void * pData2);
} DATA_COMPARATOR;
typedef struct _TRAVERSER {
	void * pTraverser;
	void (*traverse)(void *, void *);				// void traverse (void * pTraverser, void * pData);
} DATA_STRUCTURE_TRAVERSER;
typedef enum _DATA_STRUCTURE_TYPE {
	ARRAY_LIST_FLAT,		// All elements are stored on a single big array
	LINKED_LIST_POINTER,	// One element per node
	//ARRAY_LIST_POINTER,	// Similar to ARRAY_LIST_FLAT except that a pointer is pointed to each element
	//LINKED_LIST_BLOCK,	// Similar to LINKED_LIST_POINTER except that each node is a ARRAY_LIST_POINTER with fixed allocation size.
	BST_NAIVE_POINTER,
	BST_NAIVE_ARRAY
} DATA_STRUCTURE_TYPE;
typedef enum _BST_TRAVERSE_ORDER {
	BST_TRAVERSE_IN_ORDER,
	BST_TRAVERSE_POST_ORDER,
	BST_TRAVERSE_PRE_ORDER
} BST_TRAVERSE_ORDER;
// List (essential)
void * __list_create (void ** pList,  enum _DATA_STRUCTURE_TYPE enType, struct _ERROR_HANDLER  * pstError, struct _ALLOCATOR * pstAllocator);
void __list_destroy (void * pList);
unsigned int __list_count (void * pList);
void * __list_get (void * pList, unsigned int index, size_t * pSize);
void * __list_insert (void * pList, unsigned int index, size_t size, const void * pEntry);
void __list_remove (void * pList, unsigned int index);
void * __list_update (void * pList, unsigned int index, size_t size, const void * pEntry);
// List (iterator)
void __list_start_iterator (void * pList, unsigned int startIndex);
void * __list_next (void * pList, size_t *pSize, unsigned int *pIndex);
bool __list_has_next (void * pList);
void __list_traverse (void * pList, struct _TRAVERSER * pstTraverser);
// Binary search tree
void * __bst_create (void ** pTree,  enum _DATA_STRUCTURE_TYPE enType, struct _COMPARATOR * pstComparator, struct _ERROR_HANDLER  * pstError, struct _ALLOCATOR * pstAllocator);
void __bst_destroy (void * pTree);
void * __bst_update (void * pTree, size_t size, void * pData);
void * __bst_retrieve (void * pTree, void * pKey);
void __bst_remove (void * pTree, void * pKey);
void __bst_traverse(void * pTree, struct _TRAVERSER * pstTraverser, enum _BST_TRAVERSE_ORDER enOrder);
#endif