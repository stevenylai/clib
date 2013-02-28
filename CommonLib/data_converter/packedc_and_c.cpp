#include "detect_mem_leak.h"
#include <stdio.h>
#include <string.h>
#include "error/error.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
static void * packedc_push_context (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter; // Must be a mem converter
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	struct _PACKEDC_NODE_CONTEXT stContext = {(enInput == DATA_CONVERTER_LEFT ? pstMemConverter->leftMaxSize : pstMemConverter->rightMaxSize), (enInput == DATA_CONVERTER_LEFT ? pstMemConverter->leftCurSize : pstMemConverter->rightCurSize), (enInput == DATA_CONVERTER_LEFT ? pstConverter->pLeft : pstConverter->pRight)};
	return __list_insert(pstNodeConverter->pAddressStack, 0, sizeof(struct _PACKEDC_NODE_CONTEXT), &stContext);
}
static void * packedc_pop_context (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, enum _DATA_CONVERTER_INPUT enInput) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter; // Must be a mem converter
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	struct _PACKEDC_NODE_CONTEXT * pstContext = (struct _PACKEDC_NODE_CONTEXT *)__list_get(pstNodeConverter->pAddressStack, 0, NULL);
	if (pstContext) {
		enInput == DATA_CONVERTER_LEFT ? (pstMemConverter->leftMaxSize = pstContext->maxSize) : (pstMemConverter->rightMaxSize = pstContext->maxSize);
		enInput == DATA_CONVERTER_LEFT ? (pstMemConverter->leftCurSize = pstContext->curSize) : (pstMemConverter->rightCurSize = pstContext->curSize);
		enInput == DATA_CONVERTER_LEFT ? (pstConverter->pLeft = pstContext->pData) : (pstConverter->pRight = pstContext->pData);
		__list_remove(pstNodeConverter->pAddressStack, 0);
	}
	return pstContext;
}
static int packedc_to_c (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, struct _FORMAT_TYPE_NODE * pstNode, bool bStart) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	switch (pstNode->nodeType) {
		case VARRAY:
		{	if (bStart) {
				if (pstConverter->bHitKey) {
					if (!packedc_push_context(pstConverter, pstNConverter, DATA_CONVERTER_RIGHT)) {
						pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to make room for data conversion");
						goto error;
					}
					pstConverter->pRight = NULL;
					pstMemConverter->rightMaxSize = pstMemConverter->rightCurSize = 0;
				}
			} else {
				if (pstConverter->bHitKey) {
					void * pCurrentOutput = pstConverter->pRight;
					if (!packedc_pop_context(pstConverter, pstNConverter, DATA_CONVERTER_RIGHT)) {
						pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to retrieve previously saved information for data conversion");
						goto error;
					}
					for (size_t s = 0; s < sizeof(void *); s++) {
						if (pstConverter->write_byte(pstConverter, *((char *)&pCurrentOutput + s), DATA_CONVERTER_RIGHT) == EOF) {
							pstMemConverter->iErr = EOF;
							goto error;
						}
					}
				}
			}
			break;
		}
		case ARRAY:
		case STRUCT: break; // Nothing to do
		case STRING:
		{	pstNode->value.pcVal = (char *)pstConverter->pLeft + pstMemConverter->leftCurSize;
			if (pstConverter->bHitKey) {
				for (size_t s = 0; s < sizeof(char *); s++) {  // Shallow copy for strings (only copy the pointer instead of the string contents)
					if (pstConverter->write_byte(pstConverter, *((char *)&pstNode->value.pcVal + s), DATA_CONVERTER_RIGHT) == EOF) {
						pstMemConverter->iErr = EOF;
						goto error;
					}
				}
			}
			int iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			while (iGet != EOF && (char)iGet != '\0') {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			}
			break;
		}
		case CHAR_FORMAT:
		case INT_FORMAT:
		case LONG_FORMAT:
		case SIZE_T_FORMAT:
		case FLOAT_FORMAT:
		case DOUBLE_FORMAT:
		case POINTER:
		{	int iGet = 0, iPut = 0;
			for (size_t s = 0; s < pstNode->size; s++) {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
				if (iGet == EOF) break;
				else {
					if (pstConverter->bHitKey && (iPut = pstConverter->write_byte(pstConverter, (char)iGet, DATA_CONVERTER_RIGHT)) != iGet) break;
					memcpy((char *)&pstNode->value + s, &iGet, sizeof(char));
				}
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			} else if (iPut != iGet) {
				pstMemConverter->iErr = iPut;
				goto error;
			}
			break;
		}
		default:
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = NO_IMPLEMENTATION, "There is no implementation found for converting data type: %d", pstNode->nodeType);
			goto error;
	}
