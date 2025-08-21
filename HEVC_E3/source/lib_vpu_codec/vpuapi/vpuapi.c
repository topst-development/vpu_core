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

#include "vpuapi.h"
#include "wave420l_pre_define.h"

#if defined(CONFIG_HEVC_E3_2)
#include "wave420l_core_2.h"
#else
#include "wave420l_core.h"
#endif

#include "vpuapifunc.h"
#include "product.h"
#include "common_regdefine.h"

#ifdef BIT_CODE_FILE_PATH
#include BIT_CODE_FILE_PATH
#endif

#define INVALID_CORE_INDEX_RETURN_ERROR(_coreIdx)  \
	if (_coreIdx >= MAX_NUM_VPU_CORE) \
	return -1;

Uint32 __VPU_BUSY_TIMEOUT = VPU_BUSY_CHECK_TIMEOUT;

void wave420l_VpuWriteReg(int coreIdx, unsigned int ADDR, unsigned int DATA)
{
	if (gHevcEncVirtualRegBaseAddr == 0) {
		return;
	}

	if (wave420l_write_reg == NULL) {
		*((volatile codec_addr_t *)(gHevcEncVirtualRegBaseAddr+ADDR)) = (unsigned int)(DATA);
	} else {
		wave420l_write_reg( (void*)gHevcEncVirtualRegBaseAddr, ADDR, DATA);
	}
	return;
}

unsigned int wave420l_VpuReadReg(int coreIdx, unsigned int ADDR)
{
	if (gHevcEncVirtualRegBaseAddr == 0)
		return 0;

	if (wave420l_read_reg == NULL) {
		return *(volatile codec_addr_t *)(gHevcEncVirtualRegBaseAddr+ADDR);
	} else {
		return wave420l_read_reg((void *)gHevcEncVirtualRegBaseAddr, ADDR);
	}
}

Int32 wave420l_VPU_IsBusy(Uint32 coreIdx)
{
	Uint32 ret = 0;

	INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

	ret = wave420l_ProductVpuIsBusy(coreIdx);
	return ret != 0;
}

Int32 wave420l_VPU_IsInit(Uint32 coreIdx)
{
	Int32 pc;

	INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

	pc = wave420l_ProductVpuIsInit(coreIdx);
	return pc;
}

Int32 wave420l_VPU_WaitInterrupt(Uint32 coreIdx, int timeout)
{
	Int32 ret;
	CodecInst* instance;

	INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);
	if ((instance=wave420l_GetPendingInst(coreIdx)) != NULL) {
		ret = wave420l_ProductVpuWaitInterrupt(instance, timeout);
	} else {
		ret = -1;
	}

	return ret;
}

Int32 wave420l_VPU_WaitInterruptEx(VpuHandle handle, int timeout)
{
	Int32 ret;
	CodecInst *pCodecInst;

	pCodecInst  = handle;

	INVALID_CORE_INDEX_RETURN_ERROR(pCodecInst->coreIdx);
	ret = wave420l_ProductVpuWaitInterrupt(pCodecInst, timeout);
	return ret;
}

void wave420l_VPU_ClearInterrupt(Uint32 coreIdx)
{
	/* clear all interrupt flags */
	wave420l_ProductVpuClearInterrupt(coreIdx, 0xffff);
}

void wave420l_VPU_ClearInterruptEx(VpuHandle handle, Int32 intrFlag)
{
	CodecInst *pCodecInst;

	pCodecInst  = handle;

	wave420l_ProductVpuClearInterrupt(pCodecInst->coreIdx, intrFlag);
}

int wave420l_VPU_GetMvColBufSize(CodStd codStd, int width, int height, int num)
{
	int size_mvcolbuf = wave420l_ProductCalculateAuxBufferSize(AUX_BUF_TYPE_MVCOL, codStd, width, height);

	size_mvcolbuf *= num;
	return size_mvcolbuf;
}

RetCode wave420l_VPU_GetFBCOffsetTableSize(CodStd codStd, int width, int height, int *ysize, int *csize)
{
	if ((ysize == NULL) || (csize == NULL)) {
		return RETCODE_INVALID_PARAM;
	}

	*ysize = wave420l_ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_Y_OFFSET, codStd, width, height);
	*csize = wave420l_ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_C_OFFSET, codStd, width, height);
	return RETCODE_SUCCESS;
}

