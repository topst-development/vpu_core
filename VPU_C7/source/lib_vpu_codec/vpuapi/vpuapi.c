//-----------------------------------------------------------------------------
// COPYRIGHT (C) 2020   CHIPS&MEDIA INC. ALL RIGHTS RESERVED
// Copyright (C) 2025  Telechips Inc.
//
// This file is distributed under BSD 3 clause or LGPL2.1 (dual license)
// SPDX License Identifier: BSD-3-Clause
// SPDX License Identifier: LGPL-2.1-only
//
// The entire notice above must be reproduced on all authorized copies.
//
// File         :
// Description  :
//-----------------------------------------------------------------------------

#include "vpuapifunc.h"
#include "vpuapi.h"
#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
#include "TCC_VPU_D6.h"
#else
#include "TCC_VPU_C7.h"
#endif

#include "vpu_core.h"

static PhysicalAddress paraBuffer;
static codec_addr_t paraBuffer_VA;
static PhysicalAddress codeBuffer;
static PhysicalAddress tempBuffer;

typedef void* (vpu_memcpy_func ) ( void*, const void*, unsigned int, unsigned int );	//!< memcpy
typedef void (vpu_memset_func ) ( void*, int, unsigned int, unsigned int );		//!< memset
typedef int  (vpu_interrupt_func ) ( void );						//!< Interrupt
typedef void* (vpu_ioremap_func ) ( unsigned int, unsigned int );
typedef void (vpu_iounmap_func ) ( void* );
typedef unsigned int (vpu_reg_read_func ) ( void*, unsigned int );
typedef void (vpu_reg_write_func ) ( void*, unsigned int, unsigned int );
typedef void (vpu_usleep_func ) ( unsigned int, unsigned int );				//!< usleep_range(min, max)

vpu_memcpy_func    *vpu_memcpy    = NULL;
vpu_memset_func    *vpu_memset    = NULL;
vpu_interrupt_func *vpu_interrupt = NULL;
vpu_ioremap_func   *vpu_ioremap   = NULL;
vpu_iounmap_func   *vpu_iounmap   = NULL;
vpu_reg_read_func  *vpu_read_reg  = NULL;
vpu_reg_write_func *vpu_write_reg = NULL;
vpu_usleep_func    *vpu_usleep    = NULL;

int gInitFirst = 0;
int gInitFirst_exit = 0;
int gMaxInstanceNum = MAX_NUM_INSTANCE;

codec_addr_t gVirtualBitWorkAddr;
codec_addr_t gVirtualRegBaseAddr;
codec_addr_t gFWAddr;

CodecInst codecInstPool[MAX_NUM_INSTANCE];

static CodecInst *pendingInst;
static unsigned int bit_code_size;
static const unsigned short *bit_code;

int resetAllCodecInstanceMember(unsigned int option)
{
	CodecInst *pCodecInst = &codecInstPool[0];
	int i;
	//int flaginstIndex  = (option >> 0 ) & 0x3;
	int flaginUse = (option >> 2 ) & 0x3;
	int flagcodecMode = (option >> 4 ) & 0x3;
	//int flagcodecModeAux = (option >> 6 ) & 0x3;

	for (i = 0; i < gMaxInstanceNum; i++, pCodecInst++) {
		if (flaginUse > 0) {
			pCodecInst->inUse = 0;
		}

		if (flagcodecMode > 0) {
			pCodecInst->codecMode = COD_STD_NULL; // = 0x7FFFFFFF (MJPG_ENC_NULL)
		}
	}
	return 1;	//sucess
}

int checkCodecInstanceAddr(void *ptr)
{
	int ret = RETCODE_FAILURE;
	CodecInst *pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		int i;
		CodecInst *pCodecInstRef = &codecInstPool[0];

		for (i = 0; i < gMaxInstanceNum; ++i, ++pCodecInstRef) {
			if (pCodecInst == pCodecInstRef) {
				i = gMaxInstanceNum;
				ret = RETCODE_SUCCESS;
			}
		}
	}
	return ret;
}

unsigned int checkCodecType(int bitstreamformat, void *ptr)
{
	CodecInst *pCodecInst;
	unsigned int ret_codecType = 0;
	short isEncoder = 0;    //0: Decoder(default), 1: Encoder
	short isCheckCodec = 0; //0: not yet, 1: confirmed, 2: codec mis-match
	int cmode, cmodeaux;

	pCodecInst = (CodecInst *)ptr;
	cmode = pCodecInst->codecMode;
	cmodeaux = pCodecInst->codecModeAux;

	switch (bitstreamformat)
	{
	case STD_AVC:   //0 DEC / ENC
		if (cmode == AVC_ENC) {
		#if TCC_AVC____ENC_SUPPORT == 1
			isEncoder = 1;
			isCheckCodec = 1;
		#endif
		} else if (cmode == AVC_DEC) {
			isEncoder = 0;
			isCheckCodec = 1;
		} else {
			isCheckCodec = 2;
		}
		break;
	default :
		//not susported codec
		break;
	}
	ret_codecType = (isEncoder << 8) | isCheckCodec;
	return ret_codecType;
}

int checkCodecInstanceUse(void *ptr)
{
	int ret = RETCODE_FAILURE;
	CodecInst *pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		if (pCodecInst->inUse == 1) {
			ret = RETCODE_SUCCESS;
		}
	}
	return ret;
}

int checkCodecInstancePended(void *ptr)
{
	int ret = RETCODE_FAILURE;
	CodecInst * pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		if (GetPendingInst() == pCodecInst || GetPendingInst() == 0) {
			ret = RETCODE_SUCCESS;
		}
	}
	return ret;
}

void *GetPendingInstPointer(void)
{
	return ((void *)&pendingInst);
}

void SetPendingInst(CodecInst *inst)
{
	pendingInst = inst;
}

void ClearPendingInst()
{
	if(pendingInst)
		pendingInst = 0;
}

CodecInst *GetPendingInst()
{
	return pendingInst;
}

void VpuWriteReg(unsigned int ADDR, unsigned int DATA)
{
	if (vpu_write_reg == NULL)
		*((volatile unsigned long *)(gVirtualRegBaseAddr+ADDR)) = (unsigned int)(DATA);
	else
		vpu_write_reg((void*)gVirtualRegBaseAddr, ADDR, DATA);
}

unsigned int VpuReadReg(unsigned int ADDR)
{
	unsigned int val;

	if (vpu_read_reg == NULL)
		val = *(volatile unsigned long *)(gVirtualRegBaseAddr+ADDR);
	else
		val = vpu_read_reg((void*)gVirtualRegBaseAddr, ADDR);
	return val;
}

Uint32 VPU_IsBusy()
{
	Uint32 ret = 0;

	ret = VpuReadReg(BIT_BUSY_FLAG);
	return ret != 0;
}

Uint32 VPU_IsInit()
{
	Uint32 pc;

	pc = VpuReadReg(BIT_CUR_PC);
	return pc;
}

RetCode VPU_Init(PhysicalAddress workBuf, PhysicalAddress codeBuf, int fw_writing_disable)
{
	Uint32 data;
	int i;
	int reinit_flag = 0;
	unsigned int RTLVersion, HwDate;
	//unsigned int HwRevision;
	//PhysicalAddress workBuffer;
	codec_addr_t codeBufferVA;

	CodecInst *pCodecInst = &codecInstPool[0];

	//reset codec instance memory
	if( vpu_memset )
		vpu_memset(pCodecInst, 0, sizeof(CodecInst)*gMaxInstanceNum, 0);

	for( i = 0; i < gMaxInstanceNum; ++i, ++pCodecInst )
	{
		pCodecInst->instIndex = i;
		pCodecInst->inUse = 0;
	}

	ClearPendingInst();

	VpuWriteReg(0x4A0, 0);
	for (i = 0; i < (64*4); i += 4)
	{
		VpuWriteReg((BIT_CODE_RUN + 0x100 + i), 0);
	}
	VpuWriteReg(BIT_FRM_DIS_FLG, 0);
	VpuWriteReg(BIT_ROLLBACK_STATUS, 0);

	codeBuffer    = workBuf;
	codeBufferVA  = gVirtualBitWorkAddr;
	tempBuffer    = codeBuffer   + CODE_BUF_SIZE;
	paraBuffer    = codeBuffer   + CODE_BUF_SIZE + TEMP_BUF_SIZE;
	paraBuffer_VA = codeBufferVA + CODE_BUF_SIZE + TEMP_BUF_SIZE;

	if (fw_writing_disable == 1)
		codeBuffer = codeBuf;

	if (VPU_IsInit() != 0)
	{
		reinit_flag = 1;

		MACRO_VPU_SWRESET_EXIT
		if (VpuReadReg(BIT_CUR_PC) < 0x10)
		{
			reinit_flag = 0;
			//VPU_SWReset(SW_RESET_ON_BOOT);
			return RETCODE_CODEC_EXIT;
		}
	}

	//get RTL version
	// val          Description
	//0x0000        v2.1.1 previous Hardware (v1.1.1)
	//0x0211        v2.1.1 Hardware
	//0x0212        v2.1.2 Hardware
	//0x00020103    v2.1.3 Hardware
	//0x0002040a    v2.4.10 Hardware
	RTLVersion = VpuReadReg(GDI_HW_VERINFO);
	HwDate = VpuReadReg(GDI_CFG_DATE);
	// HwRevision = VpuReadReg(GDI_HW_CFG_REV);


	if (reinit_flag == 0)
	{
		int writeBytes = 2048;
		unsigned short new_data0, new_data1, new_data2, new_data3;
		unsigned short *new_bit_code =
			(unsigned short *)vpu_ioremap(codeBuf, CODE_BUF_SIZE);

		VpuWriteReg(BIT_INT_ENABLE, 0);
		VpuWriteReg(BIT_CODE_RUN, 0);

		for (i = 0; i < 2048; i+=4) {
			new_data0 = new_bit_code[i+3];
			VpuWriteReg(BIT_CODE_DOWN, ((i+0) << 16) | new_data0);
			new_data1 = new_bit_code[i+2];
			VpuWriteReg(BIT_CODE_DOWN, ((i+1) << 16) | new_data1);
			new_data2 = new_bit_code[i+1];
			VpuWriteReg(BIT_CODE_DOWN, ((i+2) << 16) | new_data2);
			new_data3 = new_bit_code[i+0];
			VpuWriteReg(BIT_CODE_DOWN, ((i+3) << 16) | new_data3);
		}
	}

	VpuWriteReg(BIT_PARA_BUF_ADDR, paraBuffer);
	VpuWriteReg(BIT_CODE_BUF_ADDR, codeBuffer);
	VpuWriteReg(BIT_TEMP_BUF_ADDR, tempBuffer);

	VpuWriteReg(BIT_BIT_STREAM_CTRL, VPU_STREAM_ENDIAN);
	VpuWriteReg(BIT_FRAME_MEM_CTRL, CBCR_INTERLEAVE<<2|VPU_FRAME_ENDIAN);
	VpuWriteReg(BIT_BIT_STREAM_PARAM, 0);

	VpuWriteReg(BIT_AXI_SRAM_USE, 0);
	VpuWriteReg(BIT_INT_ENABLE, 0);
	VpuWriteReg(BIT_ROLLBACK_STATUS, 0);

	if (reinit_flag)
	{
		VpuWriteReg(BIT_INT_CLEAR, 0x1);
		MACRO_VPU_SWRESET_EXIT
		return RETCODE_SUCCESS;
	}

	data = (1<<INT_BIT_BIT_BUF_FULL);
	data |= (1<<INT_BIT_BIT_BUF_EMPTY);
	data |= (1<<INT_BIT_DEC_MB_ROWS);
	data |= (1<<INT_BIT_SEQ_INIT);
	data |= (1<<INT_BIT_PIC_RUN);
	data |= (1<<INT_BIT_DEC_FIELD);
	data |= (1<<INT_BIT_INIT);

	VpuWriteReg(BIT_INT_ENABLE, data);
	VpuWriteReg(BIT_INT_CLEAR, 0x1);

	VpuWriteReg(BIT_BUSY_FLAG, 0x1);
	VpuWriteReg(BIT_CODE_RESET, 1);
	VpuWriteReg(BIT_CODE_RUN, 1);

	while( 1 )
	{
		int iRet = 0;

	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		if (VpuReadReg(BIT_CUR_PC) == 0x0)
			return RETCODE_CODEC_EXIT;
	#endif
		if (vpu_interrupt != NULL)
			iRet = vpu_interrupt();

		if (iRet == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_BIT_INT_ENABLE_RESET1
			return RETCODE_CODEC_EXIT;
		}

		iRet = VpuReadReg(BIT_INT_REASON);
		if (iRet)
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
			if (iRet & (1 << INT_BIT_INIT))
			{
				break;
			}else
			{
				if (iRet & (1 << INT_BIT_BIT_BUF_EMPTY))
					return RETCODE_FRAME_NOT_COMPLETE;
				else if (iRet & (1 << INT_BIT_BIT_BUF_FULL))
					return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
				else
					return RETCODE_FAIL_INIT_RUN;
			}
		}
	}

	return RETCODE_SUCCESS;
}

RetCode VPU_GetVersionInfo(Uint32 *versionInfo, Uint32 *revision, Uint32 *productId)
{
	Uint32 ver, rev, pid;

	if ((versionInfo != NULL) && (revision != NULL))
	{
		if (VPU_IsInit() == 0)
			return RETCODE_NOT_INITIALIZED;

		if (GetPendingInst())
			return RETCODE_FRAME_NOT_COMPLETE;

		VpuWriteReg(RET_FW_VER_NUM , 0);
		BitIssueCommand(NULL, FIRMWARE_GET);
		MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

		ver = VpuReadReg(RET_FW_VER_NUM);
		if (ver == 0)
			return RETCODE_NOT_INITIALIZED;	//RETCODE_FAILURE
		*versionInfo = ver;

		rev = VpuReadReg(RET_FW_CODE_REV);
		*revision = rev;
	}

	if (productId != NULL)
	{
		pid = VpuReadReg(DBG_CONFIG_REPORT_1);	//VPU_PRODUCT_CODE_REGISTER
		switch (pid & 0x0000FF00)
		{
			case BODA950_CODE:
				pid = VPUCNM_PRODUCT_ID_950;
				break;
			case CODA960_CODE:
				pid = VPUCNM_PRODUCT_ID_960;
				break;
			default:
				pid = VPUCNM_PRODUCT_ID_NONE;
				break;
		}
		*productId = pid;
	}

	return RETCODE_SUCCESS;
}

