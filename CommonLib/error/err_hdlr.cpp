#include <stdio.h>
#include <stdarg.h>
#include "error.h"

void __err_log_printf (char * pcFuncName, int iErrCode, char * pcFormat, ...) {
	va_list argList;
	va_start(argList, pcFormat);
	printf(pcFormat, argList);
	va_end(argList);
}