int wave420l_VPU_GetFrameBufSize(int coreIdx, int stride, int height, int mapType, int format, int interleave, DRAMConfig *pDramCfg)
{
	int productId = PRODUCT_ID_420L;
	UNREFERENCED_PARAMETER(interleave);	/*!<< for backward compatiblity */

	if (coreIdx < 0 || coreIdx >= MAX_NUM_VPU_CORE) {
		return -1;
	}

	return wave420l_ProductCalculateFrameBufSize(productId, stride, height, (TiledMapType)mapType, (FrameBufferFormat)format, (BOOL)interleave, pDramCfg);
}

static RetCode wave420l_InitializeVPU(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, const Uint16* code, Uint32 size)
{
	RetCode     ret;
	int i;
	CodecInst *pCodecInst = &gsCodecInstPoolWave420l[0];

	if (wave420l_VPU_IsInit(coreIdx) != 0) {
		ret = wave420l_ProductVpuReInit(0, workBuf, codeBuf, (void *)code, size);
		if (ret != RETCODE_SUCCESS)
			return ret; //RETCODE_CALLED_BEFORE;

		//reset codec instance memory
		if (pCodecInst != 0) {
			if (wave420l_memset) {
				wave420l_memset(pCodecInst, 0, sizeof(CodecInst)*gHevcEncMaxInstanceNum , 0);
			}
		}

		for (i = 0; i < gHevcEncMaxInstanceNum ; ++i, ++pCodecInst) {
			pCodecInst->instIndex = i;
			pCodecInst->inUse = 0;
		}
		return RETCODE_SUCCESS;
	}

	//reset codec instance memory
	if (pCodecInst != 0) {
		if (wave420l_memset) {
			wave420l_memset(pCodecInst, 0, sizeof(CodecInst)*gHevcEncMaxInstanceNum , 0);
		}
	}

	for (i = 0; i < gHevcEncMaxInstanceNum ; ++i, ++pCodecInst) {
		pCodecInst->instIndex = i;
		pCodecInst->inUse = 0;
	}

	ret = wave420l_ProductVpuReset(coreIdx, SW_RESET_ON_BOOT);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	ret = wave420l_ProductVpuInit(coreIdx, workBuf, codeBuf, (void *)code, size);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	return RETCODE_SUCCESS;
}

RetCode wave420l_VPU_InitWithBitcode(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, Uint32 size)
{
	return wave420l_InitializeVPU(coreIdx, workBuf, codeBuf, 0, size);
}

RetCode wave420l_VPU_GetVersionInfo(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision, Uint32 *productId)
{
	RetCode  ret;

	if (coreIdx >= MAX_NUM_VPU_CORE) {
		return RETCODE_INVALID_PARAM;
	}

	if (wave420l_ProductVpuIsInit(coreIdx) == 0) {
		return RETCODE_NOT_INITIALIZED;
	}

	if (wave420l_GetPendingInst(coreIdx)) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	if (productId != NULL) {
		*productId = PRODUCT_ID_420L;
	}

	ret = wave420l_ProductVpuGetVersion(coreIdx, versionInfo, revision);
	return ret;
}