RetCode VPU_DecOpen(DecHandle *pHandle, DecOpenParam *pop)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	int val;
	RetCode ret;

	if (pop->bitstreamFormat != STD_MJPG) {
		if (VPU_IsInit() == 0)
			return RETCODE_NOT_INITIALIZED;
	}

	ret = CheckDecOpenParam(pop);
	if (ret != RETCODE_SUCCESS)
		return ret;

	ret = GetCodecInstance(&pCodecInst);
	if (ret == RETCODE_FAILURE) {
		*pHandle = 0;
		return RETCODE_FAIL_GET_INSTANCE;
	}

	*pHandle = pCodecInst;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	pDecInfo->openParam = *pop;
	pCodecInst->CodecInfo.decInfo.isFirstFrame = 1;
	pCodecInst->codecMode = 0;
	pCodecInst->codecModeAux = 0;
	if (pop->bitstreamFormat == STD_AVC) {
		pCodecInst->codecMode = AVC_DEC;
		pCodecInst->codecModeAux = pop->avcExtension;
	}
	else
	{
		return RETCODE_CODEC_SPECOUT;
	}

	pDecInfo->workBufBaseAddr = pop->workBuffer;
	pDecInfo->streamWrPtr = pop->bitstreamBuffer;
	pDecInfo->streamRdPtr = pop->bitstreamBuffer;

	pDecInfo->streamRdPtrRegAddr = BIT_RD_PTR;
	pDecInfo->streamWrPtrRegAddr = BIT_WR_PTR;
	pDecInfo->frameDisplayFlagRegAddr = BIT_FRM_DIS_FLG;

	pDecInfo->frameDisplayFlag = 0;
	pDecInfo->clearDisplayIndexes = 0;
	pDecInfo->setDisplayIndexes = 0;

	pDecInfo->frameBufPool = 0;

	pDecInfo->frameDelay = -1;	// V2.1.8

	pDecInfo->streamBufStartAddr = pop->bitstreamBuffer;
	pDecInfo->streamBufSize = pop->bitstreamBufferSize;
	pDecInfo->streamBufEndAddr = pop->bitstreamBuffer + pop->bitstreamBufferSize;

	pDecInfo->reorderEnable = pop->ReorderEnable;//VPU_REORDER_ENABLE;
	pDecInfo->seqInitEscape = 0;
	pDecInfo->rotationEnable = 0;
	pDecInfo->mirrorEnable = 0;
	pDecInfo->mirrorDirection = MIRDIR_NONE;
	pDecInfo->rotationAngle = 0;
	pDecInfo->rotatorOutputValid = 0;
	pDecInfo->rotatorStride = 0;
	pDecInfo->deringEnable	= 0;
	pDecInfo->stride = 0;

	pDecInfo->secAxiUse.useBitEnable  = 0;
	pDecInfo->secAxiUse.useIpEnable   = 0;
	pDecInfo->secAxiUse.useDbkYEnable = 0;
	pDecInfo->secAxiUse.useDbkCEnable = 0;
	pDecInfo->secAxiUse.useOvlEnable  = 0;
	pDecInfo->secAxiUse.useBtpEnable  = 0;
	pDecInfo->secAxiUse.bufBitUse    = 0;
	pDecInfo->secAxiUse.bufIpAcDcUse = 0;
	pDecInfo->secAxiUse.bufDbkYUse   = 0;
	pDecInfo->secAxiUse.bufDbkCUse   = 0;
	pDecInfo->secAxiUse.bufOvlUse    = 0;
	pDecInfo->secAxiUse.bufBtpUse    = 0;

	pDecInfo->initialInfoObtained = 0;
	pDecInfo->vc1BframeDisplayValid = 0;
	pDecInfo->userDataBufAddr = 0;
	pDecInfo->userDataEnable = 0;
	pDecInfo->userDataBufSize = 0;
	pDecInfo->mapType = pop->mapType;
	pDecInfo->tiled2LinearEnable = pop->tiled2LinearEnable;
	pDecInfo->cacheConfig.CacheMode = 0;

	pDecInfo->avcErrorConcealMode = AVC_ERROR_CONCEAL_MODE_DEFAULT;
	pDecInfo->avcNpfFieldIdx = 0;

	VpuWriteReg(pDecInfo->streamRdPtrRegAddr, pDecInfo->streamBufStartAddr);
	VpuWriteReg(pDecInfo->streamWrPtrRegAddr, pDecInfo->streamWrPtr);

	VpuWriteReg(pDecInfo->frameDisplayFlagRegAddr, 0);

	val = VpuReadReg(BIT_BIT_STREAM_PARAM);
	val &= ~(1 << 2);
	VpuWriteReg(BIT_BIT_STREAM_PARAM, val);

	pDecInfo->streamEndflag = val;
	//VpuWriteReg(GDI_WPROT_RGN_EN, 0);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecClose(DecHandle handle)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if( pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT )
	{
		int reason;
		reason = VpuReadReg(BIT_INT_REASON);
		if(reason & (1<<INT_BIT_DEC_FIELD))
		{
			if (GetPendingInst() == pCodecInst)
			{
				VpuWriteReg(pCodecInst->CodecInfo.decInfo.streamRdPtrRegAddr, pCodecInst->CodecInfo.decInfo.streamBufStartAddr);
				VpuWriteReg(pCodecInst->CodecInfo.decInfo.streamWrPtrRegAddr, pCodecInst->CodecInfo.decInfo.streamBufStartAddr);
			#ifdef ADD_VPU_CLOSE_SWRESET
				MACRO_VPU_SWRESET
				//VPU_SWReset2();
			#endif
				if( VpuReadReg(BIT_INT_STS) != 0 )
				{
					VpuWriteReg(BIT_INT_REASON, 0);
					VpuWriteReg(BIT_INT_CLEAR, 1);
				}
			}
		}
	}

	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
	{
		if (pDecInfo->initialInfoObtained)
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			BitIssueCommand(pCodecInst, SEQ_END);
			MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());
		}
		SetPendingInst(0);
		if( VpuReadReg(BIT_INT_STS) != 0 )
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
		}
	}

	if (GetPendingInst() == pCodecInst)
	{
		if( VpuReadReg(BIT_INT_STS) != 0 )
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
		}
	}
	FreeCodecInstance(pCodecInst);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecSetEscSeqInit(DecHandle handle, int escape)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_INTERRUPT)
		return RETCODE_INVALID_PARAM;

	pDecInfo->seqInitEscape = escape;

	return RETCODE_SUCCESS;
}

RetCode VPU_DecGetInitialInfo(DecHandle handle, DecInitialInfo *info)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	Uint32 val, val2;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (info == 0) {
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (GetPendingInst()) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	if (DecBitstreamBufEmpty(pDecInfo)) {
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	VpuWriteReg(CMD_DEC_SEQ_BB_START, pDecInfo->streamBufStartAddr);
	VpuWriteReg(CMD_DEC_SEQ_BB_SIZE, pDecInfo->streamBufSize / 1024); // size in KBytes

	if(pDecInfo->userDataEnable) {
		val  = 0;
		val |= (pDecInfo->userDataReportMode << 10);
	#ifdef SEQ_INIT_USERDATA_DISABLE
		val |= (0 << 5);
	#else
		val |= (pDecInfo->userDataEnable << 5);
	#endif
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_OPTION, val);
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_BASE_ADDR, pDecInfo->userDataBufAddr);
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_BUF_SIZE, pDecInfo->userDataBufSize);
	}
	else {
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_OPTION, 0);
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_BASE_ADDR, 0);
		VpuWriteReg(CMD_DEC_SEQ_USER_DATA_BUF_SIZE, 0);
	}

	val  = 0;

	val |= (pDecInfo->reorderEnable<<1) & 0x2;
	val |= (pDecInfo->openParam.mp4DeblkEnable & 0x1);
	val |= (pDecInfo->avcErrorConcealMode << 2);
	val |= (pDecInfo->openParam.H263_SKIPMB_ON << 9);
	VpuWriteReg(CMD_DEC_SEQ_OPTION, val);

	switch(pCodecInst->codecMode) {
	case AVC_DEC:
		VpuWriteReg(CMD_DEC_SEQ_X264_MV_EN, pDecInfo->openParam.x264Enable);
		break;
	}

	if( pCodecInst->codecMode == AVC_DEC )
		VpuWriteReg(CMD_DEC_SEQ_SPP_CHUNK_SIZE, VPU_GBU_SIZE);	//VPU_SPP_CHUNK_SIZE

	VpuWriteReg(pDecInfo->streamWrPtrRegAddr, pDecInfo->streamWrPtr);
	VpuWriteReg(pDecInfo->streamRdPtrRegAddr, pDecInfo->streamRdPtr);

	pDecInfo->streamEndflag &= ~(3<<3);
	if (pDecInfo->openParam.bitstreamMode == BS_MODE_ROLLBACK)	//rollback mode
		pDecInfo->streamEndflag |= (1<<3);
	else if (pDecInfo->openParam.bitstreamMode == BS_MODE_PIC_END)
		pDecInfo->streamEndflag |= (2<<3);
	else {	// Interrupt Mode
		if (pDecInfo->seqInitEscape) {
			pDecInfo->streamEndflag |= (2<<3);
		}
	}
	VpuWriteReg(BIT_BIT_STREAM_PARAM, pDecInfo->streamEndflag);

	val = pDecInfo->openParam.streamEndian;
	VpuWriteReg(BIT_BIT_STREAM_CTRL, val);

	val = 0;
	val |= (pDecInfo->openParam.bwbEnable<<12);
	if (pDecInfo->mapType) {
		if (pDecInfo->mapType == TILED_FRAME_MB_RASTER_MAP)
			val |= (pDecInfo->tiled2LinearEnable<<11)|(0x03<<9)|(FORMAT_420<<6);
		else
			val |= (pDecInfo->tiled2LinearEnable<<11)|(0x02<<9)|(FORMAT_420<<6);
	}
	val |= ((pDecInfo->openParam.cbcrInterleave)<<2);	// Interleave bit position is modified
	val |= pDecInfo->openParam.frameEndian;
	VpuWriteReg(BIT_FRAME_MEM_CTRL, val);

	VpuWriteReg(pDecInfo->frameDisplayFlagRegAddr, 0);

	SetPendingInst(pCodecInst);

	MACRO_VPU_BIT_INT_ENABLE_RESET1

	VpuWriteReg(BIT_INT_ENABLE,
		   (1<<INT_BIT_SEQ_INIT)
		  |(1<<INT_BIT_SEQ_END)
		  |(1<<INT_BIT_BIT_BUF_FULL)
		  |(1<<INT_BIT_BIT_BUF_EMPTY)
			);
	BitIssueCommand(pCodecInst, SEQ_INIT);

	while( 1 )
	{
		int iRet = 0;

	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		if(VpuReadReg(BIT_CUR_PC) == 0x0)
			return RETCODE_CODEC_EXIT;
	#endif
		if( vpu_interrupt != NULL )
			iRet = vpu_interrupt();
		if( iRet == RETCODE_CODEC_EXIT )
		{
			MACRO_VPU_BIT_INT_ENABLE_RESET1
			return RETCODE_CODEC_EXIT;
		}

		iRet = VpuReadReg(BIT_INT_REASON);
		if( iRet )
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
			if( iRet & (1<<INT_BIT_SEQ_INIT) )
			{
				break;
			} else
			{
				if( iRet & (1<<INT_BIT_BIT_BUF_EMPTY) )
				{
					return RETCODE_FRAME_NOT_COMPLETE;
				}
				else if( iRet & (1<<INT_BIT_BIT_BUF_FULL) )
				{
					return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
				}
				else
				{
					return RETCODE_FAILURE;
				}
			}
		}
	}

	if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT &&
		pDecInfo->seqInitEscape) {
		pDecInfo->streamEndflag &= ~(3<<3);
		VpuWriteReg(BIT_BIT_STREAM_PARAM, pDecInfo->streamEndflag);
		pDecInfo->seqInitEscape = 0;
	}

	pDecInfo->frameDisplayFlag = VpuReadReg(pDecInfo->frameDisplayFlagRegAddr);
	pDecInfo->streamRdPtr = VpuReadReg(pDecInfo->streamRdPtrRegAddr);
	pDecInfo->streamEndflag = VpuReadReg(BIT_BIT_STREAM_PARAM);

	info->rdPtr = pDecInfo->streamRdPtr;
	info->wrPtr = VpuReadReg(pDecInfo->streamWrPtrRegAddr);

	val = VpuReadReg(RET_DEC_SEQ_SUCCESS);
	if (val & (1<<31))
		return RETCODE_MEMORY_ACCESS_VIOLATION;

	if (pDecInfo->openParam.bitstreamMode == BS_MODE_ROLLBACK)	// || (pDecInfo->openParam.bitstreamMode == BS_MODE_PIC_END)
	{
		// BitEmptyRollback R This flag is valid only
		//  when the RetStatus (0th bit) is 0,  and StreamMode in BIT_BIT_STREAM_PARAM register is set as rollback mode.
		//   0 - DEC_SEQ_INIT command failed due to other syntax error.
		//   1 - DEC_SEQ_INIT command failed due to insufficient bitstream for one picture in rollback mode.
		if (val & (1 << 4))
		{
			info->seqInitErrReason = (VpuReadReg(RET_DEC_SEQ_SEQ_ERR_REASON) );	//| (1<<20)
			SetPendingInst(0);
			return RETCODE_FAILURE;
		}
	}

	if (val == 0)
	{
		info->seqInitErrReason = VpuReadReg(RET_DEC_SEQ_SEQ_ERR_REASON);
		SetPendingInst(0);
		return RETCODE_FAILURE;
	}

	val = VpuReadReg(RET_DEC_SEQ_SRC_SIZE);
	info->picWidth = ((val >> 16) & 0xffff);
	info->picHeight = (val & 0xffff);

	info->fRateNumerator    = VpuReadReg(RET_DEC_SEQ_FRATE_NR);
	info->fRateDenominator  = VpuReadReg(RET_DEC_SEQ_FRATE_DR);

	if (pCodecInst->codecMode == AVC_DEC && info->fRateDenominator>0)
		info->fRateDenominator *= 2;

	if ( (info->fRateNumerator < 0) || (info->fRateDenominator < 0))	// TELBB-262
	{
		info->fRateNumerator = 0;
		info->fRateDenominator = 1;
	}

	info->frameRateRes = info->fRateNumerator;
	info->frameRateDiv = info->fRateDenominator;

	info->minFrameBufferCount = VpuReadReg(RET_DEC_SEQ_FRAME_NEED);
	info->frameBufDelay = VpuReadReg(RET_DEC_SEQ_FRAME_DELAY);

	if (pCodecInst->codecMode == AVC_DEC)
	{
		info->avcCtType = VpuReadReg(RET_DEC_SEQ_CTYPE_INFO);
		val = VpuReadReg(RET_DEC_SEQ_CROP_LEFT_RIGHT);
		val2 = VpuReadReg(RET_DEC_SEQ_CROP_TOP_BOTTOM);
		if( val == 0 && val2 == 0 )
		{
			info->picCropRect.left = 0;
			info->picCropRect.right = 0;
			info->picCropRect.top = 0;
			info->picCropRect.bottom = 0;
		}
		else
		{
			info->picCropRect.left = ( (val>>16) & 0xFFFF );
			info->picCropRect.right = info->picWidth - ( ( val & 0xFFFF ) );
			info->picCropRect.top = ( (val2>>16) & 0xFFFF );
			info->picCropRect.bottom = info->picHeight - ( ( val2 & 0xFFFF ) );
		}

		val = (info->picWidth * info->picHeight * 3 / 2) / 1024;
		info->normalSliceSize = val / 4;
		info->worstSliceSize = val / 2;
	}


	val = VpuReadReg(RET_DEC_SEQ_HEADER_REPORT);
	info->profile =                (val >>  0) & 0xFF;	// [ 7: 0]
	info->level =                  (val >>  8) & 0xFF;	// [15: 8]
	info->interlace =              (val >> 16) & 0x01;	// [16   ]
	info->direct8x8Flag =          (val >> 17) & 0x01;	// [17   ]
	info->vc1Psf =                 (val >> 18) & 0x01;	// [18   ]
	info->constraint_set_flag[0] = (val >> 19) & 0x01;	// [19   ]
	info->constraint_set_flag[1] = (val >> 20) & 0x01;	// [20   ]
	info->constraint_set_flag[2] = (val >> 21) & 0x01;	// [21   ]
	info->constraint_set_flag[3] = (val >> 22) & 0x01;	// [22   ]
	info->chromaFormat =           (val >> 23) & 0x03;	// [24:23]
	//info->avcIsExtSAR =            (val >> 25) & 0x01;	// [25   ]
	//info->maxNumRefFrm =           (val >> 27) & 0x0f;	// [30:27]
	//info->maxNumRefFrmFlag =       (val >> 31) & 0x01;	// [31   ]

	val = VpuReadReg(RET_DEC_SEQ_ASPECT);
	info->aspectRateInfo = val;

	val = VpuReadReg(RET_DEC_SEQ_BIT_RATE);
	info->bitRate = val;

	if (pCodecInst->codecMode == AVC_DEC) {
		val = VpuReadReg(RET_DEC_SEQ_VUI_INFO);
		info->avcVuiInfo.fixedFrameRateFlag    = val &1;
		info->avcVuiInfo.timingInfoPresent     = (val>>1) & 0x01;
		info->avcVuiInfo.chromaLocBotField     = (val>>2) & 0x07;
		info->avcVuiInfo.chromaLocTopField     = (val>>5) & 0x07;
		info->avcVuiInfo.chromaLocInfoPresent  = (val>>8) & 0x01;
		info->avcVuiInfo.colorPrimaries        = (val>>16) & 0xff;
		info->avcVuiInfo.colorDescPresent      = (val>>24) & 0x01;
		info->avcVuiInfo.isExtSAR              = (val>>25) & 0x01;
		info->avcVuiInfo.vidFullRange          = (val>>26) & 0x01;
		info->avcVuiInfo.vidFormat             = (val>>27) & 0x07;
		info->avcVuiInfo.vidSigTypePresent     = (val>>30) & 0x01;
		info->avcVuiInfo.vuiParamPresent       = (val>>31) & 0x01;

		val = VpuReadReg(RET_DEC_SEQ_VUI_PIC_STRUCT);
		info->avcVuiInfo.vuiPicStructPresent = (val & 0x1);
		info->avcVuiInfo.vuiPicStruct = (val>>1);

		val = VpuReadReg(RET_DEC_SEQ_DISPLAY_EXT);
		info->avcVuiInfo.vuiTransferCharacteristics = (val>>8) & 0xff;
		info->avcVuiInfo.vuiMatrixCoefficients = val & 0xff;
	}

	SetPendingInst(0);

	pDecInfo->initialInfo = *info;
	pDecInfo->initialInfoObtained = 1;

	return RETCODE_SUCCESS;
}

