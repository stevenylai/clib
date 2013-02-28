#ifndef __DATA_CONVERTER_INTERNAL_H__
#define __DATA_CONVERTER_INTERNAL_H__
#include <setjmp.h>
#include "error/error.h"
#include "data_converter_interface.h"
//#define _DATA_CONVERTER_DEBUG_
typedef struct _FORMAT_TYPE_NODE {
	unsigned int nodeType;
	bool bSigned;
	size_t size;
	size_t count;
	struct _FORMAT_TYPE_NODE * pstParent;
	union {
		unsigned int uiVal;
		double dbVal;
		char * pcVal;
	} value;
	char * pcName;
	void * pSubNodeList;
	void * pCurrentTable;
} FORMAT_TYPE_NODE;
typedef struct _FORMAT_PARSER_DATA {
	/* General */
	size_t numChar;
	/* Input */
	const char * pcInput;
	const char * pcCurInput;
	/* Intermediate results */
	struct _FORMAT_TYPE_NODE * pstRoot;
	struct _FORMAT_TYPE_NODE * pstCurrentNode;
	/* Error handling */
	struct _ERROR_HANDLER * pstError;
	int iErr;
	jmp_buf pFatalError;
} FORMAT_PARSER_DATA;
typedef enum _DATA_CONVERTER_INPUT {
	DATA_CONVERTER_LEFT,
	DATA_CONVERTER_RIGHT
} DATA_CONVERTER_INPUT;
typedef struct _DATA_CONVERTER {
	enum _CONVERTER_TYPE enType;
	struct _FORMAT_TYPE_NODE stLeftRoot;
	struct _FORMAT_TYPE_NODE stRightRoot;
	void * pLeft;
	void * pRight;
	const char * pcKey;
	bool bHitKey;
	struct _ERROR_HANDLER * pstError;
	struct _ALLOCATOR * pstAllocator;
	void * pConverter;
	void * (*create) (struct _DATA_CONVERTER *, const char * ,struct _ERROR_HANDLER *);
	void (*destroy)  (struct _DATA_CONVERTER *);

	void * (*convert) (struct _DATA_CONVERTER *, enum _FORMAT_TYPE, struct _DATA_CONVERT_INFO *, struct _ALLOCATOR *);
	int (*compare) (struct _DATA_CONVERTER *, enum _FORMAT_TYPE, struct _DATA_CONVERT_INFO *, struct _ALLOCATOR *);

	int (*read_byte) (struct _DATA_CONVERTER *, enum _DATA_CONVERTER_INPUT);
	int (*unread_byte) (struct _DATA_CONVERTER *, char, enum _DATA_CONVERTER_INPUT);
	int (*write_byte) (struct _DATA_CONVERTER *, char, enum _DATA_CONVERTER_INPUT);
	void (*seek_byte) (struct _DATA_CONVERTER *, enum _DATA_CONVERTER_INPUT);
} DATA_CONVERTER;
typedef struct _NODE_CONVERTER {
	enum _FORMAT_TYPE enType;
	void * pConverter;
	void * (*create) (struct _DATA_CONVERTER *, struct _NODE_CONVERTER *, enum _FORMAT_TYPE);
	void (*destroy) (struct _DATA_CONVERTER *, struct _NODE_CONVERTER *);

	int (*convert_node) (struct _DATA_CONVERTER *, struct _NODE_CONVERTER *, struct _FORMAT_TYPE_NODE *, bool);
	int (*compare_node) (struct _DATA_CONVERTER *, struct _NODE_CONVERTER *, struct _FORMAT_TYPE_NODE *, struct _FORMAT_TYPE_NODE *, bool);
} NODE_CONVERTER;
typedef struct _MEM_CONVERTER { // Type: struct _DATA_CONVERTER
	size_t leftMaxSize;
	size_t leftCurSize;
	size_t rightMaxSize;
	size_t rightCurSize;
	int iErr;
	struct _NODE_CONVERTER stNodeConverter;
} MEM_CONVERTER;
typedef struct _PACKEDC_NODE_CONVERTER { // Type: struct _NODE_CONVERTER
	void * pAddressStack;
	void * pValuePairs;
} PACKEDC_NODE_CONVERTER;
typedef struct _PACKEDC_NODE_CONTEXT {
	size_t maxSize;
	size_t curSize;
	void * pData;
} PACKEDC_NODE_CONTEXT;
/* Type tree related */
void * format_create_type_tree (const char * pcFormat, struct _FORMAT_TYPE_NODE * pstRoot, struct _ERROR_HANDLER * pstError);
void format_destroy_type_tree (struct _FORMAT_TYPE_NODE * pstNode);
void * format_retrieve_symbol (struct _FORMAT_TYPE_NODE * pstNode, char * pcName);
/* Function pointer setter for converters and alike */
void __mem_converter_set_func_ptr (struct _DATA_CONVERTER * pstConverter, enum _CONVERTER_TYPE enType);
void __node_converter_packedc_set_func_ptr (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNodeConverter, enum _FORMAT_TYPE enType);
/* Auxiliary */
void __data_converter_debug_printf (char * pcFormat, ...);
#endif