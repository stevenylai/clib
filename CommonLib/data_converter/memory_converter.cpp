#include "detect_mem_leak.h"
#include <string.h>
#include <stdio.h>
#include "error/error.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
static void * converter_default_allocate(void * pAllocator, size_t size) {
	return malloc(size);
}
static void * converter_default_reallocate(void * pAllocator, void * pOld, size_t size) {
	return realloc(pOld, size);
}
static void converter_default_free(void * pAllocator, void * pData) {
	free(pData);
}
static struct _ALLOCATOR g_stDefaultAllocator = {NULL, 32, 2, 1, converter_default_allocate, converter_default_reallocate, converter_default_free};
static void * mem_create (struct _DATA_CONVERTER * pstConverter, const char * pcFormat, struct _ERROR_HANDLER * pstError) {
	bool bErr = false;
	pstConverter->bHitKey = false, pstConverter->pLeft = pstConverter->pRight = NULL;
	pstConverter->pstError = pstError, pstConverter->pConverter = pstConverter->pstAllocator = NULL;
	if (!format_create_type_tree(pcFormat, &pstConverter->stLeftRoot, pstConverter->pstError)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to create syntax tree for memory converter");
		bErr = true;
		goto error;
	}
	if (!format_create_type_tree(pcFormat, &pstConverter->stRightRoot, pstConverter->pstError)) {
		format_destroy_type_tree(&pstConverter->stLeftRoot); // Clean up before handling the error
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to create syntax tree for memory converter");
		bErr = true;
		goto error;
	}
error:
	if (bErr) {
		return NULL;
	} else {
		return pstConverter;
	}
}
static void mem_destroy (struct _DATA_CONVERTER * pstConverter) {
	format_destroy_type_tree(&pstConverter->stLeftRoot);
	format_destroy_type_tree(&pstConverter->stRightRoot);
	free (pstConverter->pConverter), pstConverter->pConverter = NULL;
}
static int mem_convert_node (struct _DATA_CONVERTER * pstConverter, struct _FORMAT_TYPE_NODE * pstNode) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	if (!pstConverter->bHitKey && pstNode->pcName && pstConverter->pcKey && !strcmp(pstNode->pcName, pstConverter->pcKey)) pstConverter->bHitKey = true;
	switch (pstNode->nodeType) { // Composite type handling
		case VARRAY:	// Retrieve the index info
		{	struct _FORMAT_TYPE_NODE * pstIndexNodeName = (struct _FORMAT_TYPE_NODE *)__list_get(pstNode->pSubNodeList, __list_count(pstNode->pSubNodeList) - 1, NULL);
			struct _FORMAT_TYPE_NODE * pstActualNode = (pstIndexNodeName && pstIndexNodeName->pcName ? (struct _FORMAT_TYPE_NODE *)format_retrieve_symbol(pstNode, pstIndexNodeName->pcName) : NULL);
			if (!pstActualNode) {
				pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = DATA_FORMAT_SYNTAX_ERROR, "Failed to find index for VARRAY");
				goto error;
			}
			unsigned int count = pstNode->count = pstActualNode->value.uiVal;
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, true)) goto error;
			for (unsigned int i = 0; i < count; i++) {
				unsigned int index = 0;
				for (__list_start_iterator(pstNode->pSubNodeList, 0); __list_has_next(pstNode->pSubNodeList);) {
					struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstNode->pSubNodeList, NULL, &index);
					if (pstSubNode->pstParent == pstNode) {
						if (pstMemConverter->iErr = mem_convert_node(pstConverter, pstSubNode)) goto error;
					}
				}
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, false)) goto error;
			break;
		}
		case ARRAY:
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, true)) goto error;
			for (unsigned int i = 0; i < pstNode->count; i++) {
				for (__list_start_iterator(pstNode->pSubNodeList, 0); __list_has_next(pstNode->pSubNodeList);) {
					struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstNode->pSubNodeList, NULL, NULL);
					if (pstMemConverter->iErr = mem_convert_node(pstConverter, pstSubNode)) {
						goto error;
					}
				}
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, false)) goto error;
			break;
		case STRUCT:
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, true)) goto error;
			for (__list_start_iterator(pstNode->pSubNodeList, 0); __list_has_next(pstNode->pSubNodeList);) {
				struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstNode->pSubNodeList, NULL, NULL);
				if (pstMemConverter->iErr = mem_convert_node(pstConverter, pstSubNode)) goto error;
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, false)) goto error;
			break;
		default: // Primitive types
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.convert_node(pstConverter, &pstMemConverter->stNodeConverter, pstNode, false)) goto error;
			break;
	}
	if (pstConverter->bHitKey && pstNode->pcName && pstConverter->pcKey && !strcmp(pstNode->pcName, pstConverter->pcKey)) pstConverter->bHitKey = false;
