#include "detect_mem_leak.h"
#include <string.h>
#include "platform/platform_interface.h"
#include "error/error.h"
#include "mem_pool_internal.h"
#include "parser/parser_interface.h"
#include "data_converter/data_converter_interface.h"
#include "data_structure/data_structure_internal.h"
#include "data_glue/data_glue_interface.h"
static int pool_anon_file_load_pool_info (struct _MEM_POOL * pstPool, bool bRenewMemRec);
static int list_refresh_pointer_mem_record (struct _MEM_POOL * pstPool);
static void pool_anon_file_deref_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord, bool bDerefLocal);
static char * pool_anon_file_gen_name (struct _MEM_POOL * pstParentPool, const char * pcID) {
	if (pcID) {
		return __asprintf("%s", pcID);
	} else if (pcID == NULL && pstParentPool && pstParentPool->pcID) {
		return __asprintf("%s_SUB%d", pcID, __list_count(pstParentPool->pSubPools));
	} else {
		return __asprintf("%s", MEM_POOL_DEFAULT_NAME);
	}
}
static void pool_anon_file_destroy_list (MEM_POOL_DATA_STRUCTURE * pstData) {
	if (pstData->pGlue) __glue_destroy_packer_for_data_structure(pstData->pGlue), pstData->pGlue = NULL;
	if (pstData->pData) __list_destroy(pstData->pData), pstData->pData = NULL;
	if (pstData->pPacker) __data_converter_destroy(pstData->pPacker), pstData->pPacker = NULL;
}
static int pool_anon_file_create_list (MEM_POOL_DATA_STRUCTURE * pstData, char * pcPackerString, enum _DATA_STRUCTURE_TYPE enType, struct _ALLOCATOR * pstAllocator) {
	int err = SUCCESS;
	pstData->pGlue = pstData->pData = pstData->pPacker = NULL;
	if (!__list_create(&pstData->pData, enType, NULL, pstAllocator)) {
		err = FATAL_CREATION_ERROR;
		goto error;
	}
	if (!__data_converter_create(&pstData->pPacker, MEMORY_TO_MEMORY, pcPackerString, NULL)) {
		err = FATAL_CREATION_ERROR;
		goto error;
	}
	if (!__glue_create_packer_for_data_structure(&pstData->pGlue, pstData->pPacker, GLUE_PACKER_FOR_LIST_AUTO_FREE, pstData->pData, NULL)) {
		err = FATAL_CREATION_ERROR;
		goto error;
	}
error:
	if (err) pool_anon_file_destroy_list(pstData);
	return err;
}
static void pool_anon_file_destroy_simple_data (MEM_POOL_DATA_STRUCTURE * pstData) {
	if (pstData->pPacker) __data_converter_destroy(pstData->pPacker), pstData->pPacker = NULL;
	pstData->pData = NULL;
}
static int pool_anon_file_create_simple_data (MEM_POOL_DATA_STRUCTURE * pstData, char * pcPackerString) {
	int err = SUCCESS;
	pstData->pGlue = pstData->pData = pstData->pPacker = NULL;
	return err = (__data_converter_create(&pstData->pPacker, MEMORY_TO_MEMORY, pcPackerString, NULL) ? SUCCESS : MALLOC_NO_MEM);
}
static void pool_anon_file_init_mmap (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstMmap) {
	pstMmap->pMmap = NULL, pstMmap->offsetCount = 0;
	pstMmap->pcID = NULL, pstMmap->pData = NULL, pstMmap->offsets = NULL;
}
static void pool_anon_file_update_mmap_offset (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstHandle, size_t oldOffset, size_t newOffset) {
	int err = SUCCESS;
	struct _MMAP_RECORD * pstExisting = NULL;
	unsigned int uiRecIndex = __list_count(pstPool->stHandles.pData) + 1;
	for(__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstExisting = NULL) {
		pstExisting = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, &uiRecIndex);
		if (!strcmp(pstExisting->pcID, pstHandle->pcID)) break;
	}
	if (pstExisting) {
		for (unsigned int i = 0; i < pstExisting->offsetCount; i++) {
			if (pstExisting->offsets[i] == oldOffset) {
				pstExisting->offsets[i] = newOffset;
				break;
			}
		}
		__list_update(pstPool->stHandles.pData, uiRecIndex, 0, pstExisting);
	}
	if (pstExisting) {
		free (pstExisting->offsets), pstExisting->offsets = NULL;
		free(pstExisting), pstExisting = NULL;
	}
}
static int pool_anon_file_insert_mmap_offset (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstHandle, size_t offset) {
	int err = SUCCESS;
	struct _MMAP_RECORD * pstExisting = NULL;
	unsigned int uiRecIndex = __list_count(pstPool->stHandles.pData) + 1;
	for(__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstExisting = NULL) {
		pstExisting = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, &uiRecIndex);
		if (!strcmp(pstExisting->pcID, pstHandle->pcID)) break;
	}
	if (pstExisting) {
		size_t * pNewOffset = (size_t *)realloc(pstExisting->offsets, sizeof(size_t) * (pstExisting->offsetCount + 1));
		if (!pNewOffset) {
			err = MALLOC_NO_MEM;
			goto error;
		}
		pstExisting->offsets = pNewOffset;
		unsigned int uiInsertionIndex = 0;
		for (uiInsertionIndex = 0; uiInsertionIndex < pstExisting->offsetCount; uiInsertionIndex++) {
			if (pstExisting->offsets[uiInsertionIndex] == offset) {
				goto error;
			} else if (pstExisting->offsets[uiInsertionIndex] > offset) {
				break;
			}
		}
		for (unsigned int i = uiInsertionIndex; i + 1 < pstExisting->offsetCount + 1; i++) {
			pstExisting->offsets[i + 1] = pstExisting->offsets[i];
		}
		pstExisting->offsets[uiInsertionIndex] = offset, pstExisting->offsetCount++;
		if (!__list_update(pstPool->stHandles.pData, uiRecIndex, 0, pstExisting)) {
			err = MEM_POOL_HANDLE_ERROR;
			goto error;
		}
	}
