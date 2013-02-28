%pure-parser
%name-prefix "data_format_"
%parse-param {struct _FORMAT_PARSER_DATA * pstData}
%parse-param {void * scanner}
%lex-param {void * scanner}
%{
#include "detect_mem_leak.h"
#include <stdio.h>
#include <string.h>
#include "error/error.h"
#include "data_structure/data_structure_interface.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
int data_format_lex_init (void ** scanner);
int data_format_lex_destroy (void * scanner);
void data_format_set_extra (struct _FORMAT_PARSER_DATA * pstData, void * scanner);
void data_format_set_in  (FILE * in_str , void * scanner);
data_format_lex (YYSTYPE * tun_lval_param , void * scanner);
void data_format_free (void * ptr, void * scanner);
void * data_format_realloc (void * ptr, size_t size, void * scanner);
void * data_format_malloc (size_t size, void * scanner);
void data_format_error(struct _FORMAT_PARSER_DATA * pstData, void * scanner, const char *s);
static const char * format_node_type_string (struct _FORMAT_TYPE_NODE * pstNode) {
	if (!pstNode) return "null";
	switch (pstNode->nodeType) {
		case VOID_FORMAT: return "void";
		case STRING: return "string";
		case CHAR_FORMAT: return "char";
		case INT_FORMAT: return "int";
		case LONG_FORMAT: return "long";
		case SIZE_T_FORMAT: return "size_t";
		case FLOAT_FORMAT: return "float";
		case DOUBLE_FORMAT: return "double";
		case ARRAY: return "array";
		case VARRAY: return "varray";
		case STRUCT: return "struct";
		default: return "unknown";
	}
}
static const char * format_node_name_string (struct _FORMAT_TYPE_NODE * pstNode) {
	return pstNode->pcName ? pstNode->pcName : "NULL";
}
static bool format_node_initialized (struct _FORMAT_PARSER_DATA * pstData, struct _FORMAT_TYPE_NODE * pstNode, unsigned int nodeType) {
	bool bInitialized = true;
	pstNode->pcName = NULL;
	pstNode->bSigned = false;
	switch (pstNode->nodeType = nodeType) {
		case CHAR_FORMAT: pstNode->size = sizeof(char); break;
		case INT_FORMAT: pstNode->size = sizeof(int); break;
		case LONG_FORMAT: pstNode->size = sizeof(long); break;
		case SIZE_T_FORMAT: pstNode->size = sizeof(size_t); break;
		case FLOAT_FORMAT: pstNode->size = sizeof(float); break;
		case DOUBLE_FORMAT: pstNode->size = sizeof(double); break;
		case POINTER: pstNode->size = sizeof(void *); break;
		default: pstNode->size = 0; break;
	}
	switch (pstNode->nodeType) {
		case VOID_FORMAT: // Root
		case ARRAY: 
		case VARRAY:
		case STRUCT:
			pstNode->pSubNodeList = pstNode->pCurrentTable = NULL;
			if (!__list_create(&pstNode->pCurrentTable, LINKED_LIST_POINTER, NULL, NULL)) { // FIXME: use binary search tree instead
				pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = FORMAT_TREE_CREATION_ERROR, "Failed to create packer symbol table");
				bInitialized = false;
			}
			if (bInitialized && !__list_create(&pstNode->pSubNodeList, LINKED_LIST_POINTER, NULL, NULL)) {
				pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = FORMAT_TREE_CREATION_ERROR, "Failed to create packer sub node list");
				bInitialized = false;
			}
			break;
		default: pstNode->pSubNodeList = pstNode->pCurrentTable = NULL; break;
	}
	switch (pstNode->nodeType) {
		case CHAR_FORMAT:
		case INT_FORMAT:
		case LONG_FORMAT:
		case SIZE_T_FORMAT: pstNode->value.uiVal = 0; break;
		case STRING: pstNode->value.pcVal = NULL; break;
		case FLOAT_FORMAT:
		case DOUBLE_FORMAT: pstNode->value.dbVal = 0.0; break;
		default: break;
	}
	pstNode->count = 1, pstNode->pstParent = NULL;
	return bInitialized;
}
static struct _FORMAT_TYPE_NODE * format_add_subnode_tree (struct _FORMAT_PARSER_DATA * pstData, struct _FORMAT_TYPE_NODE * pstParentNode, struct _FORMAT_TYPE_NODE * pstChildNode) {
	pstChildNode->pstParent = pstParentNode;
	struct _FORMAT_TYPE_NODE * pstNode = (struct _FORMAT_TYPE_NODE *)__list_insert(pstParentNode->pSubNodeList, __list_count(pstParentNode->pSubNodeList), sizeof(struct _FORMAT_TYPE_NODE), pstChildNode);
	if (!pstNode) {
		pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = FORMAT_TREE_CREATION_ERROR, "Failed to create a subnode in the syntax tree");
	}
	return pstNode;
}
static void format_destroy_type_node (struct _FORMAT_TYPE_NODE * pstNode) {
	if (pstNode->pcName) free(pstNode->pcName), pstNode->pcName = NULL;
	//if (pstNode->nodeType == STRING && pstNode->value.pcVal) free(pstNode->value.pcVal), pstNode->value.pcVal = NULL;
	if (pstNode->pSubNodeList) __list_destroy(pstNode->pSubNodeList), pstNode->pSubNodeList = NULL;
	if (pstNode->pCurrentTable) __list_destroy(pstNode->pCurrentTable), pstNode->pCurrentTable = NULL;
}
static bool format_symbol_added (struct _FORMAT_PARSER_DATA * pstData, struct _FORMAT_TYPE_NODE * pstNode, char * pcName, void * pData) {
	void * pNewSymbol = NULL;
	bool bAdded = true;
	if (format_retrieve_symbol(pstNode, pcName)) {
		pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "Symbol %s is already defined previously", pcName);
		bAdded = false;
		goto error;
	}
	pNewSymbol = malloc(strlen(pcName) + 1 + sizeof(struct _FORMAT_TYPE_NODE *));
	if (!pNewSymbol) {
		pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = FORMAT_TREE_CREATION_ERROR, "Failed to allocate space for symbol %s", pcName);
		bAdded = false;
		goto error;
	}
	strcpy((char *)pNewSymbol, pcName);
	memcpy((char *)pNewSymbol + strlen(pcName) + 1, &pData, sizeof(struct _FORMAT_TYPE_NODE *));
	for (struct _FORMAT_TYPE_NODE * pstNodeWithTable = pstNode; pstNodeWithTable->pstParent; pstNodeWithTable = pstNodeWithTable->pstParent) {
		if (pstNodeWithTable->pCurrentTable) {
			if (!__list_insert(pstNodeWithTable->pCurrentTable, 0, strlen(pcName) + 1 + sizeof(struct _FORMAT_TYPE_NODE *), pNewSymbol)) {
				if (pNewSymbol) free(pNewSymbol), pNewSymbol = NULL;
				pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = FORMAT_TREE_CREATION_ERROR, "Failed to insert symbol %s to symbol table", pcName);
				bAdded = false;
				goto error;
			}
			__data_converter_debug_printf("Added %s to symbol table. Type: %s. Name: %s. Parent type: %s. Parent name: %s, address: 0x%X\n",
				pcName, format_node_type_string((struct _FORMAT_TYPE_NODE *)pData), format_node_name_string((struct _FORMAT_TYPE_NODE *)pData), format_node_type_string(((struct _FORMAT_TYPE_NODE *)pData)->pstParent), format_node_name_string(((struct _FORMAT_TYPE_NODE *)pData)->pstParent), pData);
			goto error;
		}
	}
	if (pNewSymbol) free(pNewSymbol), pNewSymbol = NULL;
	pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "Failed to find any symbol table for ID: %s", pcName);
	bAdded = false;
