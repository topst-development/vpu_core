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

#include "product.h"
#include "wave5.h"
#include "wave5_core.h"
#include "vpu4k_d2_log.h"

VpuAttr g_VpuCoreAttributes[MAX_NUM_VPU_CORE];

typedef struct FrameBufInfoStruct {
	Uint32 unitSizeHorLuma;
	Uint32 sizeLuma;
	Uint32 sizeChroma;
	BOOL   fieldMap;
} FrameBufInfo;


RetCode ProductVpuGetVersion(Uint32  coreIdx, Uint32 *versionInfo, Uint32 *revision)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuGetVersion(coreIdx, versionInfo, revision);
	return ret;
}

RetCode ProductVpuInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size, Int32 cq_count)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuInit(coreIdx, workBuf, codeBuf, firmware, size, cq_count);
	return ret;
}

RetCode ProductVpuReInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size, Int32 cq_count)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuReInit(coreIdx, workBuf, codeBuf, firmware, size, cq_count);
	return ret;
}

Uint32 ProductVpuIsInit(Uint32 coreIdx)
{
	Uint32  pc = 0;

	pc = Wave5VpuIsInit(coreIdx);
	return pc;
}

Int32 ProductVpuIsBusy(Uint32 coreIdx)
{
	Int32  busy;

	busy = Wave5VpuIsBusy(coreIdx);
	return busy;
}

RetCode ProductVpuReset(Uint32 coreIdx, SWResetMode resetMode, Int32 cq_count)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuReset(coreIdx, resetMode, cq_count);
	return ret;
}

RetCode ProductVpuClearInterrupt(Uint32 coreIdx, Uint32 flags)
{
	RetCode ret;

	ret = Wave5VpuClearInterrupt(coreIdx, flags);
	return ret;
}

RetCode ProductVpuDecBuildUpOpenParam(CodecInst *pCodec, DecOpenParam *param)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuBuildUpDecParam(pCodec, param);
	return ret;
}

PhysicalAddress ProductVpuDecGetRdPtr(CodecInst *instance)
{
	PhysicalAddress retRdPtr;
	DecInfo *pDecInfo;
	RetCode ret = RETCODE_SUCCESS;

	pDecInfo = VPU_HANDLE_TO_DECINFO(instance);
	ret = Wave5VpuDecGetRdPtr(instance, &retRdPtr);
	if (ret != RETCODE_SUCCESS) {
		retRdPtr = pDecInfo->streamRdPtr;

	#ifdef ENABLE_FORCE_ESCAPE_2
		if (ret == RETCODE_CODEC_EXIT) {
			if (gWave5InitFirst_exit == 0)
				gWave5InitFirst_exit = 5;

			return RETCODE_CODEC_EXIT;
		}
	#endif
	}
	else {
		if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT)
			pDecInfo->streamRdPtr = retRdPtr;
	}

    return retRdPtr;
}

RetCode ProductCheckDecOpenParam(DecOpenParam *param)
{
	if (param == 0)
		return RETCODE_INVALID_PARAM;

	if (param->coreIdx > MAX_NUM_VPU_CORE)
		return RETCODE_INVALID_PARAM;

	if (param->bitstreamBuffer % 8)
		return RETCODE_INVALID_PARAM;

	if (param->bitstreamBufferSize < 1024)
		return RETCODE_INVALID_PARAM;

	if (param->bitstreamMode == BS_MODE_INTERRUPT) {
		if (param->bitstreamBufferSize % 1024 || param->bitstreamBufferSize < 1024)
			return RETCODE_INVALID_PARAM;
	}

	if (param->virtAxiID > 16)
		return RETCODE_INVALID_PARAM;	// Maximum number of AXI channels is 15

	if ((param->bitstreamFormat != STD_HEVC) && (param->bitstreamFormat != STD_VP9))
		return RETCODE_INVALID_PARAM;

	#if TCC_HEVC___DEC_SUPPORT == 1
	#else
	if (param->bitstreamFormat == STD_HEVC)
		return RETCODE_INVALID_PARAM;
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	#else
	if (param->bitstreamFormat == STD_VP9)
		return RETCODE_INVALID_PARAM;
	#endif

	return RETCODE_SUCCESS;
}

