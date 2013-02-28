#ifndef __DATA_STRUCTURE_INTERNAL_H__
#define __DATA_STRUCTURE_INTERNAL_H__
#include "error/error.h"
#include "data_structure_interface.h"
//#define _DATA_STRUCTURE_DEBUG_
#define ARRAY_REALLOC_DEFAULT_SIZE 128
#define _BINARY_TREE_DEBUG_TRAVERSE_
typedef struct _LIST {
	enum _DATA_STRUCTURE_TYPE enType;
	void * pList;
	struct _ERROR_HANDLER * pstError;
	struct _ALLOCATOR * pstAllocator;
	void * pGlue;
	// Essential operations
	void * (*list_create) (struct _LIST *, struct _ERROR_HANDLER *, struct _ALLOCATOR *);
	void (*list_destroy) (struct _LIST *);
	void * (*list_get) (struct _LIST *, unsigned int, size_t *);
	unsigned int (*list_count) (struct _LIST *);
	void * (*list_insert) (struct _LIST *, unsigned int, size_t, const void *);
	void * (*list_update) (struct _LIST *, unsigned int, size_t, const void *);
	void (*list_remove) (struct _LIST *, unsigned int);
	// Iterator operations
	void (*start_iterator) (struct _LIST *, unsigned int);
	void * (*next) (struct _LIST *, size_t *, unsigned int *);
	bool (*has_next) (struct _LIST *);
} C_LIST;
/* Array list - continuous: all data in the array are stored continously in a single large array.
	Benefit: flat data structure - less fragmentation and the data structure may be safely stored and shared among different processes */
typedef struct _ARRAY_LIST_NODE {
	size_t size;
} ARRAY_LIST_NODE;
typedef struct _ARRAY_LIST_HEAD {
	// Essential
	unsigned int count;
	size_t usedCapacity;
	size_t capacity;
	// Iterator helper
	size_t iteratorLocation;
	unsigned int iteratorIndex;
} ARRAY_LIST_HEAD;
/* Linked list - pointer: each data will be stored in a single node.
	Benefit: all pointers obtained from list_get will be valid until the entry is updated / deleted or the list is destroyed. This is not the case for arra lists since the entries may get reallocated */
typedef struct _LINKED_LIST_NODE {
	size_t size;
	size_t capacity;
	struct _LINKED_LIST_NODE * pstNext;
} LINKED_LIST_NODE;
typedef struct _LINKED_LIST_HEAD {
	// Essential
	unsigned int count;
	struct _LINKED_LIST_NODE *pstFirst;
	// Iterator helper
	unsigned int iteratorIndex;
	struct _LINKED_LIST_NODE *pstPrevious;
	struct _LINKED_LIST_NODE *pstCurrent;
} LINKED_LIST_HEAD;
typedef enum _BINARY_TREE_STORAGE_TYPE {
	BINARY_TREE_POINTER,			// One element per node
	BINARY_TREE_ARRAY				// All elements are stored on a single big array
} BINARY_TREE_STORAGE_TYPE;
typedef enum _BINARY_TREE_NODE_RELATION {
	BINARY_TREE_PARENT,
	BINARY_TREE_LEFT,
	BINARY_TREE_RIGHT
} BINARY_TREE_NODE_RELATION;
typedef struct _BINARY_TREE_STORAGE {
	enum _BINARY_TREE_STORAGE_TYPE enType;
	void * pStorage;
	struct _ERROR_HANDLER * pstError;
	struct _ALLOCATOR * pstAllocator;
	// Storage related
	void * (*storage_create) (struct _BINARY_TREE_STORAGE *, struct _ERROR_HANDLER *, struct _ALLOCATOR *);
	void (*storage_destroy) (struct _BINARY_TREE_STORAGE *);
	// Node specific
	void (*node_remove) (struct _BINARY_TREE_STORAGE *, void *);
	void * (*node_update) (struct _BINARY_TREE_STORAGE *, void *, void *, size_t);
	void (*node_swap_payload) (struct _BINARY_TREE_STORAGE *, void **, void **);
	void * (*node_create) (struct _BINARY_TREE_STORAGE *, void *, size_t);
	void * (*node_get) (struct _BINARY_TREE_STORAGE *, void *, enum _BINARY_TREE_NODE_RELATION);
	void (*node_set) (struct _BINARY_TREE_STORAGE *, void *, enum _BINARY_TREE_NODE_RELATION, const void *);
	void * (*node_payload) (struct _BINARY_TREE_STORAGE *, void *, size_t *);
} BINARY_TREE_STORAGE;
typedef struct _BINARY_TREE_POINTER_NODE {
	size_t size;
	size_t capacity;
	struct _BINARY_TREE_POINTER_NODE * pstParent;
	struct _BINARY_TREE_POINTER_NODE * pstLeft;
	struct _BINARY_TREE_POINTER_NODE * pstRight;
} BINARY_TREE_POINTER_NODE;
typedef struct _BINARY_TREE_ARRAY_HEAD {
	size_t capacity;
	size_t size;
	unsigned int totalCount;
	size_t root;
} BINARY_TREE_ARRAY_HEAD;
typedef struct _BINARY_TREE_ARRAY_NODE {
	size_t size;
	size_t parent;
	size_t left;
	size_t right;
} BINARY_TREE_ARRAY_NODE;
typedef struct _BINARY_SEARCH_TREE {
	enum _DATA_STRUCTURE_TYPE enType;
	struct _BINARY_TREE_STORAGE stStorage;
	struct _COMPARATOR * pstComparator;
	void * pTree;

	void * (*tree_create) (struct _BINARY_SEARCH_TREE *);
	void (*tree_destroy) (struct _BINARY_SEARCH_TREE *);
	void * (*tree_update) (struct _BINARY_SEARCH_TREE *, size_t, void *);
	void * (*tree_retrieve) (struct _BINARY_SEARCH_TREE *, void *);
	void (*tree_remove) (struct _BINARY_SEARCH_TREE *, void *);
} BINARY_SEARCH_TREE;
typedef enum _RED_BLACK_TREE_COLOR {
	_RED_BLACK_TREE_RED,
	_RED_BLACK_TREE_BLACK
} RED_BLACK_TREE_COLOR;
typedef struct _RED_BLACK_TREE_NODE {
	enum _RED_BLACK_TREE_COLOR enColor;
} RED_BLACK_TREE_NODE;

void __array_list_flat_set_func_ptr (struct _LIST * pstList);
void __linked_list_pointer_set_func_ptr (struct _LIST * pstList);

void __binary_tree_pointer_set_func_ptr (struct _BINARY_TREE_STORAGE * pstTreeStorage);
void __binary_tree_array_set_func_ptr (struct _BINARY_TREE_STORAGE * pstTreeStorage);
void __binary_search_tree_naive_set_func_ptr (struct _BINARY_SEARCH_TREE * pstTree);

void __data_structure_debug_printf (char * pcFormat, ...);
#endif