error:
	if (pstExisting) {
		free (pstExisting->offsets), pstExisting->offsets = NULL;
		free(pstExisting), pstExisting = NULL;
	}
	return err;
}
static void pool_anon_file_init_mem_record (struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	pstRecord->pcID = pstRecord->pcMemName = NULL, pstRecord->enType = pstPool->enType;
	pstRecord->availCount = pstRecord->usedCount = 0, pstRecord->pAvailableBlocks = pstRecord->pUsedBlocks = NULL;
}
static void pool_anon_file_deep_destroy_mem_record (struct _MEM_RECORD * pstRecord) {
		free(pstRecord->pcID), pstRecord->pcID = NULL;
		free(pstRecord->pcMemName), pstRecord->pcMemName = NULL;
		free(pstRecord->pAvailableBlocks), pstRecord->pAvailableBlocks = NULL;
		free(pstRecord->pUsedBlocks), pstRecord->pUsedBlocks = NULL;
}
static int pool_anon_file_deep_clone_mem_record (struct _MEM_RECORD * pstDest, const struct _MEM_RECORD * pstSource) {
	int err = SUCCESS;
	pstDest->pcID = __asprintf("%s", pstSource->pcID);
	pstDest->pcMemName = __asprintf("%s", pstSource->pcMemName);
	pstDest->enType = pstSource->enType;
	pstDest->availCount = pstSource->availCount;
	pstDest->usedCount = pstSource->usedCount;
	pstDest->pAvailableBlocks = pstDest->availCount ?  (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * pstDest->availCount) : NULL;
	if (pstDest->pAvailableBlocks) memcpy(pstDest->pAvailableBlocks, pstSource->pAvailableBlocks, sizeof(struct _MEM_RECORD_BLOCK) * pstDest->availCount);
	pstDest->pUsedBlocks = pstDest->usedCount ?  (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * pstDest->usedCount) : NULL;
	if (pstDest->pUsedBlocks) memcpy(pstDest->pUsedBlocks, pstSource->pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK) * pstDest->usedCount);
	if (!pstDest->pcID || !pstDest->pcMemName || (!pstDest->pAvailableBlocks && pstDest->availCount) || (!pstDest->pUsedBlocks && pstDest->usedCount))
		err = MALLOC_NO_MEM;
	if (err)
		pool_anon_file_deep_destroy_mem_record(pstDest);
	return err;
}
static void pool_anon_file_insert_block_list(struct _MEM_RECORD_BLOCK * pstList, unsigned int count, struct _MEM_RECORD_BLOCK * pstRecord) {
	unsigned int index = count;
	for (unsigned int i = 0; i < count - 1; i++) {
		if (pstList[i].offset <= pstRecord->offset) continue;
		index = i;
		break;
	}
	for (unsigned int i = index; i + 1 < count; i++) {
		pstList[i + 1].length = pstList[i].length;
		pstList[i + 1].offset = pstList[i].offset;
	}
	pstList[index].length = pstRecord->length;
	pstList[index].offset = pstRecord->offset;
}
static void pool_anon_file_adjust_realloc_size_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	struct _ALLOCATOR * pstAllocator = &((struct _SHARED_MEM_EXTRA *)pstPool->pExtra)->stMemRecordAllocator;
	size_t estimated = strlen(pstRecord->pcMemName) + strlen(pstRecord->pcID) + sizeof(struct _MEM_RECORD) + sizeof(struct _MEM_RECORD_BLOCK) * (pstRecord->availCount + pstRecord->usedCount);
	if (pstAllocator->reallocSize < estimated * 2) { // Adjust realloc size to avoid infinite recursion
		pstAllocator->reallocSize = (estimated * 2 / pstAllocator->reallocSize + (estimated * 2 % pstAllocator->reallocSize ? 1 : 0)) * pstAllocator->reallocSize;
	}
}
static void pool_anon_file_construct_mem_record_self(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	pstRecord->pcID = pstRecord->pcMemName = pstPool->pstPoolInfo->pcMemName;
	pstRecord->availCount = pstRecord->usedCount = 1;
	pstRecord->enType = pstPool->enType;
}
static bool pool_anon_file_check_fit (struct _MEM_RECORD * pstRecord, size_t size, struct _MEM_RECORD_BLOCK * pstBlock) {
	if (pstRecord->availCount == 0) return false;

	unsigned int availIndex = 0, usedIndex = 0;
	size_t wanted_len = size <= MEM_POOL_MIN_FIT_SIZE ? MEM_POOL_MIN_FIT_SIZE : ((size / MEM_POOL_MIN_FIT_SIZE + (size % MEM_POOL_MIN_FIT_SIZE ? 1 : 0)) * MEM_POOL_MIN_FIT_SIZE);
	// MEM_POOL_MIN_FIT_SIZE here is reserved for master
	
	for (availIndex = 0, usedIndex = 0; availIndex < pstRecord->availCount; availIndex++) {
		//size_t avail_offset = pstRecord->pAvailableBlocks[availIndex].offset + (availIndex ? 0 : MEM_POOL_MIN_FIT_SIZE);
		//size_t avail_len = pstRecord->pAvailableBlocks[availIndex].length - (availIndex ? 0 : MEM_POOL_MIN_FIT_SIZE);
		size_t avail_offset = pstRecord->pAvailableBlocks[availIndex].offset;
		size_t avail_len = pstRecord->pAvailableBlocks[availIndex].length;
		if (avail_len < wanted_len) continue;
		for (; usedIndex < pstRecord->usedCount; usedIndex++) {
			if (pstRecord->pUsedBlocks[usedIndex].offset + pstRecord->pUsedBlocks[usedIndex].length <= avail_offset) continue;
			if (pstRecord->pUsedBlocks[usedIndex].offset >= avail_offset + wanted_len) {
				pstBlock->offset = avail_offset; // Available block is before the used
				pstBlock->length = wanted_len;
				return true;
			}
			size_t reserved_len = MEM_POOL_MIN_FIT_SIZE; // Reserve some space for existing
			if (pstRecord->pUsedBlocks[usedIndex].length > reserved_len) {
				reserved_len = (pstRecord->pUsedBlocks[usedIndex].length / reserved_len + (pstRecord->pUsedBlocks[usedIndex].length % reserved_len ? 1 : 0)) * reserved_len;
			} // Keep on searching
			avail_len = avail_offset + avail_len - (pstRecord->pUsedBlocks[usedIndex].offset + reserved_len);
			avail_offset = pstRecord->pUsedBlocks[usedIndex].offset + reserved_len;
			if (avail_len < wanted_len) break;
		}
		if (avail_len < wanted_len) continue;
		if (usedIndex >= pstRecord->usedCount) { // No more used blocks
			pstBlock->offset = avail_offset;
			pstBlock->length = wanted_len;
			return true;
		}
	}
	return false;
}
static void pool_anon_file_close_mapped_file (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstMmap) {
	__close_mmap_file (pstMmap->pMmap, pstMmap->pData);
	pstMmap->pMmap = pstMmap->pData = NULL;
}
static void * pool_anon_file_open_mapped_file (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstMmap) {
	return pstMmap->pData = __open_mmap_file(&pstMmap->pMmap, NULL, pstMmap->pcID, 0, 0);
}
static void * pool_anon_file_open_existing_mmap (struct _MEM_POOL * pstPool, struct _MMAP_RECORD * pstHandle, size_t handleOffset, bool *pbHasSeen) {
	struct _MMAP_RECORD * pstExisting = NULL;
	for(__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstExisting = NULL) {
		pstExisting = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, NULL);
		if (!strcmp(pstExisting->pcID, pstHandle->pcID)) {
			break;
		}
	}
	if (pstExisting) {
		if (pbHasSeen) *pbHasSeen = true;
		pstHandle->pMmap = pstExisting->pMmap, pstHandle->pData = pstExisting->pData;
		pstHandle->offsetCount = pstExisting->offsetCount;

		free (pstExisting->offsets), pstExisting->offsets = NULL;
		free (pstExisting);
		return pstHandle->pData;
	} else {
		if (pbHasSeen) *pbHasSeen = false;
		if (!pool_anon_file_open_mapped_file(pstPool, pstHandle)) {
			return pstHandle->pMmap = pstHandle->pData = NULL;
		}
		size_t offset[1] = {handleOffset};
		pstHandle->offsetCount = 1, pstHandle->offsets = offset;
		if (!__list_insert(pstPool->stHandles.pData, 0, 0, pstHandle)) {
			pool_anon_file_close_mapped_file(pstPool, pstHandle);
		} // Clean up in case of failure to insert into the list
		pstHandle->offsets = NULL;
		return pstHandle->pData;
	}
}
static char * pool_anon_file_next_avail_name (struct _MEM_POOL * pstPool) {
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	stHandle.pcID = __asprintf("%s%X", pstPool->pcID, pstPool->pstPoolInfo->poolCount++);
	while (stHandle.pcID) {
		if (!pool_anon_file_open_mapped_file(pstPool, &stHandle)) break;
		pool_anon_file_close_mapped_file(pstPool, &stHandle);
		free(stHandle.pcID), stHandle.pcID = __asprintf("%s%X", pstPool->pcID, pstPool->pstPoolInfo->poolCount++);
	}
	return stHandle.pcID;
}
static void * pool_anon_file_create_mmap (struct _MEM_POOL * pstPool, size_t size, struct _MMAP_RECORD * pstMmap) {
	if (!pstMmap->pcID) { // Generate a name if it's not here already
		pstMmap->pcID = pool_anon_file_next_avail_name(pstPool);
		if (!pstMmap->pcID) return NULL;
	}
	if (!(pstMmap->pData = __create_mmap_file(&pstMmap->pMmap, NULL, pstMmap->pcID, size, 0))) {
		pstMmap->pMmap = NULL;
		return pstMmap->pData;
	} else {	
		size_t offset[1] = {0};
		pstMmap->offsetCount = 1, pstMmap->offsets = offset;
		if (!__list_insert(pstPool->stHandles.pData, 0, 0, pstMmap)) { // Created but cannot insert
			pstMmap->offsets = NULL;
			pool_anon_file_close_mapped_file(pstPool, pstMmap);
			return pstMmap->pData;
		}
	}
	pstMmap->offsets = NULL;
	return pstMmap->pData;
}
static void * pool_anon_file_find_mem_record_by_id(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord, unsigned int * pIndex) {
	unsigned int index = __list_count(pstPool->stMemRecords.pData) + 1;
	struct _MEM_RECORD * pstExisting = NULL;
	for (__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData); pstExisting = NULL) {
		pstExisting = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, &index);
		if (!strcmp(pstExisting->pcID, pstRecord->pcID)) break;
	}
	if (pstExisting) {
		pstRecord->enType = pstExisting->enType, pstRecord->pcMemName = pstExisting->pcMemName;
		pstRecord->pAvailableBlocks = pstExisting->pAvailableBlocks, pstRecord->pUsedBlocks = pstExisting->pUsedBlocks;
		pstRecord->availCount = pstExisting->availCount, pstRecord->usedCount = pstExisting->usedCount;
		free (pstExisting), pstExisting = NULL;
		if (pIndex) *pIndex = index;
		return pstRecord;
	} else {
		return NULL;
	}
}
static void * pool_anon_file_find_mem_record_by_name_offset(struct _MEM_POOL * pstPool, size_t offset, struct _MEM_RECORD * pstRecord, unsigned int * pIndex) {
	unsigned int index = __list_count(pstPool->stMemRecords.pData) + 1;
	struct _MEM_RECORD * pstExisting = NULL;
	for (__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData); pstExisting = NULL) {
		pstExisting = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, &index);
		if (!strcmp(pstRecord->pcMemName, pstExisting->pcMemName) && pstExisting->usedCount > 0 && pstExisting->availCount > 0 && pstExisting->pUsedBlocks[0].offset == offset && offset == pstExisting->pAvailableBlocks[0].offset) {
			break;
		}
	}
	if (pstExisting) {
		pstRecord->enType = pstExisting->enType, pstRecord->pcID = pstExisting->pcID;
		pstRecord->pAvailableBlocks = pstExisting->pAvailableBlocks, pstRecord->pUsedBlocks = pstExisting->pUsedBlocks;
		pstRecord->pcMemName = pstExisting->pcMemName;
		pstRecord->availCount = pstExisting->availCount, pstRecord->usedCount = pstExisting->usedCount;
		free (pstExisting), pstExisting = NULL;
		if (pIndex) *pIndex = index;
		return pstRecord;
	} else {
		return NULL;
	}
}
static void * pool_anon_file_insert_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	pool_anon_file_adjust_realloc_size_mem_record(pstPool, pstRecord);
	char * pcRecNameOld = __asprintf("%s", pstPool->pstPoolInfo->pcMemName);
	if (!pcRecNameOld) return NULL; // Cannot afford to lose track of the record list name
	void * pInserted = __list_insert(pstPool->stMemRecords.pData, __list_count(pstPool->stMemRecords.pData), 0, pstRecord);
	if (strcmp(pstPool->pstPoolInfo->pcMemName, pcRecNameOld)) { // List changed
		list_refresh_pointer_mem_record(pstPool);
		// De-reference the old one:
		struct _MEM_RECORD stOldRecord;
		stOldRecord.pcID = pcRecNameOld;
		if (pool_anon_file_find_mem_record_by_id(pstPool, &stOldRecord, NULL)) {
			pool_anon_file_deref_mem_record(pstPool, &stOldRecord, true);
		}
		struct _MEM_RECORD stNewRec; pool_anon_file_construct_mem_record_self(pstPool, &stNewRec);
		struct _MEM_RECORD_BLOCK rgstNewBlock[1] = {{0, pstPool->pstPoolInfo->memRecListSize}};
		stNewRec.pAvailableBlocks = stNewRec.pUsedBlocks = rgstNewBlock;
		__list_insert(pstPool->stMemRecords.pData, __list_count(pstPool->stMemRecords.pData), 0, &stNewRec);
	}
	free(pcRecNameOld), pcRecNameOld = NULL;
	return pInserted;
}
static void * pool_anon_file_update_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	pool_anon_file_adjust_realloc_size_mem_record(pstPool, pstRecord);
	char * pcRecNameOld = __asprintf("%s", pstPool->pstPoolInfo->pcMemName);
	if (!pcRecNameOld) return NULL; // Cannot afford to lose track of the record list name

	struct _MEM_RECORD stTempRecord; pool_anon_file_init_mem_record(pstPool, &stTempRecord);
	unsigned int index = __list_count(pstPool->stMemRecords.pData) + 1;
	stTempRecord.pcID = pstRecord->pcID;
	void * pUpdated = NULL;
	if (pool_anon_file_find_mem_record_by_id(pstPool, &stTempRecord, &index)) {
		pUpdated = __list_update(pstPool->stMemRecords.pData, index, 0, pstRecord);
		free (stTempRecord.pUsedBlocks), stTempRecord.pUsedBlocks = NULL;
		free (stTempRecord.pAvailableBlocks), stTempRecord.pAvailableBlocks = NULL;
	}
	free(pstRecord->pAvailableBlocks), pstRecord->pAvailableBlocks = NULL;
	free(pstRecord->pUsedBlocks), pstRecord->pUsedBlocks = NULL;
	if (strcmp(pstPool->pstPoolInfo->pcMemName, pcRecNameOld)) { // List reallocated
		list_refresh_pointer_mem_record(pstPool); // Refresh pointer 
		// De-reference the old one:
		struct _MEM_RECORD stOldRecord;
		stOldRecord.pcID = pcRecNameOld;
		if (pool_anon_file_find_mem_record_by_id(pstPool, &stOldRecord, NULL)) {
			pool_anon_file_deref_mem_record(pstPool, &stOldRecord, true);
		}
		pool_anon_file_construct_mem_record_self(pstPool, &stTempRecord); // Add record
		struct _MEM_RECORD_BLOCK rgstNewBlock[1] = {{0, pstPool->pstPoolInfo->memRecListSize}};
		stTempRecord.pAvailableBlocks = stTempRecord.pUsedBlocks = rgstNewBlock;
		__list_insert(pstPool->stMemRecords.pData, __list_count(pstPool->stMemRecords.pData), 0, &stTempRecord);
	} 
	free(pcRecNameOld), pcRecNameOld = NULL;
	return pUpdated;
}
static void pool_anon_file_remove_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord) {
	struct _MEM_RECORD stTempRecord; pool_anon_file_init_mem_record(pstPool, &stTempRecord);
	unsigned int index = __list_count(pstPool->stMemRecords.pData) + 1;
	stTempRecord.pcID = pstRecord->pcID;
	if (pool_anon_file_find_mem_record_by_id(pstPool, &stTempRecord, &index)) {
		__list_remove(pstPool->stMemRecords.pData, index);
		free (stTempRecord.pAvailableBlocks), stTempRecord.pAvailableBlocks = NULL;
		free (stTempRecord.pUsedBlocks), stTempRecord.pUsedBlocks = NULL;
	}
	free(pstRecord->pAvailableBlocks), pstRecord->pAvailableBlocks = NULL;
	free(pstRecord->pUsedBlocks), pstRecord->pUsedBlocks = NULL;
}
static void pool_anon_file_deref_mem_record(struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstRecord, bool bDerefLocal) {
	char * pcMemName = NULL, * pcID = NULL;
	if (!pstRecord->availCount || !pstRecord->usedCount) {
		goto error; // Shouldn't happen. Need to issue a warning
	}
	if (pstRecord->pAvailableBlocks[0].offset != pstRecord->pUsedBlocks[0].offset) {
		goto error; // Must be the first - master or slave
	} else if (pstRecord->enType != pstPool->enType && pstRecord->usedCount > 1) {
		goto error; // Slave record must only have 1 used block
	} else if (pstRecord->enType != pstPool->enType) { // Valid slave record - need to detach from the master first
		struct _MEM_RECORD * pstExisting = NULL;
		unsigned int index = __list_count(pstPool->stMemRecords.pData) + 1;
		for (__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData); pstExisting = NULL) {
			pstExisting = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, &index);
			if (pstExisting->enType == MEM_SUB_BLOCK || strcmp(pstExisting->pcMemName, pstRecord->pcMemName)) continue;
			bool bFound = false; // Master found, start to remove self from the used list
			for (unsigned int i = 0; i < pstExisting->usedCount; i++) {
				if (pstExisting->pUsedBlocks[i].offset == pstRecord->pUsedBlocks[0].offset) {
					bFound = true;
				} // If found self then shift every afterwards 
				if (bFound && i + 1 < pstExisting->usedCount) {
					pstExisting->pUsedBlocks[i].offset = pstExisting->pUsedBlocks[i + 1].offset;
					pstExisting->pUsedBlocks[i].length = pstExisting->pUsedBlocks[i + 1].length;
				}
			}
			if (bFound) {
				pstRecord->pcMemName = pcMemName = __asprintf("%s", pstRecord->pcMemName);
				pstRecord->pcID = pcID = __asprintf("%s", pstRecord->pcID);
				if (!pstRecord->pcMemName || !pstRecord->pcID) goto error;
				if (--pstExisting->usedCount > 0)
					__list_update(pstPool->stMemRecords.pData, index, 0, pstExisting);
				else
					__list_remove(pstPool->stMemRecords.pData, index);
				break;
			} else goto error;
		}
		if (pstExisting) {
			free(pstExisting->pAvailableBlocks), pstExisting->pAvailableBlocks = NULL;
			free(pstExisting->pUsedBlocks), pstExisting->pUsedBlocks = NULL;
			free(pstExisting), pstExisting = NULL;
		}
	}
	if (bDerefLocal) { // Remove from local
		unsigned int index = __list_count(pstPool->stHandles.pData) + 1;
		struct _MMAP_RECORD * pstHandle = NULL;
		for(__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstHandle = NULL) {
			pstHandle = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, &index);
			if (strcmp(pstHandle->pcID, pstRecord->pcMemName)) continue;
			bool bFound = false;
			for (unsigned int i = 0; i < pstHandle->offsetCount; i++) {
				if (pstHandle->offsets[i] == pstRecord->pUsedBlocks[0].offset) {
					bFound = true;
				} // Found and remove
				if (bFound && i + 1 < pstHandle->offsetCount) {
					pstHandle->offsets[i] = pstHandle->offsets[i + 1];
				}
			}
			if (bFound) break;
		}
		if (pstHandle) { // Found 
			if ((--pstHandle->offsetCount) == 0) {
				pool_anon_file_close_mapped_file(pstPool, pstHandle);
				__list_remove(pstPool->stHandles.pData, index);
			} else {
				__list_update(pstPool->stHandles.pData, index, 0, pstHandle);
			}
			free(pstHandle->offsets), pstHandle->offsets = NULL;
			free (pstHandle), pstHandle = NULL;
		}
	}
	for (unsigned int i = 1; i < pstRecord->usedCount; i++) {
		pstRecord->pUsedBlocks[i - 1].length = pstRecord->pUsedBlocks[i].length;
		pstRecord->pUsedBlocks[i - 1].offset = pstRecord->pUsedBlocks[i].offset;
	}
	pstRecord->usedCount--;
	if (pstRecord->usedCount == 0) { // Nothing left - remove from the mem record
		pool_anon_file_remove_mem_record(pstPool, pstRecord);
	} else {
		pool_anon_file_update_mem_record(pstPool, pstRecord);
	}
