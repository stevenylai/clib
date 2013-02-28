#ifndef __PARSER_INTERNAL_H__
#define __PARSER_INTERNAL_H__
#include <setjmp.h>
#include "parser_interface.h"
#define ASPRINTF_MAX_WIDTH 16777216
#define ASPRINTF_MAX_PRECISION ASPRINTF_MAX_WIDTH
typedef struct _ASPRINTF_PARSER_DATA {
	// General
	unsigned int charCount;
	// Input
	const char * pcInput;
	const char * pcCurInput;
	va_list args;
	unsigned int uiArgCount;
	// Output
	char * pcFormatter;
	char * pcOutput;
	char * pcCurOutput;
	size_t iOutputLen;
	// Actual result used
	bool bPrecision;
	size_t precision;
	bool bWidth;
	size_t width;
	// Error handling
	bool bSyntaxError;
	jmp_buf pFatalError;
	// Add data structures for other sections here
} ASPRINTF_PARSER_DATA;
#endif