RetCode VPU_DecRegisterFrameBuffer(
				   DecHandle handle,
				   FrameBuffer *bufArray,
				   int num,
				   int stride,
				   int height,
				   DecBufInfo *pBufInfo)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	int i, k;
	Uint32 val;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (!pDecInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if (bufArray == 0)
		return RETCODE_INVALID_FRAME_BUFFER;

	if (num < pDecInfo->initialInfo.minFrameBufferCount)
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

	if ((stride < (pDecInfo->initialInfo.picWidth>>3)) || (stride % 8) != 0)
		return RETCODE_INVALID_STRIDE;

	if (pDecInfo->openParam.bitstreamFormat != STD_MJPG)
	{
		if ((stride < pDecInfo->initialInfo.picWidth) || (stride % 8 != 0) || (height<pDecInfo->initialInfo.picHeight))
			return RETCODE_INVALID_STRIDE;
	}

	if (GetPendingInst()) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	if (bufArray)
	{
		pDecInfo->frameBufPool = bufArray;
	}
	pDecInfo->numFrameBuffers = num;
	pDecInfo->stride = stride;

	for( i = 0, k = 0; i < num; i+=4, k+=48 )
	{
		VirtualWriteReg(paraBuffer_VA + k +  4, bufArray[i+0].bufY );
		VirtualWriteReg(paraBuffer_VA + k +  0, bufArray[i+0].bufCb);
		if (pDecInfo->openParam.cbcrInterleave)
			VirtualWriteReg(paraBuffer_VA + k + 12, 0);
		else
			VirtualWriteReg(paraBuffer_VA + k + 12, bufArray[i+0].bufCr);
		if( i+1 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k +  8, bufArray[i+1].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 20, bufArray[i+1].bufCb);
		if (pDecInfo->openParam.cbcrInterleave)
			VirtualWriteReg(paraBuffer_VA + k + 16, 0);
		else
			VirtualWriteReg(paraBuffer_VA + k + 16, bufArray[i+1].bufCr);
		if( i+2 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k + 28, bufArray[i+2].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 24, bufArray[i+2].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 36, bufArray[i+2].bufCr);
		if (pDecInfo->openParam.cbcrInterleave)
			VirtualWriteReg(paraBuffer_VA + k + 36, 0);
		else
			VirtualWriteReg(paraBuffer_VA + k + 36, bufArray[i+2].bufCr);
		if( i+3 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k + 32, bufArray[i+3].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 44, bufArray[i+3].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 40, bufArray[i+3].bufCr);
		if (pDecInfo->openParam.cbcrInterleave)
			VirtualWriteReg(paraBuffer_VA + k + 40, 0);
		else
			VirtualWriteReg(paraBuffer_VA + k + 40, bufArray[i+3].bufCr);
		if( i+4 == num ) break;
	}

	for( i = 0, k = 0; i < num; i+=2, k+=8 )
	{
		VirtualWriteReg(paraBuffer_VA + 384 + k + 4, bufArray[i+0].bufMvCol);
		if( i+1 == num ) break;
		VirtualWriteReg(paraBuffer_VA + 384 + k + 0, bufArray[i+1].bufMvCol);
		if( i+2 == num ) break;
	}

	// Tell the decoder how much frame buffers were allocated.

	VpuWriteReg(CMD_SET_FRAME_BUF_NUM, num);
	VpuWriteReg(CMD_SET_FRAME_BUF_STRIDE, stride);

	VpuWriteReg(CMD_SET_FRAME_AXI_BIT_ADDR, pDecInfo->secAxiUse.bufBitUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_IPACDC_ADDR, pDecInfo->secAxiUse.bufIpAcDcUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_DBKY_ADDR, pDecInfo->secAxiUse.bufDbkYUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_DBKC_ADDR, pDecInfo->secAxiUse.bufDbkCUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_OVL_ADDR, pDecInfo->secAxiUse.bufOvlUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_BTP_ADDR, pDecInfo->secAxiUse.bufBtpUse);

	VpuWriteReg(CMD_SET_FRAME_DELAY, pDecInfo->frameDelay);

	// Maverick Cache Configuration
	if (pCodecInst->m_iVersionRTL == 0)
	{	// RTL 1.1.1
		val = 0;
		val |= (1<<28);	// MAVERICK_CACHE_CONFIG1 Disable
		VpuWriteReg(CMD_SET_FRAME_CACHE_CONFIG, val);
	}
	else
	{	// RTL 2.1.3 or above
		VpuWriteReg(CMD_SET_FRAME_CACHE_CONFIG, pDecInfo->cacheConfig.CacheMode);
	}

	if (pCodecInst->codecMode == VPX_DEC) {
		if (pCodecInst->codecModeAux == VPX_AUX_VP8) {
			val = pBufInfo->vp8MbDataBufInfo.bufferBase;
			VpuWriteReg(CMD_SET_FRAME_MB_BUF_BASE, val);
		}
	}

	if( pCodecInst->codecMode == AVC_DEC ) {
		VpuWriteReg( CMD_SET_FRAME_SLICE_BB_START, pBufInfo->avcSliceBufInfo.bufferBase );
		VpuWriteReg( CMD_SET_FRAME_SLICE_BB_SIZE, (pBufInfo->avcSliceBufInfo.bufferSize/1024) );
	}

	BitIssueCommand(pCodecInst, SET_FRAME_BUF);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	if (VpuReadReg(RET_SET_FRAME_SUCCESS) & (1<<31)) {
		return RETCODE_MEMORY_ACCESS_VIOLATION;
	}

	return RETCODE_SUCCESS;
}

RetCode VPU_DecGetBitstreamBuffer( DecHandle handle,
				  PhysicalAddress * prdPrt,
				  PhysicalAddress * pwrPtr,
				  int * size)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	int room;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;


	if ( prdPrt == 0 || pwrPtr == 0 || size == 0) {
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (GetPendingInst() == pCodecInst)
		rdPtr = VpuReadReg(pDecInfo->streamRdPtrRegAddr);
	else
		rdPtr = pDecInfo->streamRdPtr;

	if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT)
	{
		if (rdPtr >= pDecInfo->streamBufEndAddr)
			rdPtr = pDecInfo->streamBufStartAddr + (rdPtr - pDecInfo->streamBufEndAddr);
	}

	wrPtr = pDecInfo->streamWrPtr;

	if (wrPtr < rdPtr) {
		room = rdPtr - wrPtr - VPU_GBU_SIZE*2 - 1;
	}
	else {
		room = ( pDecInfo->streamBufEndAddr - wrPtr ) + ( rdPtr - pDecInfo->streamBufStartAddr ) - VPU_GBU_SIZE*2 - 1;
	}

	*prdPrt = rdPtr;
	*pwrPtr = wrPtr;
	if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT)
		room = ((room >> 9) << 9); // multiple of 512
	*size = room;

	return RETCODE_SUCCESS;
}

RetCode VPU_DecUpdateBitstreamBuffer(DecHandle handle, int size)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	PhysicalAddress wrPtr;
	PhysicalAddress rdPtr;
	RetCode ret;
	int val = 0;
	int room = 0;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	wrPtr = pDecInfo->streamWrPtr;

	if (size == 0)
	{
		val = VpuReadReg(BIT_BIT_STREAM_PARAM);
		val |= 1 << 2;
		pDecInfo->streamEndflag = val;
		if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
			VpuWriteReg(BIT_BIT_STREAM_PARAM, val);

		return RETCODE_SUCCESS;
	}

	if (size == -1)
	{
		val = VpuReadReg(BIT_BIT_STREAM_PARAM);
		val &= ~(1 << 2);
		pDecInfo->streamEndflag = val;
		if (GetPendingInst() == pCodecInst)
			VpuWriteReg(BIT_BIT_STREAM_PARAM, val);

		return RETCODE_SUCCESS;
	}

	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
		rdPtr = VpuReadReg(pDecInfo->streamRdPtrRegAddr);
	else
		rdPtr = pDecInfo->streamRdPtr;

	if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT)
	{
		if (rdPtr >= pDecInfo->streamBufEndAddr)
			rdPtr = pDecInfo->streamBufStartAddr + (rdPtr - pDecInfo->streamBufEndAddr);
	}

	if (wrPtr < rdPtr) {
		if (rdPtr <= wrPtr + size)
			return RETCODE_INVALID_PARAM;
	}

	wrPtr += size;
	if (wrPtr > pDecInfo->streamBufEndAddr) {
		room = wrPtr - pDecInfo->streamBufEndAddr;
		wrPtr = pDecInfo->streamBufStartAddr;
		wrPtr += room;
	}
	if (wrPtr == pDecInfo->streamBufEndAddr) {
		wrPtr = pDecInfo->streamBufStartAddr;
	}

	pDecInfo->streamWrPtr = wrPtr;
	pDecInfo->streamRdPtr = rdPtr;
	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
		VpuWriteReg(pDecInfo->streamWrPtrRegAddr, wrPtr);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecUpdateBitstreamBuffer2(DecHandle handle, int size, int updateEnableFlag)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	PhysicalAddress wrPtr;
	PhysicalAddress rdPtr;
	RetCode ret;
	int		val = 0;
	int		room = 0;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	wrPtr = pDecInfo->streamWrPtr;

	if (size == 0)
	{
		val = VpuReadReg( BIT_BIT_STREAM_PARAM );
		val |= 1 << 2;
		pDecInfo->streamEndflag = val;
		if (GetPendingInst() == pCodecInst )
			VpuWriteReg(BIT_BIT_STREAM_PARAM, val);

		return RETCODE_SUCCESS;
	}

	if (GetPendingInst() == pCodecInst) //FIXME ??? || GetPendingInst() == 0)
		rdPtr = VpuReadReg(pDecInfo->streamRdPtrRegAddr);
	else
		rdPtr = pDecInfo->streamRdPtr;

	if( pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT )
	{
		if( rdPtr >= pDecInfo->streamBufEndAddr)
			rdPtr = pDecInfo->streamBufStartAddr + (rdPtr - pDecInfo->streamBufEndAddr);
	}

	if (wrPtr < rdPtr)
	{
		if (rdPtr <= wrPtr + size)
			return RETCODE_INVALID_PARAM;
	}

	wrPtr += size;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END)
	{
		if (wrPtr > pDecInfo->streamBufEndAddr) {
			room = wrPtr - pDecInfo->streamBufEndAddr;
			wrPtr = pDecInfo->streamBufStartAddr;
			wrPtr += room;
		}
		if (wrPtr == pDecInfo->streamBufEndAddr)
			wrPtr = pDecInfo->streamBufStartAddr;
	}

	pDecInfo->streamWrPtr = wrPtr;
	pDecInfo->streamRdPtr = rdPtr;
	if( updateEnableFlag )
	{
		if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
			VpuWriteReg(pDecInfo->streamWrPtrRegAddr, wrPtr);
	}

	return RETCODE_SUCCESS;
}


/**
* VPU_SWReset
* IN
*    forcedReset : 1 if there is no need to waiting for BUS transaction,
*                  0 for otherwise
* OUT
*    RetCode : RETCODE_FAILURE if failed to reset,
*              RETCODE_SUCCESS for otherwise
*/

RetCode VPU_SWReset(int resetMode)
{
	Uint32 cmd;
	int reason;

	reason = VpuReadReg(BIT_INT_REASON);
	if(reason & (1<<INT_BIT_DEC_FIELD))
	{
	}
	else
	{
		MACRO_VPU_BIT_INT_ENABLE_RESET1
		MACRO_VPU_DELAY_1MS_FOR_INIT
	}

	if (resetMode == SW_RESET_SAFETY || resetMode == SW_RESET_ON_BOOT) {
		// Waiting for completion of bus transaction
		// Step1 : No more request
		VpuWriteReg(GDI_BUS_CTRL, 0x11);	// no more request {3'b0,no_more_req_sec,3'b0,no_more_req}
		MACRO_VPU_DELAY_1MS_FOR_INIT;	//VPU_DELAY_US(1);	//around 20cycle(cclk)

		// Step2 : Waiting for completion of bus transaction
		MACRO_VPU_GDI_BUS_WAITING_EXIT(0x7FFFF);	//while (VpuReadReg(GDI_BUS_STATUS) != 0x77);

		// Step3 : clear GDI_BUS_CTRL
		VpuWriteReg(GDI_BUS_CTRL, 0x00);
	}

	cmd = 0;
	// Software Reset Trigger
	if (resetMode != SW_RESET_ON_BOOT)
		cmd =  VPU_SW_RESET_BPU_CORE | VPU_SW_RESET_BPU_BUS;

	cmd |= VPU_SW_RESET_VCE_CORE | VPU_SW_RESET_VCE_BUS;

	if (resetMode == SW_RESET_ON_BOOT)
		cmd |= VPU_SW_RESET_GDI_CORE | VPU_SW_RESET_GDI_BUS;// If you reset GDI, tiled map should be reconfigured

	VpuWriteReg(BIT_SW_RESET, cmd);
	MACRO_VPU_DELAY_1MS_FOR_INIT;	//VPU_DELAY_US(1);

	MACRO_VPU_BIT_SW_RESET_WAITING_EXIT(0x7FFFF);	//while(VpuReadReg(BIT_SW_RESET_STATUS) != 0);	// wait until reset is done

	// clear sw reset (not automatically cleared)
	VpuWriteReg(BIT_SW_RESET, 0);

	// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
	VpuWriteReg(GDI_BUS_CTRL, 0x00);
	MACRO_VPU_BIT_INT_ENABLE_RESET2

	return RETCODE_SUCCESS;
}