error:
	free (pcMemName), pcMemName = NULL;
	free (pcID), pcID = NULL;
	free(pstRecord->pAvailableBlocks), pstRecord->pAvailableBlocks = NULL;
	free(pstRecord->pUsedBlocks), pstRecord->pUsedBlocks = NULL;
}
static int list_refresh_pointer_mem_record (struct _MEM_POOL * pstPool) {
	int err = SUCCESS;
	struct _ALLOCATOR * pstAllocator = &((struct _SHARED_MEM_EXTRA *)pstPool->pExtra)->stMemRecordAllocator;

	size_t minReallocSize = (sizeof(struct _MEM_RECORD) + (strlen(pstPool->pstPoolInfo->pcMemName) + 1 + sizeof(struct _MEM_RECORD_BLOCK))* 2) * 2;
	if (pstAllocator->reallocSize < minReallocSize) { // Adjust the realloc size to avoid infinite recursion
		pstAllocator->reallocSize = (minReallocSize / pstAllocator->reallocSize + 1) * pstAllocator->reallocSize;
	}

	struct _MMAP_RECORD stHandle; // May be added to handles
	 pool_anon_file_init_mmap(pstPool, &stHandle), stHandle.pcID = pstPool->pstPoolInfo->pcMemName;
	struct _MEM_RECORD stMem; pool_anon_file_init_mem_record(pstPool, &stMem); // May need to be added to record list (if the pool is just created)
	stMem.pcID = stMem.pcMemName = pstPool->pstPoolInfo->pcMemName;
	stMem.availCount = stMem.usedCount = 1; // Since it's new, only one reference up to now
	struct _MEM_RECORD_BLOCK blocks[1] = {{0, pstAllocator->reallocSize}};
	stMem.pAvailableBlocks = stMem.pUsedBlocks = blocks;

	bool bHasSeen = false;
	if (pool_anon_file_open_existing_mmap(pstPool, &stHandle, 0, &bHasSeen)) {
		((struct _LIST *)pstPool->stMemRecords.pData)->pList = stHandle.pData;
		if (!bHasSeen && !pool_anon_file_find_mem_record_by_id(pstPool, &stMem, NULL)) {
			if (!pool_anon_file_insert_mem_record(pstPool, &stMem)) {
				err = MEM_POOL_MEM_RECORD_ERROR;
				goto error;
			}
		} else if (!bHasSeen) {
			free(stMem.pAvailableBlocks), stMem.pAvailableBlocks = NULL;
			free(stMem.pUsedBlocks), stMem.pUsedBlocks = NULL;
		}
	} else if (!pool_anon_file_create_mmap(pstPool, minReallocSize, &stHandle)) {
		err = MEM_POOL_CREAT_FILE_MAPPING_ERROR;
		goto error; // Error
	} else { // New mem rec list successfully created
		((struct _LIST *)pstPool->stMemRecords.pData)->pList = stHandle.pData;
		// Init the list head
		struct _ARRAY_LIST_HEAD * pstHead = (struct _ARRAY_LIST_HEAD *)stHandle.pData;
		pstPool->pstPoolInfo->memRecListSize = pstHead->capacity = pstAllocator->reallocSize;
		pstHead->count = 0, pstHead->iteratorIndex = 0, pstHead->iteratorLocation = 0;
		pstHead->usedCapacity = sizeof(struct _ARRAY_LIST_HEAD);
		if (!pool_anon_file_insert_mem_record(pstPool, &stMem)) {
			err = MEM_POOL_MEM_RECORD_ERROR;
			goto error;
		} // Added to mem record
	}
error:
	return err;
}
static int pool_anon_file_save_pool_info (struct _MEM_POOL * pstPool, MEM_POOL_INFO * pstInfo) {
	int err = SUCCESS;
	struct _DATA_CONVERT_INFO stData = {NULL, pstInfo, NULL, 0, 0};
	__data_converter_convert(pstPool->stPoolInfo.pPacker, C_TO_PACKEDC, &stData, NULL);
	if (stData.iErr) {
		free (stData.pRight), stData.pRight = NULL;
		err = MEM_POOL_INFO_ERROR;
	} else if (stData.pRight) {
		memcpy(pstPool->stPoolInfo.pData, stData.pRight, stData.size), free (stData.pRight), stData.pRight = NULL;
	} else {
		err = MEM_POOL_INFO_ERROR;
	}
	return err;
}
static int pool_anon_file_load_pool_info (struct _MEM_POOL * pstPool, bool bRenewMemRec) {
	int err = SUCCESS;
	if (pstPool->pstPoolInfo) free(pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
	bool bHasSeen = false;
	struct _MMAP_RECORD stPoolInfoHandle;
	pool_anon_file_init_mmap(pstPool, &stPoolInfoHandle), stPoolInfoHandle.pcID = pstPool->pcID; // In case we need to save it to handle list
	MEM_POOL_INFO stPoolInfo = {0, 0, 0, 0, NULL, NULL}; // In case we need to update the pool info
	struct _DATA_CONVERT_INFO stConvert = {NULL, NULL, NULL, 0, 0};
	size_t size = sizeof(MEM_POOL_INFO) + strlen(pstPool->pcID) + strlen("_MEMORY_RECORD") + strlen("FFFFFFFF") + 1 + strlen(pstPool->pcID) + strlen("_INDEX") + strlen("FFFFFFFF") + 1;
	if (size < MEM_POOL_REALLOC_SIZE_FILE) size = MEM_POOL_REALLOC_SIZE_FILE;
	if (pool_anon_file_open_existing_mmap(pstPool, &stPoolInfoHandle, 0, &bHasSeen)) { // Existing pool
		stConvert.pLeft = pstPool->stPoolInfo.pData = stPoolInfoHandle.pData;
		__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C, &stConvert, NULL);
		if (stConvert.iErr) {
			if (stConvert.pRight) {
				__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C_FREE, &stConvert, NULL);
			}
			pstPool->pstPoolInfo = NULL;
			err = MEM_POOL_INFO_ERROR;
			goto error;
		} else {
			pstPool->pstPoolInfo = (struct _MEM_POOL_INFO *)stConvert.pRight;
		} // Existing pool but not referenced locally - need to increment the ref count
		if (!bHasSeen) pstPool->pstPoolInfo->refCount++, err = pool_anon_file_save_pool_info(pstPool, pstPool->pstPoolInfo); 
		goto error;
	}
	stPoolInfo.pcMemName = __asprintf("%s_MEMORY_RECORD", pstPool->pcID), stPoolInfo.pcIndexName = __asprintf("%s_INDEX", pstPool->pcID);
	if (stPoolInfo.pcMemName == NULL || stPoolInfo.pcIndexName == NULL) {
		err = MALLOC_NO_MEM;
		goto error;
	} // New pool
	if (!pool_anon_file_create_mmap(pstPool, size, &stPoolInfoHandle)) {
		err = MEM_POOL_CREAT_FILE_MAPPING_ERROR;
		goto error;
	} else {
		stPoolInfo.poolCount = 0, stPoolInfo.refCount = 1;
		pstPool->stPoolInfo.pData = stPoolInfoHandle.pData;
		if (err = pool_anon_file_save_pool_info(pstPool, &stPoolInfo)) goto error;
		stConvert.pLeft = pstPool->stPoolInfo.pData;
		__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C, &stConvert, NULL);
		if (stConvert.iErr) {
			if (stConvert.pRight) {
				__data_converter_convert(pstPool->stPoolInfo.pPacker, PACKEDC_TO_C_FREE, &stConvert, NULL);
			}
			pstPool->pstPoolInfo = NULL;
			err = MEM_POOL_INFO_ERROR;
			goto error;
		} else {
			pstPool->pstPoolInfo = (struct _MEM_POOL_INFO *)stConvert.pRight;
		}
	}
	