RetCode wave420l_VPU_HWReset(Uint32 coreIdx)
{
	if (wave420l_GetPendingInst(coreIdx)) {
		wave420l_SetPendingInst(coreIdx, 0);
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
RetCode wave420l_VPU_SWReset(Uint32 coreIdx, SWResetMode resetMode, void *pendingInst)
{
	CodecInst *pCodecInst = (CodecInst *)pendingInst;
	RetCode ret = RETCODE_SUCCESS;

	if (pCodecInst) {
		wave420l_SetPendingInst (pCodecInst->coreIdx, 0);
		if (pCodecInst->loggingEnable) {
			wave420l_vdi_log(pCodecInst->coreIdx, 0x10000, 1);
		}
	}

	ret = wave420l_ProductVpuReset(coreIdx, resetMode);

	if (pCodecInst) {
		if (pCodecInst->loggingEnable) {
			wave420l_vdi_log(pCodecInst->coreIdx, 0x10000, 0);
		}
	}

	return ret;
}

RetCode wave420l_VPU_EncOpen(EncHandle *pHandle, EncOpenParam *pop)
{
	CodecInst*  pCodecInst;
	EncInfo*    pEncInfo;
	RetCode     ret;

	if ((ret=wave420l_ProductCheckEncOpenParam(pop)) != RETCODE_SUCCESS)
		return ret;

	if (wave420l_VPU_IsInit(pop->coreIdx) == 0)
		return RETCODE_NOT_INITIALIZED;

	ret = wave420l_GetCodecInstance(pop->coreIdx, &pCodecInst);
	if (ret == RETCODE_FAILURE) {
		*pHandle = 0;
		return RETCODE_FAILURE;
	}

	pCodecInst->isDecoder = FALSE;
	*pHandle = pCodecInst;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	wave420l_memset(pEncInfo, 0x00, sizeof(EncInfo), 0);
	pEncInfo->openParam = *pop;

	if ((ret=wave420l_ProductVpuEncBuildUpOpenParam(pCodecInst, pop)) != RETCODE_SUCCESS)
		*pHandle = 0;

	return ret;
}

RetCode wave420l_VPU_EncClose(EncHandle handle)
{
	CodecInst*  pCodecInst;
	EncInfo*    pEncInfo;
	RetCode     ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	if (pEncInfo->initialInfoObtained) {
		VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
		VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);

		if ((ret=wave420l_ProductVpuEncFiniSeq(pCodecInst)) != RETCODE_SUCCESS) {
			if (pCodecInst->loggingEnable)
			wave420l_vdi_log(pCodecInst->coreIdx, ENC_SEQ_END, 0);
		}

		if (pCodecInst->loggingEnable)
			wave420l_vdi_log(pCodecInst->coreIdx, ENC_SEQ_END, 0);
		pEncInfo->streamWrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
	}

	wave420l_FreeCodecInstance(pCodecInst);

	return ret;
}

RetCode wave420l_VPU_EncGetInitialInfo(EncHandle handle, EncInitialInfo *info)
{
	CodecInst*  pCodecInst;
	EncInfo*    pEncInfo;
	RetCode     ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if (info == 0)
		return RETCODE_INVALID_PARAM;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	if (wave420l_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	if ((ret=wave420l_ProductVpuEncSetup(pCodecInst)) != RETCODE_SUCCESS)
		return ret;

	info->minFrameBufferCount   = pEncInfo->initialInfo.minFrameBufferCount;	//2
	info->minSrcFrameCount      = pEncInfo->initialInfo.minSrcFrameCount;	//1

	pEncInfo->initialInfo         = *info;
	pEncInfo->initialInfoObtained = TRUE;

	return RETCODE_SUCCESS;
}

RetCode wave420l_VPU_EncRegisterFrameBuffer(EncHandle handle, FrameBuffer *bufArray, int num, int stride, int height, int mapType)
{
	CodecInst*      pCodecInst;
	EncInfo*        pEncInfo;
	Int32           i;
	RetCode         ret;
	EncOpenParam*   openParam;
	FrameBuffer*    fb;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;
	openParam = &pEncInfo->openParam;

	if (pEncInfo->stride)
		return RETCODE_CALLED_BEFORE;

	if (!pEncInfo->initialInfoObtained)
		return RETCODE_WRONG_CALL_SEQUENCE;

	if (num < pEncInfo->initialInfo.minFrameBufferCount)
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

	if (stride == 0 || (stride % 8 != 0) || stride < 0)
		return RETCODE_INVALID_STRIDE;

	if (height == 0 || height < 0)
		return RETCODE_INVALID_PARAM;

	if (stride % 32 != 0)
		return RETCODE_INVALID_STRIDE;

	if (wave420l_GetPendingInst(pCodecInst->coreIdx))
		return RETCODE_FRAME_NOT_COMPLETE;

	pEncInfo->numFrameBuffers   = num;
	pEncInfo->stride            = stride;
	pEncInfo->frameBufferHeight = height;
	pEncInfo->mapType           = mapType;

	if (bufArray) {
		for(i=0; i<num; i++)
			pEncInfo->frameBufPool[i] = bufArray[i];
	}

	if (pEncInfo->frameAllocExt == FALSE) {
		fb = pEncInfo->frameBufPool;
		if (bufArray) {
			if (bufArray[0].bufCb == (Uint32)-1 && bufArray[0].bufCr == (Uint32)-1) {
				Uint32 size;
				pEncInfo->frameAllocExt = TRUE;
				size = wave420l_ProductCalculateFrameBufSize(pCodecInst->productId, stride, height,
										(TiledMapType)mapType, (FrameBufferFormat)openParam->srcFormat,
										(BOOL)openParam->cbcrInterleave, NULL);
				if (mapType == LINEAR_FRAME_MAP) {
					pEncInfo->vbFrame.phys_addr = bufArray[0].bufY;
					pEncInfo->vbFrame.size      = size * num;
				}
			}
		}
		ret = wave420l_ProductVpuAllocateFramebuffer(
				pCodecInst, fb,
				(TiledMapType)mapType, num, stride, height,
				(FrameBufferFormat)openParam->srcFormat,
				openParam->cbcrInterleave,
				FALSE,
				openParam->frameEndian, &pEncInfo->vbFrame, 0,
				FB_TYPE_CODEC);

		if (ret != RETCODE_SUCCESS) {
			wave420l_SetPendingInst(pCodecInst->coreIdx, 0);

			return ret;
		}
	}

	ret = wave420l_ProductVpuRegisterFramebuffer(pCodecInst);

	wave420l_SetPendingInst(pCodecInst->coreIdx, 0);

	return ret;
}

RetCode wave420l_VPU_EncGetBitstreamBuffer( EncHandle handle, PhysicalAddress *prdPrt, PhysicalAddress *pwrPtr, int *size)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	Uint32 room;
	RetCode ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	if ( prdPrt == 0 || pwrPtr == 0 || size == 0)
		return RETCODE_INVALID_PARAM;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	rdPtr = pEncInfo->streamRdPtr;

	if (wave420l_GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
		wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
	else
		wrPtr = pEncInfo->streamWrPtr;

	if( pEncInfo->lineBufIntEn == 1) {
		if (wrPtr >= rdPtr)
			room = wrPtr - rdPtr;
		else
			room = (pEncInfo->streamBufEndAddr - rdPtr) + (wrPtr - pEncInfo->streamBufStartAddr);
	}
	else {
		if(wrPtr >= rdPtr)
			room = wrPtr - rdPtr;
		else
			return RETCODE_INVALID_PARAM;
	}

	*prdPrt = rdPtr;
	*pwrPtr = wrPtr;
	*size = room;

	return RETCODE_SUCCESS;
}

RetCode wave420l_VPU_EncUpdateBitstreamBuffer(EncHandle handle, int size)
{
	CodecInst *pCodecInst;
	EncInfo *pEncInfo;
	PhysicalAddress wrPtr;
	PhysicalAddress rdPtr;
	RetCode ret;
	int room = 0;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS)
		return ret;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	rdPtr = pEncInfo->streamRdPtr;

	if (wave420l_GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
		wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
	else
		wrPtr = pEncInfo->streamWrPtr;

	if (rdPtr < wrPtr) {
		if (rdPtr + size  > wrPtr)
			return RETCODE_INVALID_PARAM;
	}

	if (pEncInfo->lineBufIntEn == TRUE) {
		rdPtr += size;
		if (rdPtr > pEncInfo->streamBufEndAddr) {
			if (pEncInfo->lineBufIntEn == TRUE) {
				return RETCODE_INVALID_PARAM;
			}

			room = rdPtr - pEncInfo->streamBufEndAddr;
			rdPtr = pEncInfo->streamBufStartAddr;
			rdPtr += room;
		}

		if (rdPtr == pEncInfo->streamBufEndAddr) {
			rdPtr = pEncInfo->streamBufStartAddr;
		}
	}
	else {
		rdPtr = pEncInfo->streamBufStartAddr;
	}

	pEncInfo->streamRdPtr = rdPtr;
	pEncInfo->streamWrPtr = wrPtr;

	if (wave420l_GetPendingInst(pCodecInst->coreIdx) == pCodecInst) {
		VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr, rdPtr);
	}

	if (pEncInfo->lineBufIntEn == TRUE) {
		pEncInfo->streamRdPtr = pEncInfo->streamBufStartAddr;
	}

	return RETCODE_SUCCESS;
}

RetCode wave420l_VPU_EncStartOneFrame(EncHandle handle, EncParam *param)
{
	CodecInst*          pCodecInst;
	EncInfo*            pEncInfo;
	RetCode             ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	pCodecInst = handle;
	pEncInfo   = &pCodecInst->CodecInfo->encInfo;

	if (pEncInfo->stride == 0) {
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	ret = wave420l_CheckEncParam(handle, param);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	if (wave420l_GetPendingInst(pCodecInst->coreIdx)) {
		return RETCODE_FRAME_NOT_COMPLETE;
	}

	ret = wave420l_ProductVpuEncode(pCodecInst, param);

	wave420l_SetPendingInst(pCodecInst->coreIdx, pCodecInst);

	return ret;
}

RetCode wave420l_VPU_EncGetOutputInfo(EncHandle handle, EncOutputInfo *info)
{
	CodecInst*  pCodecInst;
	RetCode     ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	if (info == 0) {
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	if (pCodecInst != wave420l_GetPendingInst (pCodecInst->coreIdx)) {
		wave420l_SetPendingInst(pCodecInst->coreIdx, 0);
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	ret = wave420l_ProductVpuEncGetResult(pCodecInst, info);

	wave420l_SetPendingInst(pCodecInst->coreIdx, 0);

	return ret;
}

RetCode wave420l_VPU_EncGiveCommand(EncHandle handle, CodecCommand cmd, void *param)
{
	CodecInst*  pCodecInst;
	EncInfo*    pEncInfo;
	RetCode     ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;
	switch (cmd)
	{
	case ENC_PUT_VIDEO_HEADER:
		{
			EncHeaderParam *encHeaderParam;

			if (param == 0) {
				return RETCODE_INVALID_PARAM;
			}
			encHeaderParam = (EncHeaderParam *)param;
			if (pCodecInst->codecMode == HEVC_ENC) {
				if (!( CODEOPT_ENC_VPS<=encHeaderParam->headerType && encHeaderParam->headerType <= (CODEOPT_ENC_VPS|CODEOPT_ENC_SPS|CODEOPT_ENC_PPS))) {
					return RETCODE_INVALID_PARAM;
				}
				//if (pEncInfo->ringBufferEnable == 0 )
				{
					if (encHeaderParam->buf % 16 || encHeaderParam->size == 0)
						return RETCODE_INVALID_PARAM;
				}
				if (encHeaderParam->headerType & CODEOPT_ENC_VCL)   // ENC_PUT_VIDEO_HEADER encode only non-vcl header.
					return RETCODE_INVALID_PARAM;

			}
			else
				return RETCODE_INVALID_PARAM;

			//if (pEncInfo->ringBufferEnable == 0 )
			{
				if (encHeaderParam->buf % 8 || encHeaderParam->size == 0) {
					return RETCODE_INVALID_PARAM;
				}
			}

			return wave420l_Wave4VpuEncGetHeader(handle, encHeaderParam);
		}
	case SET_SEC_AXI:
		{
			SecAxiUse secAxiUse;

			if (param == 0) {
				return RETCODE_INVALID_PARAM;
			}
			secAxiUse = *(SecAxiUse *)param;
			if (handle->productId == PRODUCT_ID_420L) {
				pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable = secAxiUse.u.wave4.useEncRdoEnable;
				pEncInfo->secAxiInfo.u.wave4.useEncLfEnable  = secAxiUse.u.wave4.useEncLfEnable;
			}
			else { // coda9 or coda7q or ...
				return RETCODE_INVALID_PARAM;
			}
		}
		break;
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
	case ENC_SET_PARA_CHANGE:
		{
			EncChangeParam *option = (EncChangeParam *)param;

			return wave420l_Wave4VpuEncParaChange(handle, option);
		}
		break;
	case ENC_SET_SEI_NAL_DATA:
		{
			HevcSEIDataEnc *option = (HevcSEIDataEnc *)param;
			pEncInfo->prefixSeiNalEnable    = option->prefixSeiNalEnable;
			pEncInfo->prefixSeiDataSize     = option->prefixSeiDataSize;
			pEncInfo->prefixSeiDataEncOrder = option->prefixSeiDataEncOrder;
			pEncInfo->prefixSeiNalAddr      = option->prefixSeiNalAddr;

			pEncInfo->suffixSeiNalEnable    = option->suffixSeiNalEnable;
			pEncInfo->suffixSeiDataSize     = option->suffixSeiDataSize;
			pEncInfo->suffixSeiDataEncOrder = option->suffixSeiDataEncOrder;
			pEncInfo->suffixSeiNalAddr      = option->suffixSeiNalAddr;
		}
		break;
	default:
		return RETCODE_INVALID_COMMAND;
	}
	return RETCODE_SUCCESS;
}


RetCode wave420l_VPU_EncCompleteSeqInit(EncHandle handle, EncInitialInfo *info)
{
	CodecInst*  pCodecInst;
	EncInfo*    pEncInfo;
	RetCode     ret;

	ret = wave420l_CheckEncInstanceValidity(handle);
		if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	if (info == 0) {
		return RETCODE_INVALID_PARAM;
	}

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo->encInfo;

	if (pCodecInst != wave420l_GetPendingInst(pCodecInst->coreIdx)) {
		wave420l_SetPendingInst(pCodecInst->coreIdx, 0);
		return RETCODE_WRONG_CALL_SEQUENCE;
	}

	pEncInfo->initialInfoObtained = 1;

	//info->rdPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr);
	//info->wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
	//pEncInfo->prevFrameEndPos = info->rdPtr;

	pEncInfo->initialInfo = *info;

	wave420l_SetPendingInst(pCodecInst->coreIdx, NULL);

	return ret;
}
