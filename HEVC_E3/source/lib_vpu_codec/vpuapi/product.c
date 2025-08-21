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

#include "wave420l_pre_define.h"
#if defined(CONFIG_HEVC_E3_2)
#include "wave420l_core_2.h"
#else
#include "wave420l_core.h"
#endif

#include "product.h"
#include "common.h"
#include "wave4.h"

typedef struct FrameBufInfoStruct {
	Uint32 unitSizeHorLuma;
	Uint32 sizeLuma;
	Uint32 sizeChroma;
	BOOL   fieldMap;
} FrameBufInfo;

RetCode wave420l_ProductVpuGetVersion(
		Uint32  coreIdx,
		Uint32* versionInfo,
		Uint32* revision
		)
{
	RetCode ret = 0;
	ret = wave420l_Wave4VpuGetVersion(coreIdx, versionInfo, revision);

	return ret;
}

RetCode wave420l_ProductVpuInit(Uint32 coreIdx,
		PhysicalAddress workBuf,
		PhysicalAddress codeBuf,
		void* firmware,
		Uint32 size
		)
{
	RetCode ret = WAVE420L_RETCODE_SUCCESS;

	ret = wave420l_Wave4VpuInit(coreIdx, workBuf, codeBuf, firmware, size);
	return ret;
}

RetCode wave420l_ProductVpuReInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void* firmware, Uint32 size)
{
	RetCode ret = WAVE420L_RETCODE_SUCCESS;

	ret = wave420l_Wave4VpuReInit(coreIdx, workBuf, codeBuf, firmware, size);
	return ret;
}

Uint32 wave420l_ProductVpuIsInit(Uint32 coreIdx)
{
	Uint32 pc = 0;

	pc = wave420l_Wave4VpuIsInit(coreIdx);
	return pc;
}

Int32 wave420l_ProductVpuIsBusy(Uint32 coreIdx)
{
	Int32 busy;

	busy = wave420l_Wave4VpuIsBusy(coreIdx);
	return busy;
}

Int32 wave420l_ProductVpuWaitInterrupt(CodecInst *instance, Int32 timeout)
{
	int flag = -1;

	flag = wave420l_Wave4VpuWaitInterrupt(instance, timeout);
	return flag;
}

RetCode wave420l_ProductVpuReset(Uint32 coreIdx, SWResetMode resetMode)
{
	RetCode ret = WAVE420L_RETCODE_SUCCESS;

	ret = wave420l_Wave4VpuReset(coreIdx, resetMode);
	return ret;
}

RetCode wave420l_ProductVpuClearInterrupt(Uint32 coreIdx, Uint32 flags)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	ret = wave420l_Wave4VpuClearInterrupt(coreIdx, 0xffff);
	return ret;
}

RetCode wave420l_ProductVpuEncBuildUpOpenParam(CodecInst* pCodec, EncOpenParam* param)
{
	RetCode ret = RETCODE_NOT_SUPPORTED_FEATURE;

	ret = wave420l_Wave4VpuBuildUpEncParam(pCodec, param);
	return ret;
}


/************************************************************************/
/* Decoder & Encoder                                                    */
/************************************************************************/

/**
 * \param   stride          stride of framebuffer in pixel.
 */

