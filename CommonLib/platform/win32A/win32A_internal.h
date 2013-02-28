#ifndef _WIN32A_INTERNAL_
#define _WIN32A_INTERNAL_
#include <windows.h>
typedef struct _WIN32_MMAP {
	HANDLE file;
	HANDLE memory;
} WIN32_MMAP;
typedef struct _WIN32_SEMAPHORE {
	HANDLE semaphore;
} WIN32_SEMAPHORE;
#endif