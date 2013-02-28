#include "detect_mem_leak.h"
#include "win32A_internal.h"
#include "stdlib.h"
void * __create_mmap_file (void ** pHandle, const char * pcFileName, const char * pcTagName, size_t size, size_t offset) {
	bool err = false;
	struct _WIN32_MMAP * pstMapped = NULL;
	void * mappedAddr = NULL;
	if (pstMapped = (struct _WIN32_MMAP * )(*pHandle = malloc(sizeof(struct _WIN32_MMAP)))) {
		pstMapped->file = pstMapped->memory = INVALID_HANDLE_VALUE;
	} else {
		return *pHandle = NULL;
	}
	if (pcFileName) {
		if ((pstMapped->file = CreateFileA(pcFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
			err = true;
			goto error;
		}
	}
	if (!(pstMapped->memory = CreateFileMappingA(pstMapped->file, 0, PAGE_READWRITE, 0, size, pcTagName))) {
		pstMapped->memory = INVALID_HANDLE_VALUE, err = true;
		goto error;
	}
	if (!(mappedAddr = MapViewOfFile(pstMapped->memory, FILE_MAP_WRITE, 0, offset, size))) {
		err = true;
		goto error;
	}
error:
	if (err) {
		if (pstMapped) {
			if (pstMapped->memory != INVALID_HANDLE_VALUE) {
				CloseHandle(pstMapped->memory), pstMapped->memory = INVALID_HANDLE_VALUE;
			}
			if (pstMapped->file != INVALID_HANDLE_VALUE) {
				CloseHandle(pstMapped->file), pstMapped->file = INVALID_HANDLE_VALUE;
			}
			free (pstMapped), *pHandle = pstMapped = NULL;
		}
		return *pHandle;
	}
	return mappedAddr;
}
void * __open_mmap_file (void ** pHandle, const char * pcFileName, const char * pcTagName, size_t size, size_t offset) {
	bool err = false;
	struct _WIN32_MMAP * pstMapped = NULL;
	void * mappedAddr = NULL;
	if (pstMapped = (struct _WIN32_MMAP * )(*pHandle = malloc(sizeof(struct _WIN32_MMAP)))) {
		pstMapped->file = pstMapped->memory = INVALID_HANDLE_VALUE;
	} else {
		return *pHandle = NULL;
	}
	if (pcTagName) {
		if (pstMapped->memory = OpenFileMappingA(FILE_MAP_WRITE, 0, pcTagName)) {
			if (mappedAddr = MapViewOfFile(pstMapped->memory, FILE_MAP_WRITE, 0, offset, size)) {
				return mappedAddr;
			} else {
				err = true;
				goto error;
			}
		} else {
			pstMapped->memory = INVALID_HANDLE_VALUE, err = true;
			goto error;
		}
	} else if (pcFileName) {
		if ((pstMapped->file = CreateFileA(pcFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
			err = true;
			goto error;
		}
		if (!(pstMapped->memory = CreateFileMappingA(pstMapped->file, 0, PAGE_READWRITE, 0, size, pcTagName))) {
			pstMapped->memory = INVALID_HANDLE_VALUE, err = true;
			goto error;
		}
	} else {
		err = true;
		goto error;
	}
	if (!(mappedAddr = MapViewOfFile(pstMapped->memory, FILE_MAP_WRITE, 0, offset, size))) {
		err = true;
		goto error;
	}
error:
	if (err) {
		if (pstMapped) {
			if (pstMapped->memory != INVALID_HANDLE_VALUE) {
				CloseHandle(pstMapped->memory), pstMapped->memory = INVALID_HANDLE_VALUE;
			}
			if (pstMapped->file != INVALID_HANDLE_VALUE) {
				CloseHandle(pstMapped->file), pstMapped->file = INVALID_HANDLE_VALUE;
			}
			free (pstMapped), *pHandle = pstMapped = NULL;
		}
		return *pHandle;
	}
	return mappedAddr;
}
void __close_mmap_file (void * pHandle, void * pView) {
	struct _WIN32_MMAP * pstMapped = (struct _WIN32_MMAP *)pHandle;
	if (pView) UnmapViewOfFile(pView);
	if (pstMapped->memory != INVALID_HANDLE_VALUE) {
		CloseHandle(pstMapped->memory), pstMapped->memory = INVALID_HANDLE_VALUE;
	}
	if (pstMapped->file != INVALID_HANDLE_VALUE) {
		CloseHandle(pstMapped->file), pstMapped->file = INVALID_HANDLE_VALUE;
	}
	free (pstMapped);
}