error:
	if (bRenewMemRec) err = list_refresh_pointer_mem_record(pstPool);
	free (stPoolInfo.pcMemName), stPoolInfo.pcMemName = NULL;
	free (stPoolInfo.pcIndexName), stPoolInfo.pcIndexName = NULL;
	if (err) pool_anon_file_close_mapped_file(pstPool, &stPoolInfoHandle);
	return err;
}
static int pool_anon_file_create_master_record (struct _MEM_POOL * pstPool, size_t size, struct _MEM_RECORD * pstRecord, struct _MMAP_RECORD * pstHandle, bool bUpdateMemRec) {
	int err = SUCCESS; // Will only at pstRecord->pcID 
	size_t total_size = (size / MEM_POOL_BLOCK_SIZE + (size % MEM_POOL_BLOCK_SIZE ? 1 : 0)) * MEM_POOL_BLOCK_SIZE;
	struct _MEM_RECORD_BLOCK rgstUsed[1] = {{0, size}}, rgstAval[1] = {{0, total_size}};
	pstHandle->pcID = NULL;
	if (!pool_anon_file_create_mmap (pstPool, total_size, pstHandle)) {
		err = MEM_POOL_HANDLE_ERROR;
		goto error;
	}
	pstRecord->availCount = pstRecord->usedCount = 1;
	pstRecord->pAvailableBlocks = rgstAval, pstRecord->pUsedBlocks = rgstUsed;
	pstRecord->pcMemName = pstHandle->pcID;
	if (!pstRecord->pcID) pstRecord->pcID = pstHandle->pcID;
	if (bUpdateMemRec && !pool_anon_file_insert_mem_record(pstPool, pstRecord)) {
		err = MEM_POOL_MEM_RECORD_ERROR;
		goto error;
	}
	if (err = pool_anon_file_save_pool_info(pstPool, pstPool->pstPoolInfo)) goto error;
	//__trace_shared_mem_pool(pstPool);
error:
	pstRecord->pAvailableBlocks = pstRecord->pUsedBlocks = NULL;
	return err;
}
static int pool_anon_file_create_slave_record (struct _MEM_POOL * pstPool, struct _MEM_RECORD * pstMaster, struct _MEM_RECORD_BLOCK * pstPos, struct _MEM_RECORD * pstSlave) {
	int err = SUCCESS;			// Will look at: pstMaster->pcMemName, pstSlave->pcID
	bool bIsMasterNew = false;	// pstMaster->pUsedBlocks must be a realloc'able memory
	struct _MMAP_RECORD * pstMasterHandle = NULL, stNewMasterHandle;
	unsigned int index = __list_count(pstPool->stHandles.pData) + 1;
	struct _MEM_RECORD_BLOCK rgstSlaveBlock[1] = {{pstPos->offset, pstPos->length}};
	pool_anon_file_init_mmap(pstPool, &stNewMasterHandle);
	for (__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstMasterHandle = NULL) {
		pstMasterHandle = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, &index);
		if (!strcmp(pstMasterHandle->pcID, pstMaster->pcMemName)) break;
	}
	if (pstMasterHandle) { // Master is already used by another record. Need to update the offset list
		size_t * newOffsets = (size_t *)realloc(pstMasterHandle->offsets, sizeof(size_t) * (pstMasterHandle->offsetCount + 1));
		if (!newOffsets) {
			err = MALLOC_NO_MEM;
			goto error;
		} else {
			pstMasterHandle->offsets = newOffsets;
			pstMasterHandle->offsets[pstMasterHandle->offsetCount++] = pstPos->offset;
			if (!__list_update(pstPool->stHandles.pData, index, 0, pstMasterHandle)) {
				err = MEM_POOL_HANDLE_ERROR;
				goto error;
			}
		}
	} else {
		stNewMasterHandle.pcID = pstMaster->pcMemName;
		if (!pool_anon_file_open_existing_mmap(pstPool, &stNewMasterHandle, pstPos->offset, NULL)) {
				err = MEM_POOL_HANDLE_ERROR;
				goto error;
		}
	} // Update the master's used block list
	struct _MEM_RECORD_BLOCK * pstNewList = (struct _MEM_RECORD_BLOCK *)realloc(pstMaster->pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK) * (pstMaster->usedCount + 1));
	if (!pstNewList) { // Used blocks
			err = MALLOC_NO_MEM;
			goto error;
	}
	pstMaster->pUsedBlocks =  pstNewList;
	pstMaster->pUsedBlocks[pstMaster->usedCount].length = pstPos->length;
	pstMaster->pUsedBlocks[pstMaster->usedCount++].offset = pstPos->offset;
	if (!pool_anon_file_update_mem_record(pstPool, pstMaster)) {
		err = MEM_POOL_MEM_RECORD_ERROR;
		goto error;
	}
	// Insert the slave
	pstSlave->pcMemName = pstMaster->pcMemName, pstSlave->availCount = pstSlave->usedCount = 1;
	pstSlave->pAvailableBlocks = pstSlave->pUsedBlocks = rgstSlaveBlock, pstSlave->enType = MEM_SUB_BLOCK;
	if (!pool_anon_file_insert_mem_record(pstPool, pstSlave)) {
		err = MEM_POOL_MEM_RECORD_ERROR;
		goto error;
	}
