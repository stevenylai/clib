#include "detect_mem_leak.h"
#include <string.h>
#include "error/error.h"
#include "data_glue_internal.h"
static void glue_default_error (void * pHandler, int iErrNo, char * pcFormat, ...) {
}
static struct _ERROR_HANDLER g_stDefaultError = {NULL, glue_default_error};
void * __glue_create_packer_for_data_structure (void ** pGlue, void * pPacker, enum _DATA_GLUE_TYPES enType, void * pDataStructure, struct _ERROR_HANDLER * pstError) {
	bool bErr = false;
	if (!pstError) pstError = &g_stDefaultError;
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = (struct _PACKER_FOR_DATA_STRUCTURE *)(*pGlue = malloc(sizeof(struct _PACKER_FOR_DATA_STRUCTURE)));
	if (!pstGlue) {
		pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "Failed to allocate memory for packer glue for data structure");
		bErr = true;
		goto error;
	} else {
		memset(pstGlue, 0, sizeof(struct _PACKER_FOR_DATA_STRUCTURE));
	}
	pstGlue->pPacker = pPacker, pstGlue->pDataStructure = pDataStructure, pstGlue->pstError = pstError;
	switch(pstGlue->enType = enType) {
		case GLUE_PACKER_FOR_LIST:
		case GLUE_PACKER_FOR_LIST_AUTO_FREE:
			__packer_glue_list_set_func_ptr(pstGlue);
			break;
		default:
			pstError->handle_error(pstError->pHandler, FATAL_CREATION_ERROR, "This type of packer glue is not implemented");
			bErr = true;
			goto error;
	}
	if (!pstGlue->create(pstGlue, pstError)) {
		bErr = true;
		goto error;
	}
error:
	if (bErr) {
		free (pstGlue), *pGlue = NULL;
		return NULL;
	} else {
		return pDataStructure;
	}
}
void __glue_destroy_packer_for_data_structure (void * pGlue) {
	struct _PACKER_FOR_DATA_STRUCTURE * pstGlue = (struct _PACKER_FOR_DATA_STRUCTURE *)pGlue;
	pstGlue->destroy(pstGlue);
	free(pGlue);
}