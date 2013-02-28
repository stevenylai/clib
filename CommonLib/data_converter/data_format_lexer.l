%option nounistd
%option never-interactive
%option prefix="data_format_"
%option reentrant
%option bison-bridge
%option yylineno
%{
#include "detect_mem_leak.h"
#include <setjmp.h>
#include "error/error.h"
#include "data_structure/data_structure_interface.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
#define YY_EXTRA_TYPE  struct _FORMAT_PARSER_DATA *
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
void data_format_error(struct _FORMAT_PARSER_DATA * pstData, void * yyscanner, const char *s);
/*extern*/ char * __asprintf(char * pcFormatter, ...);
/*void data_format_free (void * ptr, void * scanner) {
	free(ptr);
}
void * data_format_realloc (void * ptr, size_t size, void * scanner) {
	return realloc(ptr, size);
}
void * data_format_malloc (size_t size, void * scanner) {
	return malloc(size);
}*/
%}
NAME			[A-Za-z_][A-Za-z0-9_]*
NUM_HEX			0x[a-fA-F0-9]+
NUM_OCT			0[0-7]+
NUM_DEC			[-]?[1-9][0-9]*
NUM_FLOAT		[-]?[0-9]*\.[0-9]+
STRING_LITERAL	\"(\\\"|[^\\"])*\"

%%
"void"				{ yyextra->numChar += yyleng; return VOID_FORMAT; }
"string"			{ yyextra->numChar += yyleng; yylval->uiVal = STRING; return STRING; }
"char"				{ yyextra->numChar += yyleng; yylval->uiVal = CHAR_FORMAT; return CHAR_FORMAT; }
"int"				{ yyextra->numChar += yyleng; yylval->uiVal = INT_FORMAT; return INT_FORMAT; }
"long"				{ yyextra->numChar += yyleng; yylval->uiVal = LONG_FORMAT; return LONG_FORMAT; }
"size_t"			{ yyextra->numChar += yyleng; yylval->uiVal = SIZE_T_FORMAT; return SIZE_T_FORMAT; }
"unsigned"			{ yyextra->numChar += yyleng; return UNSIGNED_FORMAT; }
"float"				{ yyextra->numChar += yyleng; yylval->uiVal = FLOAT_FORMAT; return FLOAT_FORMAT; }
"double"			{ yyextra->numChar += yyleng; yylval->uiVal = DOUBLE_FORMAT; return DOUBLE_FORMAT; }
"array"				{ yyextra->numChar += yyleng; yylval->uiVal = ARRAY; return ARRAY; }
"varray"			{ yyextra->numChar += yyleng; yylval->uiVal = VARRAY; return VARRAY; }
"struct"			{ yyextra->numChar += yyleng; yylval->uiVal = STRUCT; return STRUCT; }
{NAME}				{ yyextra->numChar += yyleng; yylval->pcVal = __asprintf("%s", yytext); return NAME; }
{NUM_HEX}			{ yyextra->numChar += yyleng; sscanf(yytext + 2, "%x", &(yylval->uiVal)); return NUM_HEX; }
{NUM_OCT}			{ yyextra->numChar += yyleng; sscanf(yytext + 1, "%o", &(yylval->uiVal)); return NUM_OCT; }
{NUM_DEC}			{ yyextra->numChar += yyleng; sscanf(yytext, "%d", &(yylval->uiVal)); return NUM_DEC; }
{NUM_FLOAT}			{ yyextra->numChar += yyleng; sscanf(yytext, "%f", &(yylval->dbVal)); return NUM_FLOAT; }
{STRING_LITERAL}	{ yyextra->numChar += yyleng; return STRING_LITERAL; }
[ \t\n]				{ yyextra->numChar += yyleng; }
[{};*=\[\]]			{ yyextra->numChar += yyleng; return yytext[0]; }
.					{ data_format_error(yyextra, NULL, "Unrecognized character"); yyextra->numChar += yyleng; }

%%
int data_format_wrap (void * scanner) {
	return 1;
}
void data_format_error(struct _FORMAT_PARSER_DATA * pstData, void * scanner, const char *s) {
	struct yyguts_t * yyg = NULL;
	if (scanner) {
		yyg = (struct yyguts_t*)scanner;
	}
	if (yyg)
		pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "%s at: line: %d, column: %d, character %d", s, yylineno, yycolumn, pstData->numChar);
	else
		pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "%s at: character %d", s, pstData->numChar);
}