error:
	pstSlave->pAvailableBlocks = pstSlave->pUsedBlocks = NULL;
	if (pstMasterHandle) {
		free (pstMasterHandle->offsets), pstMasterHandle->offsets = NULL;
		free (pstMasterHandle), pstMasterHandle = NULL;
	}
	return err;
}
static void * list_realloc_mem_record(void * pAllocator, void * pOld, size_t size) {
	int err = SUCCESS;
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pAllocator;
	struct _MEM_RECORD stNewRecord; pool_anon_file_init_mem_record(pstPool, &stNewRecord);
	struct _MMAP_RECORD stNewHandle; pool_anon_file_init_mmap(pstPool, &stNewHandle);
	if (!pOld && size <= pstPool->pstPoolInfo->memRecListSize) return pOld; // Do nothing - still enough space

	size_t allocSize = (size / MEM_POOL_BLOCK_SIZE + (size % MEM_POOL_BLOCK_SIZE ? 1 : 0)) * MEM_POOL_BLOCK_SIZE;
	if (err = pool_anon_file_create_master_record(pstPool, allocSize, &stNewRecord, &stNewHandle, false)) goto error;
	if (pOld) memcpy(stNewHandle.pData, pOld, pstPool->pstPoolInfo->memRecListSize); // Copy the data
	
	// Update the pool info
	pstPool->pstPoolInfo->pcMemName = stNewHandle.pcID;
	pstPool->pstPoolInfo->memRecListSize = allocSize;
	if (err = pool_anon_file_save_pool_info(pstPool, pstPool->pstPoolInfo)) goto error;
	if (err = pool_anon_file_load_pool_info(pstPool, false)) goto error;
error:
	free(stNewHandle.pcID), stNewHandle.pcID = NULL;
	return stNewHandle.pData;
}
static void * list_malloc_mem_record(void * pAllocator, size_t size) {
	return list_realloc_mem_record(pAllocator, NULL, size);
}
static void list_free_mem_record(void *pAllocator, void *pData) {
}
static void * pool_anon_file_create (struct _MEM_POOL * pstParentPool, const char * pcID, struct _MEM_POOL * pstPool, struct _ERROR_HANDLER * pstError) {
	int err = SUCCESS;
	// Init members:
	pstPool->pcID = NULL, pstPool->pstPoolInfo = NULL;
	pstPool->pSubPools = pstPool->pExtra = NULL;
	pstPool->pstParent = pstParentPool, pstPool->pstError = pstError;
	memset(&pstPool->stPoolInfo, 0, sizeof(MEM_POOL_DATA_STRUCTURE));
	memset(&pstPool->stMemRecords, 0, sizeof(MEM_POOL_DATA_STRUCTURE));
	memset(&pstPool->stHandles, 0, sizeof(MEM_POOL_DATA_STRUCTURE));
	// Extra
	if (!(pstPool->pExtra = malloc(sizeof(struct _SHARED_MEM_EXTRA)))) {
		pstError->handle_error(pstError->pHandler, err = FATAL_CREATION_ERROR, "Failed to allocate memory to construct the pool");
		goto error;
	}
	memset(pstPool->pExtra, 0, sizeof(struct _SHARED_MEM_EXTRA));
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.allocate = list_malloc_mem_record;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.reallocate = list_realloc_mem_record;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.memfree = list_free_mem_record;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.reallocSize = MEM_POOL_REALLOC_SIZE_FILE;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.expansionSize = 2;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.expansionFactor = 1;
	((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator.pAllocator = pstPool;
	// Sub-pool
	if (!__list_create(&pstPool->pSubPools, ARRAY_LIST_FLAT, NULL, NULL)) {
		pstError->handle_error(pstError->pHandler, err = FATAL_CREATION_ERROR, "Failed to allocate memory to construct the sub-pool list");
		goto error;
	}
	// Pool info
	if (err = pool_anon_file_create_simple_data(&pstPool->stPoolInfo, "struct {unsigned int; unsigned int; unsigned int; unsigned int; string; string;};")) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to create data structure for the pool information");
		goto error;
	}
	// Memory & handle lists
	if (err = pool_anon_file_create_list(&pstPool->stMemRecords,
		"struct {string; string; unsigned int; unsigned int avail; varray {size_t; size_t;}[avail]; unsigned int used; varray {size_t; size_t;}[used];};",
		ARRAY_LIST_FLAT, &((struct _SHARED_MEM_EXTRA *)(pstPool->pExtra))->stMemRecordAllocator)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to create data structure for the pool memory records");
		goto error;
	}
	if (err = pool_anon_file_create_list(&pstPool->stHandles, "struct {string; void *; void *; unsigned int count; varray {size_t;}[count];};", LINKED_LIST_POINTER, NULL)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to create data structure for the pool mmap list");
		goto error;
	} // Get name (ID)
	if ((pstPool->pcID = pool_anon_file_gen_name(pstParentPool, pcID)) == NULL) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to name the pool");
		err = MEM_POOL_NAME_ERROR;
		goto error;
	}
	// Load the info after getting the name
	if (err = pool_anon_file_load_pool_info(pstPool, true)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to load the pool's infomation");
		goto error;
	}
	if (err = pool_anon_file_save_pool_info(pstPool, pstPool->pstPoolInfo)) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to save the pool's infomation");
		goto error;
	}
	if (pstPool->pstPoolInfo) free(pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
	__trace_shared_mem_pool (pstPool);
error:
	if (err) {
		if (pstPool->pSubPools) __list_destroy(pstPool->pSubPools), pstPool->pSubPools = NULL;
		pool_anon_file_destroy_list(&pstPool->stMemRecords);
		pool_anon_file_destroy_list(&pstPool->stHandles);
		pool_anon_file_destroy_simple_data(&pstPool->stPoolInfo);
		if (pstPool->pExtra) {
			free(pstPool->pExtra), pstPool->pExtra = NULL;
		}
		if (pstPool->pcID) free(pstPool->pcID), pstPool->pcID = NULL;
		return NULL;
	} else {
		return pstPool;
	}
}
static void pool_anon_file_destroy (struct _MEM_POOL * pstPool) {
	if (pool_anon_file_load_pool_info(pstPool, true)) return;
	__trace_shared_mem_pool(pstPool);

	for (__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); ) {
		struct _MMAP_RECORD * pstHandle = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, NULL);
		if (!strcmp(pstHandle->pcID, pstPool->pcID) || !strcmp(pstHandle->pcID, pstPool->pstPoolInfo->pcMemName)) continue;
		for (unsigned int i = 0; i < pstHandle->offsetCount; i++) {
			struct _MEM_RECORD stToDelete;
			stToDelete.pcMemName = pstHandle->pcID;
			if (pool_anon_file_find_mem_record_by_name_offset(pstPool, pstHandle->offsets[i], &stToDelete, NULL)) {
				pool_anon_file_deref_mem_record(pstPool, &stToDelete, false);
			}
		}
	} // Unmap view / close handle for all
	for (__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData);) {
		struct _MMAP_RECORD * pstHandle = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, NULL);
		pool_anon_file_close_mapped_file(pstPool, pstHandle);
	}
	free(pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
	__list_destroy(pstPool->pSubPools), pstPool->pSubPools = NULL;
	pool_anon_file_destroy_list(&pstPool->stMemRecords);
	pool_anon_file_destroy_list(&pstPool->stHandles);
	pool_anon_file_destroy_simple_data(&pstPool->stPoolInfo);
	free(pstPool->pExtra), pstPool->pExtra = NULL;
	free(pstPool->pcID), pstPool->pcID = NULL;
}
static int pool_anon_file_malloc_anon(struct _MEM_POOL * pstPool, size_t size, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	struct _MEM_RECORD stMemRec;
	struct _MMAP_RECORD stHandle;
	if (err = pool_anon_file_load_pool_info(pstPool, true)) goto error;
	stMemRec.pcID = NULL;
	if (err = pool_anon_file_create_master_record(pstPool, size, &stMemRec, &stHandle, true)) goto error;
	pstPtr->pID = stHandle.pcID, pstPtr->offset = 0, pstPtr->size = size, pstPtr->pData = stHandle.pData;
error:
	free (pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
	return err;
}
static int pool_anon_file_reincarnate(struct _MEM_POOL * pstPool, size_t size, struct _MEM_RECORD * pstRecord, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	struct _MEM_RECORD_BLOCK stFitRecord = {0, 0}, *pstUsed = NULL;
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	struct _MEM_RECORD stCloned, stNew;
	 pool_anon_file_init_mem_record(pstPool, &stCloned);
	 pool_anon_file_init_mem_record(pstPool, &stNew);

	if (pstRecord->enType == pstPool->enType && pool_anon_file_check_fit(pstRecord, size, &stFitRecord) && stFitRecord.offset == 0) { // Case 4.1: the master can be reincarnated directly
		stHandle.pcID = pstRecord->pcMemName; // Open local handle
		bool bHasSeen = false;
		if (!pool_anon_file_open_existing_mmap(pstPool, &stHandle, stFitRecord.offset, &bHasSeen)) {
			err = MEM_POOL_HANDLE_ERROR;
			goto error;
		} // If it's already there, need to insert the offset 0 for the reincarnated master
		if (bHasSeen && (err = pool_anon_file_insert_mmap_offset (pstPool, &stHandle, stFitRecord.offset))) goto error;
		if (!(pstUsed = (struct _MEM_RECORD_BLOCK *)realloc(pstRecord->pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK) * (pstRecord->usedCount + 1)))) {
			err = MALLOC_NO_MEM;
			goto error;
		} // Insert the new fit record into the used list
		pool_anon_file_insert_block_list(pstRecord->pUsedBlocks = pstUsed, ++pstRecord->usedCount, &stFitRecord);
		if (!pool_anon_file_update_mem_record(pstPool, pstRecord)) { // Update the record with the new list
			err = MEM_POOL_MEM_RECORD_ERROR;
			goto error;
		}
		pstPtr->offset = 0, pstPtr->size = size, pstPtr->pData = stHandle.pData;
	} else { // Case 4.2: need to rename the old record and create a new master
		if (err = pool_anon_file_deep_clone_mem_record (&stCloned, pstRecord)) goto error;
		stNew.pcID = stCloned.pcID; // Save the record's original ID
		if (!(stCloned.pcID = pool_anon_file_next_avail_name(pstPool))) {
			err = MEM_POOL_NAME_ERROR;
			goto error;
		} // Rename the free'd record - get a new ID for it
		pool_anon_file_remove_mem_record(pstPool, pstRecord); // Remove the existing one
		if (!pool_anon_file_insert_mem_record(pstPool, &stCloned)) { // Insert the one with newly obtained ID
			err = MEM_POOL_MEM_RECORD_ERROR;
			goto error;
		}
		stNew.enType = pstPool->enType;
		if (err = pool_anon_file_create_master_record(pstPool, size, &stNew, &stHandle, true)) goto error;
		free(stHandle.pcID), stHandle.pcID = NULL;
		free(stNew.pcID), stNew.pcID = NULL;
		pstPtr->pData = stHandle.pData, pstPtr->offset = 0, pstPtr->size = size;
	}
error:
	pool_anon_file_deep_destroy_mem_record(&stCloned);
	return err;
}
static int pool_anon_file_relocate(struct _MEM_POOL * pstPool, size_t size, struct _MEM_RECORD * pstRecord, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	bool bFit = false;
	char * pcOldName = NULL;
	void * pPayload = NULL;
	struct _MEM_RECORD_BLOCK stFitPos = {0, 0}, stOldPos = {0, 0};
	struct _MEM_RECORD stCloned, stDeref, *pstFitter = NULL, *pstAfterRef = NULL;
	pool_anon_file_init_mem_record(pstPool, &stCloned);
	memcpy(&stDeref, pstRecord, sizeof(struct _MEM_RECORD));
	struct _MMAP_RECORD stOld, stNew;
	pool_anon_file_init_mmap(pstPool, &stOld);
	pool_anon_file_init_mmap(pstPool, &stNew);

	if (err = pool_anon_file_deep_clone_mem_record(&stCloned, pstRecord)) goto error; // Save the existing info: mem record, handle and stOldPos
	memcpy(&stOldPos, pstRecord->pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK));
	stOld.pcID = stCloned.pcMemName; // Don't use pstRecord from here
	if (!pool_anon_file_open_mapped_file(pstPool, &stOld)) {
		err = MEM_POOL_HANDLE_ERROR;
		goto error;
	}
	if (!(pPayload = malloc(stOldPos.length))) {
		err = MALLOC_NO_MEM;
		goto error;
	}
	memcpy(pPayload, (char *)stOld.pData + stOldPos.offset, stOldPos.length); // Copy the data first
	pool_anon_file_close_mapped_file(pstPool, &stOld);
	pool_anon_file_deref_mem_record(pstPool, &stDeref, true); // Must de-ref before searching for a new fit
	__trace_shared_mem_pool(pstPool);
	pstRecord->pAvailableBlocks = pstRecord->pUsedBlocks = NULL, stDeref.pcID = pstRecord->pcID;

	if (pool_anon_file_find_mem_record_by_id(pstPool, &stDeref, NULL)) { // Case 2.1: after de-ref, the record is still found (must be a master used by others) - need to rename it
		memcpy(stCloned.pUsedBlocks, stDeref.pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK) * (stCloned.usedCount = stDeref.usedCount));
		memcpy(stCloned.pAvailableBlocks, stDeref.pAvailableBlocks, sizeof(struct _MEM_RECORD_BLOCK) * (stCloned.availCount = stDeref.availCount));
		pool_anon_file_remove_mem_record(pstPool, &stDeref);
		pcOldName = stCloned.pcID;
		if (!(stCloned.pcID = pool_anon_file_next_avail_name(pstPool))) { // Get a new ID
			err = MEM_POOL_NAME_ERROR;
			goto error;
		}
		if (!pool_anon_file_insert_mem_record(pstPool, &stCloned)) {
			err = MEM_POOL_MEM_RECORD_ERROR; // Insert the record with the new ID
			goto error;
		}
		free(stCloned.pcID), stCloned.pcID = pcOldName;
		pcOldName = NULL, stDeref.pcID = pstRecord->pcID;
	} // else { } // Case 2.2: after de-ref, the record is gone altogether

	for(__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData); pstFitter = NULL) {
		pstFitter = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, NULL);
		if (pool_anon_file_check_fit(pstFitter, size, &stFitPos)) break;
	}
	if (pstFitter) {
		if (err = pool_anon_file_create_slave_record(pstPool, pstFitter, &stFitPos, &stDeref)) goto error;
		stNew.pcID = stDeref.pcMemName;
		if (!pool_anon_file_open_existing_mmap(pstPool, &stNew, stFitPos.offset, NULL)) {
			err = MEM_POOL_HANDLE_ERROR;
			goto error;
		}
		memcpy(pstPtr->pData = (char *)stNew.pData + stFitPos.offset, pPayload, stOldPos.length);
		pstPtr->offset = stFitPos.offset, pstPtr->size = stFitPos.length;
	} else {
		stDeref.enType = pstPool->enType;
		if (err = pool_anon_file_create_master_record(pstPool, size, &stDeref, &stNew, true)) goto error;
		memcpy(stNew.pData, pPayload, stOldPos.length);
		pstPtr->pData = stNew.pData; pstPtr->size = size, pstPtr->offset = 0;
		if (stNew.pcID) free(stNew.pcID), stNew.pcID = NULL;
	}