error:
	return pstMemConverter->iErr;
}
static int packedc_to_freed_c (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, struct _FORMAT_TYPE_NODE * pstNode, bool bStart) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	switch (pstNode->nodeType) {
		case VARRAY:
		{	if (bStart) {
				if (pstConverter->bHitKey) {
					void * pNewRight = NULL;
					for (size_t s = 0; s < sizeof(void *); s++) {
						int byte = pstConverter->read_byte(pstConverter, DATA_CONVERTER_RIGHT);
						if (byte != EOF) {
							memcpy((char *)&pNewRight + s, &byte, sizeof(char));
						} else {
							pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to read data to free");
							goto error;
						}
					}
					if (!packedc_push_context(pstConverter, pstNConverter, DATA_CONVERTER_RIGHT)) {
						pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to make room for data conversion");
						goto error;
					}
					pstConverter->pRight = pNewRight;
					pstMemConverter->rightMaxSize = pstMemConverter->rightCurSize = 0;
				}
			} else {
				if (pstConverter->bHitKey) {
					if (pstConverter->pRight) pstConverter->pstAllocator->memfree(pstConverter->pstAllocator->pAllocator, pstConverter->pRight);
					if (!packedc_pop_context(pstConverter, pstNConverter, DATA_CONVERTER_RIGHT)) {
						pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to retrieve previously saved information for data conversion");
						goto error;
					}
				}
			}
			break;
		}
		case ARRAY:
		case STRUCT: break; // Nothin to do
		case STRING:
		{	pstNode->value.pcVal = (char *)pstConverter->pLeft + pstMemConverter->leftCurSize;
			if (pstConverter->bHitKey) {
				for (size_t s = 0; s < sizeof(char *); s++) {
					pstConverter->seek_byte(pstConverter, DATA_CONVERTER_RIGHT);
				}
			}
			int iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			while (iGet != EOF && (char)iGet != '\0') {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			}
			break;
		}
		case CHAR_FORMAT:
		case INT_FORMAT:
		case LONG_FORMAT:
		case SIZE_T_FORMAT:
		case FLOAT_FORMAT:
		case DOUBLE_FORMAT:
		case POINTER:
		{	int iGet = 0, iPut = 0;
			for (size_t s = 0; s < pstNode->size; s++) {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
				if (iGet == EOF) break;
				else {
					if (pstConverter->bHitKey) pstConverter->seek_byte(pstConverter, DATA_CONVERTER_RIGHT);
					memcpy((char *)&pstNode->value + s, &iGet, sizeof(char));
				}
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			} else if (iPut != iGet) {
				pstMemConverter->iErr = iPut;
				goto error;
			}
			break;
		}
		default:
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = NO_IMPLEMENTATION, "There is no implementation found for converting data type: %d", pstNode->nodeType);
			goto error;
	}