RetCode VPU_SWReset2(void)
{
	Uint32 cmd;

	MACRO_VPU_BIT_INT_ENABLE_RESET1

	if ( 1 ) //resetMode == SW_RESET_SAFETY || resetMode == SW_RESET_ON_BOOT)
	{
		// Waiting for completion of bus transaction
		// Step1 : No more request
		VpuWriteReg(GDI_BUS_CTRL, 0x11);	// no more request {3'b0,no_more_req_sec,3'b0,no_more_req}
		MACRO_VPU_DELAY_1MS_FOR_INIT;	//VPU_DELAY_US(1);	//around 20cycle(cclk)

		// Step2 : Waiting for completion of bus transaction
		MACRO_VPU_GDI_BUS_WAITING_EXIT(0x7FFFF);	//while (VpuReadReg(GDI_BUS_STATUS) != 0x77);

		// Step3 : clear GDI_BUS_CTRL
		VpuWriteReg(GDI_BUS_CTRL, 0x00);
	}

    #ifdef USE_VPU_SWRESET_BPU_CORE_ONLY
	cmd =  VPU_SW_RESET_BPU_CORE;
    #else
	cmd =  VPU_SW_RESET_BPU_CORE | VPU_SW_RESET_BPU_BUS;
    #endif
	VpuWriteReg(BIT_SW_RESET, cmd);

	MACRO_VPU_DELAY_1MS_FOR_INIT;	//VPU_DELAY_US(1);

	// wait until reset is done
	MACRO_VPU_BIT_SW_RESET_WAITING_EXIT(0x7FFFF);	//while(VpuReadReg(BIT_SW_RESET_STATUS) != 0);

	// clear sw reset (not automatically cleared)
	VpuWriteReg(BIT_SW_RESET, 0);

	MACRO_VPU_BIT_INT_ENABLE_RESET2

	return RETCODE_SUCCESS;
}

RetCode VPU_DecStartOneFrame(DecHandle handle, DecParam *param)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	Uint32 rotMir;
	Uint32 val;
	RetCode ret;
	GDI_TILED_MAP_TYPE mapType;
	Uint32 mapEnable;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	if (pDecInfo->stride == 0)	// This means frame buffers have not been registered.
		return RETCODE_WRONG_CALL_SEQUENCE;


	rotMir = 0;
	if (pDecInfo->rotationEnable) {
		rotMir |= 0x10; // Enable rotator
		switch (pDecInfo->rotationAngle) {
			case   0: rotMir |= 0x0; break;
			case  90: rotMir |= 0x1; break;
			case 180: rotMir |= 0x2; break;
			case 270: rotMir |= 0x3; break;
		}
	}

	if (pDecInfo->mirrorEnable) {
		rotMir |= 0x10; // Enable rotator
		switch (pDecInfo->mirrorDirection) {
			case MIRDIR_NONE    : rotMir |= 0x0; break;
			case MIRDIR_VER     : rotMir |= 0x4; break;
			case MIRDIR_HOR     : rotMir |= 0x8; break;
			case MIRDIR_HOR_VER : rotMir |= 0xc; break;
			case MIRDIR_NULL    :                break;
		}
	}

	if (pDecInfo->tiled2LinearEnable)
		rotMir |= 0x10;

	if (pDecInfo->deringEnable)
		rotMir |= 0x20; // Enable Dering Filter

	if (rotMir && !pDecInfo->rotatorOutputValid)
		return RETCODE_ROTATOR_OUTPUT_NOT_SET;

	if ((GetPendingInst() != pCodecInst) && (GetPendingInst() != 0))
		return RETCODE_FRAME_NOT_COMPLETE;

	{
		int i;
		for (i = 0x180; i < 0x200; i = i + 4)
			VpuWriteReg(BIT_BASE + i, 0x00);
	}

	// Set frame buffer for MJPEG or Post-processor
	if (rotMir & 0x30) // rotator or dering or tiled2linear enabled
	{
		VpuWriteReg(CMD_DEC_PIC_ROT_MODE, rotMir);
		VpuWriteReg(CMD_DEC_PIC_ROT_INDEX, pDecInfo->rotatorOutput.myIndex);
		VpuWriteReg(CMD_DEC_PIC_ROT_ADDR_Y,  pDecInfo->rotatorOutput.bufY);
		VpuWriteReg(CMD_DEC_PIC_ROT_ADDR_CB, pDecInfo->rotatorOutput.bufCb);
		VpuWriteReg(CMD_DEC_PIC_ROT_ADDR_CR, pDecInfo->rotatorOutput.bufCr);
		VpuWriteReg(CMD_DEC_PIC_ROT_STRIDE, pDecInfo->rotatorStride);
	}
	else
	{
		VpuWriteReg(CMD_DEC_PIC_ROT_MODE, rotMir);
	}

	if(pDecInfo->userDataEnable)
	{
		VpuWriteReg(CMD_DEC_PIC_USER_DATA_BASE_ADDR, pDecInfo->userDataBufAddr);
		VpuWriteReg(CMD_DEC_PIC_USER_DATA_BUF_SIZE, pDecInfo->userDataBufSize);
	}
	else
	{
		VpuWriteReg(CMD_DEC_PIC_USER_DATA_BASE_ADDR, 0);
		VpuWriteReg(CMD_DEC_PIC_USER_DATA_BUF_SIZE, 0);
	}

	val = 0;
	if (param->iframeSearchEnable != 0) // if iframeSearch is Enable, other bit is ignore;
	{
		val |= (pDecInfo->userDataReportMode		<<10 );

		if (pCodecInst->codecMode == AVC_DEC)
		{
			if (param->iframeSearchEnable == 1)	// IDR
				val |= ((1 << 11) | (1 << 2));
			else if (param->iframeSearchEnable == 0x201)	// I-Slice
				val |= (1 << 2);
			if (param->avcDpbResetOnIframeSearch)	//TELBB_535_IFRAMESEARCH_RESET_ENABLE
				val |= (1 << 9);	// AVC dpbresetOnIframeSearch control (CMD_DEC_PIC_OPTION reg., TELBB-535)
		}
		else
		{
			val |= ((param->iframeSearchEnable & 0x1) << 2);
		}
	}
	else
	{
		val |= (pDecInfo->userDataReportMode << 10);
		if (!param->skipframeMode)
			val |= (pDecInfo->userDataEnable << 5);
		val |= (param->skipframeMode << 3);
	}

	if (pCodecInst->codecMode == MP2_DEC)
		val |= ((param->DecStdParam.mp2PicFlush & 1) << 15);

	VpuWriteReg(CMD_DEC_PIC_OPTION, val);
	VpuWriteReg(CMD_DEC_PIC_NUM_ROWS, 0);

	val = 0;
	val |= ((pDecInfo->secAxiUse.useBitEnable  & 0x01) << 0);
	val |= ((pDecInfo->secAxiUse.useIpEnable   & 0x01) << 1);
	val |= ((pDecInfo->secAxiUse.useDbkYEnable & 0x01) << 2);
	val |= ((pDecInfo->secAxiUse.useDbkCEnable & 0x01) << 3);
	val |= ((pDecInfo->secAxiUse.useOvlEnable  & 0x01) << 4);
	val |= ((pDecInfo->secAxiUse.useBtpEnable  & 0x01) << 5);
	val |= ((pDecInfo->secAxiUse.useBitEnable  & 0x01) << 8);
	val |= ((pDecInfo->secAxiUse.useIpEnable   & 0x01) << 9);
	val |= ((pDecInfo->secAxiUse.useDbkYEnable & 0x01) <<10);
	val |= ((pDecInfo->secAxiUse.useDbkCEnable & 0x01) <<11);
	val |= ((pDecInfo->secAxiUse.useOvlEnable  & 0x01) <<12);
	val |= ((pDecInfo->secAxiUse.useBtpEnable  & 0x01) <<13);

	VpuWriteReg(BIT_AXI_SRAM_USE, val);

	VpuWriteReg(pDecInfo->streamWrPtrRegAddr, pDecInfo->streamWrPtr);
	VpuWriteReg(pDecInfo->streamRdPtrRegAddr, pDecInfo->streamRdPtr);

	val = pDecInfo->frameDisplayFlag;

	val &= ~pDecInfo->clearDisplayIndexes;
	VpuWriteReg(pDecInfo->frameDisplayFlagRegAddr, val);

	pDecInfo->clearDisplayIndexes = 0;
	pDecInfo->setDisplayIndexes = 0;

	pDecInfo->streamEndflag &= ~(3<<3);
	if (pDecInfo->openParam.bitstreamMode == BS_MODE_ROLLBACK)  //rollback mode
		pDecInfo->streamEndflag |= (1<<3);
	else if (pDecInfo->openParam.bitstreamMode == BS_MODE_PIC_END)
		pDecInfo->streamEndflag |= (2<<3);
	VpuWriteReg(BIT_BIT_STREAM_PARAM, pDecInfo->streamEndflag);

	val = 0;
	val |= (pDecInfo->openParam.bwbEnable<<12);
	if (pDecInfo->mapType)
	{
		if (pDecInfo->mapType == TILED_FRAME_MB_RASTER_MAP)
			val |= (pDecInfo->tiled2LinearEnable<<11)|(0x03<<9)|(FORMAT_420<<6);
		else
			val |= (pDecInfo->tiled2LinearEnable<<11)|(0x02<<9)|(FORMAT_420<<6);
	}
	val |= ((pDecInfo->openParam.cbcrInterleave)<<2); // Interleave bit position is modified
	val |= pDecInfo->openParam.frameEndian;
	VpuWriteReg(BIT_FRAME_MEM_CTRL, val);

	val = pDecInfo->openParam.streamEndian;
	VpuWriteReg(BIT_BIT_STREAM_CTRL, val);

#ifdef USE_CODEC_PIC_RUN_INTERRUPT
	VpuWriteReg(BIT_INT_ENABLE,  (1 << INT_BIT_PIC_RUN      )
				   | (1 << INT_BIT_SEQ_END      )
				   | (1 << INT_BIT_BIT_BUF_EMPTY)
				   | (1 << INT_BIT_BIT_BUF_FULL )
				   | (1 << INT_BIT_DEC_FIELD    ));
