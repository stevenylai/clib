#include "detect_mem_leak.h"
#include <stdarg.h>
#include "platform/platform_interface.h"
#include "parser/parser_interface.h"
#include "data_converter/data_converter_interface.h"
#include "mem_pool_internal.h"
static bool g_bTracePool =
									#ifdef _SHARED_MEM_DEBUG_ 
									true;
									#else
									false;
									#endif
static bool g_bTraceDetail = 
									#ifdef _SHARED_MEM_DEBUG_DETAIL_ 
									true;
									#else
									false;
									#endif
static const char * mem_pool_type_to_string(struct _MEM_POOL * pstPool, enum _MEM_TYPE enType) {
	if (pstPool->enType == enType) return "MASTER";
	switch (enType) {
		case MEM_SUB_BLOCK: return "SLAVE";
		case MEM_EXTERN: return "EXTERN";
		default: return "UNKNOWN";
	}
}
void __mem_pool_debug_printf (char * pcFormat, ...) {
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
void __trace_shared_mem_pool (struct _MEM_POOL * pstPool) {
	if (!g_bTracePool) return;
	struct _SHARED_MEM_EXTRA * pstExtra = (struct _SHARED_MEM_EXTRA *)pstPool->pExtra;
	struct _DATA_CONVERT_INFO stData = {NULL, NULL, NULL, 0, 0};
	if (!pstPool->pstPoolInfo && pstPool->stPoolInfo.pData) {
		stData.pLeft = pstPool->stPoolInfo.pData;
		__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C, &stData, NULL);
		if (stData.iErr) {
			if (stData.pRight) {
				__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C_FREE, &stData, NULL);
			}
			pstPool->pstPoolInfo = NULL;
		} else {
			pstPool->pstPoolInfo = (struct _MEM_POOL_INFO *)stData.pRight;
		}
		if (pstPool->pstPoolInfo) {
			__mem_pool_debug_printf("\nPool info (%s). Count: %d. Ref: %d. Mem rec: (%s,%d), Index rec: (%s,%d)\n", mem_pool_type_to_string(pstPool, pstPool->enType), pstPool->pstPoolInfo->poolCount, pstPool->pstPoolInfo->refCount, pstPool->pstPoolInfo->pcMemName, pstPool->pstPoolInfo->memRecListSize, pstPool->pstPoolInfo->pcIndexName, pstPool->pstPoolInfo->indexListSize);
			__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C_FREE, &stData, NULL), free (stData.pRight), pstPool->pstPoolInfo = NULL;
		} else {
			__mem_pool_debug_printf("\nError in getting pool info.\n");
		}
	} else if (pstPool->pstPoolInfo) {
		__mem_pool_debug_printf("\nPool info (%s). Count: %d. Ref: %d. Mem rec: (%s,%d), Index rec: (%s,%d)\n", mem_pool_type_to_string(pstPool, pstPool->enType), pstPool->pstPoolInfo->poolCount, pstPool->pstPoolInfo->refCount, pstPool->pstPoolInfo->pcMemName, pstPool->pstPoolInfo->memRecListSize, pstPool->pstPoolInfo->pcIndexName, pstPool->pstPoolInfo->indexListSize);
	} else {
		__mem_pool_debug_printf("\nPool info: NULL\n");
	}
	if (!g_bTraceDetail) return;
	__mem_pool_debug_printf("HANDLE rec details:\n");
	for(__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData);) {
		struct _MMAP_RECORD * pstRecord = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, NULL);
		if (!pstRecord) continue;
		__mem_pool_debug_printf("\t%s: 0x%X (handle: 0x%X). Offsets[%d]:", pstRecord->pcID, pstRecord->pData, pstRecord->pMmap, pstRecord->offsetCount);
		for (unsigned int i = 0; i < pstRecord->offsetCount; i++) {
			i + 1 < pstRecord->offsetCount ? __mem_pool_debug_printf(" %d,", pstRecord->offsets[i]) : __mem_pool_debug_printf(" %d", pstRecord->offsets[i]);
		}
		__mem_pool_debug_printf("\n");
	}

	__mem_pool_debug_printf("Mem rec details:\n");
	for(__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData);) {
		struct _MEM_RECORD * pstRecord = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, NULL);
		if (!pstRecord) continue;

		__mem_pool_debug_printf("\t%s -> %s (%s). ", pstRecord->pcID, pstRecord->pcMemName, mem_pool_type_to_string(pstPool, pstRecord->enType));
		__mem_pool_debug_printf("Aval (%d): [", pstRecord->availCount);
		for (unsigned int i = 0; i < pstRecord->availCount; i++) {
			if (i == 0) __mem_pool_debug_printf("(%d,%d)", pstRecord->pAvailableBlocks[i].offset, pstRecord->pAvailableBlocks[i].length);
			else __mem_pool_debug_printf(", (%d,%d)", pstRecord->pAvailableBlocks[i].offset, pstRecord->pAvailableBlocks[i].length);
		}
		__mem_pool_debug_printf("]. ");

		__mem_pool_debug_printf("Used (%d): [", pstRecord->usedCount);
		for (unsigned int i = 0; i < pstRecord->usedCount; i++) {
			if (i == 0) __mem_pool_debug_printf("(%d,%d)", pstRecord->pUsedBlocks[i].offset, pstRecord->pUsedBlocks[i].length);
			else __mem_pool_debug_printf(", (%d,%d)", pstRecord->pUsedBlocks[i].offset, pstRecord->pUsedBlocks[i].length);
		}
		__mem_pool_debug_printf("].\n");
	}
	__mem_pool_debug_printf("\n");
}