error:
	if (pNewSymbol) free(pNewSymbol), pNewSymbol = NULL;
	return bAdded;
}
static bool format_node_is_int (struct _FORMAT_TYPE_NODE * pstNode) {
	switch (pstNode->nodeType) {
		case CHAR_FORMAT: 
		case INT_FORMAT: 
		case LONG_FORMAT:
		case SIZE_T_FORMAT: return true;
		default: return false;
	}
}
static void format_debug_traverse_type_tree (struct _FORMAT_TYPE_NODE * pstNode) {
	__data_converter_debug_printf("Node type: %s, name: %s, parent type: %s\n", format_node_type_string(pstNode), format_node_name_string(pstNode), format_node_type_string(pstNode->pstParent));
	if (!pstNode->pSubNodeList) return;
	for (__list_start_iterator(pstNode->pSubNodeList, 0); __list_has_next(pstNode->pSubNodeList);) {
		struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE *)__list_next(pstNode->pSubNodeList, NULL, NULL);
		format_debug_traverse_type_tree (pstSubNode);
	}
}
%}

%union {
	unsigned int uiVal;
	double dbVal;
	char * pcVal;
	struct _FORMAT_TYPE_NODE * pstNode;
}
%token <uiVal> NUM_DEC NUM_OCT NUM_HEX
%token <uiVal> VOID_FORMAT STRING CHAR_FORMAT INT_FORMAT LONG_FORMAT SIZE_T_FORMAT FLOAT_FORMAT DOUBLE_FORMAT POINTER ARRAY VARRAY STRUCT
%token <dbVal> NUM_FLOAT
%token <pcVal> NAME STRING_LITERAL
%token UNSIGNED_FORMAT