error:
	return pstMemConverter->iErr;
}
static void * mem_convert (struct _DATA_CONVERTER * pstConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator) {
	bool bErr = false;
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)(pstConverter->pConverter = malloc(sizeof(struct _MEM_CONVERTER)));
	void * pConverted = NULL;
	if (!pstMemConverter) {
		bErr = true;
		pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory for data conversion");
		goto error;
	} else {
		memset(pstMemConverter, 0, sizeof(struct _MEM_CONVERTER));
	}
	switch(pstMemConverter->stNodeConverter.enType = enType) {
		case C_TO_PACKEDC:
		case PACKEDC_TO_C:
		case PACKEDC_TO_C_FREE:
			__node_converter_packedc_set_func_ptr(pstConverter, &pstMemConverter->stNodeConverter, enType);
			break;
		default:
			bErr = true;
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, NO_IMPLEMENTATION, "No implmentation for converting between such data formats");
			goto error;
	}
	if (!pstMemConverter->stNodeConverter.create(pstConverter, &pstMemConverter->stNodeConverter, enType)) {
		bErr = true;
		goto error;
	}
	pstConverter->pLeft = pstData->pLeft, pstConverter->pRight = pstData->pRight;
	pstConverter->bHitKey = ((pstConverter->pcKey = pstData->pcKey) ? false : true);
	pstMemConverter->leftCurSize = pstMemConverter->leftMaxSize = pstMemConverter->rightCurSize = pstMemConverter->rightMaxSize = 0, pstMemConverter->iErr = SUCCESS;
	pstConverter->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;

	for (__list_start_iterator(pstConverter->stLeftRoot.pSubNodeList, 0); __list_has_next(pstConverter->stLeftRoot.pSubNodeList);) { // Start from left (input) to right (output)
		struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstConverter->stLeftRoot.pSubNodeList, NULL, NULL);
		if (pstData->iErr = mem_convert_node(pstConverter, pstSubNode)) goto error;
	}
	pConverted = pstData->pRight = pstConverter->pRight;
	pstData->size = pstMemConverter->rightCurSize;