error:
	return pstMemConverter->iErr;
}
static int c_to_packedc (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, struct _FORMAT_TYPE_NODE * pstNode, bool bStart) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	switch (pstNode->nodeType) {
		case VARRAY:
		{	if (bStart) {
				void * pNewStart = NULL;
				for (size_t s = 0; s < sizeof(void *); s++) {
					int iByte = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
					if (iByte == EOF) {
						goto error;
					}
					memcpy((char *)&pNewStart + s, &iByte, sizeof(char));
				} // read_byte to advance the input position
				if (!packedc_push_context(pstConverter, pstNConverter, DATA_CONVERTER_LEFT)) {
					pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to make room for data conversion");
					goto error;
				}
				pstConverter->pLeft = pNewStart; // Replace the context with a new one
				pstMemConverter->leftMaxSize = pstMemConverter->leftCurSize = 0;
			} else {
				if (!packedc_pop_context(pstConverter, pstNConverter, DATA_CONVERTER_LEFT)) {
					pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = MALLOC_NO_MEM, "Failed to retrieve previously saved information for data conversion");
					goto error;
				}
			}
			break;
		}
		case ARRAY:
		case STRUCT: break; // Nothin to do
		case STRING:
		{	pstNode->value.pcVal = NULL;
			for (size_t s = 0; s < sizeof(char *); s++) { /* copy the pointer */
				int iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
				memcpy((char *)&pstNode->value.pcVal + s, &iGet, sizeof(char));
			}
			int iPut = 0;
			for (size_t s = 0; pstNode->value.pcVal[s] != '\0'; s++) {
				if (pstConverter->bHitKey && (iPut = pstConverter->write_byte(pstConverter, pstNode->value.pcVal[s], DATA_CONVERTER_RIGHT)) != pstNode->value.pcVal[s]) {
					pstMemConverter->iErr = iPut;
					goto error;
				}
			}
			if (pstConverter->bHitKey && pstConverter->write_byte(pstConverter, '\0', DATA_CONVERTER_RIGHT) != '\0') {
				pstMemConverter->iErr = iPut;
				goto error;
			}
			break;
		}
		case CHAR_FORMAT:
		case INT_FORMAT:
		case LONG_FORMAT:
		case SIZE_T_FORMAT:
		case FLOAT_FORMAT:
		case DOUBLE_FORMAT:
		case POINTER:
		{	int iGet = 0, iPut = 0;
			for (size_t s = 0; s < pstNode->size; s++) {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
				if (iGet == EOF) break;
				else {
					if (pstConverter->bHitKey && (iPut = pstConverter->write_byte(pstConverter, (char)iGet, DATA_CONVERTER_RIGHT)) != iGet) break;
					memcpy((char *)&pstNode->value + s, &iGet, sizeof(char));
				}
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			} else if (iPut != iGet) {
				pstMemConverter->iErr = iPut;
				goto error;
			}
			break;
		}
		default:
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = NO_IMPLEMENTATION, "There is no implementation found for converting data type: %d", pstNode->nodeType);
			goto error;
	}