#endif

	pDecInfo->frameStartPos = pDecInfo->streamRdPtr;
	pDecInfo->streamRdPtrBackUp = pDecInfo->streamRdPtr;

	BitIssueCommand(pCodecInst, PIC_RUN);
	SetPendingInst(pCodecInst);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecGetOutputInfo(DecHandle handle, DecOutputInfo *info)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;
	Uint32 val = 0;
	Uint32 val2 = 0;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS) {
		SetPendingInst(0);
		return ret;
	}

	if (info == 0) {
		SetPendingInst(0);
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (pCodecInst != GetPendingInst()) {
		SetPendingInst(0);
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	pDecInfo->streamRdPtr = VpuReadReg(pDecInfo->streamRdPtrRegAddr);
	pDecInfo->frameDisplayFlag = VpuReadReg(pDecInfo->frameDisplayFlagRegAddr);
	pDecInfo->frameEndPos = pDecInfo->streamRdPtr;
	if (pDecInfo->frameEndPos < pDecInfo->frameStartPos)
		info->consumedByte = pDecInfo->frameEndPos + pDecInfo->streamBufSize - pDecInfo->frameStartPos;
	else
		info->consumedByte = pDecInfo->frameEndPos - pDecInfo->frameStartPos;
	info->frameCycle = VpuReadReg(BIT_FRAME_CYCLE);

	val = VpuReadReg(RET_DEC_PIC_SUCCESS);
	if (val & (1<<31)) {
		//info->wprotErrReason = VpuReadReg(GDI_WPROT_ERR_RSN);
		//info->wprotErrAddress = VpuReadReg(GDI_WPROT_ERR_ADR);
		SetPendingInst(0);
		return RETCODE_MEMORY_ACCESS_VIOLATION;
	}

	if( pCodecInst->codecMode == AVC_DEC ) {
		info->notSufficientPsBuffer = (val >> 3) & 0x1;
		info->notSufficientSliceBuffer = (val >> 2) & 0x1;
	}

	info->chunkReuseRequired = 0;
	if (pDecInfo->openParam.bitstreamMode == BS_MODE_PIC_END)
	{
		if (pCodecInst->codecMode == AVC_DEC)
		{
			info->chunkReuseRequired = ((val >> 16) & 0x01);	// in case of NPF frame
					// H.264/AVC: This flag is valid when AVC decoding and StreamMode in BIT_BIT_STREAM_PARAM[4:3] register is set as picend mode.
					//   0 - Decoded normally
					//   1 - Input stream for decoding should be reused by Non-Paired Field.
		}
	}

	info->indexFrameDecoded = (int)VpuReadReg(RET_DEC_PIC_DECODED_IDX);
	if (info->indexFrameDecoded == -1)
		info->chunkReuseRequired = 0;

	info->indexFrameDisplay = VpuReadReg(RET_DEC_PIC_DISPLAY_IDX);

	val = VpuReadReg(RET_DEC_PIC_SIZE);				// decoding picture size
	info->decPicWidth  = (val>>16) & 0xFFFF;
	info->decPicHeight =  val      & 0xFFFF;

	if (pCodecInst->codecMode == AVC_DEC)
	{
		val = VpuReadReg(RET_DEC_PIC_CROP_LEFT_RIGHT);	// frame crop information(left, right)
		val2 = VpuReadReg(RET_DEC_PIC_CROP_TOP_BOTTOM);	// frame crop information(top, bottom)
		if ((val == 0xFFFFFFFF) && (val2 == 0xFFFFFFFF))
		{
			// Keep current crop information
		}
		else if ((val == 0) && (val2 == 0))
		{
			info->decPicCrop.left = 0;
			info->decPicCrop.right = 0;
			info->decPicCrop.top = 0;
			info->decPicCrop.bottom	= 0;
		}
		else
		{
			info->decPicCrop.left = ((val>>16) & 0xFFFF);
			info->decPicCrop.right = info->decPicWidth - (val&0xFFFF);
			info->decPicCrop.top = ((val2>>16) & 0xFFFF);
			info->decPicCrop.bottom	= info->decPicHeight - (val2&0xFFFF);
		}
	}

	val = VpuReadReg(RET_DEC_PIC_TYPE);
	info->interlacedFrame	= (val >> 18) & 0x1;
	info->topFieldFirst     = (val >> 21) & 0x0001;	// TopFieldFirst[18]

	if (info->interlacedFrame)
	{
		info->picTypeFirst      = (val & 0x38) >> 3;  // pic_type of 1st field
		info->picType           = val & 7;            // pic_type of 2nd field
	}
	else
	{
		info->picTypeFirst   = PIC_TYPE_MAX;	// no meaning
		info->picType = val & 7;
	}

	info->pictureStructure  = (val >> 19) & 0x0003;	// MbAffFlag[17], FieldPicFlag[16]
	info->repeatFirstField  = (val >> 22) & 0x0001;
	info->progressiveFrame  = (val >> 23) & 0x0003;
	info->mp4NVOP = (val >> 6) & 0x1;	// MPEG4 PackedPB Stream N-VOP

	if (pCodecInst->codecMode == AVC_DEC)
	{
		if (info->pictureStructure == 1)
			info->picType = val & 0x3F;

	#ifdef USE_AVC_SEI_PICTIMING_INFO
		info->nalRefIdc         = (val >>  7) & 0x0003;
		info->decFrameInfo      = (val >> 15) & 0x0001;
		info->picStrPresent     = (val >> 27) & 0x0001;
		info->picTimingStruct   = (val >> 28) & 0x000f;

		if ( (info->picStrPresent) && ((info->picTimingStruct >= 1) && (info->picTimingStruct <= 6)) )
		{
			info->interlacedFrame = 1;

			if ( info->picTimingStruct == 3 )
				info->topFieldFirst = 1;
			else if ( info->picTimingStruct == 4 )
				info->topFieldFirst = 0;
		}

		if ( (info->interlacedFrame==1) && (info->picTimingStruct>=3) && (info->picTimingStruct<=6) )
		{
			info->pictureStructure = 1;
			info->picType = val & 0x3F;
		}

		//update picture type when IDR frame
		if (val & 0x40) // 6th bit
		{
			if (info->interlacedFrame)
				info->picTypeFirst = PIC_TYPE_I;//PIC_TYPE_IDR;
			else
				info->picType = PIC_TYPE_I;//PIC_TYPE_IDR;
		}

		info->avcNpfFieldInfo  = (val >> 16) & 0x0003;		// Field information of display frame index returned on RET_DEC_PIC_DISPLAY_IDX(0x1C4)
									//  0 - paired field
									//  1 - bottom (top-field missing)
									//  2 - top (bottom-field missing)
									//  3 - none (top-bottom missing)

		if ((val>>27)&0x01) // pic_struct_present_flag TELBB-184
			info->pic_struct_PicTimingSei =  ((val >> 28) & 0xf) + 1;

		val = VpuReadReg(RET_DEC_PIC_HRD_INFO);
		info->avcHrdInfo.cpbMinus1 = val>>2;
		info->avcHrdInfo.vclHrdParamFlag = (val>>1)&1;
		info->avcHrdInfo.nalHrdParamFlag = val&1;

		val = VpuReadReg(RET_DEC_PIC_VUI_INFO);
		info->avcVuiInfo.fixedFrameRateFlag    = val &1;
		info->avcVuiInfo.timingInfoPresent     = (val>>1) & 0x01;
		info->avcVuiInfo.chromaLocBotField     = (val>>2) & 0x07;
		info->avcVuiInfo.chromaLocTopField     = (val>>5) & 0x07;
		info->avcVuiInfo.chromaLocInfoPresent  = (val>>8) & 0x01;
		info->avcVuiInfo.colorPrimaries        = (val>>16) & 0xff;
		info->avcVuiInfo.colorDescPresent      = (val>>24) & 0x01;
		info->avcVuiInfo.isExtSAR              = (val>>25) & 0x01;
		info->avcVuiInfo.vidFullRange          = (val>>26) & 0x01;
		info->avcVuiInfo.vidFormat             = (val>>27) & 0x07;
		info->avcVuiInfo.vidSigTypePresent     = (val>>30) & 0x01;
		info->avcVuiInfo.vuiParamPresent       = (val>>31) & 0x01;

		val = VpuReadReg(RET_DEC_PIC_VUI_PIC_STRUCT);
		info->avcVuiInfo.vuiPicStructPresent = (val & 0x1);
		info->avcVuiInfo.vuiPicStruct = (val>>1);

		val = VpuReadReg(RET_DEC_PIC_VUI_INFO_2);
		info->avcVuiInfo.vuiMatrixCoefficients = val & 0xff;
		info->avcVuiInfo.vuiTransferCharacteristics = (val >> 8) & 0xff;
		info->avcCtType = VpuReadReg(RET_DEC_PIC_CTYPE_INFO);
	#endif	//USE_AVC_SEI_PICTIMING_INFO

		if( pCodecInst->CodecInfo.decInfo.initialInfo.interlace == 0 )  //frame_mbs_only_flag (If it is not Progress only)
		{
			if (info->avcNpfFieldInfo)
				info->picType = info->picType | (info->avcNpfFieldInfo << 16);
		}
	}

	info->fRateNumerator    = VpuReadReg(RET_DEC_PIC_FRATE_NR); //Frame rate, Aspect ratio can be changed frame by frame.
	info->fRateDenominator  = VpuReadReg(RET_DEC_PIC_FRATE_DR);

	if ((pCodecInst->codecMode == AVC_DEC) && (info->fRateDenominator > 0))
		info->fRateDenominator *= 2;

	info->frameRateRes      = info->fRateNumerator;
	info->frameRateDiv      = info->fRateDenominator;


	info->aspectRateInfo = VpuReadReg(RET_DEC_PIC_ASPECT);

	info->numOfErrMBs = VpuReadReg(RET_DEC_PIC_ERR_MB);

	val = VpuReadReg(RET_DEC_PIC_SUCCESS);
	info->decodingSuccess = val & 0x01;
	info->sequenceChanged = ((val >> 20) & 0x1);
	//info->streamEndFlag = ((pDecInfo->streamEndflag>>2) & 0x01);

	if (pCodecInst->codecMode == AVC_DEC)
	{
		if (pCodecInst->codecModeAux == AVC_AUX_MVC)
		{
			val = VpuReadReg(RET_DEC_PIC_MVC_REPORT);
			info->mvcPicInfo.viewIdxDisplay = (val>>0) & 1;
			info->mvcPicInfo.viewIdxDecoded = (val>>1) & 1;
		}

		val = VpuReadReg(RET_DEC_PIC_AVC_FPA_SEI0);
		if (val < 0) {
			info->avcFpaSei.exist = 0;
		}
		else
		{
			info->avcFpaSei.exist = 1;
			info->avcFpaSei.framePackingArrangementId = val;

			val = VpuReadReg(RET_DEC_PIC_AVC_FPA_SEI1);
			info->avcFpaSei.contentInterpretationType                =  val & 0x3F;      // [5:0]
			info->avcFpaSei.framePackingArrangementType              = (val >>  6)&0x7F; // [12:6]
			info->avcFpaSei.framePackingArrangementExtensionFlag     = (val >> 13)&0x01; // [13]
			info->avcFpaSei.frame1SelfContainedFlag                  = (val >> 14)&0x01; // [14]
			info->avcFpaSei.frame0SelfContainedFlag                  = (val >> 15)&0x01; // [15]
			info->avcFpaSei.currentFrameIsFrame0Flag                 = (val >> 16)&0x01; // [16]
			info->avcFpaSei.fieldViewsFlag                           = (val >> 17)&0x01; // [17]
			info->avcFpaSei.frame0FlippedFlag                        = (val >> 18)&0x01; // [18]
			info->avcFpaSei.spatialFlippingFlag                      = (val >> 19)&0x01; // [19]
			info->avcFpaSei.quincunxSamplingFlag                     = (val >> 20)&0x01; // [20]
			info->avcFpaSei.framePackingArrangementCancelFlag        = (val >> 21)&0x01; // [21]

			val = VpuReadReg(RET_DEC_PIC_AVC_FPA_SEI2);
			info->avcFpaSei.framePackingArrangementRepetitionPeriod  =  val & 0x7FFF;    // [14:0]
			info->avcFpaSei.frame1GridPositionY                      = (val >> 16)&0x0F; // [19:16]
			info->avcFpaSei.frame1GridPositionX                      = (val >> 20)&0x0F; // [23:20]
			info->avcFpaSei.frame0GridPositionY                      = (val >> 24)&0x0F; // [27:24]
			info->avcFpaSei.frame0GridPositionX                      = (val >> 28)&0x0F; // [31:28]
		}

		info->avcPocTop = VpuReadReg(RET_DEC_PIC_POC_TOP);
		info->avcPocBot = VpuReadReg(RET_DEC_PIC_POC_BOT);

		if ( info->interlacedFrame )
		{
			if (info->avcPocTop > info->avcPocBot)
				info->avcPocPic = info->avcPocBot;
			else
				info->avcPocPic = info->avcPocTop;
		}
		else
			info->avcPocPic = VpuReadReg(RET_DEC_PIC_POC);
	}

	if( pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT )
	{
		info->rdPtr = VpuReadReg(BIT_RD_PTR);
		info->wrPtr = VpuReadReg(BIT_WR_PTR);
	}

	info->bytePosFrameStart = VpuReadReg(BIT_BYTE_POS_FRAME_START);
	info->bytePosFrameEnd   = VpuReadReg(BIT_BYTE_POS_FRAME_END);

    #ifdef USE_VPU_DEC_RING_MODE_RDPTR_WRAPAROUND
	if (pDecInfo->streamRdPtr > pDecInfo->streamBufEndAddr)
		pDecInfo->streamRdPtr = pDecInfo->streamBufStartAddr + pDecInfo->streamRdPtr - pDecInfo->streamBufEndAddr;
    #endif

	info->frameCycle = VpuReadReg(BIT_FRAME_CYCLE); // Cycle Log
	info->rdPtr = pDecInfo->streamRdPtr;
	info->wrPtr = pDecInfo->streamWrPtr;

	//if (info->sequenceChanged != 0)
	//	pDecInfo->initialInfo.sequenceNo++;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_INTERRUPT)
	{
		SetPendingInst(0);
	}
	else
	{
		if ((pDecInfo->m_iPendingReason & (1 << INT_BIT_BIT_BUF_EMPTY)) == 0)
			SetPendingInst(0);
	}

	return RETCODE_SUCCESS;
}

RetCode VPU_DecBitBufferFlush(DecHandle handle)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (GetPendingInst()) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	//reset read pointer before buffer flush
	pDecInfo->streamRdPtr = pDecInfo->streamBufStartAddr;
	pDecInfo->streamRdPtrBackUp = pDecInfo->streamRdPtr;
	VpuWriteReg(pDecInfo->streamRdPtrRegAddr, pDecInfo->streamBufStartAddr);

    #ifdef TEST_WRPTR_UPDATE_TIMEOUT
	VpuWriteReg(BIT_INT_ENABLE,
		  (1<< INT_BIT_PIC_RUN)
		| (1<<INT_BIT_SEQ_END)
		| (1<<INT_BIT_BIT_BUF_EMPTY)
		| (1<<INT_BIT_BIT_BUF_FULL)
		| (1<<INT_BIT_DEC_BUF_FLUSH)
			);
    #endif

	BitIssueCommand(pCodecInst, DEC_BUF_FLUSH);

    #ifndef TEST_WRPTR_UPDATE_TIMEOUT
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());
    #else	// TEST_WRPTR_UPDATE_TIMEOUT
	while( 1 )
	{
		int iRet = 0;

	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		if(VpuReadReg(BIT_CUR_PC) == 0x0)
			return RETCODE_CODEC_EXIT;
	#endif
		if( vpu_interrupt != NULL )
			iRet = vpu_interrupt();
		if( iRet == RETCODE_CODEC_EXIT )
		{
			MACRO_VPU_BIT_INT_ENABLE_RESET1
			return RETCODE_CODEC_EXIT;
		}

		iRet = VpuReadReg(BIT_INT_REASON);
		if( iRet )
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
			if( (iRet & (1<<INT_BIT_DEC_BUF_FLUSH)) || (iRet & (1<<INT_BIT_PIC_RUN)) )
			{
				break;
			}else
			{
				return RETCODE_FAILURE;
			}
		}
	}
    #endif	// TEST_WRPTR_UPDATE_TIMEOUT

	if (VpuReadReg(RET_DEC_BUF_FLUSH_SUCCESS) & (1<<31)) {
		return RETCODE_MEMORY_ACCESS_VIOLATION;
	}

	VpuWriteReg(pDecInfo->frameDisplayFlagRegAddr, 0);

	pDecInfo->streamWrPtr = pDecInfo->streamBufStartAddr;
	VpuWriteReg(pDecInfo->streamWrPtrRegAddr, pDecInfo->streamWrPtr);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecFrameBufferFlush(DecHandle handle)
{
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	RetCode ret;
	Uint32 val;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (GetPendingInst()) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	val = pDecInfo->frameDisplayFlag;

	val &= ~pDecInfo->clearDisplayIndexes;
	VpuWriteReg(pDecInfo->frameDisplayFlagRegAddr, val);

	pDecInfo->clearDisplayIndexes = 0;
	pDecInfo->setDisplayIndexes = 0;

	BitIssueCommand(pCodecInst, DEC_BUF_FLUSH);
    #ifdef ENABLE_FORCE_ESCAPE
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);
    #else
	while(VPU_IsBusy());
    #endif

	if (VpuReadReg(RET_DEC_BUF_FLUSH_SUCCESS) & (1<<31)) {
		return RETCODE_MEMORY_ACCESS_VIOLATION;
	}

	pDecInfo->frameDisplayFlag = VpuReadReg(pDecInfo->frameDisplayFlagRegAddr);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecSetRdPtr(DecHandle handle, PhysicalAddress addr, int updateWrPtr)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	VpuWriteReg(pDecInfo->streamRdPtrRegAddr, addr);
	pDecInfo->streamRdPtr = addr;
	if (updateWrPtr)
		pDecInfo->streamWrPtr = addr;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END)
	{
		if (GetPendingInst())
			return RETCODE_FRAME_NOT_COMPLETE;

		BitIssueCommand(pCodecInst, DEC_BUF_FLUSH);
		MACRO_VPU_BUSY_WAITING_DEC_BUF_FLUSH(0x7FFFFFF0)	//while(VPU_IsBusy());

		if (VpuReadReg(RET_DEC_BUF_FLUSH_SUCCESS) & (1<<31))
			return RETCODE_MEMORY_ACCESS_VIOLATION;
	}

	return RETCODE_SUCCESS;
}

RetCode VPU_DecClrDispFlag(DecHandle handle, int index)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (pDecInfo->stride == 0)	// This means frame buffers have not been registered.
		return RETCODE_WRONG_CALL_SEQUENCE;

	if ((index < 0) || (index > (pDecInfo->numFrameBuffers - 1)))
		return	RETCODE_INVALID_PARAM;

	pDecInfo->clearDisplayIndexes |= (1<<index);

	return RETCODE_SUCCESS;
}

