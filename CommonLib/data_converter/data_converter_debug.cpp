#include "detect_mem_leak.h"
#include <stdarg.h>
#include "platform/platform_interface.h"
#include "parser/parser_interface.h"
#include "data_converter_internal.h"
#include "data_format_parser.tab.h"
static bool g_bConverterDebug = 
									#ifdef _DATA_CONVERTER_DEBUG_
									true;
									#else
									false;
									#endif
void __data_converter_debug_printf (char * pcFormat, ...) {
	if (!g_bConverterDebug) return;
	char *pcAllString = NULL;
	va_list argList;
	va_start(argList, pcFormat);
	pcAllString = __vasprintf(pcFormat, argList);
	va_end(argList);
	if (pcAllString) {
		__write_to_console(pcAllString);
		free(pcAllString), pcAllString = NULL;
	} else {
		__write_to_console("Cannot create string!\n");
	}
}