error:
	if (pstMemConverter) {
		pstMemConverter->stNodeConverter.destroy(pstConverter, &pstMemConverter->stNodeConverter);
		free (pstMemConverter), pstConverter->pConverter = pstMemConverter = NULL;
	}
	return bErr ? NULL : pConverted;
}
static int mem_compare_node (struct _DATA_CONVERTER * pstConverter, struct _FORMAT_TYPE_NODE * pstLeftNode, struct _FORMAT_TYPE_NODE * pstRightNode) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	if (!pstConverter->bHitKey && pstLeftNode->pcName && pstConverter->pcKey && !strcmp(pstLeftNode->pcName, pstConverter->pcKey)) pstConverter->bHitKey = true;
	switch (pstLeftNode->nodeType) { // Composite type handling
		case VARRAY:	// Retrieve the index info
		{	struct _FORMAT_TYPE_NODE * pstLeftIndexNodeName = (struct _FORMAT_TYPE_NODE *)__list_get(pstLeftNode->pSubNodeList, __list_count(pstLeftNode->pSubNodeList) - 1, NULL);
			struct _FORMAT_TYPE_NODE * pstLeftIndexNode = (pstLeftIndexNodeName && pstLeftIndexNodeName->pcName ? (struct _FORMAT_TYPE_NODE *)format_retrieve_symbol(pstLeftNode, pstLeftIndexNodeName->pcName) : NULL);
			if (!pstLeftIndexNode) {
				pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = DATA_FORMAT_SYNTAX_ERROR, "Failed to find index for VARRAY");
				goto error;
			}
			struct _FORMAT_TYPE_NODE * pstRightIndexNodeName = (struct _FORMAT_TYPE_NODE *)__list_get(pstRightNode->pSubNodeList, __list_count(pstRightNode->pSubNodeList) - 1, NULL);
			struct _FORMAT_TYPE_NODE * pstRightIndexNode = (pstRightIndexNodeName && pstRightIndexNodeName->pcName ? (struct _FORMAT_TYPE_NODE *)format_retrieve_symbol(pstRightNode, pstRightIndexNodeName->pcName) : NULL);
			if (!pstRightIndexNode) {
				pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = DATA_FORMAT_SYNTAX_ERROR, "Failed to find index for VARRAY");
				goto error;
			}
			unsigned int leftCount = pstLeftNode->count = pstLeftIndexNode->value.uiVal;
			unsigned int rightCount = pstRightNode->count = pstRightIndexNode->value.uiVal;
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, true)) goto error;
			for (unsigned int i = 0; i < leftCount && i < rightCount; i++) {
				for (__list_start_iterator(pstLeftNode->pSubNodeList, 0), __list_start_iterator(pstRightNode->pSubNodeList, 0); __list_has_next(pstLeftNode->pSubNodeList) && __list_has_next(pstRightNode->pSubNodeList);) {
					struct _FORMAT_TYPE_NODE * pstLeftSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstLeftNode->pSubNodeList, NULL, NULL);
					struct _FORMAT_TYPE_NODE * pstRightSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstLeftNode->pSubNodeList, NULL, NULL);
					if (pstLeftSubNode->pstParent == pstLeftNode && pstRightSubNode->pstParent == pstRightNode) {
						if (pstMemConverter->iErr = mem_compare_node(pstConverter, pstLeftSubNode, pstRightSubNode)) goto error;
					}
				}
			}
			if (leftCount < rightCount) {
				pstMemConverter->iErr = -1;
				goto error;
			} else if (leftCount > rightCount) {
				pstMemConverter->iErr = 1;
				goto error;
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, false)) goto error;
			break;
		}
		case ARRAY:
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, true)) goto error;
			for (unsigned int i = 0; i < pstLeftNode->count; i++) {
				for (__list_start_iterator(pstLeftNode->pSubNodeList, 0), __list_start_iterator(pstRightNode->pSubNodeList, 0); __list_has_next(pstLeftNode->pSubNodeList) && __list_has_next(pstRightNode->pSubNodeList);) {
					struct _FORMAT_TYPE_NODE * pstLeftSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstLeftNode->pSubNodeList, NULL, NULL);
					struct _FORMAT_TYPE_NODE * pstRightSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstRightNode->pSubNodeList, NULL, NULL);
					if (pstMemConverter->iErr = mem_compare_node(pstConverter, pstLeftSubNode, pstRightSubNode)) {
						goto error;
					}
				}
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, false)) goto error;
			break;
		case STRUCT:
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, true)) goto error;
			for (__list_start_iterator(pstLeftNode->pSubNodeList, 0), __list_start_iterator(pstRightNode->pSubNodeList, 0); __list_has_next(pstLeftNode->pSubNodeList) && __list_has_next(pstRightNode->pSubNodeList);) {
				struct _FORMAT_TYPE_NODE * pstLeftSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstLeftNode->pSubNodeList, NULL, NULL);
				struct _FORMAT_TYPE_NODE * pstRightSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstRightNode->pSubNodeList, NULL, NULL);
				if (pstMemConverter->iErr = mem_compare_node(pstConverter, pstLeftSubNode, pstRightSubNode)) goto error;
			}
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, false)) goto error;
			break;
		default: // Primitive types
			if (pstMemConverter->iErr = pstMemConverter->stNodeConverter.compare_node(pstConverter, &pstMemConverter->stNodeConverter, pstLeftNode, pstRightNode, false)) goto error;
			break;
	}
	if (pstConverter->bHitKey && pstLeftNode->pcName && pstConverter->pcKey && !strcmp(pstLeftNode->pcName, pstConverter->pcKey)) pstConverter->bHitKey = false;