%type <uiVal> int_value int_type_name_no_sign pointer_type_name
%type <pstNode> type_name primitive_type_name int_type_name float_type_name composite_type_name array_type_name struct_type_name varray_type_name
%destructor { data_format_free($$, scanner); } <pcVal>

%%
type_def	:	type_decl ';'
	 	/*|	type_decl type_value ';'*/
			;
type_decl	:	type_name
     		|	type_name NAME		{	bool bAdded = ($NAME ? format_symbol_added(pstData, pstData->pstCurrentNode, $NAME, $type_name) : false);
     									if (bAdded) ((struct _FORMAT_TYPE_NODE *)$type_name)->pcName = $NAME;
     									else {
     										if ($NAME) free($NAME);
     										YYABORT;
     									}
     								}
			;
type_name	:	primitive_type_name	{ $$ = $1; }
     		|	composite_type_name	{ $$ = $1; }
			;
primitive_type_name	:	int_type_name			{	$$ = $1; }
	       			|	float_type_name			{	$$ = $1; }
					|	pointer_type_name		{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  										if (!format_node_initialized(pstData, &stNewNode, $1))YYABORT;
			  										pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pInsertedNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  										$$ = pInsertedNode;
			  									}
			  		|	CHAR_FORMAT				{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  										if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  										pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pInsertedNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  										$$ = pInsertedNode;
			  									}
					|	STRING					{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  										if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  										pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pInsertedNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  										$$ = pInsertedNode;
			  									}
					;
int_type_name		:	UNSIGNED_FORMAT int_type_name_no_sign	{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  														if (!format_node_initialized(pstData, &stNewNode, $2)) YYABORT;
			  														pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  														if (!pInsertedNode) {
			  															format_destroy_type_node(&stNewNode);
			  															YYABORT;
			  														}
			  														$$ = pInsertedNode;
			  													}
					|	int_type_name_no_sign					{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  														if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  														stNewNode.bSigned = true;
			  														pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  														if (!pInsertedNode) {
			  															format_destroy_type_node(&stNewNode);
			  															YYABORT;
			  														}
			  														$$ = pInsertedNode;
			  													}
			  		|	SIZE_T_FORMAT									{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  														if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  														pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  														if (!pInsertedNode) {
			  															format_destroy_type_node(&stNewNode);
			  															YYABORT;
			  														}
			  														$$ = pInsertedNode;
			  													}
					;
