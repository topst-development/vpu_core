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

#include "wave5apifunc.h"
#include "wave5api.h"
#include "product.h"
#include "wave5_regdefine.h"
#include "wave5_pre_define.h"
#include "TCC_VPU_4K_D2.h"
#include "wave5_def.h"
#include "wave5.h"
#include "vpu4k_d2_log.h"

extern int WAVE5_dec_reset( wave5_t * pVpu, int iOption );

typedef void* (wave5_memcpy_func) (void *, const void *, unsigned int, unsigned int);	//!< memcpy
typedef void  (wave5_memset_func) (void *, int, unsigned int, unsigned int);		//!< memset
typedef int  (wave5_interrupt_func) (void);						//!< Interrupt
typedef void* (wave5_ioremap_func) (unsigned int, unsigned int);
typedef void (wave5_iounmap_func) (void *);
typedef unsigned int (wave5_reg_read_func) (void*, unsigned int);
typedef void (wave5_reg_write_func) (void*, unsigned int, unsigned int);
typedef void (wave5_usleep_func) (unsigned int, unsigned int);				//!< usleep_range(min, max)

wave5_memcpy_func    *wave5_memcpy    = NULL;
wave5_memset_func    *wave5_memset    = NULL;
wave5_interrupt_func *wave5_interrupt = NULL;
wave5_ioremap_func   *wave5_ioremap   = NULL;
wave5_iounmap_func   *wave5_iounmap   = NULL;
wave5_reg_read_func  *wave5_read_reg  = NULL;
wave5_reg_write_func *wave5_write_reg = NULL;
wave5_usleep_func    *wave5_usleep    = NULL;

int gWave5InitFirst = 0;
int gWave5InitFirst_exit = 0;
codec_addr_t gWave5FWAddr = 0;

int gWave5MaxInstanceNum = WAVE5_MAX_NUM_INSTANCE;

codec_addr_t gWave5VirtualBitWorkAddr;
PhysicalAddress gWave5PhysicalBitWorkAddr;
codec_addr_t gWave5VirtualRegBaseAddr;

CodecInst codecInstPoolWave5[WAVE5_MAX_NUM_INSTANCE];

extern wave5_t gsWave5HandlePool[WAVE5_MAX_NUM_INSTANCE];

int gWave5AccessViolationError = 0;


int checkWave5CodecInstanceAddr(void *ptr)
{
	int ret = 0;
	CodecInst *pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		int i;
		CodecInst *pCodecInstRef = &codecInstPoolWave5[0];

		for (i = 0; i < gWave5MaxInstanceNum; ++i, ++pCodecInstRef) {
			if (pCodecInst == pCodecInstRef) {
				i = gWave5MaxInstanceNum;
				ret = 1;    //success
			}
		}
	}
	return ret;
}

int checkWave5CodecInstanceUse(void *ptr)
{
	int ret = 0;
	CodecInst *pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		if (pCodecInst->inUse == 1)
			ret = 1;    //success
	}
	return ret;
}

int checkWave5CodecInstancePended(void *ptr)
{
	int ret = 0;
	CodecInst * pCodecInst = (CodecInst *)ptr;

	if (pCodecInst != NULL) {
		if (WAVE5_GetPendingInst(pCodecInst->coreIdx) == pCodecInst || WAVE5_GetPendingInst(pCodecInst->coreIdx) == 0)
			ret = 1;
		else
			ret = 0;
	} else {
			ret = 0;
	}
	return ret;
}


int Wave5CheckAccessViolationV251(unsigned int ADDR)
{
	int ret;
	unsigned int access_offset = ADDR & 0x0000FFFF;

	switch( access_offset )
	{
		case 0x028:	case 0x02C:
		case 0x084:	case 0x088:	case 0x08C:
		case 0x0B0:	case 0x0B4:	case 0x0B8:	case 0x0BC:
		case 0x0C0:	case 0x0C4:	case 0x0C8:	case 0x0CC:
		case 0x0D0:	case 0x0D4:	case 0x0D8:	case 0x0DC:
		case 0x0E0:	case 0x0E4:	case 0x0E8:	case 0x0EC:
				ret = 1; //fail
			break;
		default:
			ret = 0; //success
	}
	return ret;
}

void Wave5WriteReg(int coreIdx, unsigned int ADDR, unsigned int DATA)
{
	if( Wave5CheckAccessViolationV251(ADDR) )
	{
		gWave5AccessViolationError |=  (1<<(coreIdx*4+16));
		gWave5InitFirst_exit =3;
		return;
	}
	*((volatile unsigned int *)(gWave5VirtualRegBaseAddr+ADDR)) = (unsigned int)(DATA);
}

unsigned int Wave5ReadReg(int coreIdx, unsigned int ADDR)
{
	if( Wave5CheckAccessViolationV251(ADDR) )
	{
		gWave5AccessViolationError |=  (2<<(coreIdx*4+16));
		gWave5InitFirst_exit =4;
		return 0;
	}
	return *(volatile unsigned int *)(gWave5VirtualRegBaseAddr+ADDR);
}

#define W5_RET_DEC_DISPLAY_SIZE (W5_REG_BASE + 0x01D8)

#define INVALID_CORE_INDEX_RETURN_ERROR(_coreIdx)  \
	if (_coreIdx >= MAX_NUM_VPU_CORE) \
		return -1;

Uint32 __VPU_BUSY_TIMEOUT = VPU_BUSY_CHECK_TIMEOUT;

static RetCode WAVE5_CheckInstanceValidity(CodecInst *pci)
{
	int i;
	CodecInst * pCodecInst;

	pCodecInst = &codecInstPoolWave5[0];
	for (i = 0; i < gWave5MaxInstanceNum; ++i, ++pCodecInst) {
		if (pCodecInst == pci)
		{
		#ifdef TCC_ONBOARD
			if( pCodecInst->inUse != 0 )
				return RETCODE_SUCCESS;
		#else
			return RETCODE_SUCCESS;
		#endif
		}
	}

	return RETCODE_INVALID_HANDLE;
}

static RetCode CheckDecInstanceValidity(CodecInst *pCodecInst)
{
	RetCode ret;

	if (pCodecInst == NULL)
		return RETCODE_INVALID_HANDLE;

	ret = WAVE5_CheckInstanceValidity(pCodecInst);
	if (ret != RETCODE_SUCCESS)
		return RETCODE_INVALID_HANDLE;

	if (!pCodecInst->inUse)
		return RETCODE_INVALID_HANDLE;

	return ProductVpuDecCheckCapability(pCodecInst);
}

Int32 WAVE5_IsBusy(Uint32 coreIdx)
{
	Uint32 ret = 0;

	INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

	ret = ProductVpuIsBusy(coreIdx);

	return ret != 0;
}

Int32 WAVE5_IsInit(Uint32 coreIdx)
{
	Int32 pc;

	INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

	pc = ProductVpuIsInit(coreIdx);

	return pc;
}

void WAVE5_ClearInterrupt(Uint32 coreIdx)
{
	// clear all interrupt flags
	ProductVpuClearInterrupt(coreIdx, 0xffff);
}

void WAVE5_ClearInterruptEx(VpuHandle handle, Int32 intrFlag)
{
	CodecInst *pCodecInst;

	pCodecInst  = handle;

	ProductVpuClearInterrupt(pCodecInst->coreIdx, intrFlag);
}

int WAVE5_GetMvColBufSize(Uint32 codStd, int width, int height, int num)
{
	int size_mvcolbuf = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_MVCOL, codStd, width, height);

	if (codStd == STD_AVC || codStd == STD_HEVC || codStd == STD_VP9)
		size_mvcolbuf *= num;

	return size_mvcolbuf;
}

int WAVE5_GetFrameBufSize(int coreIdx, int stride, int height, int mapType, int format, int interleave, DRAMConfig *pDramCfg)
{
	int productId;
	UNREFERENCED_PARAMETER(interleave);             /*!<< for backward compatiblity */

	if (coreIdx < 0 || coreIdx >= MAX_NUM_VPU_CORE)
		return -1;

	productId = PRODUCT_ID_512;

	return ProductCalculateFrameBufSize(productId, stride, height, (TiledMapType)mapType, (FrameBufferFormat)format, (BOOL)interleave, pDramCfg);
}


static RetCode InitializeVPU(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, const Uint16 *code, Uint32 size, Int32 cq_count)
{
	RetCode     ret;
	int i;
	CodecInst * pCodecInst = &codecInstPoolWave5[0];


	if (WAVE5_IsInit(coreIdx) != 0) {
		ProductVpuReInit(coreIdx, workBuf, codeBuf, (void *)code, size, cq_count);
		return RETCODE_CALLED_BEFORE;
	}

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0)
		return RETCODE_CODEC_EXIT;
	#endif

	//reset codec instance memory
	if (pCodecInst != 0)
	{
		if( wave5_memset )
			wave5_memset(pCodecInst, 0, sizeof(CodecInst)*gWave5MaxInstanceNum, 0);
	}

	for( i = 0; i < gWave5MaxInstanceNum; ++i, ++pCodecInst )
	{
		pCodecInst->instIndex = i;
		pCodecInst->inUse = 0;
	}

	WAVE5_ClearPendingInst(coreIdx);

	/*
	 * g_KeepPendingInstForSimInts at next time in case that interrputs
	 * for both INT_WAVE5_DEC_PIC and INT_WAVE5_BSBUF_EMPTY
	 * happen at the same time.
	*/
	WAVE5_PeekPendingInstForSimInts(0);

	if (wave5_memset)
		wave5_memset(gsWave5HandlePool, 0, sizeof(wave5_t)*gWave5MaxInstanceNum, 0);

	ret = ProductVpuReset(coreIdx, SW_RESET_ON_BOOT, cq_count);
	if (ret != RETCODE_SUCCESS)
		return ret;

	ret = ProductVpuInit(coreIdx, workBuf, codeBuf, (void *)code, size, cq_count);
	if (ret != RETCODE_SUCCESS)
		return ret;

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0)
		return RETCODE_CODEC_EXIT;
	#endif

	return RETCODE_SUCCESS;
}

RetCode WAVE5_Init(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, Int32 cq_count)
{
	if (coreIdx >= MAX_NUM_VPU_CORE)
		return RETCODE_INVALID_PARAM;

	return InitializeVPU(coreIdx, workBuf, codeBuf, 0, 0, cq_count);
}

