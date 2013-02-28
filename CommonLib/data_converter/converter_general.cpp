#include "detect_mem_leak.h"
#include <string.h>
#include "error/error.h"
#include "data_structure/data_structure_interface.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
static void converter_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, converter_default_error};
void * __data_converter_create(void ** pConverter, enum _CONVERTER_TYPE enType, const char * pcFormat, struct _ERROR_HANDLER * pstError) {
	bool bErr = false;
	if (!pstError) pstError = &g_stDefaultError;
	struct _DATA_CONVERTER * pstConverter = (struct _DATA_CONVERTER *)(*pConverter = malloc(sizeof(struct _DATA_CONVERTER)));
	if (!pstConverter) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory to create data converter");
		bErr = true;
		goto error;
	} else {
		memset(pstConverter, 0, sizeof(struct _DATA_CONVERTER));
	}
	switch (pstConverter->enType = enType) {
		case MEMORY_TO_MEMORY:
			__mem_converter_set_func_ptr(pstConverter, enType);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Such data converter has no implementation");
			bErr = true;
			goto error;
	}
	if (!pstConverter->create(pstConverter, pcFormat, pstError)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr && *pConverter) free (*pConverter), *pConverter = pstConverter = NULL;
	return *pConverter;
}
void * __data_converter_convert(void * pConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator) {
	struct _DATA_CONVERTER * pstConverter = (struct _DATA_CONVERTER *)pConverter;
	return pstConverter->convert (pstConverter, enType, pstData, pstAllocator);
}
int __data_converter_compare(void * pConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator) {
	struct _DATA_CONVERTER * pstConverter = (struct _DATA_CONVERTER *)pConverter;
	return pstConverter->compare (pstConverter, enType, pstData, pstAllocator);
}
void __data_converter_destroy(void * pConverter) {
	struct _DATA_CONVERTER * pstConverter = (struct _DATA_CONVERTER *)pConverter;
	pstConverter->destroy(pstConverter);
	free(pConverter);
}