int_type_name_no_sign	:	INT_FORMAT		{ $$ = $1; }
				      	|	LONG_FORMAT		{ $$ = $1; }
						;
float_type_name		:	FLOAT_FORMAT		{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  									if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  									pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  									if (!pInsertedNode) {
			  										format_destroy_type_node(&stNewNode);
			  										YYABORT;
			  									}
			  									$$ = pInsertedNode;
			  								}
		 			|	DOUBLE_FORMAT		{	struct _FORMAT_TYPE_NODE stNewNode; struct _FORMAT_TYPE_NODE * pInsertedNode = NULL;
			  									if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  									pInsertedNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  									if (!pInsertedNode) {
			  										format_destroy_type_node(&stNewNode);
			  										YYABORT;
			  									}
			  									$$ = pInsertedNode;
			  								}
					;
pointer_type_name	:	int_type_name '*'	{ $$ = POINTER; }
				 	|	float_type_name '*' { $$ = POINTER; }
					|	CHAR_FORMAT '*'		{ $$ = POINTER; }
					|	VOID_FORMAT '*'		{ $$ = POINTER; }
					;
composite_type_name	:	array_type_name		{ $$ = $1; }
					|	struct_type_name	{ $$ = $1; }
					|	varray_type_name	{ $$ = $1; }
					;
struct_type_name	:	STRUCT					{	struct _FORMAT_TYPE_NODE stNewNode;
			  										if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  										pstData->pstCurrentNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pstData->pstCurrentNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  									}
						'{' type_def_list '}'	{ $$ = pstData->pstCurrentNode; pstData->pstCurrentNode = pstData->pstCurrentNode->pstParent; }
		 			;
array_type_name		:	ARRAY					{	struct _FORMAT_TYPE_NODE stNewNode;
			  										if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  										pstData->pstCurrentNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pstData->pstCurrentNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  									}
						'{' type_def_list '}'
						'[' int_value ']'		{ pstData->pstCurrentNode->count = $int_value; $$ = pstData->pstCurrentNode; pstData->pstCurrentNode = pstData->pstCurrentNode->pstParent; }
		 			;
varray_type_name	:	VARRAY					{	struct _FORMAT_TYPE_NODE stNewNode;
			  										if (!format_node_initialized(pstData, &stNewNode, $1)) YYABORT;
			  										pstData->pstCurrentNode = format_add_subnode_tree(pstData, pstData->pstCurrentNode, &stNewNode);
			  										if (!pstData->pstCurrentNode) {
			  											format_destroy_type_node(&stNewNode);
			  											YYABORT;
			  										}
			  									}
						'{' type_def_list '}'
						'[' NAME ']'			{	struct _FORMAT_TYPE_NODE * pstNode = $NAME ? (struct _FORMAT_TYPE_NODE *)format_retrieve_symbol(pstData->pstCurrentNode, $NAME) : NULL;
													if ($NAME) free ($NAME);
													if (!pstNode) {
														pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "Symbol not defiled");
														YYABORT;
													} else if (!format_node_is_int(pstNode)) {
														pstData->pstError->handle_error(pstData->pstError->pHandler, pstData->iErr = DATA_FORMAT_SYNTAX_ERROR, "Array index must be an int type");
														YYABORT;
													} else {
														pstData->pstCurrentNode->count = pstNode->value.uiVal;
														__list_insert(pstData->pstCurrentNode->pSubNodeList, __list_count(pstData->pstCurrentNode->pSubNodeList), sizeof(struct _FORMAT_TYPE_NODE), pstNode);
													}
													$$ = pstData->pstCurrentNode; pstData->pstCurrentNode = pstData->pstCurrentNode->pstParent;
												}
		 			;