RetCode ProductVpuDecInitSeq(CodecInst *instance)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuDecInitSeq(instance);
	return ret;
}

RetCode ProductVpuDecFiniSeq(CodecInst *instance)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	ret = Wave5VpuDecFiniSeq(instance);
	return ret;
}

RetCode ProductVpuDecGetSeqInfo(CodecInst *instance, DecInitialInfo *info)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuDecGetSeqInfo(instance, info);
	return ret;
}

RetCode ProductVpuDecCheckCapability(CodecInst *instance)
{
	DecInfo* pDecInfo;

	pDecInfo = &instance->CodecInfo.decInfo;

	if ((pDecInfo->openParam.bitstreamFormat != STD_HEVC) && (pDecInfo->openParam.bitstreamFormat != STD_VP9))
		return RETCODE_NOT_SUPPORTED_FEATURE;

	#if TCC_HEVC___DEC_SUPPORT == 1
	#else
	if (pDecInfo->openParam.bitstreamFormat == STD_HEVC)
		return RETCODE_NOT_SUPPORTED_FEATURE;
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	#else
	if (pDecInfo->openParam.bitstreamFormat == STD_VP9)
		return RETCODE_NOT_SUPPORTED_FEATURE;
	#endif

	if ((pDecInfo->mapType != LINEAR_FRAME_MAP) && (pDecInfo->mapType != COMPRESSED_FRAME_MAP) && (pDecInfo->mapType != ARM_COMPRESSED_FRAME_MAP))
		return RETCODE_NOT_SUPPORTED_FEATURE;

	return RETCODE_SUCCESS;
}

RetCode ProductVpuDecode(CodecInst *instance, DecParam *option)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuDecode(instance, option);
	return ret;
}

RetCode ProductVpuDecGetResult(CodecInst *instance, DecOutputInfo *result)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuDecGetResult(instance, result);

	return ret;
}

RetCode ProductVpuDecFlush(CodecInst *instance, FramebufferIndex *retIndexes, Uint32 size)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5VpuDecFlush(instance, retIndexes, size);
	return ret;
}

RetCode ProductVpuDecSetBitstreamFlag(CodecInst *instance, BOOL running, Int32 size)
{
	RetCode     ret = RETCODE_NOT_FOUND_VPU_DEVICE;
	BOOL        eos;
	BOOL        checkEos;
	BOOL        explicitEnd;
	DecInfo    *pDecInfo = &instance->CodecInfo.decInfo;

	eos      = (BOOL)(size == 0);
	checkEos = (BOOL)(size > 0);
	explicitEnd = (BOOL)(size == -2);

	if (checkEos) {
		eos = (BOOL)pDecInfo->streamEndflag;
	}

	ret = Wave5VpuDecSetBitstreamFlag(instance, running, eos, explicitEnd);

	return ret;
}