error:
	return pstMemConverter->iErr;
}
static int packedc_compare_packedc (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, struct _FORMAT_TYPE_NODE * pstLeftNode, struct _FORMAT_TYPE_NODE * pstRightNode, bool bStart) {
	struct _MEM_CONVERTER * pstMemConverter = (struct _MEM_CONVERTER *)pstConverter->pConverter;
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	switch (pstLeftNode->nodeType) {
		case VARRAY:
		case ARRAY:
		case STRUCT: break; // Nothing to do
		case STRING:
		{	pstLeftNode->value.pcVal = (char *)pstConverter->pLeft + pstMemConverter->leftCurSize;
			pstRightNode->value.pcVal = (char *)pstConverter->pRight + pstMemConverter->rightCurSize;
			if (pstConverter->bHitKey) {
				if (pstMemConverter->iErr = strcmp(pstLeftNode->value.pcVal, pstRightNode->value.pcVal))
					goto error;
			}
			int iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			while (iGet != EOF && (char)iGet != '\0') {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			}
			iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_RIGHT);
			while (iGet != EOF && (char)iGet != '\0') {
				iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_RIGHT);
			}
			if (iGet == EOF) {
				pstMemConverter->iErr = iGet;
				goto error;
			}
			break;
		}
		case CHAR_FORMAT:
		case INT_FORMAT:
		case LONG_FORMAT:
		case SIZE_T_FORMAT:
		case POINTER:
		case FLOAT_FORMAT:
		case DOUBLE_FORMAT:
		//{	int iGet = 0, iPut = 0;
		//	for (size_t s = 0; s < pstNode->size; s++) {
		//		iGet = pstConverter->read_byte(pstConverter, DATA_CONVERTER_LEFT);
		//		if (iGet == EOF) break;
		//		else {
		//			if (pstConverter->bHitKey && (iPut = pstConverter->write_byte(pstConverter, (char)iGet, DATA_CONVERTER_RIGHT)) != iGet) break;
		//			memcpy((char *)&pstNode->value + s, &iGet, sizeof(char));
		//		}
		//	}
		//	if (iGet == EOF) {
		//		pstMemConverter->iErr = iGet;
		//		goto error;
		//	} else if (iPut != iGet) {
		//		pstMemConverter->iErr = iPut;
		//		goto error;
		//	}
		//	break;
		//}
			break;
		default:
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, pstMemConverter->iErr = NO_IMPLEMENTATION, "There is no implementation found for comparing data type: %d", pstLeftNode->nodeType);
			goto error;
	}
error:
	return pstMemConverter->iErr;
}
static void * node_packedc_create (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, enum _FORMAT_TYPE enType) {
	bool bErr = false;
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)(pstNConverter->pConverter = malloc(sizeof(struct _PACKEDC_NODE_CONVERTER)));
	if (!pstNodeConverter) {
		pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, MALLOC_NO_MEM, "Failed to allocate memory for packed C data conversion");
		bErr = true;
		goto error;
	} else {
		memset(pstNodeConverter, 0, sizeof(struct _PACKEDC_NODE_CONVERTER));
	}
	pstNodeConverter->pAddressStack = NULL;
	switch (pstNConverter->enType) {
		case C_TO_PACKEDC:
			pstNConverter->convert_node = c_to_packedc; break;
		case PACKEDC_TO_C:
			pstNConverter->convert_node = packedc_to_c; break;
		case PACKEDC_TO_C_FREE:
			pstNConverter->convert_node = packedc_to_freed_c; break;
		case PACKEDC_COMPARE_PACKEDC:
			pstNConverter->compare_node = packedc_compare_packedc; break;
		default:
			pstConverter->pstError->handle_error(pstConverter->pstError->pHandler, NO_IMPLEMENTATION, "This conversion type is not implemented by packed C");
			bErr = true;
			goto error;
	}
	if (!__list_create(&pstNodeConverter->pAddressStack, ARRAY_LIST_FLAT, NULL, NULL)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr) {
		if (pstNodeConverter) {
			if (pstNodeConverter->pAddressStack) __list_destroy(pstNodeConverter->pAddressStack);
			free (pstNodeConverter), pstNConverter->pConverter = pstNodeConverter = NULL;
		}
	}
	return pstNodeConverter;
}
static void node_packedc_destroy (struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter) {
	struct _PACKEDC_NODE_CONVERTER * pstNodeConverter = (struct _PACKEDC_NODE_CONVERTER *)pstNConverter->pConverter;
	if (pstNodeConverter) {
		if (pstNodeConverter->pAddressStack) __list_destroy(pstNodeConverter->pAddressStack), pstNodeConverter->pAddressStack = NULL;
		free(pstNConverter->pConverter), pstNConverter->pConverter = pstNodeConverter = NULL;
	}
}
void __node_converter_packedc_set_func_ptr(struct _DATA_CONVERTER * pstConverter, struct _NODE_CONVERTER * pstNConverter, enum _FORMAT_TYPE enType) {
	pstNConverter->create = node_packedc_create;
	pstNConverter->destroy = node_packedc_destroy;
	pstNConverter->enType = enType;
}