type_def_list		:	type_def_list type_def
	       			|	type_def
					;

int_value		:	NUM_HEX	{ $$ = $1; }
	   			|	NUM_OCT	{ $$ = $1; }
				|	NUM_DEC	{ $$ = $1; }
				;
%%
void * format_retrieve_symbol (struct _FORMAT_TYPE_NODE * pstNode, char * pcName) {
	for (struct _FORMAT_TYPE_NODE * pstNodeWithTable = pstNode; pstNodeWithTable->pstParent; pstNodeWithTable = pstNodeWithTable->pstParent) {
		if (!pstNodeWithTable->pCurrentTable) continue;
		for (__list_start_iterator(pstNodeWithTable->pCurrentTable, 0); __list_has_next(pstNodeWithTable->pCurrentTable);) {
			char * pcExistingName = (char *)__list_next(pstNodeWithTable->pCurrentTable, NULL, NULL);
			if (pcExistingName && strcmp(pcExistingName, pcName) == 0) {
				__data_converter_debug_printf("Retrieved %s from symbol table. Address: 0x%X\n", pcName, *(void **)(pcExistingName + strlen(pcExistingName) + 1));
				return *(void **)(pcExistingName + strlen(pcExistingName) + 1);
			}
		}
	}
	return NULL;
}
void * format_create_type_tree (const char * pcFormat, struct _FORMAT_TYPE_NODE * pstRoot, struct _ERROR_HANDLER * pstError) {
	struct _FORMAT_PARSER_DATA stParserData = {	0,				//numChar
											pcFormat,		//pcInput
											pcFormat,		//pcCurInput
											pstRoot,		//pstRoot
											pstRoot,		//pstCurrentNode
											pstError,		//pstError
											SUCCESS		//iErr
											};
	struct _FORMAT_TYPE_NODE * pstResult = NULL;
	void * scanner = NULL;
	if (setjmp(stParserData.pFatalError)) {
		if (scanner) data_format_lex_destroy(scanner), scanner = NULL;
		pstError->handle_error(pstError->pHandler, stParserData.iErr = FATAL_CREATION_ERROR, "Fatal error in parsing the type string");
		goto error;
	}
	if (!format_node_initialized(&stParserData, stParserData.pstRoot, VOID_FORMAT)) {
		pstError->handle_error(pstError->pHandler, stParserData.iErr = FATAL_CREATION_ERROR, "Fatal error in creating syntax tree");
		goto error;
	}
	data_format_lex_init(&scanner);
	if (scanner == NULL) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Cannot create packer scanner");
		goto error;
	}
	data_format_set_extra (&stParserData, scanner);
	do {
		data_format_parse(&stParserData, scanner);
	} while (stParserData.pcCurInput - stParserData.pcInput < strlen(stParserData.pcInput));
	if (stParserData.iErr) goto error;
	format_debug_traverse_type_tree(stParserData.pstRoot);
	pstResult = stParserData.pstRoot;
error:
	if (scanner) data_format_lex_destroy(scanner), scanner = NULL;
	if (stParserData.iErr) format_destroy_type_tree(stParserData.pstRoot);
	return pstResult;
}
void format_destroy_type_tree (struct _FORMAT_TYPE_NODE * pstNode) {
	unsigned int index = 0;
	if (pstNode->pSubNodeList) {
		for (__list_start_iterator(pstNode->pSubNodeList, 0); __list_has_next(pstNode->pSubNodeList);) {
			struct _FORMAT_TYPE_NODE * pstSubNode = (struct _FORMAT_TYPE_NODE * )__list_next(pstNode->pSubNodeList, NULL, &index);
			//if (pstNode->nodeType == VARRAY && index == __list_count(pstNode->pSubNodeList) - 1) break;
			if (pstSubNode->pstParent == pstNode) {
				format_destroy_type_tree (pstSubNode);
			}
		}
	}
	format_destroy_type_node(pstNode);
}