RetCode VPU_DecGiveCommand(DecHandle handle,
			   CodecCommand cmd,
			   void *param)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	switch (cmd)
	{
	case ENABLE_ROTATION :
		if (pDecInfo->rotatorStride == 0)
			return RETCODE_ROTATOR_STRIDE_NOT_SET;
		pDecInfo->rotationEnable = 1;
		break;
	case DISABLE_ROTATION :
		pDecInfo->rotationEnable = 0;
		break;
	case ENABLE_MIRRORING :
		if (pDecInfo->rotatorStride == 0)
			return RETCODE_ROTATOR_STRIDE_NOT_SET;
		pDecInfo->mirrorEnable = 1;
		break;
	case DISABLE_MIRRORING :
		pDecInfo->mirrorEnable = 0;
		break;
	case SET_MIRROR_DIRECTION :
		{
			MirrorDirection mirDir;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			mirDir = *(MirrorDirection *)param;
			if (!(MIRDIR_NONE <= mirDir && mirDir <= MIRDIR_HOR_VER))
				return RETCODE_INVALID_PARAM;
			pDecInfo->mirrorDirection = mirDir;
		}
		break;
	case SET_ROTATION_ANGLE :
		{
			int angle;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			angle = *(int *)param;
			if ((angle != 0) && (angle != 90) && (angle != 180) && (angle != 270))
				return RETCODE_INVALID_PARAM;

			if (pDecInfo->rotatorStride != 0) {
				if ((angle == 90) || (angle ==270)) {
					if (pDecInfo->initialInfo.picHeight > pDecInfo->rotatorStride)
						return RETCODE_INVALID_PARAM;
				}
				else
				{
					if (pDecInfo->initialInfo.picWidth > pDecInfo->rotatorStride)
						return RETCODE_INVALID_PARAM;
				}
			}
			pDecInfo->rotationAngle = angle;
		}
		break;
	case SET_ROTATOR_OUTPUT :
		{
			FrameBuffer *frame;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			frame = (FrameBuffer *)param;
			pDecInfo->rotatorOutput = *frame;
			pDecInfo->rotatorOutputValid = 1;
		}
		break;
	case SET_ROTATOR_STRIDE :
		{
			int stride;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			stride = *(int *)param;
			if ((stride % 8 != 0) || (stride == 0))
				return RETCODE_INVALID_STRIDE;

			if (pDecInfo->openParam.bitstreamFormat != STD_MJPG)
			{
				if ((pDecInfo->rotationAngle == 90) || (pDecInfo->rotationAngle == 270))
				{
					if (pDecInfo->initialInfo.picHeight > stride)
						return RETCODE_INVALID_STRIDE;
				}
				else
				{
					if (pDecInfo->initialInfo.picWidth > stride)
						return RETCODE_INVALID_STRIDE;
				}
			}
			pDecInfo->rotatorStride = stride;
		}
		break;
	case DEC_SET_SPS_RBSP:
		if (pCodecInst->codecMode != AVC_DEC)
			return RETCODE_INVALID_COMMAND;
		if (param == 0)
			return RETCODE_INVALID_PARAM;
		return SetParaSet(handle, 0, (DecParamSet *)param);
		break;
	case DEC_SET_PPS_RBSP:
		if (pCodecInst->codecMode != AVC_DEC)
			return RETCODE_INVALID_COMMAND;
		if (param == 0)
			return RETCODE_INVALID_PARAM;
		return SetParaSet(handle, 1, (DecParamSet *)param);
		break;
	case ENABLE_DERING :
		if (pDecInfo->rotatorStride == 0)
			return RETCODE_ROTATOR_STRIDE_NOT_SET;
		pDecInfo->deringEnable = 1;
		break;
	case DISABLE_DERING :
		pDecInfo->deringEnable = 0;
		break;
	case SET_SEC_AXI:
		{
			SecAxiUse secAxiUse;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			secAxiUse = *(SecAxiUse *)param;
			pDecInfo->secAxiUse = secAxiUse;
		}
		break;
	case ENABLE_REP_USERDATA:
		if (!pDecInfo->userDataBufAddr)
			return RETCODE_USERDATA_BUF_NOT_SET;
		if (pDecInfo->userDataBufSize == 0)
			return RETCODE_USERDATA_BUF_NOT_SET;
		pDecInfo->userDataEnable = 1;
		break;
	case DISABLE_REP_USERDATA:
		pDecInfo->userDataEnable = 0;
		break;
	case SET_ADDR_REP_USERDATA:
		{
			PhysicalAddress userDataBufAddr;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			userDataBufAddr = *(PhysicalAddress *)param;
			if ((userDataBufAddr % 8 != 0) || (userDataBufAddr == 0))
				return RETCODE_INVALID_PARAM;
			pDecInfo->userDataBufAddr = userDataBufAddr;
		}
		break;
	case SET_SIZE_REP_USERDATA:
		{
			PhysicalAddress userDataBufSize;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			userDataBufSize = *(PhysicalAddress *)param;
			pDecInfo->userDataBufSize = userDataBufSize;
		}
		break;
	case SET_USERDATA_REPORT_MODE:
		{
			int userDataMode;

			userDataMode = *(int *)param;
			if ((userDataMode != 1) && (userDataMode != 0))
				return RETCODE_INVALID_PARAM;
			pDecInfo->userDataReportMode = userDataMode;
		}
		break;
	case SET_CACHE_CONFIG:
		{
			MaverickCacheConfig *mcCacheConfig;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			mcCacheConfig = (MaverickCacheConfig *)param;
			pDecInfo->cacheConfig.CacheMode = mcCacheConfig->CacheMode;
		}
		break;
	case SET_DECODE_FLUSH:
		{
			Uint32 val;

			if (pDecInfo->openParam.bitstreamMode != BS_MODE_INTERRUPT)
				return RETCODE_INVALID_COMMAND;
			val = VpuReadReg(BIT_BIT_STREAM_PARAM);
			val &= ~(3<<3);
			val |= (2<<3);	// set to pic_end mode
			VpuWriteReg(BIT_BIT_STREAM_PARAM, val);
		}
		break;
	case DEC_SET_FRAME_DELAY:
		pDecInfo->frameDelay = *(int *)param;
		break;
	case DEC_GET_FIELD_PIC_TYPE:
		*((int *)param)  = VpuReadReg(RET_DEC_PIC_TYPE) & 0x1f;
		break;
	case DEC_SET_AVC_ERROR_CONCEAL_MODE:
		if (pCodecInst->codecMode != AVC_DEC)
			return RETCODE_INVALID_COMMAND;
		if (pDecInfo->initialInfoObtained)
			return RETCODE_WRONG_CALL_SEQUENCE;
		pDecInfo->avcErrorConcealMode = *(AVCErrorConcealMode *)param;
		break;
	case DEC_ENABLE_REORDER:
		if (pDecInfo->initialInfoObtained)
			return RETCODE_WRONG_CALL_SEQUENCE;
		pDecInfo->reorderEnable = 1;
		break;
	case DEC_DISABLE_REORDER:
		if (pDecInfo->initialInfoObtained) {
			return RETCODE_WRONG_CALL_SEQUENCE;
		}
		pDecInfo->reorderEnable = 0;
		break;
	case DEC_SET_DISPLAY_FLAG:
		{
			int index;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			index = *(int *)param;
			pDecInfo->setDisplayIndexes |= (1<<index);
		}
		break;
	default:
		return RETCODE_INVALID_COMMAND;
	}
	return RETCODE_SUCCESS;
}


#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

RetCode VPU_EncOpen(EncHandle *pHandle, EncOpenParam *pop)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	RetCode ret;
	int i;
	if (pop->bitstreamFormat != STD_MJPG) {
		if (VPU_IsInit() == 0)
			return RETCODE_NOT_INITIALIZED;
	}

	ret = CheckEncOpenParam(pop);
	if (ret != RETCODE_SUCCESS)
		return ret;

	ret = GetCodecInstance(&pCodecInst);
	if (ret == RETCODE_FAILURE) {
		*pHandle = 0;
		return RETCODE_FAIL_GET_INSTANCE;
	}

	*pHandle = pCodecInst;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	pEncInfo->openParam = *pop;
	#ifdef CMD_ENC_SEQ_264_VUI_PARA //soo_202409
	if (pop->bitstreamFormat == STD_AVC)
	{
		EncAvcParam *pIn, *pOut;

		pIn = &pop->EncStdParam.avcParam;
		pOut = &pEncInfo->openParam.EncStdParam.avcParam;

		if ((pIn->avc_vui_present_flag == 1) && (pIn->avc_vui_param.video_signal_type_pres_flag == 1))
		{
			pOut->avc_vui_present_flag = 1;
			pOut->avc_vui_param.video_signal_type_pres_flag = 1;
			pOut->avc_vui_param.video_format = pIn->avc_vui_param.video_format & 0x7;	//3 bits
			pOut->avc_vui_param.video_full_range_flag = pIn->avc_vui_param.video_full_range_flag & 0x1;
			pOut->avc_vui_param.colour_descrip_pres_flag = pIn->avc_vui_param.colour_descrip_pres_flag & 0x1;
			if (pOut->avc_vui_param.colour_descrip_pres_flag == 1)
			{
				pOut->avc_vui_param.colour_primaries = pIn->avc_vui_param.colour_primaries & 0x0FF;
				pOut->avc_vui_param.transfer_characteristics = pIn->avc_vui_param.transfer_characteristics & 0x0FF;
				pOut->avc_vui_param.matrix_coeff = pIn->avc_vui_param.matrix_coeff & 0x0FF;
			} else
			{
				pOut->avc_vui_param.colour_primaries = 2;
				pOut->avc_vui_param.transfer_characteristics = 2;
				pOut->avc_vui_param.matrix_coeff = 2;
			}
		}
		else
		{
			pOut->avc_vui_present_flag = 0;
			pOut->avc_vui_param.video_signal_type_pres_flag = 0;
			pOut->avc_vui_param.video_format = 5;
			pOut->avc_vui_param.video_full_range_flag = 0;
			pOut->avc_vui_param.colour_descrip_pres_flag = 0;
			pOut->avc_vui_param.colour_primaries = 2;
			pOut->avc_vui_param.transfer_characteristics = 2;
			pOut->avc_vui_param.matrix_coeff = 2;
		}
	}
	#endif
	pEncInfo->mapType = pop->mapType;

	if( pop->bitstreamFormat == STD_AVC )
		pCodecInst->codecMode = AVC_ENC;
	else
		return RETCODE_INVALID_PARAM;

	pCodecInst->codecModeAux = 0;

	pEncInfo->streamRdPtr = pop->bitstreamBuffer;
	pEncInfo->streamWrPtr = pop->bitstreamBuffer;

	if (pCodecInst->codecMode != MJPG_ENC)
	{
		pEncInfo->streamRdPtrRegAddr = BIT_RD_PTR;
		pEncInfo->streamWrPtrRegAddr = BIT_WR_PTR;
	}

	pEncInfo->streamBufStartAddr = pop->bitstreamBuffer;
	pEncInfo->streamBufSize = pop->bitstreamBufferSize;
	pEncInfo->streamBufEndAddr = pop->bitstreamBuffer + pop->bitstreamBufferSize;
	pEncInfo->frameBufPool = 0;
	pEncInfo->workBufBaseAddr = pop->workBuffer;

	pEncInfo->secAxiUse.useBitEnable = 0;
	pEncInfo->secAxiUse.useIpEnable = 0;
	pEncInfo->secAxiUse.useDbkYEnable = 0;
	pEncInfo->secAxiUse.useDbkCEnable = 0;
	pEncInfo->secAxiUse.useOvlEnable = 0;

	pEncInfo->rotationEnable = 0;
	pEncInfo->mirrorEnable = 0;
	pEncInfo->mirrorDirection = MIRDIR_NONE;
	pEncInfo->rotationAngle = 0;
	pEncInfo->enReportMBInfo = 1;
	pEncInfo->initialInfoObtained = 0;
	pEncInfo->ringBufferEnable = pop->ringBufferEnable;
	pEncInfo->linear2TiledEnable = pop->linear2TiledEnable;
	pEncInfo->subFrameSyncConfig.subFrameSyncOn = 0;        // By default, turn off SubFrameSyn
	pEncInfo->subFrameSyncConfig.sourceBufNumber = 0;
	pEncInfo->subFrameSyncConfig.sourceBufIndexBase = 0;

	VpuWriteReg(pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
	VpuWriteReg(pEncInfo->streamWrPtrRegAddr, pEncInfo->streamBufStartAddr);

	//VpuWriteReg(GDI_WPROT_RGN_EN, 0);
	return RETCODE_SUCCESS;
}

RetCode VPU_EncClose(EncHandle handle)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	RetCode ret;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (GetPendingInst())
		return RETCODE_FRAME_NOT_COMPLETE;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pEncInfo->initialInfoObtained)
	{
		VpuWriteReg(pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
		VpuWriteReg(pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);

		VpuWriteReg(BIT_INT_REASON, 0);
		BitIssueCommand(pCodecInst, SEQ_END);
		MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	while(VPU_IsBusy());

		pEncInfo->streamWrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
	}
	SetPendingInst(0);

	if( VpuReadReg(BIT_INT_STS) != 0 )
	{
		VpuWriteReg(BIT_INT_REASON, 0);
		VpuWriteReg(BIT_INT_CLEAR, 1);
	}

	FreeCodecInstance(pCodecInst);

	return RETCODE_SUCCESS;
}

RetCode VPU_EncGetInitialInfo(EncHandle handle, EncInitialInfo * info)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	int picWidth;
	int picHeight;
	Uint32	data;
	RetCode ret;
	Uint32 val;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (info == 0)
		return RETCODE_INVALID_PARAM;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pEncInfo->initialInfoObtained)
		return RETCODE_CALLED_BEFORE;

	if (GetPendingInst())
		return RETCODE_FRAME_NOT_COMPLETE;

	if (pCodecInst->m_iVersionRTL == 0)
	{
		if (pCodecInst->codecMode == AVC_ENC)
		{
			MACRO_VPU_SWRESET_EXIT	//VPU_SWReset(SW_RESET_SAFETY);	while(VPU_IsBusy());
		}
	}

	picWidth = ((pEncInfo->openParam.picWidth+15)/16)*16;
	picHeight = ((pEncInfo->openParam.picHeight+15)/16)*16;

	VpuWriteReg(CMD_ENC_SEQ_BB_START, pEncInfo->streamBufStartAddr);
	VpuWriteReg(CMD_ENC_SEQ_BB_SIZE, pEncInfo->streamBufSize / 1024); // size in KB

	// Rotation Left 90 or 270 case : Swap XY resolution for VPU internal usage
	if (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270)
		data = (picHeight<< 16) | picWidth;
	else
		data = (picWidth << 16) | picHeight;

	VpuWriteReg(CMD_ENC_SEQ_SRC_SIZE, data);
	VpuWriteReg(CMD_ENC_SEQ_SRC_F_RATE, pEncInfo->openParam.frameRateInfo);

	VpuWriteReg(CMD_ENC_SEQ_MP4_PARA, 0);
	VpuWriteReg(CMD_ENC_SEQ_263_PARA, 0);	//CMD_ENC_SEQ_264_VUI_PARA
	VpuWriteReg(CMD_ENC_SEQ_264_PARA, 0);

	if (pEncInfo->openParam.bitstreamFormat == STD_AVC)
	{
		VpuWriteReg(CMD_ENC_SEQ_COD_STD, 0x0);
		data = (pEncInfo->openParam.EncStdParam.avcParam.avc_deblkFilterOffsetBeta & 15) << 12 |
			(pEncInfo->openParam.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha & 15) << 8 |
			pEncInfo->openParam.EncStdParam.avcParam.avc_disableDeblk << 6 |
			pEncInfo->openParam.EncStdParam.avcParam.avc_constrainedIntraPredFlag << 5 |
			(pEncInfo->openParam.EncStdParam.avcParam.avc_chromaQpOffset & 31);
		VpuWriteReg(CMD_ENC_SEQ_264_PARA, data);
	#ifdef CMD_ENC_SEQ_264_VUI_PARA	//soo 20240902
		data = 0U;
		{
			unsigned char video_signal_type_present_flag = 0;
			unsigned char video_format = 5;				// 5: (default) Unspecified video format (refer to Table E-2 of H.264 or H.265 video spec.)
			unsigned char video_full_range_flag = 0;		// 0: (default)
			unsigned char color_description_present_flag = 0;	// 0: (default)
			unsigned char color_primaries = 2;			// 2: (default) Unspecified (refer to Table E-3 of H.264 or H.265 video spec.)
			unsigned char transfer_characteristics = 2;		// 2: (default) Unspecified (refer to Table E-4 of H.264 or H.265 video spec.)
			unsigned char matrix_coeffs = 2;			// 2: (default) Unspecified (refer to Table E-5 of H.264 or H.265 video spec.)

			if (pEncInfo->openParam.EncStdParam.avcParam.avc_vui_present_flag)
			{
				VUI_PARAM *pInVui = &pEncInfo->openParam.EncStdParam.avcParam.avc_vui_param;

				video_signal_type_present_flag = pInVui->video_signal_type_pres_flag;
				video_format = pInVui->video_format;
				video_full_range_flag = pInVui->video_full_range_flag;
				color_description_present_flag = pInVui->colour_descrip_pres_flag;
				color_primaries = pInVui->colour_primaries;
				transfer_characteristics = pInVui->transfer_characteristics;
				matrix_coeffs = pInVui->matrix_coeff;
			}

			// CMD_ENC_SEQ_264_VUI_PARA(0x19c)
			//  If the 'CMD_ENC_SEQ_264_VUI_PARA' equals zero, there's no VUI in SPS.
			//  Otherwise, the 'video_signal_type_present_flag' and 'color_description_present_flag' are constrained to be 1.
			//   [29]    : VUI.video_signal_type_present_flag
			//   [28:26] : VUI.video_signal_type_present_flag.video_format (refer to Table E-2 of H.264 or H.265 video spec.)
			//		video_format	Meaning
			//		0		Component
			//		1		PAL
			//		2		NTSC
			//		3		SECAM
			//		4		MAC
			//		5		Unspecified video format  (if is not present)
			//		6		Reserved
			//		7		Reserved
			//   [25]    : VUI.video_signal_type_present_flag.video_full_range_flag (0, if is not present)
			//   [24]    : VUI.video_signal_type_present_flag.color_description_present_flag
			//   [23:16] : VUI.video_signal_type_present_flag.color_primaries (refer to Table E-3 of H.264 or H.265 video spec.) (2 Unspecified, if is not present)
			//   [15: 8] : VUI.video_signal_type_present_flag.transfer_characteristics (refer to Table E-4 of H.264 or H.265 video spec.) (2 Unspecified, if is not present)
			//   [ 7: 0] : VUI.video_signal_type_present_flag.matrix_coeffs (refer to Table E-5 of H.264 or H.265 video spec.) (2 Unspecified, if is not present)
			data  = ((video_signal_type_present_flag & 0x001) << 29);
			data |= ((video_format                   & 0x007) << 26);
			data |= ((video_full_range_flag          & 0x001) << 25);
			data |= ((color_description_present_flag & 0x001) << 24);
			data |= ((color_primaries                & 0x0FF) << 16);
			data |= ((transfer_characteristics       & 0x0FF) <<  8);
			data |=  (matrix_coeffs & 0x0FF) ;
		}
		VpuWriteReg(CMD_ENC_SEQ_264_VUI_PARA, data);	//CMD_ENC_SEQ_263_PARA
	#endif
		VpuWriteReg(CMD_ENC_SEQ_ME_OPTION, (pEncInfo->openParam.me_blk_mode << 3) | (pEncInfo->openParam.MEUseZeroPmv << 2) | pEncInfo->openParam.MESearchRange);
	}

	data = pEncInfo->openParam.slicemode.sliceSize << 2 |
		pEncInfo->openParam.slicemode.sliceSizeMode << 1 |
		pEncInfo->openParam.slicemode.sliceMode;
	VpuWriteReg(CMD_ENC_SEQ_SLICE_MODE, data);
	VpuWriteReg(CMD_ENC_SEQ_GOP_NUM, pEncInfo->openParam.gopSize);

	if (pEncInfo->openParam.bitRate)
	{	// rate control enabled
		data = (!pEncInfo->openParam.enableAutoSkip) << 31 |
			pEncInfo->openParam.initialDelay     << 16 |
			pEncInfo->openParam.bitRate          <<  1 | 1;
		VpuWriteReg(CMD_ENC_SEQ_RC_PARA, data);
	}
	else
	{
		VpuWriteReg(CMD_ENC_SEQ_RC_PARA, 0);
	}

	VpuWriteReg(CMD_ENC_SEQ_RC_BUF_SIZE, pEncInfo->openParam.vbvBufferSize);
	VpuWriteReg(CMD_ENC_SEQ_INTRA_REFRESH, pEncInfo->openParam.intraRefresh);

	data = 0;
	if(pEncInfo->openParam.rcIntraQp>=0)
	{
		data = (1 << 5);
		VpuWriteReg(CMD_ENC_SEQ_INTRA_QP, pEncInfo->openParam.rcIntraQp);
	}
	else
	{
		data = 0;
		VpuWriteReg(CMD_ENC_SEQ_INTRA_QP, 0);
	}

	if (pCodecInst->codecMode == AVC_ENC)
		data |= (pEncInfo->openParam.EncStdParam.avcParam.avc_audEnable << 2);

	if(pEncInfo->openParam.userQpMax)
	{
		data |= (1<<6);
		VpuWriteReg(CMD_ENC_SEQ_RC_QP_MAX, pEncInfo->openParam.userQpMax);
	}
	else
	{
		VpuWriteReg(CMD_ENC_SEQ_RC_QP_MAX, 0);
	}

	if(pEncInfo->openParam.userGamma)
	{
		data |= (1<<7);
		VpuWriteReg(CMD_ENC_SEQ_RC_GAMMA, pEncInfo->openParam.userGamma);
	}
	else
	{
		VpuWriteReg(CMD_ENC_SEQ_RC_GAMMA, 0);
	}

	data |= pEncInfo->openParam.enhancedRCmodelEnable;