RetCode wave420l_ProductVpuAllocateFramebuffer(
		CodecInst* inst, FrameBuffer* fbArr, TiledMapType mapType, Int32 num,
		Int32 stride, Int32 height, FrameBufferFormat format,
		BOOL cbcrInterleave, BOOL nv21, Int32 endian,
		vpu_buffer_t* vb, Int32 gdiIndex,
		FramebufferAllocType fbType)
{
	Int32           i;
	vpu_buffer_t    vbFrame;
	FrameBufInfo    fbInfo;
	EncInfo*        pEncInfo = &inst->CodecInfo->encInfo;
	// Variables for TILED_FRAME/FILED_MB_RASTER
	Uint32          sizeLuma;
	Uint32          sizeChroma;
	RetCode         ret           = WAVE420L_RETCODE_SUCCESS;

	wave420l_memset((void*)&vbFrame, 0x00, sizeof(vpu_buffer_t), 0);
	wave420l_memset((void*)&fbInfo,  0x00, sizeof(FrameBufInfo), 0);

	{
		DRAMConfig* dramConfig = NULL;

		sizeLuma   = wave420l_CalcLumaSize(inst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
		sizeChroma = wave420l_CalcChromaSize(inst->productId, stride, height, format, cbcrInterleave, mapType, dramConfig);
	}

	// Framebuffer common informations
	for (i=0; i<num; i++) {
		if (fbArr[i].updateFbInfo == TRUE ) {
			fbArr[i].updateFbInfo = FALSE;
			fbArr[i].myIndex        = i+gdiIndex;
			fbArr[i].stride         = stride;
			fbArr[i].height         = height;
			fbArr[i].mapType        = mapType;
			fbArr[i].format         = format;
			fbArr[i].cbcrInterleave = (mapType == COMPRESSED_FRAME_MAP ? TRUE : cbcrInterleave);
			fbArr[i].nv21           = nv21;
			fbArr[i].endian         = endian;
			//fbArr[i].sourceLBurstEn = FALSE; ////only 0 (CODA9 encoder only).
			//if(inst->codecMode == HEVC_ENC)
			{
				if (gdiIndex != 0) {        // FB_TYPE_PPU
					fbArr[i].srcBufState = SRC_BUFFER_ALLOCATED;
				}
				fbArr[i].lumaBitDepth   = pEncInfo->openParam.EncStdParam.hevcParam.internalBitDepth;
				fbArr[i].chromaBitDepth = pEncInfo->openParam.EncStdParam.hevcParam.internalBitDepth;
			}
		}
	}

	switch (mapType) {
	case LINEAR_FRAME_MAP:
	case COMPRESSED_FRAME_MAP:
		ret = wave420l_AllocateLinearFrameBuffer(mapType, fbArr, num, sizeLuma, sizeChroma);
		break;

	default:
		/* Tiled map */
		break;
	}

	return ret;
}

RetCode wave420l_ProductVpuRegisterFramebuffer(CodecInst* instance)
{
	RetCode         ret = WAVE420L_RETCODE_FAILURE;
	FrameBuffer*    fb;
	Int32           gdiIndex = 0;
	EncInfo*        pEncInfo = &instance->CodecInfo->encInfo;

	if (pEncInfo->mapType != COMPRESSED_FRAME_MAP)
		return RETCODE_NOT_SUPPORTED_FEATURE;

	fb = pEncInfo->frameBufPool;

	gdiIndex = 0;
	ret = wave420l_Wave4VpuEncRegisterFramebuffer(instance, &fb[gdiIndex], COMPRESSED_FRAME_MAP, pEncInfo->numFrameBuffers);
	if (ret != WAVE420L_RETCODE_SUCCESS) {
		return ret;
	}

	return ret;
}

Int32 wave420l_ProductCalculateFrameBufSize(Int32 productId, Int32 stride, Int32 height, TiledMapType mapType, FrameBufferFormat format, BOOL interleave, DRAMConfig* pDramCfg)
{
	Int32 size_dpb_lum, size_dpb_chr, size_dpb_all;

	size_dpb_lum = wave420l_CalcLumaSize(productId, stride, height, format, interleave, mapType, NULL);
	size_dpb_chr = wave420l_CalcChromaSize(productId, stride, height, format, interleave, mapType, NULL);
	size_dpb_all = size_dpb_lum + size_dpb_chr*2;

	return size_dpb_all;
}

Int32 wave420l_ProductCalculateAuxBufferSize(AUX_BUF_TYPE type, CodStd codStd, Int32 width, Int32 height)
{
	Int32 size = 0;

	switch (type) {
	case AUX_BUF_TYPE_MVCOL:
			size = WAVE4_DEC_HEVC_MVCOL_BUF_SIZE(width, height);
		break;
	case AUX_BUF_TYPE_FBC_Y_OFFSET:
		size  = WAVE4_FBC_LUMA_TABLE_SIZE(width, height);
		break;
	case AUX_BUF_TYPE_FBC_C_OFFSET:
		size  = WAVE4_FBC_CHROMA_TABLE_SIZE(width, height);
		break;
	}

	return size;
}


/************************************************************************/
/* ENCODER                                                              */
/************************************************************************/
RetCode wave420l_ProductCheckEncOpenParam(EncOpenParam* pop)
{
	Int32       picWidth;
	Int32       picHeight;

	if (pop == 0)
		return WAVE420L_RETCODE_INVALID_PARAM;

	if (pop->coreIdx > MAX_NUM_VPU_CORE)
		return WAVE420L_RETCODE_INVALID_PARAM;

	picWidth  = pop->picWidth;
	picHeight = pop->picHeight;

	if (pop->frameRateInfo == 0) {
		return WAVE420L_RETCODE_INVALID_PARAM;
	}

	if (pop->bitRate > 700000000 || pop->bitRate < 0) {
		return WAVE420L_RETCODE_INVALID_PARAM;
	}

	if (pop->bitRate !=0 && pop->initialDelay > 32767) {
		return WAVE420L_RETCODE_INVALID_PARAM;
	}

	//if (pop->MEUseZeroPmv != 0 && pop->MEUseZeroPmv != 1)
	//    return WAVE420L_RETCODE_INVALID_PARAM;

	//if (pop->intraCostWeight < 0 || pop->intraCostWeight >= 65535)
	//    return WAVE420L_RETCODE_INVALID_PARAM;

	//if (pop->bitstreamFormat == WAVE420L_STD_HEVC)    //STD_HEVC=12
	{
		EncHevcParam* param     = &pop->EncStdParam.hevcParam;

		if (picWidth < W4_MIN_ENC_PIC_WIDTH || picWidth > W4_MAX_ENC_PIC_WIDTH) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (picHeight < W4_MIN_ENC_PIC_HEIGHT || picHeight > W4_MAX_ENC_PIC_HEIGHT) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->profile != HEVC_PROFILE_MAIN /*&& param->profile != HEVC_PROFILE_MAIN10*/) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->internalBitDepth != 8 /*&& param->internalBitDepth != 10*/) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->internalBitDepth > 8 && param->profile == HEVC_PROFILE_MAIN) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->chromaFormatIdc < 0 || param->chromaFormatIdc > 3) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->decodingRefreshType < 0 || param->decodingRefreshType > 2) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->gopParam.useDeriveLambdaWeight != 1 && param->gopParam.useDeriveLambdaWeight != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->gopPresetIdx == PRESET_IDX_CUSTOM_GOP) {
			if ( param->gopParam.customGopSize < 1 || param->gopParam.customGopSize > MAX_GOP_NUM) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}
		}

		if (param->constIntraPredFlag != 1 && param->constIntraPredFlag != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		// wave420
		if (param->intraRefreshMode < 0 || param->intraRefreshMode > 3) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->independSliceMode < 0 || param->independSliceMode > 1) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->independSliceMode != 0) {
			if (param->dependSliceMode < 0 || param->dependSliceMode > 2) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}
		}

		if (param->useRecommendEncParam < 0 || param->useRecommendEncParam > 3) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->useRecommendEncParam == 0 || param->useRecommendEncParam == 2 || param->useRecommendEncParam == 3) {

			if (param->intraInInterSliceEnable != 1 && param->intraInInterSliceEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->intraNxNEnable != 1 && param->intraNxNEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->skipIntraTrans != 1 && param->skipIntraTrans != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->scalingListEnable != 1 && param->scalingListEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->tmvpEnable != 1 && param->tmvpEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->wppEnable != 1 && param->wppEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->useRecommendEncParam != 3) {     // in FAST mode (recommendEncParam==3), maxNumMerge value will be decided in FW
				if (param->maxNumMerge < 0 || param->maxNumMerge > 3) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}
			}

			if (param->disableDeblk != 1 && param->disableDeblk != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->disableDeblk == 0 || param->saoEnable != 0) {
				if (param->lfCrossSliceBoundaryEnable != 1 && param->lfCrossSliceBoundaryEnable != 0) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}
			}

			if (param->disableDeblk == 0) {
				if (param->betaOffsetDiv2 < -6 || param->betaOffsetDiv2 > 6) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}

				if (param->tcOffsetDiv2 < -6 || param->tcOffsetDiv2 > 6) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}
			}
		}

		if (param->losslessEnable != 1 && param->losslessEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->intraQP < 0 || param->intraQP > 51) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (pop->rcEnable != 1 && pop->rcEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (pop->rcEnable == 1) {
			if (param->minQp < 0 || param->minQp > 51) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->maxQp < 0 || param->maxQp > 51) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->initialRcQp < 0 || param->initialRcQp > 63) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if ((param->initialRcQp < 52) && ((param->initialRcQp > param->maxQp) || (param->initialRcQp < param->minQp))) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->intraQpOffset < -10 || param->intraQpOffset > 10) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->cuLevelRCEnable != 1 && param->cuLevelRCEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->hvsQPEnable != 1 && param->hvsQPEnable != 0) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->hvsQPEnable) {
				if (param->maxDeltaQp < 0 || param->maxDeltaQp > 12) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}

				if (param->hvsQpScaleEnable != 1 && param->hvsQpScaleEnable != 0) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}

				if (param->hvsQpScaleEnable == 1) {
					if (param->hvsQpScale < 0 || param->hvsQpScale > 4)
						return WAVE420L_RETCODE_INVALID_PARAM;
				}
			}

			if (param->ctuOptParam.roiEnable) {
				if (param->ctuOptParam.roiDeltaQp < 1 || param->ctuOptParam.roiDeltaQp > 51) {
					return WAVE420L_RETCODE_INVALID_PARAM;
				}
			}

			if (param->ctuOptParam.roiEnable && param->hvsQPEnable) {	// can not use both ROI and hvsQp
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->bitAllocMode < 0 || param->bitAllocMode > 2) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (param->initBufLevelx8 < 0 || param->initBufLevelx8 > 8) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}

			if (pop->initialDelay < 10 || pop->initialDelay > 3000) {
				return WAVE420L_RETCODE_INVALID_PARAM;
			}
		}

		// packed format & cbcrInterleave & nv12 can't be set at the same time.
		if (pop->packedFormat == 1 && pop->cbcrInterleave == 1) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (pop->packedFormat == 1 && pop->nv21 == 1) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		// check valid for common param
		if (wave420l_CheckEncCommonParamValid(pop) == WAVE420L_RETCODE_FAILURE) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		// check valid for RC param
		if (wave420l_CheckEncRcParamValid(pop) == WAVE420L_RETCODE_FAILURE) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		// check additional features for WAVE420L
		if (param->chromaCbQpOffset < -10 || param->chromaCbQpOffset > 10) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->chromaCrQpOffset < -10 || param->chromaCrQpOffset > 10) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrYEnable != 1 && param->nrYEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrCbEnable != 1 && param->nrCbEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrCrEnable != 1 && param->nrCrEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrNoiseEstEnable != 1 && param->nrNoiseEstEnable != 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrNoiseSigmaY < 0 || param->nrNoiseSigmaY > 255) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrNoiseSigmaCb < 0 || param->nrNoiseSigmaCb > 255) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrNoiseSigmaCr < 0 || param->nrNoiseSigmaCr > 255) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrIntraWeightY < 0 || param->nrIntraWeightY > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrIntraWeightCb < 0 || param->nrIntraWeightCb > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrIntraWeightCr < 0 || param->nrIntraWeightCr > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrInterWeightY < 0 || param->nrInterWeightY > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrInterWeightCb < 0 || param->nrInterWeightCb > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->nrInterWeightCr < 0 || param->nrInterWeightCr > 31) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if((param->nrYEnable == 1 || param->nrCbEnable == 1 || param->nrCrEnable == 1) && (param->losslessEnable == 1)) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

		if (param->intraRefreshMode == 3 && param-> intraRefreshArg == 0) {
			return WAVE420L_RETCODE_INVALID_PARAM;
		}

	}
	return WAVE420L_RETCODE_SUCCESS;
}

RetCode wave420l_ProductVpuEncFiniSeq(CodecInst* instance)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	switch (instance->productId) {
	case PRODUCT_ID_420L:
		ret = Wave4VpuEncFiniSeq(instance);
		break;
	}
	return ret;
}

RetCode wave420l_ProductVpuEncSetup(CodecInst* instance)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	ret = wave420l_Wave4VpuEncSetup(instance);
	return ret;
}

RetCode wave420l_ProductVpuEncode(CodecInst* instance, EncParam* param)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	ret = wave420l_Wave4VpuEncode(instance, param);
	return ret;
}

RetCode wave420l_ProductVpuEncGetResult(CodecInst* instance, EncOutputInfo* result)
{
	RetCode ret = RETCODE_NOT_FOUND_VPU_DEVICE;

	ret = wave420l_Wave4VpuEncGetResult(instance, result);
	return ret;
}