RetCode ProductVpuAllocateFramebuffer(
	CodecInst* inst, FrameBuffer* fbArr, PhysicalAddress Addr, TiledMapType mapType, Int32 num,
	Int32 stride, Int32 height, FrameBufferFormat format,
	BOOL cbcrInterleave, BOOL nv21, Int32 endian,
	vpu_buffer_t* vb, Int32 gdiIndex,
	FramebufferAllocType fbType)
{
	Int32 i;
	vpu_buffer_t vbFrame;
	FrameBufInfo fbInfo;
	DecInfo *pDecInfo = &inst->CodecInfo.decInfo;
	// Variables for TILED_FRAME/FILED_MB_RASTER
	Uint32 sizeLuma = 0;
	Uint32 sizeChroma = 0;
	RetCode ret = RETCODE_SUCCESS;

	if (wave5_memset) {
		wave5_memset((void*)&vbFrame, 0x00, sizeof(vpu_buffer_t), 0);
		wave5_memset((void*)&fbInfo,  0x00, sizeof(FrameBufInfo), 0);
	}

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (inst->codecMode == W_HEVC_DEC) {
		DRAMConfig* dramConfig = NULL;

		sizeLuma   = CalcLumaSize(inst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
		sizeChroma = CalcChromaSize(inst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
	}
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (inst->codecMode == W_VP9_DEC) {
		Uint32 framebufHeight = WAVE5_ALIGN64(height);

		sizeLuma   = CalcLumaSize(inst->productId, stride, framebufHeight, format, cbcrInterleave, mapType, NULL);
		sizeChroma = CalcChromaSize(inst->productId, stride, framebufHeight, format, cbcrInterleave, mapType, NULL);
	}
	#endif

	if ((sizeLuma <= 0) || (sizeChroma <= 0)) {
		return RETCODE_INVALID_PARAM;
	}

	// Framebuffer common informations
	for (i=0; i<num; i++) {
		fbArr[i].updateFbInfo = FALSE;
		fbArr[i].myIndex        = i+gdiIndex;
		fbArr[i].stride         = stride;
		fbArr[i].height         = height;
		fbArr[i].mapType        = mapType;
		fbArr[i].format         = format;
		fbArr[i].cbcrInterleave = (mapType == COMPRESSED_FRAME_MAP ? TRUE : cbcrInterleave);
		fbArr[i].nv21           = nv21;
		fbArr[i].endian         = endian;
		fbArr[i].lumaBitDepth   = pDecInfo->initialInfo.lumaBitdepth;
		fbArr[i].chromaBitDepth = pDecInfo->initialInfo.chromaBitdepth;
	}

	switch (mapType) {
		case LINEAR_FRAME_MAP:
		case LINEAR_FIELD_MAP:
		case COMPRESSED_FRAME_MAP:
		case ARM_COMPRESSED_FRAME_MAP:
			#ifndef TEST_COMPRESSED_FBCTBL_INDEX_SET
			ret = AllocateLinearFrameBuffer(mapType, fbArr, Addr, num, sizeLuma, sizeChroma);
			#else
			ret = AllocateLinearFrameBuffer(mapType, fbArr, Addr,
							num, sizeLuma, sizeChroma,
							pDecInfo->fbcYTblSize, pDecInfo->fbcCTblSize, pDecInfo->mvColSize,
							inst->bufAlignSize, inst->bufStartAddrAlign);
			#endif
			break;
		default:
			break;
	}

	return ret;
}

RetCode ProductVpuRegisterFramebuffer(CodecInst *instance)
{
	RetCode         ret = RETCODE_FAILURE;
	FrameBuffer*    fb;
	DecInfo*        pDecInfo = &instance->CodecInfo.decInfo;
	Int32           gdiIndex = 0;

	if (pDecInfo->mapType != COMPRESSED_FRAME_MAP) {
		return RETCODE_NOT_SUPPORTED_FEATURE;
	}

	fb = pDecInfo->frameBufPool;

	gdiIndex = 0;
	if (pDecInfo->wtlEnable == TRUE) {
		if (fb[0].mapType == COMPRESSED_FRAME_MAP) {
			gdiIndex = pDecInfo->numFbsForDecoding;
		}
		ret = Wave5VpuDecRegisterFramebuffer(instance, &fb[gdiIndex], LINEAR_FRAME_MAP, pDecInfo->numFbsForWTL);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}

		gdiIndex = gdiIndex == 0 ? pDecInfo->numFbsForDecoding : 0;
	}
	else if (pDecInfo->enableAfbce == TRUE) {
		if (fb[0].mapType == COMPRESSED_FRAME_MAP) {
			gdiIndex = pDecInfo->numFbsForDecoding;
		}
		ret = Wave5VpuDecRegisterFramebuffer(instance, &fb[gdiIndex], LINEAR_FRAME_MAP, pDecInfo->numFbsForWTL);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}

		gdiIndex = gdiIndex == 0 ? pDecInfo->numFbsForDecoding: 0;
	}

	ret = Wave5VpuDecRegisterFramebuffer(instance, &fb[gdiIndex], COMPRESSED_FRAME_MAP, pDecInfo->numFbsForDecoding);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	return ret;
}

