#ifndef __PARSER_INTERFACE_H__
#define __PARSER_INTERFACE_H__
#include <stdarg.h>
char * __vasprintf(char * pcFormatter, va_list args);
char * __asprintf(char * pcFormatter, ...);
#endif