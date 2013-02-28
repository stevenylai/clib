#include "win32A_internal.h"
void __write_to_console (char * pcText) {
	OutputDebugString(pcText);
}