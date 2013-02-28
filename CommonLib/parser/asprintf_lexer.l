%option nounistd
%option never-interactive
%option prefix="asprintf_"
%option reentrant
/*%option bison-bridge*/
%x GET_FORMATTER
%x OUTPUT_STRING
%{
#include "detect_mem_leak.h"
#include <stdarg.h>
#include "error/error.h"
#include "parser_internal.h"
#define YY_EXTRA_TYPE  struct _ASPRINTF_PARSER_DATA *
#define MIN_REALLOC_BLOCK_SIZE 16
#define YY_INPUT(buf,result,max_size) { \
	if (yyextra->pcCurInput - yyextra->pcInput >= strlen(yyextra->pcInput)) { \
		result = YY_NULL; \
	} else if (strlen(yyextra->pcCurInput) <= max_size) { \
		strncpy(buf, yyextra->pcCurInput, strlen(yyextra->pcCurInput)); \
		result = strlen(yyextra->pcCurInput); \
		yyextra->pcCurInput += result; \
	} else { \
		strncpy(buf, yyextra->pcCurInput, max_size); \
		result = max_size; \
		yyextra->pcCurInput += max_size; \
	} \
}
#define YY_FATAL_ERROR(msg) { struct yyguts_t * yyg = (struct yyguts_t*)yyscanner; longjmp(yyextra->pFatalError, 1); }
#define CARRIAGE "\r\n"
static void asprintf_error(struct _ASPRINTF_PARSER_DATA * pstData, void * scanner, const char *s) {
	pstData->bSyntaxError = true;
}
static void asprintf_cleanup_data(struct _ASPRINTF_PARSER_DATA * pData) {
	free(pData->pcFormatter), pData->pcFormatter  = NULL;
}
static void asprintf_ensure_output_size(struct _ASPRINTF_PARSER_DATA * pData, size_t iExtraSize) {
	size_t curSize = pData->pcCurOutput - pData->pcOutput;
	if (curSize + iExtraSize > pData->iOutputLen) { // Increase the size at least by MIN_REALLOC_BLOCK_SIZE
		size_t newSize = pData->iOutputLen ? pData->iOutputLen * 2 : MIN_REALLOC_BLOCK_SIZE;
		while (newSize < curSize + iExtraSize) newSize *= 2;
		char * pcNewOutput = (char *)realloc(pData->pcOutput, newSize);
		if (pcNewOutput) {
			pData->pcOutput = pcNewOutput;
			pData->iOutputLen = newSize;
			pData->pcCurOutput = pData->pcOutput + curSize;
		} else {
			free (pData->pcOutput), pData->pcOutput = NULL;
			pData->pcCurOutput = NULL;
			pData->iOutputLen = 0;
		}
	}
}
static void asprintf_alloc_formatter(struct _ASPRINTF_PARSER_DATA * pData, char * pcFormat) {
	size_t curSize = pData->pcFormatter == NULL ? 0 : strlen(pData->pcFormatter) + 1;
	if (strlen(pcFormat) + 1 > curSize) {
		char * pcNewFormatter = (char *)realloc(pData->pcFormatter, strlen(pcFormat) + 1);
		if (pcNewFormatter) {
			pData->pcFormatter = pcNewFormatter;
		} else if (pData->pcFormatter) {
			free (pData->pcFormatter), pData->pcFormatter = NULL;
		}
	}
}
%}
FLAG		[+\- #0]
WIDTH		[0-9]+
PRECISION	\.[0-9]+
LENGTH		h{1,2}|l{1,2}|L|z|j|t|I|I32|I64|q
TYPE		d|i|u|f|F|e|E|g|G|x|X|o|s|c|p|n

%%
<INITIAL>"%%"	{	yyextra->charCount += yyleng;
					asprintf_ensure_output_size(yyextra, strlen("%"));
					if (yyextra->pcCurOutput) {
						*(yyextra->pcCurOutput++) = '%';
					} else {
						return MALLOC_NO_MEM;
					}
				}
<INITIAL>"%"	{	BEGIN(GET_FORMATTER);
					unput('%');
				}
<INITIAL>\n		{	yyextra->charCount += yyleng;
					asprintf_ensure_output_size(yyextra, strlen(CARRIAGE));
					if (yyextra->pcCurOutput) {
						const char * pcCarriage = CARRIAGE;
						for (unsigned int i = 0; i < strlen(CARRIAGE); i++) {
							*(yyextra->pcCurOutput++) = pcCarriage[i];
						}
					} else {
						return MALLOC_NO_MEM;
					}
				}
<INITIAL>.		{	yyextra->charCount += yyleng;
					asprintf_ensure_output_size(yyextra, yyleng);
					if (yyextra->pcCurOutput) {
						*(yyextra->pcCurOutput++) = yytext[0];
					} else {
						return MALLOC_NO_MEM;
					}
				}

<GET_FORMATTER>"%"({FLAG}?{WIDTH}?{PRECISION}?{LENGTH}?{TYPE})	{	yyextra->uiArgCount++;
																	yyextra->charCount++;
																	asprintf_alloc_formatter(yyextra, yytext);
																	if (yyextra->pcFormatter) {
																		strcpy(yyextra->pcFormatter, yytext);
																		for ( int i = yyleng - 1; i >= 1; --i )
																			unput( yyextra->pcFormatter[i] );
																		BEGIN(OUTPUT_STRING);
																	} else {
																		return MALLOC_NO_MEM;
																	}
																}
<GET_FORMATTER>.			{ yyextra->charCount += yyleng; asprintf_error(yyextra, NULL, "syntax error: unexpected character"); return ASPRINTF_SYNTAX_ERROR; }

<OUTPUT_STRING>{FLAG}		{	yyextra->charCount += yyleng; }
<OUTPUT_STRING>{WIDTH}		{	yyextra->bWidth = true; sscanf(yytext, "%d", &(yyextra->width)); if (yyextra->width > ASPRINTF_MAX_WIDTH) yyextra->width = ASPRINTF_MAX_WIDTH; yyextra->charCount += yyleng; }
<OUTPUT_STRING>{PRECISION}	{	yyextra->bPrecision = true; sscanf(yytext + 1, "%d", &(yyextra->precision)); if (yyextra->precision > ASPRINTF_MAX_PRECISION) yyextra->precision = ASPRINTF_MAX_PRECISION; yyextra->charCount += yyleng; }
<OUTPUT_STRING>{LENGTH}		{	yyextra->charCount += yyleng; }
<OUTPUT_STRING>{TYPE}		{	yyextra->charCount += yyleng;
								if (strcmp(yytext, "s") == 0) {
									char *pcArg = va_arg (yyextra->args, char *);
									size_t strLen = strlen(pcArg);
									if (yyextra->bPrecision) {
										strLen = yyextra->precision + 1 < strLen ? yyextra->precision + 1 : strLen;
									}
									asprintf_ensure_output_size(yyextra, strLen);
									if (yyextra->pcCurOutput) {
										strncpy(yyextra->pcCurOutput, pcArg, strLen);
										yyextra->pcCurOutput += strLen;
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "c") == 0) {
									char cArg = va_arg (yyextra->args, char);
									asprintf_ensure_output_size(yyextra, sizeof(cArg));
									if (yyextra->pcCurOutput) {
										*(yyextra->pcCurOutput++) = cArg;
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "f") == 0 || strcmp(yytext, "F") == 0 || strcmp(yytext, "e") == 0 || strcmp(yytext, "E") == 0 || strcmp(yytext, "g") == 0 || strcmp(yytext, "G") == 0) {
									double dbArg = va_arg (yyextra->args, double);
									size_t length = (63 + strlen("."));
									if (yyextra->bPrecision) {
										length += yyextra->precision;
									} else {
										length += 64;
									}
									if (yyextra->bWidth && yyextra->width > length) {
										length = yyextra->width + 1;
									}
									asprintf_ensure_output_size(yyextra, length + 1);
									if (yyextra->pcCurOutput) {
										sprintf(yyextra->pcCurOutput, yyextra->pcFormatter, dbArg);
										yyextra->pcCurOutput = yyextra->pcCurOutput + strlen(yyextra->pcCurOutput);
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "x") == 0 || strcmp(yytext, "X") == 0) {
									unsigned int uiArg = va_arg (yyextra->args, unsigned int);
									size_t length = 8 + 1;
									if (yyextra->bWidth && yyextra->width > length) {
										length = yyextra->width + 1;
									}
									asprintf_ensure_output_size(yyextra, length);
									if (yyextra->pcCurOutput) {
										sprintf(yyextra->pcCurOutput, yyextra->pcFormatter, uiArg);
										yyextra->pcCurOutput = yyextra->pcCurOutput + strlen(yyextra->pcCurOutput);
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "o") == 0) {
									unsigned int uiArg = va_arg (yyextra->args, unsigned int);
									size_t length = 16 + 1;
									if (yyextra->bWidth && yyextra->width > length) {
										length = yyextra->width + 1;
									}
									asprintf_ensure_output_size(yyextra, length);
									if (yyextra->pcCurOutput) {
										sprintf(yyextra->pcCurOutput, yyextra->pcFormatter, uiArg);
										yyextra->pcCurOutput = yyextra->pcCurOutput + strlen(yyextra->pcCurOutput);
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "u") == 0) {
									unsigned int uiArg = va_arg (yyextra->args, unsigned int);
									size_t length = 16 + 1;
									if (yyextra->bWidth && yyextra->width > length) {
										length = yyextra->width + 1;
									}
									asprintf_ensure_output_size(yyextra, length);
									if (yyextra->pcCurOutput) {
										sprintf(yyextra->pcCurOutput, yyextra->pcFormatter, uiArg);
										yyextra->pcCurOutput = yyextra->pcCurOutput + strlen(yyextra->pcCurOutput);
									} else {
										return MALLOC_NO_MEM;
									}
								} else if (strcmp(yytext, "d") == 0 || strcmp(yytext, "i") == 0) {
									int iArg = va_arg (yyextra->args, int);
									size_t length = 16 + 1;
									if (yyextra->bWidth && yyextra->width > length) {
										length = yyextra->width + 1;
									}
									asprintf_ensure_output_size(yyextra, length);
									if (yyextra->pcCurOutput) {
										sprintf(yyextra->pcCurOutput, yyextra->pcFormatter, iArg);
										yyextra->pcCurOutput = yyextra->pcCurOutput + strlen(yyextra->pcCurOutput);
									} else {
										return MALLOC_NO_MEM;
									}
								}
								yyextra->bWidth = yyextra->bPrecision = false;
								BEGIN(INITIAL);
							}
<OUTPUT_STRING>.		{ yyextra->charCount += yyleng; asprintf_error(yyextra, NULL, "syntax error: unexpected character"); return ASPRINTF_SYNTAX_ERROR; }

%%
static int asprintf_wrap (void * scanner) {
	return 1;
}
char * __vasprintf(char * pcFormatter, va_list args) {
	int iErr = SUCCESS;
	struct _ASPRINTF_PARSER_DATA stPrintfData =	{0,				//charCount
												pcFormatter,	//pcInput
												pcFormatter,	//pcCurInput
												args,			//args
												0,				//uiArgCount
												NULL,			//pcFormatter
												NULL,			//pcOutput
												NULL,			//pcCurOutput
												0,				//iOutputLen
												false,			//bPrecision
												0,				//precision
												false,			//bWidth
												0,				//width
												false};			//bSyntaxError
																//pFatalError (not init'd)
	

	void * scanner = NULL;
	if (setjmp(stPrintfData.pFatalError)) {
		if (scanner){
			asprintf_lex_destroy(scanner), scanner = NULL;
		}
		asprintf_cleanup_data(&stPrintfData);
		free(stPrintfData.pcOutput), stPrintfData.pcOutput = NULL;
		return NULL;
	}
	
	asprintf_lex_init(&scanner);
	if (scanner == NULL) return NULL;
		
	asprintf_set_extra (&stPrintfData, scanner);
	iErr = asprintf_lex(scanner);
	asprintf_lex_destroy(scanner), scanner = NULL;
	asprintf_cleanup_data(&stPrintfData);
	
	if (iErr != SUCCESS) {
		// Error handling
	}
	asprintf_ensure_output_size(&stPrintfData, sizeof('\0'));
	if (stPrintfData.pcCurOutput) {
		*(stPrintfData.pcCurOutput++) = '\0';
	}
	return stPrintfData.pcOutput;
}
char * __asprintf(char * pcFormatter, ...) {
	char * pcBuffer = NULL;
	va_list args;
	va_start(args, pcFormatter);
	pcBuffer = __vasprintf(pcFormatter, args);
	va_end(args);
	return pcBuffer;
}