#ifdef H264_ENC_IDR_OPTION
	if ((pCodecInst->codecMode == AVC_ENC) && (pEncInfo->openParam.h264IdrEncOption))
		data |= (1<<3);
#endif

	VpuWriteReg(CMD_ENC_SEQ_OPTION, data);
	VpuWriteReg(CMD_ENC_SEQ_RC_INTERVAL_MODE, (pEncInfo->openParam.MbInterval<<2) | pEncInfo->openParam.RcIntervalMode);
	VpuWriteReg(CMD_ENC_SEQ_INTRA_WEIGHT, pEncInfo->openParam.IntraCostWeight);

	VpuWriteReg(pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
	VpuWriteReg(pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);

	val = 0;
	if (pEncInfo->mapType)
	{
		if (pEncInfo->mapType == LINEAR_FRAME_MAP)
			val |= (pEncInfo->linear2TiledEnable<<11)|(0x02<<9)|(FORMAT_420<<6);
		//else if (pEncInfo->mapType == TILED_FRAME_MB_RASTER_MAP)
		//	val |= (pEncInfo->linear2TiledEnable<<11)|(0x03<<9)|(FORMAT_420<<6);
		else
			return RETCODE_INVALID_MAP_TYPE;
	}
	val |= ((pEncInfo->openParam.cbcrInterleave)<<2); // Interleave bit position is modified
	val |= pEncInfo->openParam.frameEndian;
	VpuWriteReg(BIT_FRAME_MEM_CTRL, val);

	val = 0;
	if (pEncInfo->ringBufferEnable == 0)
	{
		val |= (0x1<<5);
		val |= (0x1<<4);
	}
	else
	{
		val |= (0x1<<3);
	}
	val |= pEncInfo->openParam.streamEndian;
	VpuWriteReg(BIT_BIT_STREAM_CTRL, val);

	MACRO_VPU_BIT_INT_ENABLE_RESET1

	VpuWriteReg(BIT_INT_ENABLE,  (1 << INT_BIT_SEQ_INIT     )
				   | (1 << INT_BIT_SEQ_END      )
				   | (1 << INT_BIT_BIT_BUF_FULL )
				   | (1 << INT_BIT_BIT_BUF_EMPTY));
	BitIssueCommand(pCodecInst, SEQ_INIT);

	while( 1 )
	{
		int iRet = 0;

	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		if(VpuReadReg(BIT_CUR_PC) == 0x0)
			return RETCODE_CODEC_EXIT;
	#endif
		if (vpu_interrupt != NULL)
			iRet = vpu_interrupt();
		if (iRet == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_BIT_INT_ENABLE_RESET1
			return RETCODE_CODEC_EXIT;
		}

		iRet = VpuReadReg(BIT_INT_REASON);
		if (iRet)
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
			if (iRet & (1 << INT_BIT_SEQ_INIT))
			{
				break;
			}
			else
			{
				if (iRet & (1 << INT_BIT_BIT_BUF_EMPTY))
				{
					return RETCODE_FRAME_NOT_COMPLETE;
				}
				else if (iRet & (1 << INT_BIT_BIT_BUF_FULL))
				{
					return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
				}
				else
				{
					return RETCODE_FAILURE;
				}
			}
		}
	}

	if (VpuReadReg(RET_ENC_SEQ_ENC_SUCCESS) & (1 << 31))
		return RETCODE_MEMORY_ACCESS_VIOLATION;

	if (VpuReadReg(RET_ENC_SEQ_ENC_SUCCESS) == 0)
		return RETCODE_FAILURE;

	if (pCodecInst->codecMode == MJPG_ENC)
		info->minFrameBufferCount = 0;
	else
		info->minFrameBufferCount = 2;	// reconstructed frame + reference frame

	pEncInfo->initialInfo = *info;
	pEncInfo->initialInfoObtained = 1;

	pEncInfo->streamWrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
	pEncInfo->streamEndflag = VpuReadReg(BIT_BIT_STREAM_PARAM);

	return RETCODE_SUCCESS;
}

RetCode VPU_EncRegisterFrameBuffer(
				EncHandle handle,
				FrameBuffer *bufArray,
				int num,
				int stride,
				PhysicalAddress subSampBaseA,
				PhysicalAddress subSampBaseB,
				ExtBufCfg *scratchBuf)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	int i, k;
	RetCode ret;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pEncInfo->frameBufPool)
		return RETCODE_CALLED_BEFORE;

	if (!pEncInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if (bufArray == 0)
		return RETCODE_INVALID_FRAME_BUFFER;

	if (num < pEncInfo->initialInfo.minFrameBufferCount)
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

	if (stride % 8 != 0 || stride == 0)
		return RETCODE_INVALID_STRIDE;

	if (pCodecInst->codecMode == MJPG_ENC)
	{
		pEncInfo->frameBufPool = bufArray;
		pEncInfo->numFrameBuffers = num;
		pEncInfo->stride = stride;
		return RETCODE_SUCCESS;
	}

	if (GetPendingInst())
		return RETCODE_FRAME_NOT_COMPLETE;

	pEncInfo->frameBufPool = bufArray;
	pEncInfo->numFrameBuffers = num;
	pEncInfo->stride = stride;

	for( i = 0, k = 0; i < num; i+=4, k+=48 )
	{
		VirtualWriteReg(paraBuffer_VA + k +  4, bufArray[i+0].bufY );
		VirtualWriteReg(paraBuffer_VA + k +  0, bufArray[i+0].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 12, bufArray[i+0].bufCr);
		if( i+1 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k +  8, bufArray[i+1].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 20, bufArray[i+1].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 16, bufArray[i+1].bufCr);
		if( i+2 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k + 28, bufArray[i+2].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 24, bufArray[i+2].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 36, bufArray[i+2].bufCr);
		if( i+3 == num ) break;

		VirtualWriteReg(paraBuffer_VA + k + 32, bufArray[i+3].bufY );
		VirtualWriteReg(paraBuffer_VA + k + 44, bufArray[i+3].bufCb);
		VirtualWriteReg(paraBuffer_VA + k + 40, bufArray[i+3].bufCr);
		if( i+4 == num ) break;
	}

	// Tell the codec how much frame buffers were allocated.
	VpuWriteReg(CMD_SET_FRAME_BUF_NUM, num);
	VpuWriteReg(CMD_SET_FRAME_BUF_STRIDE, stride);
	VpuWriteReg(CMD_SET_FRAME_AXI_BIT_ADDR, pEncInfo->secAxiUse.bufBitUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_IPACDC_ADDR, pEncInfo->secAxiUse.bufIpAcDcUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_DBKY_ADDR, pEncInfo->secAxiUse.bufDbkYUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_DBKC_ADDR, pEncInfo->secAxiUse.bufDbkCUse);
	VpuWriteReg(CMD_SET_FRAME_AXI_OVL_ADDR, pEncInfo->secAxiUse.bufOvlUse);

	pEncInfo->cacheConfig.CacheMode = (1<<28);	// MAVERICK_CACHE_CONFIG1 Disable
	pEncInfo->cacheConfig.CacheMode |= 3;		// MAVERICK_CACHE_CONFIG2 Disable
	VpuWriteReg(CMD_SET_FRAME_CACHE_CONFIG, pEncInfo->cacheConfig.CacheMode);

	// Set Sub-Sampling buffer for ME-Reference and DBK-Reconstruction
	// BPU will swap below two buffer internally every pic by pic
	VpuWriteReg(CMD_SET_FRAME_SUBSAMP_A, subSampBaseA);
	VpuWriteReg(CMD_SET_FRAME_SUBSAMP_B, subSampBaseB);

	if (pCodecInst->codecMode == MP4_ENC)
	{
		if (scratchBuf == NULL)
			return RETCODE_INVALID_PARAM;

		// MPEG4 Encoder Data-Partitioned bitstream temporal buffer
		VpuWriteReg(CMD_SET_FRAME_DP_BUF_BASE, scratchBuf->bufferBase);
		VpuWriteReg(CMD_SET_FRAME_DP_BUF_SIZE, scratchBuf->bufferSize);
	}

	BitIssueCommand(pCodecInst, SET_FRAME_BUF);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy())	;
	if (VpuReadReg(RET_SET_FRAME_SUCCESS) & (1<<31))
		return RETCODE_MEMORY_ACCESS_VIOLATION;

	return RETCODE_SUCCESS;
}

RetCode VPU_EncGetBitstreamBuffer( EncHandle handle,
				   PhysicalAddress *prdPrt,
				   PhysicalAddress *pwrPtr,
				   int *size)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	Uint32 room;
	RetCode ret;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if ( prdPrt == 0 || pwrPtr == 0 || size == 0) {
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pCodecInst->codecMode == MJPG_ENC) {
		*prdPrt = pEncInfo->streamRdPtr;
		*pwrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
		*size = *pwrPtr - *prdPrt;
		return RETCODE_SUCCESS;
	}

	rdPtr = pEncInfo->streamRdPtr;

	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0) //FIXME ???
		wrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
	else
		wrPtr = pEncInfo->streamWrPtr;

	if( pEncInfo->ringBufferEnable == 1 )
	{
		if ( wrPtr >= rdPtr )
			room = wrPtr - rdPtr;
		else
			room = ( pEncInfo->streamBufEndAddr - rdPtr ) + ( wrPtr - pEncInfo->streamBufStartAddr );
	}
	else
	{
		if( rdPtr == pEncInfo->streamBufStartAddr && wrPtr >= rdPtr )
			room = wrPtr - rdPtr;
		else
			return RETCODE_INVALID_PARAM;
	}

	*prdPrt = rdPtr;
	*pwrPtr = wrPtr;
	*size = room;

	return RETCODE_SUCCESS;
}

RetCode VPU_EncUpdateBitstreamBuffer ( EncHandle handle, int size)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	PhysicalAddress wrPtr;
	PhysicalAddress rdPtr;
	RetCode ret;
	int	room = 0;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pCodecInst->codecMode == MJPG_ENC)
	{
		pEncInfo->streamRdPtr = pEncInfo->streamBufStartAddr;
		VpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0);
		VpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, pEncInfo->streamBufStartAddr);
		VpuWriteReg(pEncInfo->streamRdPtrRegAddr, pEncInfo->streamBufStartAddr);
		VpuWriteReg(pEncInfo->streamWrPtrRegAddr, pEncInfo->streamBufStartAddr);
		return RETCODE_SUCCESS;
	}

	rdPtr = pEncInfo->streamRdPtr;

	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
		wrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
	else
		wrPtr = pEncInfo->streamWrPtr;

	if ( rdPtr < wrPtr )
	{
		if ( rdPtr + size  > wrPtr )
			return RETCODE_INVALID_PARAM;
	}

	if( pEncInfo->ringBufferEnable == 1 )
	{
		rdPtr += size;
		if (rdPtr > pEncInfo->streamBufEndAddr)
		{
			room = rdPtr - pEncInfo->streamBufEndAddr;
			rdPtr = pEncInfo->streamBufStartAddr;
			rdPtr += room;
		}
		if (rdPtr == pEncInfo->streamBufEndAddr)
		{
			rdPtr = pEncInfo->streamBufStartAddr;
		}
	}
	else
	{
		rdPtr = pEncInfo->streamBufStartAddr;
	}

	pEncInfo->streamRdPtr = rdPtr;
	pEncInfo->streamWrPtr = wrPtr;

	if (GetPendingInst() == pCodecInst || GetPendingInst() == 0)
		VpuWriteReg(pEncInfo->streamRdPtrRegAddr, rdPtr);

	return RETCODE_SUCCESS;
}

