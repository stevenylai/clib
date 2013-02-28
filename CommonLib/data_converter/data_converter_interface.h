#ifndef __DATA_CONVERTER_INTERFACE_H__
#define __DATA_CONVERTER_INTERFACE_H__
#include "error/error.h"
#include "data_structure/data_structure_interface.h"
typedef enum _CONVERTER_TYPE {
	MEMORY_TO_MEMORY
} CONVERT_TYPE;
typedef enum _FORMAT_TYPE {
	/*C_TO_MULTISTRING,
	MULTISTRING_TO_C,
	MULTISTRING_COMPARE,*/
	C_TO_PACKEDC,
	PACKEDC_TO_C,
	PACKEDC_TO_C_FREE,
	PACKEDC_COMPARE_PACKEDC,
	PACKEDC_COMPARE_PARTIALC
} FORMAT_TYPE;
typedef struct _DATA_CONVERT_INFO { // Input to the APIs
	const char * pcKey;
	void * pLeft;
	void * pRight;
	size_t size;
	int iErr;
} DATA_CONVERT_INFO;
void * __data_converter_create(void ** pConverter, enum _CONVERTER_TYPE enType, const char * pcFormat, struct _ERROR_HANDLER * pstError);
void * __data_converter_convert(void * pConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator);
int __data_converter_compare(void * pConverter, enum _FORMAT_TYPE enType, struct _DATA_CONVERT_INFO * pstData, struct _ALLOCATOR * pstAllocator);
void __data_converter_destroy(void * pConverter);
#endif