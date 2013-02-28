#include "detect_mem_leak.h"
#include <stdarg.h>
#include "platform/platform_interface.h"
#include "parser_internal.h"
void __parser_debug_printf (char * pcFormat, ...) {
#ifdef _PARSER_DEBUG_
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
#endif
}