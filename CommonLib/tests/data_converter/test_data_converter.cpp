#include <stdlib.h>
#include <string.h>
#include "error/error.h"
#include "data_converter/data_converter_interface.h"
#include "platform/platform_interface.h"
struct COMPLEX_DATA_ENC_CFG
{
	unsigned int uiEncID;
	int iResourceID;
	const char * pcEncName;
};
struct COMPLEX_DATA_BIT_CFG
{
	const char * pcLabel;
	unsigned int uiLength;
	int uiOffset;
};
typedef struct
{
	unsigned int uiNodeID;
	unsigned int uiLinkID;
	unsigned int uiEncCount;
	COMPLEX_DATA_ENC_CFG * pstEncList;
	unsigned int uiTriggerEnaAddr;
	unsigned int uiTriggerEnableValue;
	unsigned int rguiLatchEnableAddr[3];
	unsigned int uiLatchEnableValue;
	unsigned int rguiLatchDelayAddr[6];
	unsigned int uiTriggerValueAddr;
	unsigned int uiTriggerCondAddr;
	COMPLEX_DATA_BIT_CFG rgTriggerCondition[4];
	unsigned int rguiFifoClearAddr[4];
	unsigned int uiFifoClearValue;
	unsigned int uiWriteData;
	unsigned int uiWriteAddr;
	unsigned int uiReadBackAddr;
	unsigned int uiFifoStatusAddr;
	COMPLEX_DATA_BIT_CFG rgFifoStatus[4];
	unsigned int rguiLatch[3];
	unsigned int rguiHome[3];
	unsigned int rguiEncCount[2];
	unsigned int rguiEncVal[4];
} COMPLEX_DATA_BOARD_CFG;
typedef struct
{
	unsigned int count;
	COMPLEX_DATA_BOARD_CFG *pstCfg;
} COMPLEX_DATA_CFG;
#define COMPLEX_FORMAT ("struct{"\
							"	unsigned int ESIOCount;"\
							"	varray{"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		unsigned int ENCCount;"\
							"		varray{unsigned int; int; string;}[ENCCount];"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		array{unsigned int;}[3];"\
							"		unsigned int;"\
							"		array{unsigned int;}[6];"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		array{string;unsigned int;unsigned int;}[4];"\
							"		array{unsigned int;}[4];"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		unsigned int;"\
							"		array{string;unsigned int;unsigned int;}[4];"\
							"		array{unsigned int;}[3];"\
							"		array{unsigned int;}[3];"\
							"		array{unsigned int;}[2];"\
							"		array{unsigned int;}[4];"\
							"	}[ESIOCount];"\
							"};")

static COMPLEX_DATA_ENC_CFG g_rgstEnc[2] = {{0, 189, "TEST11_ENC_PORT_0"}, {0, 190, "TEST11_ENC_PORT_0 (VER:A)"}};
static COMPLEX_DATA_BOARD_CFG g_stInfo1 = {2, 0,
	2, g_rgstEnc,
	40960, 1,
	{41216, 41232, 41248},
	1,
	{4063, 40979, 40995, 40964, 40980, 40996},
	40962, 40961,
	{{"source2", 6, 1}, {"source1", 4, 1}, {"condition", 2, 1}, {"direction", 0, 1}},
	{40965, 41223, 41239, 41255}, 1, 16, 14, 63, 8, 
	{{"empty", 0, 0}, {"full", 1, 0}, {"used", 2, 6}, {"avail", 9, 6}},
	{9, 10, 11}, {12, 13, 14}, {0, 1}, {2, 3, 4, 5}};
static COMPLEX_DATA_CFG g_stAllCfg = {1, &g_stInfo1};
static int test_packer_full() {
	COMPLEX_DATA_CFG * unpackedData = NULL;
	int err = SUCCESS;
	void * pConverter = NULL, * packedData = NULL;
	size_t size = 0;
	DATA_CONVERT_INFO stConverter = {NULL, NULL, NULL, 0, 0};
	if (!__data_converter_create(&pConverter, MEMORY_TO_MEMORY, COMPLEX_FORMAT, NULL)) return err = MALLOC_NO_MEM;
	stConverter.pLeft = &g_stAllCfg;
	packedData = __data_converter_convert(pConverter, C_TO_PACKEDC, &stConverter, NULL);
	stConverter.pLeft = packedData, stConverter.pRight = NULL;
	unpackedData = (COMPLEX_DATA_CFG *)__data_converter_convert(pConverter, PACKEDC_TO_C, &stConverter, NULL);
	if (err = (int)g_stAllCfg.count - (int)unpackedData->count) goto error;
	for (unsigned int i = 0; i < g_stAllCfg.count; i++) {
		if (err = (int)g_stAllCfg.pstCfg[i].uiEncCount - (int)unpackedData->pstCfg[i].uiEncCount) goto error;
		for (unsigned int j = 0; j < g_stAllCfg.pstCfg[i].uiEncCount; j++) {
			if (err = strcmp(g_stAllCfg.pstCfg[i].pstEncList[j].pcEncName, unpackedData->pstCfg[i].pstEncList[j].pcEncName)) goto error;
			if (err = g_stAllCfg.pstCfg[i].pstEncList[j].iResourceID - unpackedData->pstCfg[i].pstEncList[j].iResourceID) goto error;
		}
	}
error:
	if (unpackedData) {
		stConverter.pLeft = packedData, stConverter.pRight = unpackedData;
		__data_converter_convert(pConverter, PACKEDC_TO_C_FREE, &stConverter, NULL);
		free(unpackedData), unpackedData = NULL;
	}
	if (packedData) free(packedData), packedData = NULL;
	if (pConverter) __data_converter_destroy(pConverter), pConverter = NULL;
	return err;
}
static int test_packer_basic() {
	int err = 0;
	struct student {char * name; unsigned int age; char * nickname;};
	void * pConverter = NULL, * pConverted = NULL;
	struct student tom = {"TOM", 15, "tiger"}, *ptom;
	DATA_CONVERT_INFO stConverter = {NULL, &tom, NULL, 0, 0};
	if (!__data_converter_create(&pConverter, MEMORY_TO_MEMORY, "struct{string; unsigned int; string;};", NULL)) {
		return err = MALLOC_NO_MEM;
	}
	pConverted = __data_converter_convert(pConverter, C_TO_PACKEDC, &stConverter, NULL);
	stConverter.pLeft = pConverted, stConverter.pRight = NULL;
	ptom = (struct student *)__data_converter_convert(pConverter, PACKEDC_TO_C, &stConverter, NULL);
	stConverter.pRight = ptom;
	if (err = (int)ptom->age - (int)tom.age) goto error;
	if (err = strcmp(ptom->name, tom.name)) goto error;
	if (err = strcmp(ptom->nickname, tom.nickname)) goto error;
error:
	__data_converter_convert(pConverter, PACKEDC_TO_C_FREE, &stConverter, NULL);
	free (pConverted), pConverted = NULL;
	free(ptom), ptom = NULL;
	__data_converter_destroy(pConverter);
	return err;
}
int __test_data_converter() {
	int err = 0;
	if (err = test_packer_basic()) {
		__write_to_console("Failed test case on test_packer_basic\n");
		return err;
	}
	if (err = test_packer_full()) {
		__write_to_console("Failed test case on test_packer_full\n");
		return err;
	}
	return err;
}