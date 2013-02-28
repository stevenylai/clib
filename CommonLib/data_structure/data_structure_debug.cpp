#include "detect_mem_leak.h"
#include <stdarg.h>
#include <stdlib.h>
#include "platform/platform_interface.h"
#include "parser/parser_interface.h"
#include "data_structure_internal.h"
static bool g_bTraceDebug = 
									#ifdef _DATA_STRUCTURE_DEBUG_
									true;
									#else
									false;
									#endif
void __data_structure_debug_printf (char * pcFormat, ...) {
	char *pcAllString = NULL;
	if (!g_bTraceDebug) return;
	va_list argList;
	va_start(argList, pcFormat);
	pcAllString = __vasprintf(pcFormat, argList);
	va_end(argList);
	if (pcAllString) {
		__write_to_console(pcAllString);
		free(pcAllString), pcAllString = NULL;
	} else {
		__write_to_console("Cannot create debug string!\n");
	}
}