RetCode WAVE5_GetVersionInfo(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision, Uint32 *productId)
{
	RetCode  ret = RETCODE_SUCCESS;

	if (coreIdx >= MAX_NUM_VPU_CORE)
		return RETCODE_INVALID_PARAM;

	if (ProductVpuIsInit(coreIdx) == 0) {
		#ifdef ENABLE_FORCE_ESCAPE
		if(gWave5InitFirst_exit > 0)
			return RETCODE_CODEC_EXIT;
		#endif
		return RETCODE_NOT_INITIALIZED;
	}

	if (WAVE5_GetPendingInst(coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	*productId = PRODUCT_ID_512;

	ret = ProductVpuGetVersion(coreIdx, versionInfo, revision);

	return ret;
}

RetCode WAVE5_DecOpen(DecHandle *pHandle, DecOpenParam *pop)
{
	CodecInst * pCodecInst;
	DecInfo*    pDecInfo;
	RetCode     ret;

	ret = ProductCheckDecOpenParam(pop);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (WAVE5_IsInit(pop->coreIdx) == 0) {
		#ifdef ENABLE_FORCE_ESCAPE
		if( gWave5InitFirst_exit > 0 )
			return RETCODE_CODEC_EXIT;
		#endif
		return RETCODE_NOT_INITIALIZED;
	}

	ret = WAVE5_GetCodecInstance(pop->coreIdx, &pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		*pHandle = 0;
		return RETCODE_FAILURE;
	}

	pCodecInst->isDecoder = TRUE;
	*pHandle = pCodecInst;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	pDecInfo->openParam = *pop;
	pCodecInst->coreIdx = 0;	// 20180423

//	wave5_memset(pDecInfo, 0x00, sizeof(DecInfo), 0);
//	wave5_memcpy((void*)&pDecInfo->openParam, pop, sizeof(DecOpenParam), 0);

	pCodecInst->codecMode = CODES_NULL;
	#if TCC_HEVC___DEC_SUPPORT == 1
	if (pop->bitstreamFormat == STD_HEVC)
		pCodecInst->codecMode = HEVC_DEC;
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (pop->bitstreamFormat == STD_VP9)
		pCodecInst->codecMode = W_VP9_DEC;
	#endif

	if (pCodecInst->codecMode == CODES_NULL)
		return RETCODE_INVALID_PARAM;

	pDecInfo->enableAfbce = pop->afbceEnable;
	pDecInfo->afbceFormat = pop->afbceFormat;
	pDecInfo->wtlEnable = pop->wtlEnable;
	pDecInfo->wtlMode   = pop->wtlMode;
	if (!pDecInfo->wtlEnable)
		pDecInfo->wtlMode = 0;

	pDecInfo->streamWrPtr        = pop->bitstreamBuffer;
	pDecInfo->streamRdPtr        = pop->bitstreamBuffer;
	pDecInfo->prev_streamWrPtr   = pop->bitstreamBuffer;
	pDecInfo->frameDelay         = -1;
	pDecInfo->streamBufStartAddr = pop->bitstreamBuffer;
	pDecInfo->streamBufSize      = pop->bitstreamBufferSize;
	pDecInfo->streamBufEndAddr   = pop->bitstreamBuffer + pop->bitstreamBufferSize;
	pDecInfo->reorderEnable      = VPU_REORDER_ENABLE;
	pDecInfo->mirrorDirection    = MIRDIR_NONE;
	pCodecInst->CodecInfo.decInfo.isFirstFrame = 1;
	pCodecInst->CodecInfo.decInfo.prev_sequencechanged = 0;

	pDecInfo->vbWork.phys_addr = pop->vbWork.phys_addr;
	pDecInfo->vbWork.size = pop->vbWork.size;
	pDecInfo->vbTemp.phys_addr = pop->vbTemp.phys_addr;
	pDecInfo->vbTemp.size = pop->vbTemp.size;

	#ifdef TEST_CQ2_DEC_SKIPFRAME
	pDecInfo->frameNumCount = 0;
	pDecInfo->skipframeUsed = 0;
	#endif

	#ifdef TEST_CQ2_CLEAR_INTO_DECODE
	pDecInfo->clearDisplayIndexes = 0;
	#endif

	if ((ret=ProductVpuDecBuildUpOpenParam(pCodecInst, pop)) != RETCODE_SUCCESS) {
		*pHandle = 0;
		return ret;
	}

	pDecInfo->tiled2LinearEnable = 0;//pop->tiled2LinearEnable;
	pDecInfo->tiled2LinearMode   = 0;//pop->tiled2LinearMode;
	if (!pDecInfo->tiled2LinearEnable)
		pDecInfo->tiled2LinearMode = 0;     //coda980 only

	if (!pDecInfo->wtlEnable)               //coda980, wave320, wave410 only
		pDecInfo->wtlMode = 0;

	pDecInfo->deringEnable = 0;
	pDecInfo->mirrorDirection = 0;
	pDecInfo->mirrorEnable = 0;
	pDecInfo->rotationAngle = 0;
	pDecInfo->rotationEnable = 0;
	pDecInfo->openParam.fbc_mode = 0x0c;
	pDecInfo->thumbnailMode = 0;

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecClose(DecHandle handle)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	//pDecInfo   = &pCodecInst->CodecInfo.decInfo;

	if ((ret=ProductVpuDecFiniSeq(pCodecInst)) != RETCODE_SUCCESS) {
		#ifdef ENABLE_VDI_LOG
		if (pCodecInst->loggingEnable)
			vdi_log(pCodecInst->coreIdx, DEC_SEQ_END, 0);
		#endif
		if (ret == RETCODE_VPU_STILL_RUNNING)
			return ret;
	}

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
		WAVE5_ClearPendingInst(pCodecInst->coreIdx);

	WAVE5_FreeCodecInstance(pCodecInst);

	return ret;
}

RetCode WAVE5_DecSetEscSeqInit(DecHandle handle, int escape)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_INTERRUPT)
		return RETCODE_INVALID_PARAM;

	pDecInfo->seqInitEscape = escape;

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecIssueSeqInit(DecHandle handle)
{
	CodecInst  *pCodecInst;
	RetCode     ret;
	VpuAttr    *pAttr;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;

	pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	ret = ProductVpuDecInitSeq(handle);
	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0)	{
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
	#endif
	if (ret == RETCODE_SUCCESS)
		WAVE5_SetPendingInst(pCodecInst->coreIdx, pCodecInst);

	if (pAttr->supportCommandQueue == TRUE)
		WAVE5_SetPendingInst(pCodecInst->coreIdx, NULL);

	return ret;
}

RetCode WAVE5_DecCompleteSeqInit(DecHandle handle, DecInitialInfo *info)
{
	CodecInst  *pCodecInst;
	DecInfo    *pDecInfo;
	RetCode     ret;
	VpuAttr    *pAttr;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (info == 0)
		return RETCODE_INVALID_PARAM;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	if (pAttr->supportCommandQueue == TRUE) {
	}
	else {
		if (pCodecInst != WAVE5_GetPendingInst(pCodecInst->coreIdx)) {
			WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);
			return RETCODE_WRONG_CALL_SEQUENCE;
		}
	}

	ret = ProductVpuDecGetSeqInfo(handle, info);
	if (ret == RETCODE_SUCCESS)
		pDecInfo->initialInfoObtained = 1;

	info->rdPtr = ProductVpuDecGetRdPtr(pCodecInst);
	info->wrPtr = pDecInfo->streamWrPtr;
	pDecInfo->initialInfo = *info;

	WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);

	return ret;
}

