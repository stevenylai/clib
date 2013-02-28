#ifndef _PLATFORM_INTERFACE_
#define _PLATFORM_INTERFACE_
void __write_to_console (char * pcText);

void * __create_mmap_file (void ** pHandle, const char * pcFileName, const char * pcTagName, size_t size, size_t offset);
void * __open_mmap_file (void ** pHandle, const char * pcFileName, const char * pcTagName, size_t size, size_t offset);
void __close_mmap_file (void * pHandle, void * pView);

void * __semaphore_create (void ** pSem, const char * pcName, unsigned int count);
int __semaphore_acquire (void * pSem, unsigned long timeout);
void __semaphore_release (void * pSem);
void __semaphore_destroy (void * pSem);
#endif