error:
	return pstMemConverter->iErr;
}
static int mem_compare (struct _DATA_CONVERTER * pstConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator) {
	bool bErr = false;
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)(pstConverter->pConverter = malloc(sizeof(struct _MEM_CONVERTER)));
	void * pConverted = NULL;
	if (!pstMemConverter) {
		bErr = true;
		pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory for data conversion");
		goto error;
	} else {
		memset(pstMemConverter, 0, sizeof(struct _MEM_CONVERTER));
	}
	switch(pstMemConverter->stNodeConverter.enType = enType) {
		case PACKEDC_COMPARE_PACKEDC:
		case PACKEDC_COMPARE_PARTIALC:
			__node_converter_packedc_set_func_ptr(pstConverter, &pstMemConverter->stNodeConverter, enType);
			break;
		default:
			bErr = true;
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, NO_IMPLEMENTATION, "No implmentation for such comparison type in this data format");
			goto error;
	}
	if (!pstMemConverter->stNodeConverter.create(pstConverter, &pstMemConverter->stNodeConverter, enType)) {
		bErr = true;
		goto error;
	}
	pstConverter->pLeft = pstData->pLeft, pstConverter->pRight = pstData->pRight;
	pstConverter->bHitKey = ((pstConverter->pcKey = pstData->pcKey) ? false : true);
	pstMemConverter->leftCurSize = pstMemConverter->leftMaxSize = pstMemConverter->rightCurSize = pstMemConverter->rightMaxSize = 0, pstMemConverter->iErr = SUCCESS;
	pstConverter->pstAllocator = pstAllocator ? pstAllocator : &g_stDefaultAllocator;

	for (__list_start_iterator(pstConverter->stLeftRoot.pSubNodeList, 0), __list_start_iterator(pstConverter->stRightRoot.pSubNodeList, 0); __list_has_next(pstConverter->stLeftRoot.pSubNodeList) && __list_has_next(pstConverter->stRightRoot.pSubNodeList);) { // Compare between left & right
		struct _FORMAT_TYPE_NODE * pstLeftNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstConverter->stLeftRoot.pSubNodeList, NULL, NULL);
		struct _FORMAT_TYPE_NODE * pstRightNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstConverter->stRightRoot.pSubNodeList, NULL, NULL);
		if (pstData->iErr = mem_compare_node(pstConverter, pstLeftNode, pstRightNode)) goto error;
	}
	pConverted = pstData->pRight = pstConverter->pRight;
	pstData->size = pstMemConverter->rightCurSize;
