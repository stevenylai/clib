#include "detect_mem_leak.h"
#include "win32A_internal.h"
#include "error/error.h"
#include "stdlib.h"
void * __semaphore_create (void ** pSem, const char * pcName, unsigned int count) {
	bool err = false;
	struct _WIN32_SEMAPHORE * pstSem = NULL;
	if (pstSem = (struct _WIN32_SEMAPHORE * )(*pSem = malloc(sizeof(struct _WIN32_SEMAPHORE)))) {
		pstSem->semaphore = INVALID_HANDLE_VALUE;
	} else {
		return *pSem = NULL;
	}
	if (!(pstSem->semaphore = CreateSemaphoreA(NULL, count, count, pcName))) {
		free (pSem);
		return NULL;
	} else {
		return pstSem;
	}
}
int __semaphore_acquire (void * pSem, unsigned long timeout) {
	int err = SUCCESS;
	struct _WIN32_SEMAPHORE * pstSem = (struct _WIN32_SEMAPHORE *)pSem;
	DWORD dwWaitResult;
	dwWaitResult = WaitForSingleObject(pstSem->semaphore, timeout);
	switch (dwWaitResult) {
		case WAIT_OBJECT_0:
			err = SUCCESS;
			break;
		case WAIT_TIMEOUT:
			err = WAIT_FOR_SEMAPHORE_TIMEOUT;
			break;
	}
	return err;
}
void __semaphore_release (void * pSem) {
	struct _WIN32_SEMAPHORE * pstSem = (struct _WIN32_SEMAPHORE *)pSem;
	ReleaseSemaphore(pstSem->semaphore, 1, NULL);
}
void __semaphore_destroy (void * pSem) {
	struct _WIN32_SEMAPHORE * pstSem = (struct _WIN32_SEMAPHORE *)pSem;
	CloseHandle(pstSem->semaphore);
	free (pSem);
}