RetCode DecRegisterFrameBuffer(DecHandle handle, FrameBuffer *bufArray, PhysicalAddress addr, int numFbsForDecoding, int numFbsForWTL, int stride, int height, int mapType)
{
	CodecInst*      pCodecInst;
	DecInfo*        pDecInfo;
	Int32           i;
	Uint32          size, lastAddr, totalAllocSize;
	RetCode         ret = RETCODE_SUCCESS;
	FrameBuffer*    fb, nullFb;
	vpu_buffer_t*   vb;
	FrameBufferFormat format = FORMAT_420;
	Int32           totalNumOfFbs;
	Int32			linearStride;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (numFbsForDecoding > MAX_GDI_IDX || numFbsForWTL > MAX_GDI_IDX)
		return RETCODE_INVALID_PARAM;

	if( wave5_memset )
		wave5_memset(&nullFb, 0x00, sizeof(FrameBuffer), 0);

	pCodecInst                  = handle;
	pDecInfo                    = &pCodecInst->CodecInfo.decInfo;
	pDecInfo->numFbsForDecoding = numFbsForDecoding;
	pDecInfo->numFbsForWTL      = numFbsForWTL;
	pDecInfo->numFrameBuffers   = numFbsForDecoding + numFbsForWTL;
	pDecInfo->stride            = stride;

	pDecInfo->frameBufferHeight = height;

	#if TCC_VP9____DEC_SUPPORT == 1
	if (pCodecInst->codecMode == W_VP9_DEC)
		pDecInfo->frameBufferHeight = WAVE5_ALIGN64(height);
	#endif

	pDecInfo->mapType           = mapType;
	pCodecInst->productId	    = PRODUCT_ID_512;
	pDecInfo->mapCfg.productId  = pCodecInst->productId;

	ret = ProductVpuDecCheckCapability(pCodecInst);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (!pDecInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if ((stride < pDecInfo->initialInfo.picWidth) || (stride % 8 != 0) || (height<pDecInfo->initialInfo.picHeight))
		return RETCODE_INVALID_STRIDE;

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	/* clear frameBufPool */
	for (i=0; i<(int)(sizeof(pDecInfo->frameBufPool)/sizeof(FrameBuffer)); i++)
		pDecInfo->frameBufPool[i] = nullFb;

	/* LinearMap or TiledMap, compressed framebuffer inclusive. */
	if (pDecInfo->openParam.FullSizeFB) {
		format = (pDecInfo->openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			stride = CalcStride(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->openParam.maxheight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			stride = CalcStride(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, FALSE);
			height = WAVE5_ALIGN32(pDecInfo->openParam.maxheight);
		}
		#endif
	}
	else
	{
		format = (pDecInfo->initialInfo.lumaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			stride = CalcStride(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			stride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, FALSE);
			height = WAVE5_ALIGN32(pDecInfo->initialInfo.picHeight);
		}
		#endif
	}

	totalNumOfFbs = numFbsForDecoding + numFbsForWTL;
	if (bufArray) {
		for(i=0; i<totalNumOfFbs; i++)
			pDecInfo->frameBufPool[i] = bufArray[i];
	}
	else {
		vb = &pDecInfo->vbFrame;
		fb = &pDecInfo->frameBufPool[0];
		ret = ProductVpuAllocateFramebuffer(
				(CodecInst*)handle, fb, addr, (TiledMapType)mapType, numFbsForDecoding, stride, height, format,
				pDecInfo->openParam.cbcrInterleave,
				pDecInfo->openParam.nv21,
				pDecInfo->openParam.frameEndian, vb, 0, FB_TYPE_CODEC);
		if (ret != RETCODE_SUCCESS)
			return ret;
	}

	totalAllocSize = 0;
	pDecInfo->mapCfg.tiledBaseAddr = pDecInfo->frameBufPool[0].bufY;

	if (numFbsForDecoding == 1) {
		size = ProductCalculateFrameBufSize(handle->productId, stride, height, (TiledMapType)mapType, format,
											pDecInfo->openParam.cbcrInterleave, &pDecInfo->dramCfg);
		#ifdef TEST_COMPRESSED_FBCTBL_INDEX_SET
		size += ( pDecInfo->fbcYTblSize + pDecInfo->fbcCTblSize + pDecInfo->mvColSize );
		#endif
	}
	else {
		size = pDecInfo->frameBufPool[1].bufY - pDecInfo->frameBufPool[0].bufY;
	}

	size *= numFbsForDecoding;
	lastAddr = pDecInfo->frameBufPool[0].bufY + size;
	totalAllocSize += size;

	mapType = LINEAR_FRAME_MAP;
	if (pDecInfo->openParam.FullSizeFB) {
		format = (pDecInfo->openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		if (pDecInfo->openParam.TenBitDisable)
			format = FORMAT_420;

		if (pDecInfo->enableAfbce) {
			mapType = ARM_COMPRESSED_FRAME_MAP;
			format = pDecInfo->afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
		}

		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			linearStride = CalcStride(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, mapType, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->openParam.maxheight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			linearStride = CalcStride(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, mapType, FALSE);
			height = pDecInfo->openParam.maxheight;
		}
		#endif
	}
	else {
		format = (pDecInfo->initialInfo.lumaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		if (pDecInfo->openParam.TenBitDisable)
			format = FORMAT_420;

		if (pDecInfo->enableAfbce) {
			mapType = ARM_COMPRESSED_FRAME_MAP;
			format = pDecInfo->afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
		}

		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			linearStride = CalcStride(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, mapType, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			linearStride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, mapType, FALSE);
			height = pDecInfo->initialInfo.picHeight;
		}
		#endif
	}

	/* LinearMap */
	if (pDecInfo->wtlEnable == TRUE || pDecInfo->enableAfbce == TRUE || numFbsForWTL != 0) {
		FrameBufferFormat format;
		pDecInfo->stride = linearStride;
		TiledMapType map;

		if (bufArray) {
			format = pDecInfo->frameBufPool[0].format;
		}
		else {
			//TiledMapType map;
			map = pDecInfo->enableAfbce == TRUE ? ARM_COMPRESSED_FRAME_MAP : ((pDecInfo->wtlMode==FF_FRAME ? LINEAR_FRAME_MAP : LINEAR_FIELD_MAP));
			format = pDecInfo->wtlFormat;
			vb = &pDecInfo->vbWTL;
			fb = &pDecInfo->frameBufPool[numFbsForDecoding];
			ret = ProductVpuAllocateFramebuffer(
					(CodecInst*)handle, fb, lastAddr, map,
					numFbsForWTL, linearStride, height,
					pDecInfo->wtlFormat,
					pDecInfo->openParam.cbcrInterleave,
					pDecInfo->openParam.nv21,
					pDecInfo->openParam.frameEndian, vb, 0,
					FB_TYPE_CODEC);

			if (ret != RETCODE_SUCCESS)
				return ret;
		}
		if (numFbsForWTL == 1) {
			size = ProductCalculateFrameBufSize(
					handle->productId,
					linearStride, height,
					(TiledMapType)mapType,
					pDecInfo->wtlFormat,
					pDecInfo->openParam.cbcrInterleave,
					&pDecInfo->dramCfg);
		}
		else {
			size =  pDecInfo->frameBufPool[numFbsForDecoding+1].bufY
				 - pDecInfo->frameBufPool[numFbsForDecoding].bufY;
		}
		size *= numFbsForWTL;
		//lastAddr = pDecInfo->frameBufPool[numFbsForDecoding].bufY + size;
		totalAllocSize += size;
	}

	ret = ProductVpuRegisterFramebuffer(pCodecInst);

	return ret;
}

RetCode WAVE5_DecRegisterFrameBuffer(DecHandle handle, FrameBuffer *bufArray, PhysicalAddress addr, int compnum, int linearnum, int stride, int height, int mapType)
{
#if DBG_PROCESS_LEVEL >= 501
	RetCode         ret = RETCODE_SUCCESS;
	ret = DecRegisterFrameBuffer(handle, bufArray, addr, compnum, linearnum, stride, height, mapType);
	return ret;
#else
	return DecRegisterFrameBuffer(handle, bufArray, addr, compnum, linearnum, stride, height, mapType);
#endif
}

#ifdef ADD_FBREG_2
RetCode WAVE5_DecRegisterFrameBuffer2(DecHandle handle, FrameBuffer *bufArray, PhysicalAddress addr[32], int numFbsForDecoding, int numFbsForWTL, int stride, int height, int mapType)
{
	CodecInst*      pCodecInst;
	DecInfo*        pDecInfo;
	Int32           i, cbcrInterleave;
	Uint32          size, lastAddr, totalAllocSize;
	RetCode         ret;
	FrameBuffer*    fb, nullFb;
	vpu_buffer_t*   vb;
	FrameBufferFormat format = FORMAT_420;
	Uint32          totalNumOfFbs;
	Uint32			linearStride;
	Uint32        sizeLuma_comp = 0;
	Uint32        sizeLuma_linear = 0;
	Uint32        sizeChroma_comp = 0;;
	Uint32        sizeChroma_linear = 0;
	Uint32        sizeFb_comp = 0;
	Uint32        sizeFb_linear = 0;
	Uint32       mvColSize, fbcYTblSize, fbcCTblSize;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (numFbsForDecoding > MAX_FRAMEBUFFER_COUNT || numFbsForWTL > MAX_FRAMEBUFFER_COUNT) {
		return RETCODE_INVALID_PARAM;
	}

	wave4_memset(&nullFb, 0x00, sizeof(FrameBuffer));

	pCodecInst                  = handle;
	pDecInfo                    = &pCodecInst->CodecInfo.decInfo;
	pDecInfo->numFbsForDecoding = numFbsForDecoding;
	pDecInfo->numFbsForWTL      = numFbsForWTL;
	pDecInfo->numFrameBuffers   = numFbsForDecoding + numFbsForWTL;
	pDecInfo->stride            = stride;

	pDecInfo->frameBufferHeight = height;
	pDecInfo->mapType           = mapType;
	pCodecInst->productId		= PRODUCT_ID_4102;
	pDecInfo->mapCfg.productId  = pCodecInst->productId;

	cbcrInterleave = pDecInfo->openParam.cbcrInterleave;

	ret = ProductVpuDecCheckCapability(pCodecInst);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (!pDecInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if ((stride < pDecInfo->initialInfo.picWidth) || (stride % 8 != 0) || (height<pDecInfo->initialInfo.picHeight))
		return RETCODE_INVALID_STRIDE;

	if (WAVE4_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	/* clear frameBufPool */
	for (i=0; i<(int)(sizeof(pDecInfo->frameBufPool)/sizeof(FrameBuffer)); i++) {
		pDecInfo->frameBufPool[i] = nullFb;
	}

	if (pCodecInst->CodecInfo.decInfo.openParam.FullSizeFB) {
		format = (pCodecInst->CodecInfo.decInfo.openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;

		stride = CalcStride(pCodecInst->CodecInfo.decInfo.openParam.maxwidth, pCodecInst->CodecInfo.decInfo.openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, handle->codecMode);
		linearStride = CalcStride(pCodecInst->CodecInfo.decInfo.openParam.maxwidth, pCodecInst->CodecInfo.decInfo.openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, LINEAR_FRAME_MAP, handle->codecMode);
		fbcYTblSize = (((pDecInfo->openParam.maxheight+15)/16)*((pDecInfo->openParam.maxwidth+255)/256)*128);
		fbcCTblSize = (((pDecInfo->openParam.maxheight+15)/16)*((pDecInfo->openParam.maxwidth/2+255)/256)*128);
		mvColSize = (((pDecInfo->openParam.maxwidth+63)/64)*((pDecInfo->openParam.maxheight+63)/64)*256+ 64);
	}
	else {
		if (pDecInfo->initialInfo.lumaBitdepth > 8 || pDecInfo->initialInfo.chromaBitdepth > 8)
			format = FORMAT_420_P10_16BIT_LSB;

		stride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, handle->codecMode);
		linearStride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, pDecInfo->wtlFormat, pDecInfo->openParam.cbcrInterleave, LINEAR_FRAME_MAP, handle->codecMode);
		fbcYTblSize = (((pDecInfo->initialInfo.picHeight+15)/16)*((pDecInfo->initialInfo.picWidth+255)/256)*128);
		fbcCTblSize = (((pDecInfo->initialInfo.picHeight+15)/16)*((pDecInfo->initialInfo.picWidth/2+255)/256)*128);
		mvColSize = (((pDecInfo->initialInfo.picWidth+63)/64)*((pDecInfo->initialInfo.picHeight+63)/64)*256+ 64);
	}

	fbcYTblSize = WAVE4_ALIGN16(fbcYTblSize);
	fbcCTblSize = WAVE4_ALIGN16(fbcCTblSize);
	mvColSize = WAVE4_ALIGN16(mvColSize);

	totalNumOfFbs = numFbsForDecoding + numFbsForWTL;

	sizeLuma_comp   = CalcLumaSize(pCodecInst->productId, stride, height, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
	sizeChroma_comp = CalcChromaSize(pCodecInst->productId, stride, height, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
	sizeFb_comp     = sizeLuma_comp + sizeChroma_comp*2;

	if (pDecInfo->wtlEnable == TRUE) {
		sizeLuma_linear   = CalcLumaSize(pCodecInst->productId, linearStride, height, pDecInfo->wtlFormat, pDecInfo->openParam.cbcrInterleave, LINEAR_FRAME_MAP, NULL);
		sizeChroma_linear = CalcChromaSize(pCodecInst->productId, linearStride, height, pDecInfo->wtlFormat, pDecInfo->openParam.cbcrInterleave, LINEAR_FRAME_MAP, NULL);
		sizeFb_linear     = sizeLuma_linear + sizeChroma_linear*2;

		for (i = 0 ; i < numFbsForDecoding ; i++) {
			pDecInfo->frameBufPool[i].myIndex = i;
			pDecInfo->frameBufPool[i].mapType = LINEAR_FRAME_MAP;
			pDecInfo->frameBufPool[i].height  = height;

			// Addressing Y
			pDecInfo->frameBufPool[i].bufY = addr[i];

			// Addressing Cb/Cr
			pDecInfo->frameBufPool[i].bufCb = pDecInfo->frameBufPool[i].bufY + (sizeLuma_linear);

			if (!cbcrInterleave)
				pDecInfo->frameBufPool[i].bufCr = pDecInfo->frameBufPool[i].bufCb + (sizeChroma_linear);
			else
				pDecInfo->frameBufPool[i].bufCr = 0;

			pDecInfo->frameBufPool[i].stride         = linearStride;
			pDecInfo->frameBufPool[i].sourceLBurstEn = 0;
			pDecInfo->frameBufPool[i].cbcrInterleave = cbcrInterleave;
			pDecInfo->frameBufPool[i].nv21           = pDecInfo->openParam.nv21;
			pDecInfo->frameBufPool[i].endian         = pDecInfo->openParam.frameEndian;
			pDecInfo->frameBufPool[i].lumaBitDepth   = pDecInfo->initialInfo.lumaBitdepth;
			pDecInfo->frameBufPool[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
			pDecInfo->frameBufPool[i].format         = format;
		}

		for (i = numFbsForDecoding ; i < totalNumOfFbs ; i++) {
			pDecInfo->frameBufPool[i].myIndex = i;
			pDecInfo->frameBufPool[i].mapType = COMPRESSED_FRAME_MAP;
			pDecInfo->frameBufPool[i].height  = height;

			// Addressing Y
			//pDecInfo->frameBufPool[i].bufY = addr[i - numFbsForDecoding] + ((sizeFb_linear + 4095) / 4096) * 4096;
			pDecInfo->frameBufPool[i].bufY = addr[i - numFbsForDecoding] + sizeFb_linear;

			// Addressing Cb/Cr
			pDecInfo->frameBufPool[i].bufCb = pDecInfo->frameBufPool[i].bufY + (sizeLuma_comp);
			pDecInfo->frameBufPool[i].bufCr = pDecInfo->frameBufPool[i].bufCb + (sizeChroma_comp);

			pDecInfo->frameBufPool[i].stride         = stride;
			pDecInfo->frameBufPool[i].sourceLBurstEn = 0;
			pDecInfo->frameBufPool[i].cbcrInterleave = TRUE;
			pDecInfo->frameBufPool[i].nv21           = pDecInfo->openParam.nv21;
			pDecInfo->frameBufPool[i].endian         = pDecInfo->openParam.frameEndian;
			pDecInfo->frameBufPool[i].lumaBitDepth   = pDecInfo->initialInfo.lumaBitdepth;
			pDecInfo->frameBufPool[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
			pDecInfo->frameBufPool[i].format         = format;
			//pDecInfo->frameBufPool[i].bufFBCY	 = (((pDecInfo->frameBufPool[i].bufCr + sizeChroma_comp) + 4095) / 4096) * 4096;
			pDecInfo->frameBufPool[i].bufFBCY	 = pDecInfo->frameBufPool[i].bufCr + sizeChroma_comp;
			pDecInfo->frameBufPool[i].bufFBCC	 = pDecInfo->frameBufPool[i].bufFBCY + fbcYTblSize;
			pDecInfo->frameBufPool[i].bufMVCOL	 = pDecInfo->frameBufPool[i].bufFBCC + fbcCTblSize;
		}
	}
	else {
		for (i = 0 ; i < numFbsForDecoding ; i++) {
			pDecInfo->frameBufPool[i].myIndex = i;
			pDecInfo->frameBufPool[i].mapType = COMPRESSED_FRAME_MAP;
			pDecInfo->frameBufPool[i].height  = height;

			// Addressing Y
			pDecInfo->frameBufPool[i].bufY = addr[i];

			// Addressing Cb/Cr
			pDecInfo->frameBufPool[i].bufCb = pDecInfo->frameBufPool[i].bufY + (sizeLuma_comp);
			pDecInfo->frameBufPool[i].bufCr = pDecInfo->frameBufPool[i].bufCb + (sizeChroma_comp);

			pDecInfo->frameBufPool[i].stride         = stride;
			pDecInfo->frameBufPool[i].sourceLBurstEn = 0;
			pDecInfo->frameBufPool[i].cbcrInterleave = TRUE;
			pDecInfo->frameBufPool[i].nv21           = pDecInfo->openParam.nv21;
			pDecInfo->frameBufPool[i].endian         = pDecInfo->openParam.frameEndian;
			pDecInfo->frameBufPool[i].lumaBitDepth   = pDecInfo->initialInfo.lumaBitdepth;
			pDecInfo->frameBufPool[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
			pDecInfo->frameBufPool[i].format         = format;
			//pDecInfo->frameBufPool[i].bufFBCY	 = (((pDecInfo->frameBufPool[i].bufCr + sizeChroma_comp) + 4095) / 4096) * 4096;
			pDecInfo->frameBufPool[i].bufFBCY	 = pDecInfo->frameBufPool[i].bufCr + sizeChroma_comp;
			pDecInfo->frameBufPool[i].bufFBCC	 = pDecInfo->frameBufPool[i].bufFBCY + fbcYTblSize;
			pDecInfo->frameBufPool[i].bufMVCOL	 = pDecInfo->frameBufPool[i].bufFBCC + fbcCTblSize;
		}
	}

	ret = ProductVpuRegisterFramebuffer2(pCodecInst);

	{
		unsigned int val;
		val = SEQ_CHANGE_ENABLE_ALL_HEVC;
		WAVE5_DecGiveCommand(handle, DEC_SET_SEQ_CHANGE_MASK, (void*)&val);
	}

	return ret;
}
#endif

#ifdef ADD_FBREG_3
RetCode WAVE5_DecRegisterFrameBuffer3(DecHandle handle, codec_addr_t framebuffer_addrs[64][9], int numFbsForWTL, int numFbsForDecoding, int stride, int height, int mapType)
{
	CodecInst*      pCodecInst;
	DecInfo*        pDecInfo;
	Int32           i, cbcrInterleave;
	RetCode         ret;
	FrameBuffer*    fb, nullFb;
	FrameBufferFormat format = FORMAT_420;
	Uint32			linearStride;
	Uint32			sizeLuma;
	Uint32			sizeChroma;
	unsigned int bufStartAddrAlign;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	wave5_memset(&nullFb, 0x00, sizeof(FrameBuffer), 0);

	pCodecInst                  = handle;
	pCodecInst->productId		= PRODUCT_ID_512;
	pDecInfo                    = &pCodecInst->CodecInfo.decInfo;
	pDecInfo->numFbsForDecoding = numFbsForDecoding;
	pDecInfo->numFbsForWTL      = numFbsForWTL;
	pDecInfo->numFrameBuffers   = numFbsForDecoding + numFbsForWTL;
	pDecInfo->stride            = stride;

	pDecInfo->frameBufferHeight = height;
	#if TCC_VP9____DEC_SUPPORT == 1
	if (pCodecInst->codecMode == W_VP9_DEC)
		pDecInfo->frameBufferHeight = WAVE5_ALIGN64(height);
	#endif

	pDecInfo->mapType           = mapType;
	pDecInfo->mapCfg.productId  = pCodecInst->productId;

	cbcrInterleave = pDecInfo->openParam.cbcrInterleave;

	ret = ProductVpuDecCheckCapability(pCodecInst);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (!pDecInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if ((stride < pDecInfo->initialInfo.picWidth) || (stride % 8 != 0) || (height<pDecInfo->initialInfo.picHeight))
		return RETCODE_INVALID_STRIDE;

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	/* clear frameBufPool */
	for (i=0; i<(int)(sizeof(pDecInfo->frameBufPool)/sizeof(FrameBuffer)); i++)
		pDecInfo->frameBufPool[i] = nullFb;

	//compressed
	if (pDecInfo->openParam.FullSizeFB) {
		format = (pDecInfo->openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			stride = CalcStride(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->openParam.maxheight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			stride = CalcStride(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, FALSE);
			height = WAVE5_ALIGN32(pDecInfo->openParam.maxheight);
		}
		#endif
	}
	else {
		format = (pDecInfo->initialInfo.lumaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			stride = CalcStride(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			stride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, FALSE);
			height = WAVE5_ALIGN32(pDecInfo->initialInfo.picHeight);
		}
		#endif
	}

	#if TCC_VP9____DEC_SUPPORT == 1
	if (pCodecInst->codecMode == W_VP9_DEC) {
		Uint32 framebufHeight = WAVE5_ALIGN64(height);
		sizeLuma   = CalcLumaSize(pCodecInst->productId, stride, framebufHeight, format, cbcrInterleave, mapType, NULL);
		sizeChroma = CalcChromaSize(pCodecInst->productId, stride, framebufHeight, format, cbcrInterleave, mapType, NULL);
	}
	#endif

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (pCodecInst->codecMode == W_HEVC_DEC) {
		DRAMConfig* dramConfig = NULL;
		sizeLuma   = CalcLumaSize(pCodecInst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
		sizeChroma = CalcChromaSize(pCodecInst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
	}
	#endif

	// compressed Framebuffer common informations
	for (i=0; i<numFbsForDecoding; i++) {
		pDecInfo->frameBufPool[i].updateFbInfo = FALSE;
		pDecInfo->frameBufPool[i].myIndex        = i;
		pDecInfo->frameBufPool[i].stride         = stride;
		pDecInfo->frameBufPool[i].height         = height;
		pDecInfo->frameBufPool[i].mapType        = mapType;
		pDecInfo->frameBufPool[i].format         = format;
		pDecInfo->frameBufPool[i].cbcrInterleave = (mapType == COMPRESSED_FRAME_MAP ? TRUE : cbcrInterleave);
		pDecInfo->frameBufPool[i].nv21           = pDecInfo->openParam.nv21;
		pDecInfo->frameBufPool[i].endian         = pDecInfo->openParam.frameEndian;
		pDecInfo->frameBufPool[i].lumaBitDepth   = pDecInfo->initialInfo.lumaBitdepth;
		pDecInfo->frameBufPool[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
	}

	bufStartAddrAlign = pCodecInst->bufStartAddrAlign;

	if ((framebuffer_addrs != NULL) && (pDecInfo->numFrameBuffers < MAX_REG_FRAME))
	{
		BOOL yuv422Interleave = FALSE;
		BOOL fieldFrame       = (BOOL)(mapType == LINEAR_FIELD_MAP);

		if (mapType != COMPRESSED_FRAME_MAP && mapType != ARM_COMPRESSED_FRAME_MAP) {
			switch (format)
			{
			case FORMAT_YUYV:
			case FORMAT_YUYV_P10_16BIT_MSB:
			case FORMAT_YUYV_P10_16BIT_LSB:
			case FORMAT_YUYV_P10_32BIT_MSB:
			case FORMAT_YUYV_P10_32BIT_LSB:
			case FORMAT_YVYU:
			case FORMAT_YVYU_P10_16BIT_MSB:
			case FORMAT_YVYU_P10_16BIT_LSB:
			case FORMAT_YVYU_P10_32BIT_MSB:
			case FORMAT_YVYU_P10_32BIT_LSB:
			case FORMAT_UYVY:
			case FORMAT_UYVY_P10_16BIT_MSB:
			case FORMAT_UYVY_P10_16BIT_LSB:
			case FORMAT_UYVY_P10_32BIT_MSB:
			case FORMAT_UYVY_P10_32BIT_LSB:
			case FORMAT_VYUY:
			case FORMAT_VYUY_P10_16BIT_MSB:
			case FORMAT_VYUY_P10_16BIT_LSB:
			case FORMAT_VYUY_P10_32BIT_MSB:
			case FORMAT_VYUY_P10_32BIT_LSB:
				yuv422Interleave = TRUE;
				break;
			default:
				//yuv422Interleave = FALSE;
				break;
			}
		}

		for (i=0; i<numFbsForDecoding; i++) {
			//[0]compressed luma, [1]linear/compressed chroma cb, [2]linear/compressed cr,
			pDecInfo->frameBufPool[i].bufY = WAVE5_ALIGN(framebuffer_addrs[i][0], bufStartAddrAlign);
			if (yuv422Interleave == TRUE) {
				pDecInfo->frameBufPool[i].bufCb = (PhysicalAddress)-1;
				pDecInfo->frameBufPool[i].bufCr = (PhysicalAddress)-1;
			}
			else {
				pDecInfo->frameBufPool[i].bufCb = WAVE5_ALIGN(framebuffer_addrs[i][1], bufStartAddrAlign);
			}

		#ifdef TEST_COMPRESSED_FBCTBL_INDEX_SET
			if (mapType == COMPRESSED_FRAME_MAP) 	{
				//[3]fbcYTable, [4]fcCTable, [5]mvcol
				pDecInfo->frameBufPool[i].bufFBCY = WAVE5_ALIGN(framebuffer_addrs[i][3], bufStartAddrAlign);
				pDecInfo->frameBufPool[i].bufFBCC = WAVE5_ALIGN(framebuffer_addrs[i][4], bufStartAddrAlign);
				pDecInfo->frameBufPool[i].bufMVCOL = WAVE5_ALIGN(framebuffer_addrs[i][5], bufStartAddrAlign);
			}
		#endif

			DLOGD("[%d] comp y:0x%x, cb:0x%x, fbcy:0x%x, fbcc:0x%x, mvcol:0x%x, stride:%u", i, pDecInfo->frameBufPool[i].bufY, pDecInfo->frameBufPool[i].bufCb, pDecInfo->frameBufPool[i].bufFBCY, pDecInfo->frameBufPool[i].bufFBCC, pDecInfo->frameBufPool[i].bufMVCOL, stride);
		}
	}

	if (ret != RETCODE_SUCCESS)
		return ret;

	//linear
	mapType = LINEAR_FRAME_MAP;
	if (pDecInfo->openParam.FullSizeFB) {
		format = (pDecInfo->openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		if (pDecInfo->openParam.TenBitDisable)
			format = FORMAT_420;

		if (pDecInfo->enableAfbce) {
			mapType = ARM_COMPRESSED_FRAME_MAP;
			format = pDecInfo->afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
		}

		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			linearStride = CalcStride(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, mapType, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->openParam.maxheight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			linearStride = CalcStride(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight, format, pDecInfo->openParam.cbcrInterleave, mapType, FALSE);
			height = pDecInfo->openParam.maxheight;
		}
		#endif
	}
	else {
		format = (pDecInfo->initialInfo.lumaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		if (pDecInfo->openParam.TenBitDisable)
			format = FORMAT_420;

		if (pDecInfo->enableAfbce) {
			mapType = ARM_COMPRESSED_FRAME_MAP;
			format = pDecInfo->afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
		}

		#if TCC_VP9____DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_VP9_DEC) {
			linearStride = CalcStride(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, mapType, TRUE);
			height = WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pCodecInst->codecMode == W_HEVC_DEC) {
			linearStride = CalcStride(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight, format, pDecInfo->openParam.cbcrInterleave, mapType, FALSE);
			height = pDecInfo->initialInfo.picHeight;
		}
		#endif
	}

	if (pDecInfo->wtlEnable == TRUE) {
		for (i = numFbsForDecoding ; i < numFbsForDecoding + numFbsForWTL; i++) {
			pDecInfo->frameBufPool[i].updateFbInfo = FALSE;
			pDecInfo->frameBufPool[i].myIndex = i;
			pDecInfo->frameBufPool[i].mapType = LINEAR_FRAME_MAP;
			pDecInfo->frameBufPool[i].height  = pDecInfo->frameBufferHeight;
			pDecInfo->frameBufPool[i].bufY = WAVE5_ALIGN(framebuffer_addrs[i][0], bufStartAddrAlign);
			pDecInfo->frameBufPool[i].bufCb = WAVE5_ALIGN(framebuffer_addrs[i][1], bufStartAddrAlign);
			if (!cbcrInterleave)
				pDecInfo->frameBufPool[i].bufCr = WAVE5_ALIGN(framebuffer_addrs[i][2], bufStartAddrAlign);
			else
				pDecInfo->frameBufPool[i].bufCr = -1;
			pDecInfo->frameBufPool[i].stride  = linearStride;
			pDecInfo->frameBufPool[i].sourceLBurstEn = 0;
			pDecInfo->frameBufPool[i].cbcrInterleave = cbcrInterleave;
			pDecInfo->frameBufPool[i].nv21 = pDecInfo->openParam.nv21;
			pDecInfo->frameBufPool[i].endian = pDecInfo->openParam.frameEndian;
			pDecInfo->frameBufPool[i].lumaBitDepth = pDecInfo->initialInfo.lumaBitdepth;
			pDecInfo->frameBufPool[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
			pDecInfo->frameBufPool[i].format = format;

			DLOGD("[%d] linear y:0x%x, cb:0x%x, cr:0x%x, stride:%u", i, pDecInfo->frameBufPool[i].bufY, pDecInfo->frameBufPool[i].bufCb, pDecInfo->frameBufPool[i].bufCr, linearStride);
		}
	}

	ret = ProductVpuRegisterFramebuffer(pCodecInst);
	if (ret != RETCODE_SUCCESS)
		return ret;

	return ret;
}
#endif

RetCode WAVE5_DecGetFrameBuffer(DecHandle handle, int frameIdx, FrameBuffer *frameBuf)
{
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	RetCode ret;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (frameBuf==0)
		return RETCODE_INVALID_PARAM;

	pCodecInst = handle;
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;

	if (frameIdx < 0 || frameIdx >= pDecInfo->numFrameBuffers)
		return RETCODE_INVALID_PARAM;

	*frameBuf = pDecInfo->frameBufPool[frameIdx];

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecGetBitstreamBuffer(DecHandle handle, PhysicalAddress *prdPtr, PhysicalAddress *pwrPtr, Uint32 *size)
{
	CodecInst*      pCodecInst;
	DecInfo*        pDecInfo;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	PhysicalAddress tempPtr;
	int             room;
	Int32           coreIdx;
	VpuAttr*        pAttr;

	coreIdx = handle->coreIdx;
	pAttr = &g_VpuCoreAttributes[coreIdx];
	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (pAttr->supportCommandQueue == TRUE) {
		rdPtr = ProductVpuDecGetRdPtr(pCodecInst);
	}
	else {
		if (WAVE5_GetPendingInst(coreIdx) == pCodecInst)
			rdPtr = Wave5ReadReg(coreIdx, pDecInfo->streamRdPtrRegAddr);
		else
			rdPtr = pDecInfo->streamRdPtr;
	}

	#ifdef ENABLE_FORCE_ESCAPE_2
	if (gWave5InitFirst_exit > 0)
		return RETCODE_CODEC_EXIT;
	#endif

	wrPtr = pDecInfo->streamWrPtr;
	pAttr = &g_VpuCoreAttributes[coreIdx];
	tempPtr = rdPtr;

	if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END) {
		if (wrPtr < tempPtr) {
			room = tempPtr - wrPtr - pAttr->bitstreamBufferMargin*2;
		}
		else {
			room = (pDecInfo->streamBufEndAddr - wrPtr) + (tempPtr - pDecInfo->streamBufStartAddr) - pAttr->bitstreamBufferMargin*2;
		}
		room--;
	}
	else {
		room = (pDecInfo->streamBufEndAddr - wrPtr);
	}

	if (prdPtr) *prdPtr = rdPtr;
	if (pwrPtr) *pwrPtr = wrPtr;
	if (size)   *size   = room;

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecUpdateBitstreamBuffer(DecHandle handle, int size)
{
	CodecInst*      pCodecInst;
	DecInfo*        pDecInfo;
	PhysicalAddress wrPtr;
	PhysicalAddress rdPtr;
	RetCode         ret;
	BOOL            running;
	VpuAttr*    pAttr;
	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pAttr   = &g_VpuCoreAttributes[pCodecInst->coreIdx];
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;
	wrPtr      = pDecInfo->streamWrPtr;

	if (pAttr->supportCommandQueue == TRUE)
		running = FALSE;
	else
		running = (BOOL)(WAVE5_GetPendingInst(pCodecInst->coreIdx) == pCodecInst);

	if (size > 0) {
		Uint32  room = 0;

		if (running == TRUE)
			rdPtr = Wave5ReadReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr);
		else
			rdPtr = pDecInfo->streamRdPtr;

		if (wrPtr < rdPtr) {
			if (rdPtr <= wrPtr + size)
				return RETCODE_INVALID_PARAM;
		}

		wrPtr += size;

		if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END) {
			if (wrPtr > pDecInfo->streamBufEndAddr) {
				room = wrPtr - pDecInfo->streamBufEndAddr;
				wrPtr = pDecInfo->streamBufStartAddr;
				wrPtr += room;
			}
			else if (wrPtr == pDecInfo->streamBufEndAddr) {
				wrPtr = pDecInfo->streamBufStartAddr;
			}
		}

		pDecInfo->streamWrPtr = wrPtr;
		pDecInfo->streamRdPtr = rdPtr;

		if (running == TRUE)
			Wave5WriteReg(pCodecInst->coreIdx, pDecInfo->streamWrPtrRegAddr, wrPtr);
	}

	ret = ProductVpuDecSetBitstreamFlag(pCodecInst, running, size);
	return ret;
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
RetCode WAVE5_SWReset(Uint32 coreIdx, SWResetMode resetMode, void *pendingInst, Int32 cq_count)
{
	CodecInst *pCodecInst = (CodecInst *)pendingInst;
	RetCode    ret = RETCODE_SUCCESS;

	if (pCodecInst) {
		WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);
		#ifdef ENABLE_VDI_LOG
		if (pCodecInst->loggingEnable)
			vdi_log(pCodecInst->coreIdx, 0x10000, 1);
		#endif
	}

	ret = ProductVpuReset(coreIdx, resetMode, cq_count);

	#ifdef ENABLE_VDI_LOG
	if (pCodecInst) {
		if (pCodecInst->loggingEnable)
			vdi_log(pCodecInst->coreIdx, 0x10000, 0);
	}
	#endif

	return ret;
}

RetCode WAVE5_DecStartOneFrame(DecHandle handle, DecParam *param)
{
	CodecInst*  pCodecInst;
	DecInfo*    pDecInfo;
	Uint32      val     = 0;
	RetCode     ret     = RETCODE_SUCCESS;
	VpuAttr*    pAttr   = NULL;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = (CodecInst*)handle;
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;

	if (pDecInfo->stride == 0) // This means frame buffers have not been registered.
		return RETCODE_WRONG_CALL_SEQUENCE;

	pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	if (pAttr->supportCommandQueue == FALSE) {
		val  = pDecInfo->frameDisplayFlag;
		val |= pDecInfo->setDisplayIndexes;
		val &= ~(Uint32)(pDecInfo->clearDisplayIndexes);
		Wave5WriteReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr, val);
		pDecInfo->clearDisplayIndexes = 0;
		pDecInfo->setDisplayIndexes = 0;
	}

	pDecInfo->frameStartPos = pDecInfo->streamRdPtr;

	ret = ProductVpuDecode(pCodecInst, param);

	if (pAttr->supportCommandQueue == TRUE)
		WAVE5_SetPendingInst(pCodecInst->coreIdx, NULL);
	else
		WAVE5_SetPendingInst(pCodecInst->coreIdx, pCodecInst);

	return ret;
}

RetCode WAVE5_DecGetOutputInfo(DecHandle handle, DecOutputInfo *info)
{
	CodecInst*  pCodecInst;
	DecInfo*    pDecInfo;
	RetCode     ret;
	VpuRect     rectInfo;
	Uint32      val;
	Int32       decodedIndex;
	Int32       displayIndex;
	Uint32      maxDecIndex;
	VpuAttr*    pAttr;
	PhysicalAddress streamRdPtr;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (info == 0)
		return RETCODE_INVALID_PARAM;

	pCodecInst  = handle;
	pDecInfo    = &pCodecInst->CodecInfo.decInfo;
	pAttr       = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	if (pAttr->supportCommandQueue == TRUE) {
	}
	else {
		if (pCodecInst != WAVE5_GetPendingInst(pCodecInst->coreIdx)) {
			WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);
			return RETCODE_WRONG_CALL_SEQUENCE;
		}
	}

	ret = ProductVpuDecGetResult(pCodecInst, info);
	if (ret != RETCODE_SUCCESS) {
		#ifdef ENABLE_FORCE_ESCAPE
		if( gWave5InitFirst_exit > 0 )
			return RETCODE_CODEC_EXIT;
		#endif
		info->rdPtr = pDecInfo->streamRdPtr;
		info->wrPtr = pDecInfo->streamWrPtr;
		WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);
		return ret;
	}

	decodedIndex = info->indexFrameDecoded;

	if ( info->sequenceChanged ) {
		if ( pCodecInst->CodecInfo.decInfo.openParam.FullSizeFB == 1 ) {
			pCodecInst->CodecInfo.decInfo.decidx_seqchange = info->indexFrameDecoded;
			info->indexFrameDecoded = -1;
			info->indexFrameDecodedForTiled = -1;
			decodedIndex = -1;
		}
	}

	if (pDecInfo->openParam.afbceEnable) {
		maxDecIndex = (pDecInfo->numFbsForDecoding > pDecInfo->numFbsForWTL) ? pDecInfo->numFbsForDecoding : pDecInfo->numFbsForWTL;
		if (0 <= decodedIndex && decodedIndex < (int)maxDecIndex) {
			val = pDecInfo->numFbsForDecoding; //fbOffset
			pDecInfo->frameBufPool[val + decodedIndex].lfEnable = info->lfEnable;
		}
	}

	// Calculate display frame region
	val = 0;
	if (decodedIndex >= 0 && decodedIndex < MAX_GDI_IDX) {
		//default value
		rectInfo.left   = 0;
		rectInfo.right  = info->decPicWidth;
		rectInfo.top    = 0;
		rectInfo.bottom = info->decPicHeight;

		#ifndef TEST_HEVC_RC_CROPINFO_REGISTER_READ
		if (pCodecInst->codecMode == HEVC_DEC || pCodecInst->codecMode == AVC_DEC || pCodecInst->codecMode == W_AVC_DEC || pCodecInst->codecMode == AVS_DEC)
			rectInfo = pDecInfo->initialInfo.picCropRect;
		#else
		if ((pCodecInst->codecMode == HEVC_DEC) && (pDecInfo->openParam.FullSizeFB)) {
			unsigned int regVal, left, right, top, bottom;

			regVal = Wave5ReadReg(pCodecInst->coreIdx, W5_RET_DEC_CROP_LEFT_RIGHT);
			left   = (regVal >> 16) & 0xffff;
			right  = regVal & 0xffff;
			regVal = Wave5ReadReg(pCodecInst->coreIdx, W5_RET_DEC_CROP_TOP_BOTTOM);
			top    = (regVal >> 16) & 0xffff;
			bottom = regVal & 0xffff;

			rectInfo.left   = left;
			rectInfo.right  = info->decPicWidth - right;
			rectInfo.top    = top;
			rectInfo.bottom = info->decPicHeight - bottom;
		}
		#endif

		if (pCodecInst->codecMode == HEVC_DEC)
			pDecInfo->decOutInfo[decodedIndex].h265Info.decodedPOC = info->h265Info.decodedPOC;

		info->rcDecoded.left   =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.left   = rectInfo.left;
		info->rcDecoded.right  =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.right  = rectInfo.right;
		info->rcDecoded.top    =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.top    = rectInfo.top;
		info->rcDecoded.bottom =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.bottom = rectInfo.bottom;
	}
	else {
		info->rcDecoded.left   = 0;
		info->rcDecoded.right  = info->decPicWidth;
		info->rcDecoded.top    = 0;
		info->rcDecoded.bottom = info->decPicHeight;
	}

	displayIndex = info->indexFrameDisplay;
	if (info->indexFrameDisplay >= 0 && info->indexFrameDisplay < MAX_GDI_IDX) {
		if (pDecInfo->rotationEnable) {
			switch(pDecInfo->rotationAngle) {
				case 90:
					info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
					info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
					info->rcDisplay.top    = info->decPicWidth - pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
					info->rcDisplay.bottom = info->decPicWidth - pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
					break;
				case 270:
					info->rcDisplay.left   = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
					info->rcDisplay.right  = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
					info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
					info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
					break;
				case 180:
					info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
					info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
					info->rcDisplay.top    = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
					info->rcDisplay.bottom = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
					break;
				default:
					info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
					info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
					info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
					info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
					break;
			}
		}
		else {
			info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
			info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
			info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
			info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
		}

		if (pDecInfo->mirrorEnable) {
			Uint32 temp;
			if (pDecInfo->mirrorDirection & MIRDIR_VER) {
				temp = info->rcDisplay.top;
				info->rcDisplay.top    = info->decPicHeight - info->rcDisplay.bottom;
				info->rcDisplay.bottom = info->decPicHeight - temp;
			}

			if (pDecInfo->mirrorDirection & MIRDIR_HOR) {
				temp = info->rcDisplay.left;
				info->rcDisplay.left  = info->decPicWidth - info->rcDisplay.right;
				info->rcDisplay.right = info->decPicWidth - temp;
			}
		}

		switch (pCodecInst->codecMode) {
			case HEVC_DEC:
				info->h265Info.displayPOC = pDecInfo->decOutInfo[displayIndex].h265Info.decodedPOC;
				break;
			default:
				break;
		}

		if (info->indexFrameDisplay == info->indexFrameDecoded) {
			info->dispPicWidth =  info->decPicWidth;
			info->dispPicHeight = info->decPicHeight;
		}
		else {
            /*
                When indexFrameDecoded < 0, and indexFrameDisplay >= 0
                info->decPicWidth and info->decPicHeight are still valid
                But those of pDecInfo->decOutInfo[displayIndex] are invalid in VP9
            */
			info->dispPicWidth  = pDecInfo->decOutInfo[displayIndex].decPicWidth;
			info->dispPicHeight = pDecInfo->decOutInfo[displayIndex].decPicHeight;
		}

		if (pDecInfo->scalerEnable == TRUE) {
			if ((pDecInfo->scaleWidth != 0) && (pDecInfo->scaleHeight != 0)) {
				info->dispPicWidth     = pDecInfo->scaleWidth;
				info->dispPicHeight    = pDecInfo->scaleHeight;
				info->rcDisplay.right  = pDecInfo->scaleWidth;
				info->rcDisplay.bottom = pDecInfo->scaleHeight;
			}
		}
	}
	else {
		info->rcDisplay.left   = 0;
		info->rcDisplay.right  = 0;
		info->rcDisplay.top    = 0;
		info->rcDisplay.bottom = 0;

		if (pDecInfo->rotationEnable || pDecInfo->mirrorEnable || pDecInfo->tiled2LinearEnable || pDecInfo->deringEnable) {
			info->dispPicWidth  = info->decPicWidth;
			info->dispPicHeight = info->decPicHeight;
		}
		else {
			info->dispPicWidth = 0;
			info->dispPicHeight = 0;
		}
	}

	if ((pCodecInst->codecMode == VC1_DEC || pCodecInst->codecMode == W_VC1_DEC) && info->indexFrameDisplay != -3) {
		if (pDecInfo->vc1BframeDisplayValid == 0) {
			if (info->picType == 2)
				info->indexFrameDisplay = -3;
			else
				pDecInfo->vc1BframeDisplayValid = 1;
		}
	}

	streamRdPtr =  ProductVpuDecGetRdPtr(pCodecInst);
#ifdef ENABLE_FORCE_ESCAPE
	if( gWave5InitFirst_exit > 0 )
		return RETCODE_CODEC_EXIT;
#endif
	if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT)
		pDecInfo->streamRdPtr = streamRdPtr;

	pDecInfo->frameDisplayFlag = Wave5ReadReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr);
	#if TCC_VP9____DEC_SUPPORT == 1
	if (pCodecInst->codecMode == W_VP9_DEC)
		pDecInfo->frameDisplayFlag  &= 0xFFFF;
	#endif

	pDecInfo->frameEndPos = streamRdPtr;

	if (pDecInfo->frameEndPos < pDecInfo->frameStartPos)
		info->consumedByte = pDecInfo->frameEndPos + pDecInfo->streamBufSize - pDecInfo->frameStartPos;
	else
		info->consumedByte = pDecInfo->frameEndPos - pDecInfo->frameStartPos;

	if (pDecInfo->deringEnable || pDecInfo->mirrorEnable || pDecInfo->rotationEnable || pDecInfo->tiled2LinearEnable) {
		info->dispFrame        = pDecInfo->rotatorOutput;
		info->dispFrame.stride = pDecInfo->rotatorStride;
	}
	else {
		val = ((pDecInfo->openParam.wtlEnable == TRUE || pDecInfo->openParam.afbceEnable) ? pDecInfo->numFbsForDecoding: 0); //fbOffset
		maxDecIndex = (pDecInfo->numFbsForDecoding > pDecInfo->numFbsForWTL) ? pDecInfo->numFbsForDecoding : pDecInfo->numFbsForWTL;

		if (0 <= info->indexFrameDisplay && info->indexFrameDisplay < (int)maxDecIndex)
			info->dispFrame = pDecInfo->frameBufPool[val+info->indexFrameDisplay];
	}

	info->rdPtr            = streamRdPtr;
	info->wrPtr            = pDecInfo->streamWrPtr;
	info->frameDisplayFlag = pDecInfo->frameDisplayFlag;

	info->sequenceNo = pDecInfo->initialInfo.sequenceNo;
	if (decodedIndex >= 0 && decodedIndex < MAX_GDI_IDX)
		pDecInfo->decOutInfo[decodedIndex] = *info;

	if (displayIndex >= 0 && displayIndex < MAX_GDI_IDX) {
		info->numOfTotMBs = info->numOfTotMBs;
		info->numOfErrMBs = info->numOfErrMBs;
		info->numOfTotMBsInDisplay = pDecInfo->decOutInfo[displayIndex].numOfTotMBs;
		info->numOfErrMBsInDisplay = pDecInfo->decOutInfo[displayIndex].numOfErrMBs;
		info->dispFrame.sequenceNo = info->sequenceNo;
	}
	else {
		info->numOfTotMBsInDisplay = 0;
		info->numOfErrMBsInDisplay = 0;
	}

	if (info->sequenceChanged != 0) {
		if (!(pCodecInst->productId == PRODUCT_ID_960 || pCodecInst->productId == PRODUCT_ID_980)) {
			/* Update new sequence information */
			wave5_memcpy((void*)&pDecInfo->initialInfo, (void*)&pDecInfo->newSeqInfo, sizeof(DecInitialInfo), 0);
		}

		if ((info->sequenceChanged & SEQ_CHANGE_INTER_RES_CHANGE) != SEQ_CHANGE_INTER_RES_CHANGE)
			pDecInfo->initialInfo.sequenceNo++;
	}

	WAVE5_SetPendingInst(pCodecInst->coreIdx, 0);

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecFrameBufferFlush(DecHandle handle, DecOutputInfo *pRemainings, Uint32 *retNum)
{
	CodecInst*       pCodecInst;
	DecInfo*         pDecInfo;
	DecOutputInfo*   pOut;
	RetCode          ret;
	FramebufferIndex retIndex[MAX_GDI_IDX + 1]; //32 array
	Uint32           retRemainings = 0;
	Int32            i, index, val;
	VpuAttr*         pAttr      = NULL;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (WAVE5_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	if( wave5_memset )
		wave5_memset((void*)retIndex, 0xff, sizeof(retIndex), 0);

	pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];
	if (pAttr->supportCommandQueue == FALSE) {
		val  = pDecInfo->frameDisplayFlag;
		val |= pDecInfo->setDisplayIndexes;
		val &= ~(Uint32)(pDecInfo->clearDisplayIndexes);
		Wave5WriteReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr, val);
		pDecInfo->clearDisplayIndexes = 0;
		pDecInfo->setDisplayIndexes = 0;
	}

	if ((ret=ProductVpuDecFlush(pCodecInst, retIndex, MAX_GDI_IDX)) != RETCODE_SUCCESS)
		return ret;

	if (pRemainings != NULL) {
		for (i=0; i<MAX_GDI_IDX; i++) {
			index = (pDecInfo->wtlEnable == TRUE) ? retIndex[i].tiledIndex : retIndex[i].linearIndex;
			if (index < 0)
				break;

			if (index >= MAX_GDI_IDX)
				break;

			pRemainings[i] = pDecInfo->decOutInfo[index];
			pOut = &pRemainings[i];
			pOut->indexFrameDisplay         = pOut->indexFrameDecoded;
			pOut->indexFrameDisplayForTiled = pOut->indexFrameDecodedForTiled;
			if (pDecInfo->wtlEnable == TRUE)
				pOut->dispFrame = pDecInfo->frameBufPool[pDecInfo->numFbsForDecoding+retIndex[i].linearIndex];
			else
				pOut->dispFrame = pDecInfo->frameBufPool[index];

			pOut->dispFrame.sequenceNo = pOut->sequenceNo;

			#if TCC_VP9____DEC_SUPPORT == 1
			if (pCodecInst->codecMode == W_VP9_DEC) {
				Uint32   regVal;
				if ((pDecInfo->scaleWidth == 0) || (pDecInfo->scaleHeight == 0)) {
					regVal = Wave5ReadReg(pCodecInst->coreIdx, W5_RET_DEC_DISPLAY_SIZE);
					pOut->dispPicWidth   = regVal>>16;
					pOut->dispPicHeight  = regVal&0xffff;
				}
				else {
					pOut->dispPicWidth   = pDecInfo->scaleWidth;
					pOut->dispPicHeight  = pDecInfo->scaleHeight;
				}
			}
			#endif

			#if TCC_HEVC___DEC_SUPPORT == 1
			if (pCodecInst->codecMode == W_HEVC_DEC) {
				pOut->dispPicWidth  = pOut->decPicWidth;
				pOut->dispPicHeight = pOut->decPicHeight;
			}
			#endif

			if (pDecInfo->rotationEnable) {
				switch(pDecInfo->rotationAngle) {
				case 90:
					pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.top;
					pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.bottom;
					pOut->rcDisplay.top    = pOut->decPicWidth - pDecInfo->decOutInfo[index].rcDecoded.right;
					pOut->rcDisplay.bottom = pOut->decPicWidth - pDecInfo->decOutInfo[index].rcDecoded.left;
					break;
				case 270:
					pOut->rcDisplay.left   = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.bottom;
					pOut->rcDisplay.right  = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.top;
					pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.left;
					pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.right;
					break;
				case 180:
					pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
					pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
					pOut->rcDisplay.top    = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.bottom;
					pOut->rcDisplay.bottom = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.top;
					break;
				default:
					pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
					pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
					pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.top;
					pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.bottom;
					break;
				}
			}
			else {
				pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
				pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
				pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.top;
				pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.bottom;
			}
			retRemainings++;
		}
	}

	if (retNum) *retNum = retRemainings;

	#ifdef ENABLE_VDI_LOG
	if (pCodecInst->loggingEnable)
		vdi_log(pCodecInst->coreIdx, DEC_BUF_FLUSH, 0);
	#endif

	#ifdef ENABLE_FORCE_ESCAPE
	if( gWave5InitFirst_exit > 0 )
		return RETCODE_CODEC_EXIT;
	#endif

	return ret;
}


RetCode WAVE5_DecSetRdPtr(DecHandle handle, PhysicalAddress addr, int updateWrPtr)
{
	CodecInst* pCodecInst;
	CodecInst* pPendingInst;
	DecInfo * pDecInfo;
	RetCode ret;
	VpuAttr*    pAttr       = NULL;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = (CodecInst*)handle;

	ret = ProductVpuDecCheckCapability(pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	//pCodecInst   = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	pPendingInst = WAVE5_GetPendingInst((Uint32)pCodecInst->coreIdx);
	if (pCodecInst == pPendingInst) {
		Wave5WriteReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr, addr);
	}
	else {
		if (pAttr->supportCommandQueue == TRUE) {
			if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT) {
				Wave5VpuDecSetRdPtr(pCodecInst, addr);
			}
		}
		else {
			Wave5WriteReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr, addr);
		}
	}

	pDecInfo->streamRdPtr = addr;
//	pDecInfo->prevFrameEndPos = addr;
	if (updateWrPtr == TRUE)
		pDecInfo->streamWrPtr = addr;

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecClrDispFlag(DecHandle handle, int index)
{
	CodecInst*  pCodecInst;
	DecInfo*    pDecInfo;
	RetCode     ret         = RETCODE_SUCCESS;
	Int32       endIndex;
	VpuAttr*    pAttr       = NULL;
	BOOL        supportCommandQueue;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;
	pAttr      = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	endIndex = (pDecInfo->openParam.wtlEnable == TRUE) ? pDecInfo->numFbsForWTL : pDecInfo->numFbsForDecoding;

	if ((index < 0) || (index > (endIndex - 1)))
		return RETCODE_INVALID_PARAM;

	supportCommandQueue = (pAttr->supportCommandQueue == TRUE);
	if (supportCommandQueue == TRUE) {
		ret = ProductClrDispFlag(pCodecInst, index);
		if( ret != RETCODE_SUCCESS )
			return ret;
	}
	else {
		pDecInfo->clearDisplayIndexes |= (1<<index);
	}

	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecClrDispFlagEx(DecHandle handle, unsigned int dispFlag)
{
	CodecInst*  pCodecInst;
	DecInfo*    pDecInfo;
	RetCode     ret         = RETCODE_SUCCESS;
	VpuAttr*    pAttr       = NULL;
	BOOL        supportCommandQueue;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo   = &pCodecInst->CodecInfo.decInfo;
	pAttr      = &g_VpuCoreAttributes[pCodecInst->coreIdx];

	if (dispFlag == 0x00)
		return RETCODE_INVALID_PARAM;

	supportCommandQueue = (pAttr->supportCommandQueue == TRUE);
	if (supportCommandQueue == TRUE) {
		ret = ProductClrDispFlagEx(pCodecInst, dispFlag);
		if (ret != RETCODE_SUCCESS)
			return ret;
	}
	else {
		pDecInfo->clearDisplayIndexes |= dispFlag;
	}
	return RETCODE_SUCCESS;
}

RetCode WAVE5_DecGiveCommand(DecHandle handle, CodecCommand cmd, void* param)
{
	CodecInst*          pCodecInst;
	DecInfo*            pDecInfo;
	RetCode             ret;
	//PhysicalAddress     addr;

	ret = CheckDecInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;
	switch (cmd)
	{
	case ENABLE_ROTATION :
		{
			if (pDecInfo->rotatorStride == 0)
				return RETCODE_ROTATOR_STRIDE_NOT_SET;

			pDecInfo->rotationEnable = 1;
			break;
		}

	case DISABLE_ROTATION :
		{
			pDecInfo->rotationEnable = 0;
			break;
		}

	case ENABLE_MIRRORING :
		{
			if (pDecInfo->rotatorStride == 0)
				return RETCODE_ROTATOR_STRIDE_NOT_SET;

			pDecInfo->mirrorEnable = 1;
			break;
		}
	case DISABLE_MIRRORING :
		{
			pDecInfo->mirrorEnable = 0;
			break;
		}
	case SET_MIRROR_DIRECTION :
		{
			MirrorDirection mirDir;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			mirDir = *(MirrorDirection *)param;
			if ( !(mirDir == MIRDIR_NONE) && !(mirDir==MIRDIR_HOR) && !(mirDir==MIRDIR_VER) && !(mirDir==MIRDIR_HOR_VER))
				return RETCODE_INVALID_PARAM;

			pDecInfo->mirrorDirection = mirDir;
			break;
		}
	case SET_ROTATION_ANGLE :
		{
			int angle;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			angle = *(int *)param;
			if (angle != 0 && angle != 90 &&
				angle != 180 && angle != 270) {
					return RETCODE_INVALID_PARAM;
			}
			if (pDecInfo->rotatorStride != 0) {
				if (angle == 90 || angle ==270) {
					if (pDecInfo->initialInfo.picHeight > pDecInfo->rotatorStride)
						return RETCODE_INVALID_PARAM;
				} else {
					if (pDecInfo->initialInfo.picWidth > pDecInfo->rotatorStride)
						return RETCODE_INVALID_PARAM;
				}
			}

			pDecInfo->rotationAngle = angle;
			break;
		}
	case SET_ROTATOR_OUTPUT :
		{
			FrameBuffer *frame;
			if (param == 0)
				return RETCODE_INVALID_PARAM;
			frame = (FrameBuffer *)param;

			pDecInfo->rotatorOutput = *frame;
			pDecInfo->rotatorOutputValid = 1;
			break;
		}

	case SET_ROTATOR_STRIDE :
		{
			int stride;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			stride = *(int *)param;
			if (stride % 8 != 0 || stride == 0)
				return RETCODE_INVALID_STRIDE;

			if (pDecInfo->rotationAngle == 90 || pDecInfo->rotationAngle == 270) {
				if (pDecInfo->initialInfo.picHeight > stride)
					return RETCODE_INVALID_STRIDE;
			} else {
				if (pDecInfo->initialInfo.picWidth > stride)
					return RETCODE_INVALID_STRIDE;
			}

			pDecInfo->rotatorStride = stride;
			break;
		}
	case ENABLE_DERING :
		{
			if (pDecInfo->rotatorStride == 0)
				return RETCODE_ROTATOR_STRIDE_NOT_SET;

			pDecInfo->deringEnable = 1;
			break;
		}

	case DISABLE_DERING :
		{
			pDecInfo->deringEnable = 0;
			break;
		}
	case SET_SEC_AXI:
		{
			SecAxiUse secAxiUse;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			secAxiUse = *(SecAxiUse *)param;
			if (handle->productId == PRODUCT_ID_512 || handle->productId == PRODUCT_ID_520) {
				pDecInfo->secAxiInfo.u.wave.useIpEnable    = secAxiUse.u.wave.useIpEnable;
				pDecInfo->secAxiInfo.u.wave.useLfRowEnable = secAxiUse.u.wave.useLfRowEnable;
				pDecInfo->secAxiInfo.u.wave.useBitEnable   = secAxiUse.u.wave.useBitEnable;
				pDecInfo->secAxiInfo.u.wave.useSclEnable   = secAxiUse.u.wave.useSclEnable;
				pDecInfo->secAxiInfo.u.wave.useSclPackedModeEnable = secAxiUse.u.wave.useSclPackedModeEnable;
			}
			else {
				pDecInfo->secAxiInfo.u.coda9.useBitEnable  = secAxiUse.u.coda9.useBitEnable;
				pDecInfo->secAxiInfo.u.coda9.useIpEnable   = secAxiUse.u.coda9.useIpEnable;
				pDecInfo->secAxiInfo.u.coda9.useDbkYEnable = secAxiUse.u.coda9.useDbkYEnable;
				pDecInfo->secAxiInfo.u.coda9.useDbkCEnable = secAxiUse.u.coda9.useDbkCEnable;
				pDecInfo->secAxiInfo.u.coda9.useOvlEnable  = secAxiUse.u.coda9.useOvlEnable;
				pDecInfo->secAxiInfo.u.coda9.useBtpEnable  = secAxiUse.u.coda9.useBtpEnable;
			}

			break;
		}
	case ENABLE_AFBCE:
		{
			pDecInfo->enableAfbce = 1;
			break;
		}
	case DISABLE_AFBCE:
		{
			pDecInfo->enableAfbce = 0;
			break;
		}
	case ENABLE_REP_USERDATA:
		{
			if (!pDecInfo->userDataBufAddr)
				return RETCODE_USERDATA_BUF_NOT_SET;

			if (pDecInfo->userDataBufSize == 0)
				return RETCODE_USERDATA_BUF_NOT_SET;

			switch (pCodecInst->productId) {
			case PRODUCT_ID_512:
				pDecInfo->userDataEnable = *(Uint32*)param;
				break;
			case PRODUCT_ID_960:
			case PRODUCT_ID_980:
				pDecInfo->userDataEnable = TRUE;
				break;
            default:
//				VLOG(INFO, "%s(ENABLE_REP_DATA) invalid productId(%d)\n", __FUNCTION__, pCodecInst->productId);
				return RETCODE_INVALID_PARAM;
			}
			break;
		}
	case DISABLE_REP_USERDATA:
		{
			pDecInfo->userDataEnable = 0;
			break;
		}
	case SET_ADDR_REP_USERDATA:
		{
			PhysicalAddress userDataBufAddr;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			userDataBufAddr = *(PhysicalAddress *)param;
			if (userDataBufAddr % 8 != 0 || userDataBufAddr == 0)
				return RETCODE_INVALID_PARAM;

			pDecInfo->userDataBufAddr = userDataBufAddr;
			break;
		}
	case SET_VIRT_ADDR_REP_USERDATA:
		{
			codec_addr_t userDataVirtAddr;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			if (!pDecInfo->userDataBufAddr)
				return RETCODE_USERDATA_BUF_NOT_SET;

			if (pDecInfo->userDataBufSize == 0)
				return RETCODE_USERDATA_BUF_NOT_SET;

			userDataVirtAddr = *(codec_addr_t *)param;
			if (!userDataVirtAddr)
				return RETCODE_INVALID_PARAM;

			pDecInfo->vbUserData.phys_addr = pDecInfo->userDataBufAddr;
			pDecInfo->vbUserData.size = pDecInfo->userDataBufSize;
			pDecInfo->vbUserData.virt_addr = (codec_addr_t)userDataVirtAddr;
			break;
		}
	case SET_SIZE_REP_USERDATA:
		{
			PhysicalAddress userDataBufSize;

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			userDataBufSize = *(PhysicalAddress *)param;
			pDecInfo->userDataBufSize = userDataBufSize;
			break;
		}

	case SET_USERDATA_REPORT_MODE:
		{
			int userDataMode;

			userDataMode = *(int *)param;
			if (userDataMode != 1 && userDataMode != 0)
				return RETCODE_INVALID_PARAM;

			pDecInfo->userDataReportMode = userDataMode;
			break;
		}
	case SET_CACHE_CONFIG:
		{
			MaverickCacheConfig *mcCacheConfig;
			if (param == 0)
				return RETCODE_INVALID_PARAM;

			mcCacheConfig = (MaverickCacheConfig *)param;
			pDecInfo->cacheConfig = *mcCacheConfig;
		}
		break;
	case SET_LOW_DELAY_CONFIG:
		{
			LowDelayInfo *lowDelayInfo;
			if (param == 0)
				return RETCODE_INVALID_PARAM;

			if (pCodecInst->productId != PRODUCT_ID_980)
				return RETCODE_NOT_SUPPORTED_FEATURE;

			lowDelayInfo = (LowDelayInfo *)param;

			if (lowDelayInfo->lowDelayEn) {
				if ( (pCodecInst->codecMode != AVC_DEC && pCodecInst->codecMode != W_AVC_DEC) ||
					pDecInfo->rotationEnable ||
					pDecInfo->mirrorEnable ||
					pDecInfo->tiled2LinearEnable ||
					pDecInfo->deringEnable) {
						return RETCODE_INVALID_PARAM;
				}
			}

			pDecInfo->lowDelayInfo.lowDelayEn = lowDelayInfo->lowDelayEn;
			pDecInfo->lowDelayInfo.numRows    = lowDelayInfo->numRows;
		}
		break;
	case DEC_SET_FRAME_DELAY:
		{
			pDecInfo->frameDelay = *(int *)param;
			break;
		}
	case DEC_ENABLE_REORDER:
		{
			if((handle->productId == PRODUCT_ID_980) || (handle->productId == PRODUCT_ID_960) || (handle->productId == PRODUCT_ID_950))
			{
				if (pDecInfo->initialInfoObtained)
					return RETCODE_WRONG_CALL_SEQUENCE;
			}

			pDecInfo->reorderEnable = 1;
			break;
		}
	case DEC_DISABLE_REORDER:
		{
			if((handle->productId == PRODUCT_ID_980) || (handle->productId == PRODUCT_ID_960) || (handle->productId == PRODUCT_ID_950))
			{
				if (pDecInfo->initialInfoObtained)
					return RETCODE_WRONG_CALL_SEQUENCE;

				if(pCodecInst->codecMode != AVC_DEC && pCodecInst->codecMode != VC1_DEC && pCodecInst->codecMode != AVS_DEC && pCodecInst->codecMode != W_AVC_DEC && pCodecInst->codecMode != W_VC1_DEC && pCodecInst->codecMode != W_AVS_DEC)
					return RETCODE_INVALID_COMMAND;
			}

			pDecInfo->reorderEnable = 0;
			break;
		}
	case DEC_SET_AVC_ERROR_CONCEAL_MODE:
		{
			if(pCodecInst->codecMode != AVC_DEC && pCodecInst->codecMode != W_AVC_DEC)
				return RETCODE_INVALID_COMMAND;

			pDecInfo->avcErrorConcealMode = *(int *)param;
			break;
		}
	case DEC_GET_FRAMEBUF_INFO:
		{
			DecGetFramebufInfo* fbInfo = (DecGetFramebufInfo*)param;
			Uint32 i;
			fbInfo->vbFrame   = pDecInfo->vbFrame;
			fbInfo->vbWTL     = pDecInfo->vbWTL;
			for (i=0  ; i<MAX_REG_FRAME; i++) {
				fbInfo->vbFbcYTbl[i] = pDecInfo->vbFbcYTbl[i];
				fbInfo->vbFbcCTbl[i] = pDecInfo->vbFbcCTbl[i];
				fbInfo->vbMvCol[i]   = pDecInfo->vbMV[i];
			}

			for (i=0; i<MAX_GDI_IDX*2; i++) {
				fbInfo->framebufPool[i] = pDecInfo->frameBufPool[i];
			}
		}
		break;
	case DEC_RESET_FRAMEBUF_INFO:
		{
			int i;

			pDecInfo->vbFrame.base          = 0;
			pDecInfo->vbFrame.phys_addr     = 0;
			pDecInfo->vbFrame.virt_addr     = 0;
			pDecInfo->vbFrame.size          = 0;
			pDecInfo->vbWTL.base            = 0;
			pDecInfo->vbWTL.phys_addr       = 0;
			pDecInfo->vbWTL.virt_addr       = 0;
			pDecInfo->vbWTL.size            = 0;
			for (i=0  ; i<MAX_REG_FRAME; i++) {
				pDecInfo->vbFbcYTbl[i].base        = 0;
				pDecInfo->vbFbcYTbl[i].phys_addr   = 0;
				pDecInfo->vbFbcYTbl[i].virt_addr   = 0;
				pDecInfo->vbFbcYTbl[i].size        = 0;
				pDecInfo->vbFbcCTbl[i].base        = 0;
				pDecInfo->vbFbcCTbl[i].phys_addr   = 0;
				pDecInfo->vbFbcCTbl[i].virt_addr   = 0;
				pDecInfo->vbFbcCTbl[i].size        = 0;
				pDecInfo->vbMV[i].base             = 0;
				pDecInfo->vbMV[i].phys_addr        = 0;
				pDecInfo->vbMV[i].virt_addr        = 0;
				pDecInfo->vbMV[i].size             = 0;
			}

			pDecInfo->frameDisplayFlag      = 0;
			pDecInfo->setDisplayIndexes     = 0;
			pDecInfo->clearDisplayIndexes   = 0;
			break;
		}
	case DEC_GET_QUEUE_STATUS:
		{
			DecQueueStatusInfo *queueInfo = (DecQueueStatusInfo *)param;
			queueInfo->instanceQueueCount = pDecInfo->instanceQueueCount;
			queueInfo->totalQueueCount    = pDecInfo->totalQueueCount;
			break;
		}


	case ENABLE_DEC_THUMBNAIL_MODE:
		{
			pDecInfo->thumbnailMode = 1;
			break;
		}
	case DEC_GET_SEQ_INFO:
		{
			DecInitialInfo *seqInfo = (DecInitialInfo *)param;
			*seqInfo = pDecInfo->initialInfo;
			break;
		}
	case DEC_GET_FIELD_PIC_TYPE:
		{
			return RETCODE_FAILURE;
		}
	case DEC_GET_DISPLAY_OUTPUT_INFO:
		{
			DecOutputInfo *pDecOutInfo = (DecOutputInfo *)param;
			*pDecOutInfo = pDecInfo->decOutInfo[pDecOutInfo->indexFrameDisplay];
			break;
		}
	case GET_TILEDMAP_CONFIG:
		{
			TiledMapConfig *pMapCfg = (TiledMapConfig *)param;
			if (!pMapCfg)
				return RETCODE_INVALID_PARAM;

			if (!pDecInfo->stride)
				return RETCODE_WRONG_CALL_SEQUENCE;

			*pMapCfg = pDecInfo->mapCfg;
			break;
		}
	case SET_DRAM_CONFIG:
		{
			DRAMConfig *cfg = (DRAMConfig *)param;

			if (!cfg)
				return RETCODE_INVALID_PARAM;

			pDecInfo->dramCfg = *cfg;
			break;
		}
	case GET_DRAM_CONFIG:
		{
			DRAMConfig *cfg = (DRAMConfig *)param;

			if (!cfg)
				return RETCODE_INVALID_PARAM;

			*cfg = pDecInfo->dramCfg;

			break;
		}
	case ENABLE_LOGGING:
		{
			pCodecInst->loggingEnable = 1;
		}
		break;
	case DISABLE_LOGGING:
		{
			pCodecInst->loggingEnable = 0;
		}
		break;
	case DEC_SET_SEQ_CHANGE_MASK:
		if (PRODUCT_ID_NOT_W_SERIES(pCodecInst->productId))
			return RETCODE_INVALID_PARAM;
		pDecInfo->seqChangeMask = *(int*)param;
		break;
	case DEC_SET_WTL_FRAME_FORMAT:
		pDecInfo->wtlFormat = *(FrameBufferFormat*)param;
		break;
	case DEC_SET_DISPLAY_FLAG:
		{
			Int32       index;
			VpuAttr*    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			index = *(Int32 *)param;
			ProductSetDispFlag(pCodecInst, index);
		}
		break;
	case DEC_SET_DISPLAY_FLAG_EX:
		{
			Uint32      dispFlag;
			VpuAttr*    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

			if (param == 0)
				return RETCODE_INVALID_PARAM;

			dispFlag = *(Uint32 *)param;
			ProductSetDispFlagEx(pCodecInst, dispFlag);
		}
		break;
	case DEC_GET_SCALER_INFO:
		{
			ScalerInfo* scalerInfo = (ScalerInfo*)param;

			if (scalerInfo == NULL)
				return RETCODE_INVALID_PARAM;

			scalerInfo->enScaler    = pDecInfo->scalerEnable;
			scalerInfo->scaleWidth  = pDecInfo->scaleWidth;
			scalerInfo->scaleHeight = pDecInfo->scaleHeight;
		}
		break;
	case DEC_SET_SCALER_INFO:
		{
			ScalerInfo* scalerInfo = (ScalerInfo*)param;

			if (!pDecInfo->initialInfoObtained)
				return RETCODE_WRONG_CALL_SEQUENCE;

			if (scalerInfo == NULL)
				return RETCODE_INVALID_PARAM;

			pDecInfo->scalerEnable = scalerInfo->enScaler;
			if (scalerInfo->enScaler == TRUE) {
				// minW = Ceil8(picWidth/8), minH = Ceil8(picHeight/8)
				Uint32   minScaleWidth  = WAVE5_ALIGN8( (pDecInfo->initialInfo.picWidth>>3) );
				Uint32   minScaleHeight = WAVE5_ALIGN8( (pDecInfo->initialInfo.picHeight>>3) );

				if (minScaleWidth == 0)  minScaleWidth  = 8;
				if (minScaleHeight == 0) minScaleHeight = 8;

				if (scalerInfo->scaleWidth < minScaleWidth || scalerInfo->scaleHeight < minScaleHeight)
					return RETCODE_INVALID_PARAM;

				if (scalerInfo->scaleWidth > 0 || scalerInfo->scaleHeight > 0) {
					if ((scalerInfo->scaleWidth % 8) || scalerInfo->scaleWidth > (Uint32)(WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)))
						return RETCODE_INVALID_PARAM;

					if ((scalerInfo->scaleHeight % 8) || scalerInfo->scaleHeight > (Uint32)(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight)))
						return RETCODE_INVALID_PARAM;

					pDecInfo->scaleWidth   = scalerInfo->scaleWidth;
					pDecInfo->scaleHeight  = scalerInfo->scaleHeight;
					//pDecInfo->scalerEnable = scalerInfo->enScaler;
				}
			}
			break;
		}
	case DEC_SET_TARGET_TEMPORAL_ID:
		if (param == NULL)
			return RETCODE_INVALID_PARAM;

		if (pCodecInst->codecMode != HEVC_DEC) {
			return RETCODE_NOT_SUPPORTED_FEATURE;
		}
		else {
			Uint32 targetSublayerId = *(Uint32*)param;
			if (targetSublayerId > HEVC_MAX_SUB_LAYER_ID)
				return RETCODE_INVALID_PARAM;

			pDecInfo->targetSubLayerId = targetSublayerId;
		}
		break;
	case DEC_SET_BWB_CUR_FRAME_IDX:
		pDecInfo->chBwbFrameIdx = *(Uint32*)param;
		break;
	case DEC_SET_FBC_CUR_FRAME_IDX:
		pDecInfo->chFbcFrameIdx = *(Uint32*)param;
		break;
	case DEC_SET_INTER_RES_INFO_ON:
		pDecInfo->interResChange = 1;
		break;
	case DEC_SET_INTER_RES_INFO_OFF:
		pDecInfo->interResChange = 0;
		break;
	case DEC_FREE_FBC_TABLE_BUFFER:
		{
			Uint32 fbcCurFrameIdx = *(Uint32*)param;

			if(pDecInfo->vbFbcYTbl[fbcCurFrameIdx].size > 0)
				pDecInfo->vbFbcYTbl[fbcCurFrameIdx].size = 0;

			if(pDecInfo->vbFbcCTbl[fbcCurFrameIdx].size > 0)
				pDecInfo->vbFbcCTbl[fbcCurFrameIdx].size = 0;
		}
		break;
	case DEC_FREE_MV_BUFFER:
		{
			Uint32 fbcCurFrameIdx = *(Uint32*)param;

			if(pDecInfo->vbMV[fbcCurFrameIdx].size > 0)
				pDecInfo->vbMV[fbcCurFrameIdx].size = 0;
		}
		break;
	case DEC_ALLOC_MV_BUFFER:
		{
			Uint32 fbcCurFrameIdx = *(Uint32*)param;
			Uint32 size;

			size          = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
			pDecInfo->vbMV[fbcCurFrameIdx].phys_addr = 0;
			pDecInfo->vbMV[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;   /* 4096 is a margin */
		}
		break;
	case DEC_ALLOC_FBC_Y_TABLE_BUFFER:
		{
			Uint32 fbcCurFrameIdx = *(Uint32*)param;
			Uint32 size;

			size        = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
			size        = WAVE5_ALIGN16(size);
			pDecInfo->vbFbcYTbl[fbcCurFrameIdx].phys_addr = 0;
			pDecInfo->vbFbcYTbl[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;
		}
		break;
	case DEC_ALLOC_FBC_C_TABLE_BUFFER:
		{
			Uint32 fbcCurFrameIdx = *(Uint32*)param;
			Uint32 size;

			size        = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
			size        = WAVE5_ALIGN16(size);
			pDecInfo->vbFbcCTbl[fbcCurFrameIdx].phys_addr = 0;
			pDecInfo->vbFbcCTbl[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;
		}
		break;
	case DEC_GET_SUPERFRAME_INFO:
		{
			VP9Superframe *superframe = (VP9Superframe*)param;

			wave5_memcpy(superframe, &pDecInfo->superframe, sizeof(VP9Superframe), 0);
		}
		break;
	default:
		return RETCODE_INVALID_COMMAND;
	}

	return ret;
}