#ifdef ADD_FBREG_2
RetCode ProductVpuRegisterFramebuffer2(CodecInst *instance)
{
	RetCode         ret = RETCODE_FAILURE;
	FrameBuffer*    fb;
	DecInfo*        pDecInfo = &instance->CodecInfo->decInfo;
	Int32           gdiIndex = 0;

	if (pDecInfo->mapType != COMPRESSED_FRAME_MAP)
		return RETCODE_NOT_SUPPORTED_FEATURE;

	fb = pDecInfo->frameBufPool;

	gdiIndex = 0;
	if (pDecInfo->wtlEnable == TRUE) {
		if (fb[0].mapType == COMPRESSED_FRAME_MAP) {
			gdiIndex = pDecInfo->numFbsForDecoding;
		}
		ret = Wave5VpuDecRegisterFramebuffer2(instance, &fb[gdiIndex], LINEAR_FRAME_MAP, pDecInfo->numFbsForWTL);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}
		gdiIndex = gdiIndex == 0 ? pDecInfo->numFbsForDecoding : 0;
	}
	else if (pDecInfo->enableAfbce == TRUE) {
		if (fb[0].mapType == COMPRESSED_FRAME_MAP) {
			gdiIndex = pDecInfo->numFbsForDecoding;
		}
		ret = Wave5VpuDecRegisterFramebuffer2(instance, &fb[gdiIndex], LINEAR_FRAME_MAP, pDecInfo->numFbsForWTL);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}
		gdiIndex = gdiIndex == 0 ? pDecInfo->numFbsForDecoding: 0;
	}

	ret = Wave5VpuDecRegisterFramebuffer2(instance, &fb[gdiIndex], COMPRESSED_FRAME_MAP, pDecInfo->numFbsForDecoding);
	if (ret != RETCODE_SUCCESS) {
		return ret;
	}

	return ret;
}
#endif

Int32 ProductCalculateFrameBufSize(Int32 productId, Int32 stride, Int32 height, TiledMapType mapType, FrameBufferFormat format, BOOL interleave, DRAMConfig *pDramCfg)
{
	Int32 size_dpb_lum, size_dpb_chr, size_dpb_all;

	size_dpb_lum = CalcLumaSize(productId, stride, height, format, interleave, mapType, pDramCfg);
	size_dpb_chr = CalcChromaSize(productId, stride, height, format, interleave, mapType, pDramCfg);
	size_dpb_all = size_dpb_lum + size_dpb_chr * 2;
	return size_dpb_all;
}

Int32 ProductCalculateAuxBufferSize(AUX_BUF_TYPE type, Uint32 codStd, Int32 width, Int32 height)
{
	Int32 size = 0;

	switch (type) {
		case AUX_BUF_TYPE_MVCOL:
		#if TCC_HEVC___DEC_SUPPORT == 1
			if (codStd == STD_HEVC) {
				size = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(width, height);
			}
		#endif
		#if TCC_VP9____DEC_SUPPORT == 1
			if (codStd == STD_VP9) {
				size = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(width, height);
			}
		#endif
			break;
		case AUX_BUF_TYPE_FBC_Y_OFFSET:
			size  = WAVE5_FBC_LUMA_TABLE_SIZE(width, height);
			break;
		case AUX_BUF_TYPE_FBC_C_OFFSET:
			size  = WAVE5_FBC_CHROMA_TABLE_SIZE(width, height);
			break;
		default:
			break;
	}
	return size;
}

RetCode ProductClrDispFlag(CodecInst *instance, Uint32 index)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5DecClrDispFlag(instance, index);
	return ret;
}

RetCode ProductSetDispFlag(CodecInst *instance, Uint32 index)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5DecSetDispFlag(instance, index);
	return ret;
}

RetCode ProductClrDispFlagEx(CodecInst *instance, Uint32 dispFlag)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5DecClrDispFlagEx(instance, dispFlag);
	return ret;
}

RetCode ProductSetDispFlagEx(CodecInst *instance, Uint32 dispFlag)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = Wave5DecSetDispFlagEx(instance, dispFlag);
	return ret;
}