error:
	pool_anon_file_deep_destroy_mem_record(&stCloned);
	if (pPayload) free (pPayload), pPayload = NULL;
	if (pcOldName) free(pcOldName), pcOldName = NULL;
	if (pstFitter) {
		if (pstFitter->pAvailableBlocks) free(pstFitter->pAvailableBlocks), pstFitter->pAvailableBlocks = NULL;
		if (pstFitter->pUsedBlocks) free(pstFitter->pUsedBlocks), pstFitter->pUsedBlocks = NULL;
		free(pstFitter), pstFitter = NULL;
	}
	return err;
}
static int pool_anon_file_expand(struct _MEM_POOL * pstPool, size_t size, struct _MEM_RECORD * pstRecord, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	struct _MEM_RECORD stRemovedSelf;
	struct _MEM_RECORD_BLOCK stFitRecord = {0, 0};
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	//void * pData = NULL;

	if (err = pool_anon_file_deep_clone_mem_record (&stRemovedSelf, pstRecord)) goto error;
	for (unsigned int i = 1; i < stRemovedSelf.usedCount; i++) {
		stRemovedSelf.pUsedBlocks[i - 1].length = stRemovedSelf.pUsedBlocks[i].length;
		stRemovedSelf.pUsedBlocks[i - 1].offset = stRemovedSelf.pUsedBlocks[i].offset;
	}
	stRemovedSelf.usedCount--;

	if (pool_anon_file_check_fit(&stRemovedSelf, size, &stFitRecord)) { // Case 1: check the record itself. If there's still enough space left, there'll be no need to create any master
		stHandle.pcID = pstRecord->pcMemName;
		if (!(pstPtr->pData = pool_anon_file_open_existing_mmap(pstPool, &stHandle, stFitRecord.offset, NULL))) {
			err = MEM_POOL_HANDLE_ERROR;
			goto error;
		}
		//if (!(pData = malloc(pstRecord->pUsedBlocks[0].length))) {
		//	err = MALLOC_NO_MEM;
		//	goto error;
		//} // No need to copy the data or update offset, since it's the master expanding itself, offset remains unchanged - only length is changed.
		pool_anon_file_insert_block_list(stRemovedSelf.pUsedBlocks, ++stRemovedSelf.usedCount, &stFitRecord);
		if (!pool_anon_file_update_mem_record(pstPool, &stRemovedSelf)) {
			err = MEM_POOL_MEM_RECORD_ERROR;
			goto error;
		}
		pstPtr->offset = stFitRecord.offset, pstPtr->size = stFitRecord.length;
	} else { // Still cannot fit - go to case 2
		err = pool_anon_file_relocate(pstPool, size, pstRecord, pstPtr);
	}
error:
	pool_anon_file_deep_destroy_mem_record(&stRemovedSelf);
	return err;
}
static int pool_anon_file_do_alloc(struct _MEM_POOL * pstPool, bool bResize, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr) {
	int err = SUCCESS;
	bool bExactMatch = false, bCanFit = false;
	struct _MEM_RECORD_BLOCK fitRecord = {0, 0};
	struct _MEM_RECORD stMaster, stSlave, * pstRecord = NULL;
	pool_anon_file_init_mem_record(pstPool, &stMaster), pool_anon_file_init_mem_record(pstPool, &stSlave);
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);

	if (err = pool_anon_file_load_pool_info(pstPool, true)) goto error;

	for (__list_start_iterator(pstPool->stMemRecords.pData, 0); __list_has_next(pstPool->stMemRecords.pData); pstRecord = NULL) {
		pstRecord = (struct _MEM_RECORD *)__list_next(pstPool->stMemRecords.pData, NULL, NULL);
		if (!bExactMatch && strcmp(pstRecord->pcID, pcKey) == 0) {
			pstPtr->pData = NULL, bExactMatch = true;
			if (pstRecord->usedCount == 0 || pstRecord->availCount == 0 || pstRecord->pUsedBlocks[0].offset != pstRecord->pAvailableBlocks[0].offset) {
				pstPtr->offset = pstPtr->size = 0; // Record is probably already free'd
			} else {
				pstPtr->offset = pstRecord->pUsedBlocks[0].offset, pstPtr->size = pstRecord->pUsedBlocks[0].length;
			}
			if (err = pool_anon_file_deep_clone_mem_record (&stSlave, pstRecord)) goto error;
		} else if (!bCanFit) {
			if (bCanFit = pool_anon_file_check_fit(pstRecord, size, &fitRecord)) {
				if (err = pool_anon_file_deep_clone_mem_record (&stMaster, pstRecord)) goto error;
			}
		} // Must deep clone the records here cuz the list may get resized and the pointers may become invalid due to packer's lazy malloc strategy
		if (bExactMatch) break; // If an existing block is found, there is no need to look at bFit because the existing block will have to be de-ref'd and the bFit will no longer be valid
	}
	if (bExactMatch) {
		if ((pstPtr->size > 0 && pstPtr->size >= size) || !bResize) {
			stHandle.pcID = stSlave.pcMemName;
			bool bHasSeen = false;
			if (!(pstPtr->pData = pool_anon_file_open_existing_mmap(pstPool, &stHandle, pstPtr->offset, &bHasSeen))) {
				err = MEM_POOL_HANDLE_ERROR;
				goto error;
			}
			if (pstPtr->size < size) {
				err = MEM_POOL_SIZE_MISMATCH;
			}
			pstPtr->pData = (char *)pstPtr->pData + pstPtr->offset;
		} else if (!pstPtr->size) { // Case 4: record is already free'd (should only occur when stSlave is a master block)
			if (err = pool_anon_file_reincarnate(pstPool, size, &stSlave, pstPtr)) goto error;
		} else if (stSlave.enType == pstPool->enType) { // Case 1: master should be given a chance to expand itself before relocating
			if (err = pool_anon_file_expand(pstPool, size, &stSlave, pstPtr)) goto error;
		} else if (stSlave.enType == MEM_SUB_BLOCK) { // Case 2: need to relocate
			if (err = pool_anon_file_relocate(pstPool, size, &stSlave, pstPtr)) goto error;
		} else {
			err = MEM_POOL_BLOCK_CANNOT_CHANGE_SIZE;
			goto error;
		}
	} else if (bCanFit) {
		if (fitRecord.offset) { // Become slave to an existing master
			stSlave.pcID = (char *)pcKey;
			err = pool_anon_file_create_slave_record(pstPool, &stMaster, &fitRecord, &stSlave);
			stSlave.pcID = stSlave.pcMemName = NULL;
			if (err) goto error;
			stHandle.pcID = stMaster.pcMemName;
			if (!pool_anon_file_open_existing_mmap(pstPool, &stHandle, fitRecord.offset, NULL)) {
				err = MEM_POOL_HANDLE_ERROR;
				goto error;
			}
		} else { // Case 3: replace the existing master (the original master is already free'd)
			stHandle.pcID = stMaster.pcMemName;
			bool bHasSeen = false;
			if (!pool_anon_file_open_existing_mmap(pstPool, &stHandle, 0, &bHasSeen)) {
				err = MEM_POOL_HANDLE_ERROR;
				goto error;
			}
			if (bHasSeen && (err = pool_anon_file_insert_mmap_offset(pstPool, &stHandle, fitRecord.offset))) goto error;
			struct _MEM_RECORD_BLOCK * pstNewUsedList = (struct _MEM_RECORD_BLOCK *)realloc(stMaster.pUsedBlocks, sizeof(struct _MEM_RECORD_BLOCK) * (stMaster.usedCount + 1));
			if (!pstNewUsedList) {
				err = MALLOC_NO_MEM;
				goto error;
			}
			pool_anon_file_insert_block_list(stMaster.pUsedBlocks = pstNewUsedList, ++stMaster.usedCount, &fitRecord);
			stSlave.pcID = stMaster.pcID;  // Remove the original master by specifying only its ID. The ID is stored in stSlave
			pool_anon_file_remove_mem_record(pstPool, &stSlave), stMaster.pcID = (char *)pcKey;
			if (!pool_anon_file_insert_mem_record(pstPool, &stMaster)) {
				stMaster.pcID = NULL, err = MEM_POOL_MEM_RECORD_ERROR;
				goto error;
			} else {
				stMaster.pcID = NULL;
			}
		}
		pstPtr->pData = (char *)stHandle.pData + fitRecord.offset; pstPtr->size = fitRecord.length, pstPtr->offset = fitRecord.offset;
	} else {
		struct _MEM_RECORD stNamed; pool_anon_file_init_mem_record(pstPool, &stNamed);
		stNamed.pcID = (char *)pcKey;
		if (err = pool_anon_file_create_master_record(pstPool, size, &stNamed, &stHandle, true)) goto error;
		if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
		pstPtr->pData = stHandle.pData; pstPtr->size = size, pstPtr->offset = 0;
	}
	__trace_shared_mem_pool(pstPool);
	pstPtr->pID = pcKey;