RetCode VPU_EncStartOneFrame ( EncHandle handle, EncParam * param )
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	FrameBuffer * pSrcFrame;
	Uint32 rotMirEnable;
	Uint32 rotMirMode;
	Uint32 val;
	RetCode ret;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pCodecInst->codecMode != MJPG_ENC)
	{
		if (pEncInfo->frameBufPool == 0)	// This means frame buffers have not been registered.
			return RETCODE_WRONG_CALL_SEQUENCE;
	}

	ret = CheckEncParam(handle, param);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pSrcFrame = param->sourceFrame;
	rotMirEnable = 0;
	rotMirMode = 0;
	if (pEncInfo->rotationEnable)
	{
		switch (pEncInfo->rotationAngle)
		{
			case   0: rotMirMode |= 0x0; break;
			case  90: rotMirMode |= 0x1; break;
			case 180: rotMirMode |= 0x2; break;
			case 270: rotMirMode |= 0x3; break;
		}
	}

	if (pEncInfo->mirrorEnable)
	{
		switch (pEncInfo->mirrorDirection)
		{
			case MIRDIR_NONE    : rotMirMode |= 0x0; break;
			case MIRDIR_VER     : rotMirMode |= 0x4; break;
			case MIRDIR_HOR     : rotMirMode |= 0x8; break;
			case MIRDIR_HOR_VER : rotMirMode |= 0xc; break;
			case MIRDIR_NULL    :                    break;
		}
	}

	rotMirMode |= rotMirEnable;

	if (GetPendingInst())
		return RETCODE_FRAME_NOT_COMPLETE;

	{
		int i;
		for (i = 0x180; i < 0x200; i = i + 4)
			VpuWriteReg(BIT_BASE + i, 0x00);
	}

	if (pCodecInst->m_iVersionRTL == 0)
	{
		if (pCodecInst->codecMode == AVC_ENC)
		{
		#if 1
			MACRO_VPU_SWRESET_EXIT
		#else
			VPU_SWReset(SW_RESET_SAFETY);
			while(VPU_IsBusy());
		#endif
		}
	}

	VpuWriteReg(CMD_ENC_PIC_ROT_MODE, rotMirMode);
	VpuWriteReg(CMD_ENC_PIC_QS, param->quantParam);

	if (param->skipPicture)
	{
		VpuWriteReg(CMD_ENC_PIC_OPTION, (param->enReportMBInfo << 3) | 1);
	}
	else
	{
		// Registering Source Frame Buffer information
		// Hide GDI IF under FW level
		if (pEncInfo->openParam.cbcrInterleave)
			pSrcFrame->bufCr = 0;
		VpuWriteReg(CMD_ENC_PIC_SRC_INDEX, pSrcFrame->myIndex);
		VpuWriteReg(CMD_ENC_PIC_SRC_STRIDE, pSrcFrame->stride);
		VpuWriteReg(CMD_ENC_PIC_SRC_ADDR_Y, pSrcFrame->bufY);
		VpuWriteReg(CMD_ENC_PIC_SRC_ADDR_CB, pSrcFrame->bufCb);
		VpuWriteReg(CMD_ENC_PIC_SRC_ADDR_CR, pSrcFrame->bufCr);
		VpuWriteReg(CMD_ENC_PIC_OPTION,
			(param->enReportMBInfo<<3) |
			(param->forceIPicture << 1 & 0x2) );
	}

	if (pEncInfo->ringBufferEnable == 0)
	{
		VpuWriteReg(CMD_ENC_PIC_BB_START, param->picStreamBufferAddr);
		VpuWriteReg(CMD_ENC_PIC_BB_SIZE, param->picStreamBufferSize/1024); // size in KB
		VpuWriteReg(pEncInfo->streamRdPtrRegAddr, param->picStreamBufferAddr);
		pEncInfo->streamRdPtr = param->picStreamBufferAddr;
	}

	if(param->enReportMBInfo)
	{
		VpuWriteReg(CMD_ENC_PIC_PARA_BASE_ADDR, param->picParaBaseAddr);

		if(param->enReportMBInfo)
			EncSetHostParaAddr(param->picParaBaseAddr, param->picMbInfoAddr);
	}

	val = ((pEncInfo->secAxiUse.useBitEnable   & 0x01) << 0 |
		(pEncInfo->secAxiUse.useIpEnable   & 0x01) << 1 |
		(pEncInfo->secAxiUse.useDbkYEnable & 0x01) << 2 |
		(pEncInfo->secAxiUse.useDbkCEnable & 0x01) << 3 |
		(pEncInfo->secAxiUse.useOvlEnable  & 0x01) << 4 |
		(pEncInfo->secAxiUse.useBtpEnable  & 0x01) << 5 |
		(pEncInfo->secAxiUse.useBitEnable  & 0x01) << 8 |
		(pEncInfo->secAxiUse.useIpEnable   & 0x01) << 9 |
		(pEncInfo->secAxiUse.useDbkYEnable & 0x01) <<10 |
		(pEncInfo->secAxiUse.useDbkCEnable & 0x01) <<11 |
		(pEncInfo->secAxiUse.useOvlEnable  & 0x01) <<12 |
		(pEncInfo->secAxiUse.useBtpEnable  & 0x01) <<13 );
	VpuWriteReg(BIT_AXI_SRAM_USE, val);

	val = (pEncInfo->subFrameSyncConfig.subFrameSyncOn << 15 |
		pEncInfo->subFrameSyncConfig.sourceBufNumber << 8 |      // Source buffer number - 1
		pEncInfo->subFrameSyncConfig.sourceBufIndexBase << 0);   // Base index of PRP source index, Start index of PRP source
	VpuWriteReg(CMD_ENC_PIC_SUB_FRAME_SYNC, val);

	VpuWriteReg(pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
	VpuWriteReg(pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
	VpuWriteReg(BIT_BIT_STREAM_PARAM, pEncInfo->streamEndflag);

	val = 0;
	if (pEncInfo->mapType)
	{
		if (pEncInfo->mapType == LINEAR_FRAME_MAP)
			val |= ((pEncInfo->linear2TiledEnable<<11)|(0x02<<9)|(FORMAT_420<<6));
		else
			return RETCODE_INVALID_MAP_TYPE;
	}
	val |= ((pEncInfo->openParam.cbcrInterleave)<<2); // Interleave bit position is modified
	val |= pEncInfo->openParam.frameEndian;
	VpuWriteReg(BIT_FRAME_MEM_CTRL, val);

	val = 0;
	if (pEncInfo->ringBufferEnable == 0)
	{
		val |= (0x1<<5);
		val |= (0x1<<4);
	}
	else
	{
		val |= (0x1<<3);
	}
	val |= pEncInfo->openParam.streamEndian;
	VpuWriteReg(BIT_BIT_STREAM_CTRL, val);

#ifdef USE_CODEC_PIC_RUN_INTERRUPT
	VpuWriteReg(BIT_INT_ENABLE,  (1 << INT_BIT_PIC_RUN      )
				   | (1 << INT_BIT_SEQ_END      )
				   | (1 << INT_BIT_BIT_BUF_EMPTY)
				   | (1 << INT_BIT_BIT_BUF_FULL ));
#endif
	BitIssueCommand(pCodecInst, PIC_RUN);
	SetPendingInst(pCodecInst);

	return RETCODE_SUCCESS;
}

RetCode VPU_EncGetOutputInfo( EncHandle handle, EncOutputInfo * info )
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	RetCode ret;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	unsigned int pic_enc_result;
	unsigned int MbQpSum;
	unsigned int MbQpDiv;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
	{
		SetPendingInst(0);
		return ret;
	}

	if (info == 0)
	{
		SetPendingInst(0);
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (pCodecInst != GetPendingInst())
	{
		SetPendingInst(0);
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	pic_enc_result = VpuReadReg(RET_ENC_PIC_SUCCESS);
	if (pic_enc_result & (1<<31))
	{
		SetPendingInst(0);
		return RETCODE_MEMORY_ACCESS_VIOLATION;
	}
	info->picType = VpuReadReg(RET_ENC_PIC_TYPE);
	info->frameCycle = VpuReadReg(BIT_FRAME_CYCLE);

	if (pEncInfo->ringBufferEnable == 0)
	{
		rdPtr = VpuReadReg(pEncInfo->streamRdPtrRegAddr);
		wrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
		info->bitstreamBuffer = rdPtr;
		info->bitstreamSize = wrPtr - rdPtr;
	}

	info->numOfSlices = VpuReadReg(RET_ENC_PIC_SLICE_NUM);
	info->bitstreamWrapAround = VpuReadReg(RET_ENC_PIC_FLAG);
	info->reconFrameIndex = VpuReadReg(RET_ENC_PIC_FRAME_IDX);

	MbQpSum = VpuReadReg(RET_ENC_PIC_SUM_MB_QP);
	MbQpDiv = (int)((pEncInfo->openParam.picWidth+15) / 16) * (int)((pEncInfo->openParam.picHeight+15) / 16);
	info->AvgQp = (int)(MbQpSum + (MbQpDiv>>1)) / MbQpDiv;

	pEncInfo->streamWrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
	pEncInfo->streamEndflag = VpuReadReg(BIT_BIT_STREAM_PARAM);

	SetPendingInst(0);
	return RETCODE_SUCCESS;
}

RetCode VPU_EncGiveCommand( EncHandle handle, CodecCommand cmd, void * param )
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	RetCode ret;

	ret = CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	switch (cmd)
	{
	case ENABLE_ROTATION :
			pEncInfo->rotationEnable = 1;
		break;
	case DISABLE_ROTATION :
			pEncInfo->rotationEnable = 0;
		break;
	case ENABLE_MIRRORING :
			pEncInfo->mirrorEnable = 1;
		break;
	case DISABLE_MIRRORING :
			pEncInfo->mirrorEnable = 0;
		break;
	case SET_MIRROR_DIRECTION :
		{
			MirrorDirection mirDir;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			mirDir = *(MirrorDirection *)param;
			if (!(MIRDIR_NONE <= mirDir && mirDir <= MIRDIR_HOR_VER))
				return RETCODE_INVALID_PARAM;
			pEncInfo->mirrorDirection = mirDir;
		}
		break;
	case SET_ROTATION_ANGLE :
		{
			int angle;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			angle = *(int *)param;
			if ((angle != 0) && (angle != 90) && (angle != 180) && (angle != 270))
				return RETCODE_INVALID_PARAM;
			if (pEncInfo->initialInfoObtained && (angle == 90 || angle ==270))
				return RETCODE_INVALID_PARAM;
			pEncInfo->rotationAngle = angle;
		}
		break;
	case SET_CACHE_CONFIG:
		{
			MaverickCacheConfig *mcCacheConfig;

			if (param == 0)
				return RETCODE_INVALID_PARAM;
			mcCacheConfig = (MaverickCacheConfig *)param;
			pEncInfo->cacheConfig.CacheMode = mcCacheConfig->CacheMode;
		}
		break;
	case ENC_PUT_MP4_HEADER:
	case ENC_PUT_AVC_HEADER:
	case ENC_PUT_VIDEO_HEADER:
		{
			EncHeaderParam *encHeaderParam;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			encHeaderParam = (EncHeaderParam *)param;
			if  (pCodecInst->codecMode == AVC_ENC) {
				if (!( SPS_RBSP<=encHeaderParam->headerType && encHeaderParam->headerType <= PPS_RBSP/*PPS_RBSP_MVC*/)) {
					return RETCODE_INVALID_PARAM;
				}
			}
			else
				return RETCODE_INVALID_PARAM;

			if (pEncInfo->ringBufferEnable == 0 ) {
				if (encHeaderParam->buf % 8 || encHeaderParam->size == 0) {
					return RETCODE_INVALID_PARAM;
				}
			}

			{
				int iRet;
				iRet = GetEncHeader(handle, encHeaderParam);
				if( iRet != RETCODE_SUCCESS )
					return (RetCode)iRet;
			}
		}
		break;

	case ENC_SET_GOP_NUMBER:
		{
			int *pGopNumber =(int *)param;
			if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC ) {
				return RETCODE_INVALID_COMMAND;
			}
			if (*pGopNumber < 0 ||  *pGopNumber > 60) {
				return RETCODE_INVALID_PARAM;
			}
			pEncInfo->openParam.gopSize = *pGopNumber;
			SetGopNumber(handle, (Uint32 *)pGopNumber);
		}
		break;
	case ENC_SET_INTRA_QP:
		{
			int *pIntraQp =(int *)param;
			if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC ) {
				return RETCODE_INVALID_COMMAND;
			}
			if (pCodecInst->codecMode == MP4_ENC)
			{
				if(*pIntraQp<1 || *pIntraQp>31)
					return RETCODE_INVALID_PARAM;
			}
			if (pCodecInst->codecMode == AVC_ENC)
			{
				if(*pIntraQp<0 || *pIntraQp>51)
					return RETCODE_INVALID_PARAM;
			}
			SetIntraQp(handle, (Uint32 *)pIntraQp);
		}
		break;
	case ENC_SET_BITRATE:
		{
			int *pBitrate = (int *)param;
			if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC ) {
				return RETCODE_INVALID_COMMAND;
			}
			if (*pBitrate < 0 || *pBitrate> 32767) {
				return RETCODE_INVALID_PARAM;
			}
			SetBitrate(handle, (Uint32 *)pBitrate);
		}
		break;
	case ENC_SET_FRAME_RATE:
		{
			int *pFramerate = (int *)param;

			if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC ) {
				return RETCODE_INVALID_COMMAND;
			}
			if (*pFramerate <= 0) {
				return RETCODE_INVALID_PARAM;
			}
			SetFramerate(handle, (Uint32 *)pFramerate);
		}
		break;
	case ENC_SET_INTRA_MB_REFRESH_NUMBER:
		{
			int *pIntraRefreshNum =(int *)param;
			SetIntraRefreshNum(handle, (Uint32 *)pIntraRefreshNum);
		}
		break;
	case ENC_SET_SLICE_INFO:
		{
			EncSliceMode *pSliceMode = (EncSliceMode *)param;
			if(pSliceMode->sliceMode<0 || pSliceMode->sliceMode>1)
			{
				return RETCODE_INVALID_PARAM;
			}
			if(pSliceMode->sliceSizeMode<0 || pSliceMode->sliceSizeMode>1)
			{
				return RETCODE_INVALID_PARAM;
			}
			SetSliceMode(handle, (EncSliceMode *)pSliceMode);
		}
		break;
	case ENC_ENABLE_HEC:
		{
			if (pCodecInst->codecMode != MP4_ENC) {
				return RETCODE_INVALID_COMMAND;
			}
			SetHecMode(handle, 1);
		}
		break;
	case ENC_DISABLE_HEC:
		{
			if (pCodecInst->codecMode != MP4_ENC) {
				return RETCODE_INVALID_COMMAND;
			}
			SetHecMode(handle, 0);
		}
		break;
	case ENC_SET_REPORT_MBINFO:
		{
			EncParam *pEncParam = (EncParam*)param;
			pEncInfo->enReportMBInfo = pEncParam->enReportMBInfo;
			pEncInfo->picMbInfoAddr  = pEncParam->picMbInfoAddr;
		}
		break;
	case ENC_SET_PIC_PARA_ADDR:
		{
			EncParam *pEncParam = (EncParam*)param;
			pEncInfo->picParaBaseAddr   = pEncParam->picParaBaseAddr;
		}
		break;
	case SET_SEC_AXI:
		{
			SecAxiUse secAxiUse;

			if (param == 0) {
				return RETCODE_INVALID_PARAM;
			}
			secAxiUse = *(SecAxiUse *)param;
			pEncInfo->secAxiUse = secAxiUse;
		}
		break;
	case ENC_SET_SUB_FRAME_SYNC:
		{
			EncSubFrameSyncConfig *subFrameSyncConfig;
			if (param == 0) {
				return RETCODE_INVALID_PARAM;
			}
			subFrameSyncConfig = (EncSubFrameSyncConfig *)param;
			pEncInfo->subFrameSyncConfig.subFrameSyncOn = subFrameSyncConfig->subFrameSyncOn;
			pEncInfo->subFrameSyncConfig.sourceBufNumber = subFrameSyncConfig->sourceBufNumber;
			pEncInfo->subFrameSyncConfig.sourceBufIndexBase = subFrameSyncConfig->sourceBufIndexBase;

		}
		break;
	case ENC_ENABLE_SUB_FRAME_SYNC:
		{
			pEncInfo->subFrameSyncConfig.subFrameSyncOn = 1;

		}
		break;
	case ENC_DISABLE_SUB_FRAME_SYNC:
		{
			pEncInfo->subFrameSyncConfig.subFrameSyncOn = 0;

		}
		break;
	default:
		return RETCODE_INVALID_COMMAND;
	}
	return RETCODE_SUCCESS;
}

#endif	//TCC_VPU_2K_IP != 0x0630