error:
	if (pstMemConverter) {
		pstMemConverter->stNodeConverter.destroy(pstConverter, &pstMemConverter->stNodeConverter);
		free (pstMemConverter), pstConverter->pConverter = pstMemConverter = NULL;
	}
	return pstData->iErr;
}
static int mem_get_byte (struct _DATA_CONVERTER * pstConverter, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	char cInput = ((enInput == DATA_CONVERTER_LEFT) ? *((char *)pstConverter->pLeft + (pstMemConverter->leftCurSize++)) : *((char *)pstConverter->pRight + (pstMemConverter->rightCurSize++)));
	int iInput = 0;
	memcpy(&iInput, &cInput, sizeof(char)); // Do not use auto conversion by the compiler since when cInput == 0xFF, the output will happen to be EOF
	return iInput;
}
static int mem_unget_byte (struct _DATA_CONVERTER * pstConverter, char cByte, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	if (enInput == DATA_CONVERTER_LEFT && pstMemConverter->leftCurSize) pstMemConverter->leftCurSize--;
	if (enInput == DATA_CONVERTER_RIGHT && pstMemConverter->rightCurSize) pstMemConverter->rightCurSize--;
	int iOutput = 0;
	memcpy(&iOutput, &cByte, sizeof(char));
	return iOutput;
}
static void * mem_ensure_buffer_size (struct _DATA_CONVERTER * pstConverter, size_t size, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	size_t currentSize =  (enInput == DATA_CONVERTER_LEFT ? pstMemConverter->leftCurSize : pstMemConverter->rightCurSize);
	size_t currentMaxSize = (enInput == DATA_CONVERTER_LEFT ? pstMemConverter->leftMaxSize : pstMemConverter->rightMaxSize);
	if (currentMaxSize >= currentSize + size) return enInput == DATA_CONVERTER_LEFT ? pstConverter->pLeft : pstConverter->pRight;

	size_t reallocSize = (currentMaxSize ? (currentMaxSize * pstConverter->pstAllocator->expansionSize / pstConverter->pstAllocator->expansionFactor <= currentMaxSize ? currentMaxSize * 2 : currentMaxSize * pstConverter->pstAllocator->expansionSize / pstConverter->pstAllocator->expansionFactor) : pstConverter->pstAllocator->reallocSize);
	while (reallocSize < currentSize + size) reallocSize = (reallocSize * pstConverter->pstAllocator->expansionSize / pstConverter->pstAllocator->expansionFactor <= reallocSize ? reallocSize * 2 : reallocSize * pstConverter->pstAllocator->expansionSize / pstConverter->pstAllocator->expansionFactor);
	void * pNewSpace = pstConverter->pstAllocator->reallocate(pstConverter->pstAllocator->pAllocator, (enInput == DATA_CONVERTER_LEFT ? pstConverter->pLeft : pstConverter->pRight), reallocSize);
	if (!pNewSpace) {
		pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, MALLOC_NO_MEM, "Failed to reallocate memory for buffer during data conversion");
		pstConverter->pstAllocator->memfree(pstConverter->pstAllocator->pAllocator, (enInput == DATA_CONVERTER_LEFT ? pstConverter->pLeft : pstConverter->pRight));
	} else {
		enInput == DATA_CONVERTER_LEFT ? (pstMemConverter->leftMaxSize = reallocSize) : (pstMemConverter->rightMaxSize = reallocSize);
	}
	(enInput == DATA_CONVERTER_LEFT) ? (pstConverter->pLeft = pNewSpace) : (pstConverter->pRight = pNewSpace);
	return pNewSpace;
}
static int mem_put_byte (struct _DATA_CONVERTER * pstConverter, char cByte, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	if (!mem_ensure_buffer_size(pstConverter, 1, enInput)) {
		return EOF;
	}
	enInput == DATA_CONVERTER_LEFT ? (*((char *)pstConverter->pLeft + (pstMemConverter->leftCurSize++)) = cByte) : (*((char *)pstConverter->pRight + (pstMemConverter->rightCurSize++)) = cByte);
	int iOutput = 0;
	memcpy(&iOutput, &cByte, sizeof(char));
	return iOutput;
}
static void mem_skip_put (struct _DATA_CONVERTER * pstConverter, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	enInput == DATA_CONVERTER_LEFT ? pstMemConverter->leftCurSize++ : pstMemConverter->rightCurSize++;
}
void __mem_converter_set_func_ptr (struct _DATA_CONVERTER * pstConverter, enum _CONVERTER_TYPE enType) {
	pstConverter->create = mem_create;
	pstConverter->destroy = mem_destroy;
	pstConverter->convert = mem_convert;
	pstConverter->read_byte = mem_get_byte;
	pstConverter->unread_byte = mem_unget_byte;
	pstConverter->write_byte = mem_put_byte;
	pstConverter->seek_byte = mem_skip_put;
}