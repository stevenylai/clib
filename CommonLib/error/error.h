#ifndef __ERROR_H__
#define __ERROR_H__

#define SUCCESS                       	     0x0000
#define MALLOC_NO_MEM						300
#define NO_IMPLEMENTATION					301
#define FATAL_CREATION_ERROR				302
#define DATA_FORMAT_SYNTAX_ERROR			303
#define ASPRINTF_SYNTAX_ERROR				304
#define FORMAT_TREE_CREATION_ERROR			305
#define FATAL_UNEXPECTED_ERROR				306
#define WAIT_FOR_SEMAPHORE_TIMEOUT			307

#define MEM_POOL_SIZE_MISMATCH				333
#define MEM_POOL_NAME_ERROR					334
#define MEM_POOL_HANDLE_ERROR				335
#define MEM_POOL_MAP_VIEW_ERROR				336
#define MEM_POOL_INFO_ERROR					337
#define MEM_POOL_CREAT_FILE_MAPPING_ERROR	338
#define MEM_POOL_MEM_RECORD_ERROR			339
#define MEM_POOL_DUPLICATED_MEMORY			340
#define MEM_POOL_NOT_IMPLEMENTED			341
#define MEM_POOL_DATA_FORMAT_ERROR			342
#define MEM_POOL_LOCK_ERROR					343
#define MEM_POOL_LOCK_TIME_OUT				344
#define MEM_POOL_LOCK_NO_ID					345
#define MEM_POOL_BLOCK_NOT_FOUND			346
#define MEM_POOL_BLOCK_CANNOT_CHANGE_SIZE	347

typedef struct _ERROR_HANDLER {
	void * pHandler;
	void (*handle_error) (void *, int, char *, ...); // void handle_error (void * pHandler, char * pcFormat, ...);
} ERROR_HANDLER;
#endif