error:
	if (pstRecord) {
		if (pstRecord->pAvailableBlocks) free(pstRecord->pAvailableBlocks), pstRecord->pAvailableBlocks = NULL;
		if (pstRecord->pUsedBlocks) free(pstRecord->pUsedBlocks), pstRecord->pUsedBlocks = NULL;
		free(pstRecord), pstRecord = NULL;
	}
	pool_anon_file_deep_destroy_mem_record(&stMaster);
	pool_anon_file_deep_destroy_mem_record(&stSlave);
	if (pstPool->pstPoolInfo) free (pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
	return err;
}
static void * pool_anon_file_malloc(struct _MEM_POOL * pstPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr) {
	if (!pcKey)
		pstPtr->err = pool_anon_file_malloc_anon(pstPool, size, pstPtr);
	else
		pstPtr->err = pool_anon_file_do_alloc(pstPool, false, pcKey, size, pstPtr);
	return pstPtr->pData;
}
static void * pool_anon_file_realloc(struct _MEM_POOL * pstPool, const char * pcKey, size_t size, struct _MEM_PTR * pstPtr) {
	if (!pcKey)
		pstPtr->err = pool_anon_file_malloc_anon(pstPool, size, pstPtr);
	else
		pstPtr->err = pool_anon_file_do_alloc(pstPool, true, pcKey, size, pstPtr);
	return pstPtr->pData;
}
static void pool_anon_file_free (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	pstPtr->err = SUCCESS;
	struct _MEM_RECORD stToFree; pool_anon_file_init_mem_record(pstPool, &stToFree);
	stToFree.pcID = (char *)pstPtr->pID;
	if (pstPtr->err = pool_anon_file_load_pool_info(pstPool, true)) goto error;
	if (pool_anon_file_find_mem_record_by_id(pstPool, &stToFree, NULL) && stToFree.enType != MEM_EXTERN) {
		pool_anon_file_deref_mem_record(pstPool, &stToFree, true);
	} else { // FIXME: add extern blocks to the system
	}
error:
	free(stToFree.pAvailableBlocks), stToFree.pAvailableBlocks = NULL;
	free(stToFree.pUsedBlocks), stToFree.pUsedBlocks = NULL;
}
static void * pool_anon_file_open (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	pstPtr->err = SUCCESS, pstPtr->pData = NULL;
	struct _MEM_RECORD stRecord; pool_anon_file_init_mem_record(pstPool, &stRecord);
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	stRecord.pcID = (char *)pstPtr->pID;
	if (pstPtr->err = pool_anon_file_load_pool_info(pstPool, true)) goto error;
	if (pool_anon_file_find_mem_record_by_id(pstPool, &stRecord, NULL)) {
		stHandle.pcID = stRecord.pcMemName;
		if (!pool_anon_file_open_existing_mmap(pstPool, &stHandle, stRecord.pUsedBlocks[0].offset, NULL)) {
			pstPtr->pData = NULL;
			pstPtr->err = MEM_POOL_BLOCK_NOT_FOUND;
		} else if (!stRecord.availCount || !stRecord.usedCount || stRecord.pAvailableBlocks[0].offset != stRecord.pUsedBlocks[0].offset) {
			pstPtr->pData = NULL;
			pstPtr->err = MEM_POOL_BLOCK_NOT_FOUND;
		} else {
			pstPtr->pData = (char *)stHandle.pData + stRecord.pUsedBlocks[0].offset;
		}
	} else {
		pstPtr->err = MEM_POOL_BLOCK_NOT_FOUND;
	}
error:
	free(stRecord.pAvailableBlocks), stRecord.pAvailableBlocks = NULL;
	free(stRecord.pUsedBlocks), stRecord.pUsedBlocks = NULL;
	free(stHandle.offsets), stHandle.offsets = NULL;
	return pstPtr->pData;
}
static void pool_anon_file_close (struct _MEM_POOL * pstPool, struct _MEM_PTR * pstPtr) {
	// Nothing needs to be done here. Can be used to perform garbage collection
}
static void pool_anon_file_gc(struct _MEM_POOL * pstPool) {
	struct _MMAP_RECORD * pstHandle = NULL;
	struct _MEM_RECORD stRecord; pool_anon_file_init_mem_record(pstPool, &stRecord);
	unsigned int uiHandleIndex = __list_count(pstPool->stHandles.pData), uiOffsetIndex = 0;

	for (__list_start_iterator(pstPool->stHandles.pData, 0); __list_has_next(pstPool->stHandles.pData); pstHandle = NULL) {
		pstHandle = (struct _MMAP_RECORD *)__list_next(pstPool->stHandles.pData, NULL, &uiHandleIndex);
		stRecord.pcMemName = pstHandle->pcID;
		for (unsigned int i = 0; i < pstHandle->offsetCount; i++) {
			if (!pool_anon_file_find_mem_record_by_name_offset(pstPool, pstHandle->offsets[i], &stRecord, NULL)) {
				uiOffsetIndex = i;
				goto collect; // Just collect one
			} else {
				if (stRecord.pAvailableBlocks) free(stRecord.pAvailableBlocks), stRecord.pAvailableBlocks = NULL;
				if (stRecord.pUsedBlocks) free(stRecord.pUsedBlocks), stRecord.pUsedBlocks = NULL;
			}
		}
	}
collect:
	if (stRecord.pAvailableBlocks) free(stRecord.pAvailableBlocks), stRecord.pAvailableBlocks = NULL;
	if (stRecord.pUsedBlocks) free(stRecord.pUsedBlocks), stRecord.pUsedBlocks = NULL;
	if (pstHandle) {
		for (unsigned int i = uiOffsetIndex; i + 1 < pstHandle->offsetCount; i++) {
			pstHandle->offsets[i] = pstHandle->offsets[i + 1];
		}
		if (pstHandle->offsetCount) pstHandle->offsetCount--;
		if (pstHandle->offsetCount) __list_update(pstPool->stHandles.pData, uiHandleIndex, 0, pstHandle);
		else __list_remove(pstPool->stHandles.pData, uiHandleIndex);
		if (pstHandle->offsets) free(pstHandle->offsets), pstHandle->offsets = NULL;
		free(pstHandle), pstHandle = NULL;
	}
} // Allocator for mem record list
void __mem_pool_set_func_ptr_anon_file (struct _MEM_POOL *pstPool) {
		pstPool->mem_pool_create = pool_anon_file_create;
		pstPool->mem_pool_gc = pool_anon_file_gc;
		pstPool->mem_pool_destroy = pool_anon_file_destroy;
		pstPool->mem_pool_malloc = pool_anon_file_malloc;
		pstPool->mem_pool_realloc = pool_anon_file_realloc;
		pstPool->mem_pool_free = pool_anon_file_free;
		pstPool->mem_pool_open = pool_anon_file_open;
		pstPool->mem_pool_close = pool_anon_file_close;
}
/****************** (In-development) Test Cases ******************/
static void __test_mem_pool_relocate_slave(struct _MEM_POOL * pstPool) { // Case 2 (must retain data)
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	struct _MEM_PTR stPtr = {NULL, 0, 0, NULL};
	void * pTemp = NULL;
	struct _MEM_RECORD stMaster = {"Master", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stSlave = {"Slave", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stNewMem = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	struct _MEM_RECORD_BLOCK stSlaveBlock = {128, 128};

	stHandle.pcID = stMaster.pcMemName;
	err = pool_anon_file_create_master_record(pstPool, 512, &stMaster, &stHandle, true);
	stMaster.pAvailableBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	stMaster.pUsedBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	if (!stMaster.pAvailableBlocks || !stMaster.pUsedBlocks) goto error;
	stMaster.pAvailableBlocks[0].length = 512, stMaster.pAvailableBlocks[0].offset = 0;
	stMaster.pUsedBlocks[0].length = 64, stMaster.pUsedBlocks[0].offset = 0;

	err = pool_anon_file_create_slave_record(pstPool, &stMaster, &stSlaveBlock, &stSlave);
	__trace_shared_mem_pool(pstPool);

	//err = pool_anon_file_do_alloc(pstPool, true, "Slave", 129, &stPtr);	// Fit to existing
	err = pool_anon_file_do_alloc(pstPool, true, "Slave", 500, &stPtr);	// Create new
	__trace_shared_mem_pool(pstPool);
error:
	if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
	if (stMaster.pAvailableBlocks) free (stMaster.pAvailableBlocks), stMaster.pAvailableBlocks = NULL;
	if (stMaster.pUsedBlocks) free (stMaster.pUsedBlocks), stMaster.pUsedBlocks = NULL;
}
static void __test_mem_pool_expand_relocate_master(struct _MEM_POOL * pstPool) { // Case 1 (must retain data)
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	struct _MEM_PTR stPtr = {NULL, 0, 0, NULL};
	void * pTemp = NULL;
	struct _MEM_RECORD stMaster = {"Master", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stSlave = {"Slave", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stNewMem = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	struct _MEM_RECORD_BLOCK stSlaveBlock = {128, 115};

	stHandle.pcID = stMaster.pcMemName;
	err = pool_anon_file_create_master_record(pstPool, 512, &stMaster, &stHandle, true);
	stMaster.pAvailableBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	stMaster.pUsedBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	if (!stMaster.pAvailableBlocks || !stMaster.pUsedBlocks) goto error;
	stMaster.pAvailableBlocks[0].length = 512, stMaster.pAvailableBlocks[0].offset = 0;
	stMaster.pUsedBlocks[0].length = 64, stMaster.pUsedBlocks[0].offset = 0;

	err = pool_anon_file_create_slave_record(pstPool, &stMaster, &stSlaveBlock, &stSlave);
	__trace_shared_mem_pool(pstPool);

	//err = pool_anon_file_do_alloc(pstPool, true, "Master", 400, &stPtr);	// Cannot fit to existing
	err = pool_anon_file_do_alloc(pstPool, true, "Master", 115, &stPtr);	// Can fit to existing
	__trace_shared_mem_pool(pstPool);
error:
	if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
	if (stMaster.pAvailableBlocks) free (stMaster.pAvailableBlocks), stMaster.pAvailableBlocks = NULL;
	if (stMaster.pUsedBlocks) free (stMaster.pUsedBlocks), stMaster.pUsedBlocks = NULL;
}
static void __test_mem_pool_reincarnate_master(struct _MEM_POOL * pstPool) { // Case 4 (no need to retain data)
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	struct _MEM_PTR stPtr = {NULL, 0, 0, NULL};
	void * pTemp = NULL;
	struct _MEM_RECORD stMaster = {"Master", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stSlave = {"Slave", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stNewMem = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL},
		stToBeRemoved = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	struct _MEM_RECORD_BLOCK stSlaveBlock = {128, 115};

	stHandle.pcID = stMaster.pcMemName;
	err = pool_anon_file_create_master_record(pstPool, 512, &stMaster, &stHandle, true);
	stMaster.pAvailableBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	stMaster.pUsedBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	if (!stMaster.pAvailableBlocks || !stMaster.pUsedBlocks) goto error;
	stMaster.pAvailableBlocks[0].length = 512, stMaster.pAvailableBlocks[0].offset = 0;
	stMaster.pUsedBlocks[0].length = 64, stMaster.pUsedBlocks[0].offset = 0;

	err = pool_anon_file_create_slave_record(pstPool, &stMaster, &stSlaveBlock, &stSlave);
	__trace_shared_mem_pool(pstPool);

	stToBeRemoved.pcID = stMaster.pcID; // De-ref the master
	pTemp = pool_anon_file_find_mem_record_by_id(pstPool, &stToBeRemoved, NULL);
	pool_anon_file_deref_mem_record(pstPool, &stToBeRemoved, true);
	__trace_shared_mem_pool(pstPool);

	//err = pool_anon_file_do_alloc(pstPool, true, "Master", 400, &stPtr);	// Cannot fit to existing
	err = pool_anon_file_do_alloc(pstPool, true, "Master", 115, &stPtr);	// Can fit to existing
	__trace_shared_mem_pool(pstPool);
error:
	if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
	if (stMaster.pAvailableBlocks) free (stMaster.pAvailableBlocks), stMaster.pAvailableBlocks = NULL;
	if (stMaster.pUsedBlocks) free (stMaster.pUsedBlocks), stMaster.pUsedBlocks = NULL;
}
static void __test_mem_pool_replace_master(struct _MEM_POOL * pstPool) { // Case 3 (no need to retain data)
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle; pool_anon_file_init_mmap(pstPool, &stHandle);
	struct _MEM_PTR stPtr = {NULL, 0, 0, NULL};
	void * pTemp = NULL;
	struct _MEM_RECORD stMaster = {"Master", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stSlave = {"Slave", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stNewMem = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL},
		stToBeRemoved = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	struct _MEM_RECORD_BLOCK stSlaveBlock = {128, 115};

	stHandle.pcID = stMaster.pcMemName;
	err = pool_anon_file_create_master_record(pstPool, 512, &stMaster, &stHandle, true);
	stMaster.pAvailableBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	stMaster.pUsedBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	if (!stMaster.pAvailableBlocks || !stMaster.pUsedBlocks) goto error;
	stMaster.pAvailableBlocks[0].length = 512, stMaster.pAvailableBlocks[0].offset = 0;
	stMaster.pUsedBlocks[0].length = 64, stMaster.pUsedBlocks[0].offset = 0;

	err = pool_anon_file_create_slave_record(pstPool, &stMaster, &stSlaveBlock, &stSlave);
	__trace_shared_mem_pool(pstPool);

	stToBeRemoved.pcID = stMaster.pcID; // De-ref the master
	pTemp = pool_anon_file_find_mem_record_by_id(pstPool, &stToBeRemoved, NULL);
	pool_anon_file_deref_mem_record(pstPool, &stToBeRemoved, true);
	__trace_shared_mem_pool(pstPool);

	//err = pool_anon_file_do_alloc(pstPool, true, "Master", 400, &stPtr);	// Cannot fit to existing
	err = pool_anon_file_do_alloc(pstPool, true, "Master2", 115, &stPtr);	// Can fit to existing
	__trace_shared_mem_pool(pstPool);
error:
	if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
	if (stMaster.pAvailableBlocks) free (stMaster.pAvailableBlocks), stMaster.pAvailableBlocks = NULL;
	if (stMaster.pUsedBlocks) free (stMaster.pUsedBlocks), stMaster.pUsedBlocks = NULL;
}
static void __test_mem_pool_realloc(struct _MEM_POOL * pstPool) {
	__test_mem_pool_relocate_slave(pstPool); // Case 2 (must retain data)
	if (pool_anon_file_load_pool_info(pstPool, true)) goto error;
	__test_mem_pool_expand_relocate_master(pstPool); // Case 1 (must retain data)
	if (pool_anon_file_load_pool_info(pstPool, true)) goto error;
	__test_mem_pool_reincarnate_master(pstPool); // Case 4 (no need to retain data)
	if ( pool_anon_file_load_pool_info(pstPool, true)) goto error;
	__test_mem_pool_replace_master(pstPool); // Case 3 (no need to retain data)
	if (pool_anon_file_load_pool_info(pstPool, true)) goto error;
error:
	return;
}
static void __test_mem_pool_deref(struct _MEM_POOL * pstPool) {
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle;
	void * pTemp = NULL;
	struct _MEM_RECORD stMem = {"NOTEST", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stMem2 = {"NOTEST_TEST", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stMem3 = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	
	err = pool_anon_file_create_master_record(pstPool, 16, &stMem, &stHandle, true);
	stMem.pAvailableBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	stMem.pUsedBlocks = (struct _MEM_RECORD_BLOCK *)malloc(sizeof(struct _MEM_RECORD_BLOCK) * 1);
	if (stMem.pAvailableBlocks && stMem.pUsedBlocks) {
		stMem.pAvailableBlocks[0].length = 128, stMem.pAvailableBlocks[0].offset = 0;
		stMem.pUsedBlocks[0].length = 16, stMem.pUsedBlocks[0].offset = 0;
		struct _MEM_RECORD_BLOCK stBlock = {17, 64};
		err = pool_anon_file_create_slave_record(pstPool, &stMem, &stBlock, &stMem2);
		__trace_shared_mem_pool(pstPool);

		stMem3.pcID = stMem.pcID; // stMem (master)
		pTemp = pool_anon_file_find_mem_record_by_id(pstPool, &stMem3, NULL);
		pool_anon_file_deref_mem_record(pstPool, &stMem3, true);
		__trace_shared_mem_pool(pstPool);

		stMem3.pcMemName = stMem.pcMemName; // Offset: 17 - stMem2 (slave)
		pTemp = pool_anon_file_find_mem_record_by_name_offset(pstPool, 17, &stMem3, NULL);
		pool_anon_file_deref_mem_record(pstPool, &stMem3, true);
		__trace_shared_mem_pool(pstPool);

		if (stMem3.pAvailableBlocks) free (stMem3.pAvailableBlocks), stMem3.pAvailableBlocks = NULL;
		if (stMem3.pUsedBlocks) free (stMem3.pUsedBlocks), stMem3.pUsedBlocks = NULL;
	}

	if (stMem.pAvailableBlocks) free(stMem.pAvailableBlocks), stMem.pAvailableBlocks = NULL;
	if (stMem.pUsedBlocks) free(stMem.pUsedBlocks), stMem.pUsedBlocks = NULL;
	free(stHandle.pcID), stHandle.pcID = NULL;
}
static void __test_mem_pool_rec_list_expansion(struct _MEM_POOL * pstPool) {
	int err = SUCCESS;
	struct _MMAP_RECORD stHandle;
	struct _MEM_RECORD stMem = {"NOTEST", "NOTEST0", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stMem2 = {"NOTEST_TEST", "NOTEST3", MEM_ANON_FILE, 0, NULL, 0, NULL},
		stMem3 = {NULL, NULL, MEM_ANON_FILE, 0, NULL, 0, NULL};
	for (unsigned int i = 0; i < 5; i++) {
		stMem.pcID = NULL;
		if (err = pool_anon_file_create_master_record(pstPool, 64, &stMem, &stHandle, true)) break;
		if (stHandle.pcID) free(stHandle.pcID), stHandle.pcID = NULL;
	}
	__trace_shared_mem_pool(pstPool);
}
static void __test_mem_pool_fit_algo (struct _MEM_POOL * pstPool) {
	bool bFit = false;
	struct _MEM_RECORD_BLOCK stResult;
	struct _MEM_RECORD stRecord = { NULL, NULL, MEM_ANON_FILE, 1, NULL, 1, NULL};
	
	struct _MEM_RECORD_BLOCK rgstCase1 [2] = {{0, 128}, {0, 16}};
	stRecord.pAvailableBlocks = rgstCase1, stRecord.pUsedBlocks = rgstCase1 + 1;
	bFit = pool_anon_file_check_fit(&stRecord, 16, &stResult);

	stRecord.pUsedBlocks[0].offset = 64;
	bFit = pool_anon_file_check_fit(&stRecord, 16, &stResult);

	struct _MEM_RECORD_BLOCK rgstCase2 [3] = {{0, 128}, {128, 128}, {16, 64}};
	stRecord.pAvailableBlocks = rgstCase2, stRecord.pUsedBlocks = rgstCase2 + 2;
	stRecord.availCount = 2, stRecord.usedCount = 1;
	bFit = pool_anon_file_check_fit(&stRecord, 16, &stResult);
}
static void __test_mem_pool_basic(struct _MEM_POOL * pstPool) {
	
	/*pool_anon_file_insert_mem_record(pstPool, &stMem);
	pool_anon_file_update_mem_record(pstPool, &stMem2);
	pool_anon_file_remove_mem_record(pstPool, &stMem);*/
	__trace_shared_mem_pool(pstPool);
}
void __test_mem_pool_anon_file(void * pool) {
	struct _MEM_POOL * pstPool = (struct _MEM_POOL *)pool;
	int err = SUCCESS;
	if (err = pool_anon_file_load_pool_info(pstPool, true)) goto error;
	//__test_mem_pool_rec_list_expansion(pstPool);
	//__test_mem_pool_deref(pstPool);
	//__test_mem_pool_fit_algo(pstPool);
	__test_mem_pool_realloc(pstPool);
error:
	if (pstPool->pstPoolInfo) free (pstPool->pstPoolInfo), pstPool->pstPoolInfo = NULL;
}