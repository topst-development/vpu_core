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
#include "common_vpuconfig.h"
#include "common_regdefine.h"
#include "wave4.h"
#include "vpuerror.h"
#include "wave4_regdefine.h"

static Uint32   presetGopSize[] = {
	1,  /* Custom GOP, Not used */
	1,  /* All Intra */
	1,  /* IPP Cyclic GOP size 1 */
	1,  /* IBB Cyclic GOP size 1 */
	2,  /* IBP Cyclic GOP size 2 */
	4,  /* IBBBP */
	4,
	4,
	8,
};

RetCode wave420l_Wave4VpuEncRegisterFramebuffer(CodecInst *instance, FrameBuffer *fbArr, TiledMapType mapType, Uint32 count)
{
	RetCode      ret = WAVE420L_RETCODE_SUCCESS;
	EncInfo*     pEncInfo = &instance->CodecInfo->encInfo;
	Int32        q, j, i, remain, idx, bufHeight = 0, bufWidth = 0;
	Int32        coreIdx, startNo, endNo;
	Uint32       regVal, cbcrInterleave=0, nv21=0, lumaStride, chromaStride;
	Uint32       endian, yuvFormat = 0;
	Uint32       addrY, addrCb, addrCr;
	Uint32       mvColSize = 0, fbcYTblSize = 0, fbcCTblSize = 0, subSampledSize = 0;
	Uint32       bpp=1;/* byte per pixel */
	vpu_buffer_t vbBuffer;
	Int32        stride;
	Uint32       axiID;
	EncOpenParam*   pOpenParam;
	unsigned int offset = 0;
	vpu_buffer_t allocated_vbBuffer;

	allocated_vbBuffer.base = pEncInfo->vbMV.base;
	allocated_vbBuffer.phys_addr = pEncInfo->vbMV.phys_addr;
	allocated_vbBuffer.size = pEncInfo->vbMV.size;
	allocated_vbBuffer.virt_addr = pEncInfo->vbMV.virt_addr;

	if (allocated_vbBuffer.phys_addr == 0) {
		return RETCODE_INSUFFICIENT_RESOURCE;
	}

	if (allocated_vbBuffer.size == 0) {
		return RETCODE_INSUFFICIENT_RESOURCE;
	}

	pOpenParam = &pEncInfo->openParam;

	coreIdx = instance->coreIdx;
	mvColSize = fbcYTblSize = fbcCTblSize = 0;
	stride = pEncInfo->stride;
	axiID = pOpenParam->virtAxiID;

	bufWidth  = VPU_ALIGN8(pOpenParam->picWidth);
	bufHeight = VPU_ALIGN8(pOpenParam->picHeight);

	if (mapType == COMPRESSED_FRAME_MAP) {
		cbcrInterleave = 0;
		nv21 = 0;

		mvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(bufWidth, bufHeight);
		mvColSize = VPU_ALIGN16(mvColSize);
		vbBuffer.size = ((mvColSize*count+4095)&~4095)+4096;   /* 4096 is a margin */

		//if( allocated_vbBuffer.base != NULL )
		//  pEncInfo->vbMV.base = allocated_vbBuffer.base + offset;
		//pEncInfo->vbMV.phys_addr = allocated_vbBuffer.phys_addr+offset;
		pEncInfo->vbMV.size = vbBuffer.size;
		//if( allocated_vbBuffer.virt_addr != NULL )
		//  pEncInfo->vbMV.virt_addr = allocated_vbBuffer.virt_addr+offset;
		offset += vbBuffer.size;

		fbcYTblSize = WAVE4_FBC_LUMA_TABLE_SIZE(bufWidth, bufHeight);
		fbcYTblSize = VPU_ALIGN16(fbcYTblSize);

		fbcCTblSize = WAVE4_FBC_CHROMA_TABLE_SIZE(bufWidth, bufHeight);
		fbcCTblSize = VPU_ALIGN16(fbcCTblSize);

		vbBuffer.size = ((fbcYTblSize*count+4095)&~4095)+4096;

		if (allocated_vbBuffer.base != 0) {
			pEncInfo->vbFbcYTbl.base = allocated_vbBuffer.base+offset;
		}
		pEncInfo->vbFbcYTbl.phys_addr = allocated_vbBuffer.phys_addr+offset;
		pEncInfo->vbFbcYTbl.size = vbBuffer.size;
		if (allocated_vbBuffer.virt_addr != 0) {
			pEncInfo->vbFbcYTbl.virt_addr = allocated_vbBuffer.virt_addr+offset;
		}
		offset += vbBuffer.size;

		vbBuffer.size = ((fbcCTblSize * count + 4095) & ~4095) + 4096;

		if (allocated_vbBuffer.base != 0) {
			pEncInfo->vbFbcCTbl.base = allocated_vbBuffer.base+offset;
		}
		pEncInfo->vbFbcCTbl.phys_addr = allocated_vbBuffer.phys_addr+offset;
		pEncInfo->vbFbcCTbl.size = vbBuffer.size;
		if (allocated_vbBuffer.virt_addr != 0) {
			pEncInfo->vbFbcCTbl.virt_addr = allocated_vbBuffer.virt_addr+offset;
		}
		offset += vbBuffer.size;

		subSampledSize = WAVE4_SUBSAMPLED_ONE_SIZE(bufWidth, bufHeight);
		vbBuffer.size = ((subSampledSize*count+4095)&~4095)+4096;

		if (allocated_vbBuffer.base != 0) {
			pEncInfo->vbSubSamBuf.base = allocated_vbBuffer.base+offset;
		}
		pEncInfo->vbSubSamBuf.phys_addr = allocated_vbBuffer.phys_addr+offset;
		pEncInfo->vbSubSamBuf.size = vbBuffer.size;
		if (allocated_vbBuffer.virt_addr != 0) {
			pEncInfo->vbSubSamBuf.virt_addr = allocated_vbBuffer.virt_addr+offset;
		}
		offset += vbBuffer.size;
	}

	VpuWriteReg(coreIdx, W4_ADDR_SUB_SAMPLED_FB_BASE, pEncInfo->vbSubSamBuf.phys_addr);     // set sub-sampled buffer base addr
	VpuWriteReg(coreIdx, W4_SUB_SAMPLED_ONE_FB_SIZE, subSampledSize);           // set sub-sampled buffer size for one frame

	endian = wave420l_vdi_convert_endian(coreIdx, fbArr[0].endian) & VDI_128BIT_ENDIAN_MASK;

	regVal = (bufWidth<<16) | bufHeight;

	VpuWriteReg(coreIdx, W4_PIC_SIZE, regVal);

	// set stride of Luma/Chroma for compressed buffer
	{
		lumaStride = VPU_ALIGN16(pOpenParam->picWidth)*(4);
		lumaStride = VPU_ALIGN32(lumaStride);

		chromaStride = VPU_ALIGN16(pOpenParam->picWidth/2)*(4);
		chromaStride = VPU_ALIGN32(chromaStride);
	}

	VpuWriteReg(coreIdx, W4_FBC_STRIDE, lumaStride<<16 | chromaStride);

	regVal =   (nv21 << 29)
			 | ((mapType == LINEAR_FRAME_MAP)<<28)
			 | (axiID << 24)
			 | (yuvFormat << 20)
			 | (cbcrInterleave << 16)
			 | (stride*bpp);

	VpuWriteReg(coreIdx, W4_COMMON_PIC_INFO, regVal);

	remain = count;
	q      = (remain+7)/8;
	idx    = 0;
	for (j=0; j<q; j++) {
		regVal = (endian<<16) | (j==q-1)<<4 | ((j==0)<<3) ;
		VpuWriteReg(coreIdx, W4_SFB_OPTION, regVal);
		startNo = j*8;
		endNo   = startNo + (remain>=8 ? 8 : remain) - 1;

		VpuWriteReg(coreIdx, W4_SET_FB_NUM, (startNo<<8)|endNo);

		for (i=0; i<8 && i<remain; i++) {
			addrY  = fbArr[i+startNo].bufY;
			addrCb = fbArr[i+startNo].bufCb;
			addrCr = fbArr[i+startNo].bufCr;

			VpuWriteReg(coreIdx, W4_ADDR_LUMA_BASE0  + (i<<4), addrY);
			VpuWriteReg(coreIdx, W4_ADDR_CB_BASE0    + (i<<4), addrCb);
			APIDPRINT("REGISTER FB[%02d] Y(0x%08x), Cb(0x%08x) ", i, addrY, addrCb);

			if (mapType == COMPRESSED_FRAME_MAP) {
				VpuWriteReg(coreIdx, W4_ADDR_FBC_Y_OFFSET0 + (i<<4), pEncInfo->vbFbcYTbl.phys_addr+idx*fbcYTblSize); /* Luma FBC offset table */
				VpuWriteReg(coreIdx, W4_ADDR_FBC_C_OFFSET0 + (i<<4), pEncInfo->vbFbcCTbl.phys_addr+idx*fbcCTblSize);        /* Chroma FBC offset table */
				VpuWriteReg(coreIdx, W4_ADDR_MV_COL0  + (i<<2), pEncInfo->vbMV.phys_addr+idx*mvColSize);
				APIDPRINT("Yo(0x%08x) Co(0x%08x), Mv(0x%08x)\n",
					pEncInfo->vbFbcYTbl.phys_addr+idx*fbcYTblSize,
					pEncInfo->vbFbcCTbl.phys_addr+idx*fbcCTblSize,
					pEncInfo->vbMV.phys_addr+idx*mvColSize);
			}
			else {
				VpuWriteReg(coreIdx, W4_ADDR_CR_BASE0 + (i<<4), addrCr);
				VpuWriteReg(coreIdx, W4_ADDR_FBC_C_OFFSET0 + (i<<4), 0);
				VpuWriteReg(coreIdx, W4_ADDR_MV_COL0  + (i<<2), 0);
				APIDPRINT("Cr(0x%08x)\n", addrCr);
			}
			idx++;
		}
		remain -= i;

		VpuWriteReg(coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
		VpuWriteReg(coreIdx, W4_WORK_SIZE,      pEncInfo->vbWork.size);
		VpuWriteReg(coreIdx, W4_WORK_PARAM,     0);

		wave420l_Wave4BitIssueCommand(instance, SET_FRAMEBUF);
		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
			int cnt = 0;
			while(VpuReadReg(coreIdx, W4_VPU_BUSY_STATUS))
			{
				cnt++;
				if (cnt > 0x7FFFFFF)
					return RETCODE_CODEC_EXIT;
			}
		}*/
	}

	regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
	if (regVal == 0) {
		return WAVE420L_RETCODE_FAILURE;
	}


	if (wave420l_ConfigSecAXIWave(coreIdx, instance->codecMode,
		&pEncInfo->secAxiInfo, pOpenParam->picWidth, pOpenParam->picHeight,
		pOpenParam->EncStdParam.hevcParam.profile, pOpenParam->EncStdParam.hevcParam.level) == 0) {
			return RETCODE_INSUFFICIENT_RESOURCE;
	}

	return ret;
}

RetCode wave420l_Wave4VpuEncSetup(CodecInst *instance)
{
	Int32 coreIdx, int_reason = 0;
	//Int32 width32 = 0, height32=0;
	Uint32          regVal = 0, bsEndian = 0;
	EncInfo*        pEncInfo;
	EncOpenParam*   pOpenParam;
	EncHevcParam*   pHevc;

	coreIdx    = instance->coreIdx;
	pEncInfo = &instance->CodecInfo->encInfo;

	pOpenParam  = &pEncInfo->openParam;
	pHevc       = &pOpenParam->EncStdParam.hevcParam;

	//width32 = (pOpenParam->picWidth + 31) & ~31;
	//height32= (pOpenParam->picHeight+ 31) & ~31;

	regVal = wave420l_vdi_convert_endian(coreIdx, pOpenParam->streamEndian);

	/* NOTE: When endian mode is 0, SDMA reads MSB first */
	bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

	VpuWriteReg(coreIdx, W4_BS_START_ADDR, pEncInfo->streamBufStartAddr);
	VpuWriteReg(coreIdx, W4_BS_SIZE,       pEncInfo->streamBufSize);
	VpuWriteReg(coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
	VpuWriteReg(coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
	VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | bsEndian); //VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | (pEncInfo->ringBufferEnable<<4) | bsEndian);

	/* Secondary AXI */
	VpuWriteReg(coreIdx, W4_ADDR_SEC_AXI, pEncInfo->secAxiInfo.bufBase);
	VpuWriteReg(coreIdx, W4_SEC_AXI_SIZE, pEncInfo->secAxiInfo.bufSize);
	VpuWriteReg(coreIdx, W4_USE_SEC_AXI,  // (pEncInfo->secAxiInfo.u.wave4.useEncImdEnable<<9)    |  //Wave420L does not support EncImd secondary AXI (TELMON-28)
					      (pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable<<11)   |
					      (pEncInfo->secAxiInfo.u.wave4.useEncLfEnable<<15));

	/* Set up work-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(coreIdx, W4_WORK_SIZE,      pEncInfo->vbWork.size);
	VpuWriteReg(coreIdx, W4_WORK_PARAM,     0);

	/* Set up temp-buffer */
	pEncInfo->vbTemp.phys_addr = pOpenParam->vbTemp.phys_addr;  //pEncInfo->vbWork.phys_addr + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE; //vb.phys_addr + WAVE4_TEMPBUF_OFFSET;
	pEncInfo->vbTemp.virt_addr = pOpenParam->vbTemp.virt_addr;     //pEncInfo->vbWork.virt_addr + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE; //vb.virt_addr + WAVE4_TEMPBUF_OFFSET;
	pEncInfo->vbTemp.size      = DEFAULT_TEMPBUF_SIZE;

	VpuWriteReg(coreIdx, W4_ADDR_TEMP_BASE, pEncInfo->vbTemp.phys_addr);
	VpuWriteReg(coreIdx, W4_TEMP_SIZE,      pEncInfo->vbTemp.size);
	VpuWriteReg(coreIdx, W4_TEMP_PARAM,     0);

	/* Set up BitstreamBuffer */
	VpuWriteReg(coreIdx, W4_BS_START_ADDR, pEncInfo->streamBufStartAddr);
	VpuWriteReg(coreIdx, W4_BS_SIZE,       pEncInfo->streamBufSize);

	/* SET_PARAM + COMMON */
	VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_OPTION, OPT_COMMON);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_ENABLE, (unsigned int)ENC_CHANGE_SET_PARAM_ALL);

	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_SRC_SIZE,   pOpenParam->picHeight<<16 | pOpenParam->picWidth);

	{    // WAVE420L
		VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_PIC_PARAM,  (pHevc->profile<<0)                 |
								(pHevc->level<<3)                   |
								(pHevc->tier<<12)                   |
								(pHevc->internalBitDepth<<14)       |    //only 8 on Wave420L
								(pHevc->chromaFormatIdc<<18)        |
								(pHevc->losslessEnable<<20)         |
								(pHevc->constIntraPredFlag<<21)     |
								((pHevc->chromaCbQpOffset&0x1f)<<22)|
								((pHevc->chromaCrQpOffset&0x1f)<<27));
	}

	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_GOP_PARAM,  pHevc->gopPresetIdx);

	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INTRA_PARAM, (pHevc->decodingRefreshType<<0)    |
													 (pHevc->intraQP<<3)                |
													 (pHevc->forcedIdrHeaderEnable<<9)  |
													 (pHevc->intraPeriod<<16));

	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_CONF_WIN_TOP_BOT, pHevc->confWinBot<<16 | pHevc->confWinTop);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_CONF_WIN_LEFT_RIGHT, pHevc->confWinRight<<16 | pHevc->confWinLeft);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_FRAME_RATE, pOpenParam->frameRateInfo);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INDEPENDENT_SLICE, pHevc->independSliceModeArg<<16 | pHevc->independSliceMode);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_DEPENDENT_SLICE, pHevc->dependSliceModeArg<<16 | pHevc->dependSliceMode);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INTRA_REFRESH, pHevc->intraRefreshArg<<16 | pHevc->intraRefreshMode);

	regVal = (pHevc->useRecommendEncParam)           |
			 (pHevc->scalingListEnable<<3)           |
			 (pHevc->cuSizeMode<<4)                  |
			 (pHevc->tmvpEnable<<7)                  |
			 (pHevc->maxNumMerge<<9)                 |
			 (pHevc->dynamicMerge8x8Enable<<12)      |
			 (pHevc->dynamicMerge16x16Enable<<13)    |
			 (pHevc->dynamicMerge32x32Enable<<14)    |
			 (pHevc->disableDeblk<<15)               |
			 (pHevc->lfCrossSliceBoundaryEnable<<16) |
			 ((pHevc->betaOffsetDiv2&0xF)<<17)       |
			 ((pHevc->tcOffsetDiv2&0xF)<<21)         |
			 (pHevc->intraInInterSliceEnable<<27);

	VpuWriteReg(coreIdx, W4_CMD_ENC_PARAM,  regVal);

	{  // WAVE420L
		VpuWriteReg(coreIdx, W4_CMD_ENC_RC_PARAM,   (pOpenParam->rcEnable<<0)           |
													(pHevc->cuLevelRCEnable<<1)         |
													(pHevc->hvsQPEnable<<2)             |
													(pHevc->hvsQpScaleEnable<<3)        |
													(pHevc->hvsQpScale<<4)              |
													(pHevc->bitAllocMode<<7)            |
													(pHevc->initBufLevelx8<<9)          |
													(pHevc->ctuOptParam.roiEnable<<13)     |
													(pHevc->initialRcQp<<14)            |
													(pOpenParam->initialDelay<<20));
	}

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_MIN_MAX_QP, (pHevc->minQp<<0)        |
												   (pHevc->maxQp<<6)        |
												   (pHevc->maxDeltaQp<<12)  |
												   ((pHevc->intraQpOffset&0xFFFF)<<18));

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_BIT_RATIO_LAYER_0_3,   (pHevc->fixedBitRatio[0]<<0)  |
															  (pHevc->fixedBitRatio[1]<<8)  |
															  (pHevc->fixedBitRatio[2]<<16) |
															  (pHevc->fixedBitRatio[3]<<24));

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_BIT_RATIO_LAYER_4_7,   (pHevc->fixedBitRatio[4]<<0)  |
															  (pHevc->fixedBitRatio[5]<<8)  |
															  (pHevc->fixedBitRatio[6]<<16) |
															  (pHevc->fixedBitRatio[7]<<24));

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_INTRA_MIN_MAX_QP, pHevc->intraMaxQp<<6 | pHevc->intraMinQp);

	VpuWriteReg(coreIdx, W4_CMD_ENC_NR_PARAM,   (pHevc->nrYEnable<<0)       |
												(pHevc->nrCbEnable<<1)      |
												(pHevc->nrCrEnable<<2)      |
												(pHevc->nrNoiseEstEnable<<3)|
												(pHevc->nrNoiseSigmaY<<4)   |
												(pHevc->nrNoiseSigmaCb<<12) |
												(pHevc->nrNoiseSigmaCr<<20));

	VpuWriteReg(coreIdx, W4_CMD_ENC_NR_WEIGHT,  (pHevc->nrIntraWeightY<<0)  |
												(pHevc->nrIntraWeightCb<<5) |
												(pHevc->nrIntraWeightCr<<10)|
												(pHevc->nrInterWeightY<<15) |
												(pHevc->nrInterWeightCb<<20)|
												(pHevc->nrInterWeightCr<<25));

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_TARGET_RATE, pOpenParam->bitRate);

	VpuWriteReg(coreIdx, W4_CMD_ENC_RC_TRANS_RATE, pHevc->transRate);

	VpuWriteReg(coreIdx, W4_CMD_ENC_ROT_PARAM,  0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_NUM_UNITS_IN_TICK, pHevc->numUnitsInTick);
	VpuWriteReg(coreIdx, W4_CMD_ENC_TIME_SCALE, pHevc->timeScale);
	VpuWriteReg(coreIdx, W4_CMD_ENC_NUM_TICKS_POC_DIFF_ONE, pHevc->numTicksPocDiffOne);

	// comment until VPU working

	wave420l_Wave4BitIssueCommand(instance, SET_PARAM);

	//do
	{
		unsigned int use_interrupt_flag = INT_ENABLE_FLAG_CMD_SET_PARAM;
		int_reason = wave420l_vdi_wait_interrupt(0, 0x7FFF/*VPU_ENC_TIMEOUT*/, 0/*W4_VPU_VINT_REASON*/, 0/*W4_VPU_VPU_INT_STS*/, use_interrupt_flag );        //==> int_reason =0  regW4_VPU_VPU_INT_STS=0x0, regW4_VPU_VINT_REASON=0x0
	} //while (int_reason == -1 && __VPU_BUSY_TIMEOUT == 0);

	if (int_reason == -1) {
		if (instance->loggingEnable) {
			wave420l_vdi_log(coreIdx, SET_PARAM, 0);
		}
		//return RETCODE_VPU_RESPONSE_TIMEOUT;
		return RETCODE_CODEC_EXIT;
	}
	VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, int_reason);
	VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);

	int_reason = 0;

	if (instance->loggingEnable) {
		wave420l_vdi_log(coreIdx, SET_PARAM, 0);
	}

	if (VpuReadReg(coreIdx, W4_RET_SUCCESS) == 0) {
		if (VpuReadReg(coreIdx, W4_RET_FAIL_REASON) == WAVE4_SYSERR_WRITEPROTECTION) {
			return RETCODE_MEMORY_ACCESS_VIOLATION;
		}
		return WAVE420L_RETCODE_FAILURE;
	}

	pEncInfo->initialInfo.minFrameBufferCount = VpuReadReg(coreIdx, W4_RET_ENC_MIN_FB_NUM);	// =2
	pEncInfo->initialInfo.minSrcFrameCount  = VpuReadReg(coreIdx, W4_RET_ENC_MIN_SRC_BUF_NUM);	// =1

	/*
	 * SET_PARAM + CUSTOM_GOP
	 * only when gopPresetIdx == custom_gop, custom_gop related registers should be set
	 */
	if (pHevc->gopPresetIdx == PRESET_IDX_CUSTOM_GOP) {
		int i=0, j = 0;

		VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_OPTION, OPT_CUSTOM_GOP);
		VpuWriteReg(coreIdx, W4_CMD_ENC_SET_CUSTOM_GOP_ENABLE, (unsigned int)ENC_CHANGE_SET_PARAM_ALL);     // enable to change for all custom gop parameters.
		VpuWriteReg(coreIdx, W4_CMD_ENC_CUSTOM_GOP_PARAM, pHevc->gopParam.customGopSize<<0 | pHevc->gopParam.useDeriveLambdaWeight <<4);

		for (i=0 ; i<pHevc->gopParam.customGopSize; i++) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_CUSTOM_GOP_PIC_PARAM_0 + (i*4), (pHevc->gopParam.picParam[i].picType<<0)            |
											(pHevc->gopParam.picParam[i].pocOffset<<2)          |
											(pHevc->gopParam.picParam[i].picQp<<6)              |
											((pHevc->gopParam.picParam[i].refPocL0&0x1F)<<14)   |
											((pHevc->gopParam.picParam[i].refPocL1&0x1F)<<19)   |
											(pHevc->gopParam.picParam[i].temporalId<<24));

			VpuWriteReg(coreIdx, W4_CMD_ENC_CUSTOM_GOP_PIC_LAMBDA_0 + (i*4), pHevc->gopParam.gopPicLambda[i]);
		}

		for (j = i; j < MAX_GOP_NUM; j++) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_CUSTOM_GOP_PIC_PARAM_0 + (j*4), 0);
			VpuWriteReg(coreIdx, W4_CMD_ENC_CUSTOM_GOP_PIC_LAMBDA_0 + (j*4), 0);
		}

		// comment until VPU working
		wave420l_Wave4BitIssueCommand(instance, SET_PARAM);

		//do
		{
			unsigned int use_interrupt_flag = INT_ENABLE_FLAG_CMD_SET_PARAM;
			int_reason = wave420l_vdi_wait_interrupt(0, 0x7FFF/*VPU_ENC_TIMEOUT*/, 0/*W4_VPU_VINT_REASON*/, 0/*W4_VPU_VPU_INT_STS*/, use_interrupt_flag );
		} //while (int_reason == -1 && __VPU_BUSY_TIMEOUT == 0);

		if (int_reason == -1) {
			if (instance->loggingEnable) {
				wave420l_vdi_log(coreIdx, SET_PARAM, 0);
			}
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, int_reason);
		VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);

		int_reason = 0;
		if (instance->loggingEnable)
			wave420l_vdi_log(coreIdx, SET_PARAM, 0);

		if (VpuReadReg(coreIdx, W4_RET_SUCCESS) == 0) {
			if (VpuReadReg(coreIdx, W4_RET_FAIL_REASON) == WAVE4_SYSERR_WRITEPROTECTION) {
				return RETCODE_MEMORY_ACCESS_VIOLATION;
			}
			return WAVE420L_RETCODE_FAILURE;
		}

		if ((int)VpuReadReg(coreIdx, W4_RET_ENC_MIN_FB_NUM) > pEncInfo->initialInfo.minFrameBufferCount) {
			pEncInfo->initialInfo.minFrameBufferCount = VpuReadReg(coreIdx, W4_RET_ENC_MIN_FB_NUM);
		}

		if ((int)VpuReadReg(coreIdx, W4_RET_ENC_MIN_SRC_BUF_NUM) > pEncInfo->initialInfo.minSrcFrameCount) {
			pEncInfo->initialInfo.minSrcFrameCount  = VpuReadReg(coreIdx, W4_RET_ENC_MIN_SRC_BUF_NUM);
		}
	}


	if (pHevc->vuiParam.vuiParamFlags || pEncInfo->openParam.encodeVuiRbsp || pEncInfo->openParam.encodeHrdRbspInVPS || pEncInfo->openParam.encodeHrdRbspInVUI) {

		//*** VUI encoding by host registers ***/
		if (pHevc->vuiParam.vuiParamFlags) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_OPTION, OPT_VUI);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_PARAM_FLAGS, pHevc->vuiParam.vuiParamFlags);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_ASPECT_RATIO_IDC, pHevc->vuiParam.vuiAspectRatioIdc);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_SAR_SIZE, pHevc->vuiParam.vuiSarSize);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_OVERSCAN_APPROPRIATE, pHevc->vuiParam.vuiOverScanAppropriate);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_VIDEO_SIGNAL, pHevc->vuiParam.videoSignal);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_CHROMA_SAMPLE_LOC, pHevc->vuiParam.vuiChromaSampleLoc);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_DISP_WIN_LEFT_RIGHT, pHevc->vuiParam.vuiDispWinLeftRight);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_DISP_WIN_TOP_BOT, pHevc->vuiParam.vuiDispWinTopBottom);
		}
		else {
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_PARAM_FLAGS, 0x0);
		}

		if (pEncInfo->openParam.encodeVuiRbsp || pEncInfo->openParam.encodeHrdRbspInVPS || pEncInfo->openParam.encodeHrdRbspInVUI) {
			//*** VUI encoding by given rbsp data ***/
			VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_OPTION, OPT_VUI);

			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_HRD_RBSP_PARAM_FLAG, (pEncInfo->openParam.encodeHrdRbspInVPS<<2)    |
																	 (pEncInfo->openParam.encodeHrdRbspInVUI<<1)    |
																	 (pEncInfo->openParam.encodeVuiRbsp));
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_RBSP_ADDR, pEncInfo->openParam.vuiRbspDataAddr);
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_RBSP_SIZE, pEncInfo->openParam.vuiRbspDataSize);
			VpuWriteReg(coreIdx, W4_CMD_ENC_HRD_RBSP_ADDR, pEncInfo->openParam.hrdRbspDataAddr);
			VpuWriteReg(coreIdx, W4_CMD_ENC_HRD_RBSP_SIZE, pEncInfo->openParam.hrdRbspDataSize);
		}
		else {
			VpuWriteReg(coreIdx, W4_CMD_ENC_VUI_HRD_RBSP_PARAM_FLAG, 0x0);
		}

		wave420l_Wave4BitIssueCommand(instance, SET_PARAM);

		//do
		{
			unsigned int use_interrupt_flag = INT_ENABLE_FLAG_CMD_SET_PARAM;
			int_reason = wave420l_vdi_wait_interrupt(0, 0x7FFF/*VPU_ENC_TIMEOUT*/, 0/*W4_VPU_VINT_REASON*/, 0/*W4_VPU_VPU_INT_STS*/, use_interrupt_flag );
		} //while (int_reason == -1 && __VPU_BUSY_TIMEOUT == 0);

		if (int_reason == -1) {
			if (instance->loggingEnable) {
				wave420l_vdi_log(coreIdx, SET_PARAM, 0);
			}
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, int_reason);
		VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);

		if (instance->loggingEnable) {
			wave420l_vdi_log(coreIdx, SET_PARAM, 0);
		}

		if (VpuReadReg(coreIdx, W4_RET_SUCCESS) == 0) {
			if (VpuReadReg(coreIdx, W4_RET_FAIL_REASON) == WAVE4_SYSERR_WRITEPROTECTION) {
				return RETCODE_MEMORY_ACCESS_VIOLATION;
			}
			return WAVE420L_RETCODE_FAILURE;
		}
	}

	pEncInfo->streamWrPtr   = VpuReadReg(coreIdx, pEncInfo->streamWrPtrRegAddr);
	return WAVE420L_RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuEncode(CodecInst *instance, EncParam *option)
{
	Int32           coreIdx, srcFrameFormat, srcPixelFormat, packedFormat;
	Uint32          regVal = 0, bsEndian;
	Uint32          srcStrideC = 0;
	EncInfo*        pEncInfo;
	FrameBuffer*    pSrcFrame;
	EncOpenParam*   pOpenParam;
	BOOL            justified = W4_WTL_RIGHT_JUSTIFIED;
	Uint32          formatNo  = W4_WTL_PIXEL_8BIT;

	coreIdx     = instance->coreIdx;
	pEncInfo    = VPU_HANDLE_TO_ENCINFO(instance);
	pOpenParam  = &pEncInfo->openParam;
	pSrcFrame   = option->sourceFrame;


	switch (pOpenParam->srcFormat) {
	case FORMAT_420_P10_16BIT_MSB:
		justified = W4_WTL_RIGHT_JUSTIFIED;
		formatNo  = W4_WTL_PIXEL_16BIT;
		break;
	case FORMAT_420_P10_16BIT_LSB:
		justified = W4_WTL_LEFT_JUSTIFIED;
		formatNo  = W4_WTL_PIXEL_16BIT;
		break;
	case FORMAT_420_P10_32BIT_MSB:
		justified = W4_WTL_RIGHT_JUSTIFIED;
		formatNo  = W4_WTL_PIXEL_32BIT;
		break;
	case FORMAT_420_P10_32BIT_LSB:
		justified = W4_WTL_LEFT_JUSTIFIED;
		formatNo  = W4_WTL_PIXEL_32BIT;
		break;
	case FORMAT_420:
	case FORMAT_YUYV:
	case FORMAT_YVYU:
	case FORMAT_UYVY:
	case FORMAT_VYUY:
		justified = W4_WTL_LEFT_JUSTIFIED;
		formatNo  = W4_WTL_PIXEL_8BIT;
		break;
	}
	packedFormat = (pOpenParam->packedFormat >= 1) ? 1 : 0;

	srcFrameFormat = packedFormat<<2   |
		pOpenParam->cbcrInterleave<<1  |
		pOpenParam->nv21;

	switch (pOpenParam->packedFormat) {     // additional packed format (interleave & nv21 bit are used to present these modes)
	case PACKED_YVYU:
		srcFrameFormat = 0x5;
		break;
	case PACKED_UYVY:
		srcFrameFormat = 0x6;
		break;
	case PACKED_VYUY:
		srcFrameFormat = 0x7;
		break;
	default:
		break;
	}

	srcPixelFormat = justified<<2 | formatNo;

	regVal = wave420l_vdi_convert_endian(coreIdx, pOpenParam->streamEndian);

	/* NOTE: When endian mode is 0, SDMA reads MSB first */
	bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

	VpuWriteReg(coreIdx, W4_BS_START_ADDR, option->picStreamBufferAddr);
	VpuWriteReg(coreIdx, W4_BS_SIZE,	   option->picStreamBufferSize);
	pEncInfo->streamRdPtr = option->picStreamBufferAddr;
	pEncInfo->streamWrPtr = option->picStreamBufferAddr;
	pEncInfo->streamBufStartAddr = option->picStreamBufferAddr;
	pEncInfo->streamBufSize = option->picStreamBufferSize;
	pEncInfo->streamBufEndAddr = option->picStreamBufferAddr + option->picStreamBufferSize;

	VpuWriteReg(coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
	VpuWriteReg(coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);

	VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | bsEndian); //VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | (pEncInfo->ringBufferEnable<<4) | bsEndian);

	/* Secondary AXI */
	VpuWriteReg(coreIdx, W4_ADDR_SEC_AXI, pEncInfo->secAxiInfo.bufBase);
	VpuWriteReg(coreIdx, W4_SEC_AXI_SIZE, pEncInfo->secAxiInfo.bufSize);
	VpuWriteReg(coreIdx, W4_USE_SEC_AXI,  //(pEncInfo->secAxiInfo.u.wave4.useEncImdEnable<<9)    | //Wave420L does not support EncImd secondary AXI (TELMON-28)
										  (pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable<<11)   |
										  (pEncInfo->secAxiInfo.u.wave4.useEncLfEnable<<15));

	/* Set up work-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(coreIdx, W4_WORK_SIZE,      pEncInfo->vbWork.size);
	VpuWriteReg(coreIdx, W4_WORK_PARAM,     0);

	/* Set up temp-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_TEMP_BASE, pEncInfo->vbTemp.phys_addr);
	VpuWriteReg(coreIdx, W4_TEMP_SIZE,      pEncInfo->vbTemp.size);
	VpuWriteReg(coreIdx, W4_TEMP_PARAM,     0);

	VpuWriteReg(coreIdx, W4_CMD_ENC_ADDR_REPORT_BASE, 0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_REPORT_SIZE, 0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_REPORT_PARAM, 0);

	if (option->codeOption.implicitHeaderEncode == 1) {
		VpuWriteReg(coreIdx, W4_CMD_ENC_CODE_OPTION, CODEOPT_ENC_HEADER_IMPLICIT | CODEOPT_ENC_VCL | // implicitly encode a header(headers) for generating bitstream. (to encode a header only, use ENC_PUT_VIDEO_HEADER for GiveCommand)
													(option->codeOption.encodeAUD<<5)              |
													(option->codeOption.encodeEOS<<6)              |
													(option->codeOption.encodeEOB<<7));
	}
	else {
		VpuWriteReg(coreIdx, W4_CMD_ENC_CODE_OPTION, (option->codeOption.implicitHeaderEncode<<0)   |
													 (option->codeOption.encodeVCL<<1)              |
													 (option->codeOption.encodeVPS<<2)              |
													 (option->codeOption.encodeSPS<<3)              |
													 (option->codeOption.encodePPS<<4)              |
													 (option->codeOption.encodeAUD<<5)              |
													 (option->codeOption.encodeEOS<<6)              |
													 (option->codeOption.encodeEOB<<7)              |
													 (option->codeOption.encodeVUI<<9));
	}

	VpuWriteReg(coreIdx, W4_CMD_ENC_PIC_PARAM,  (option->skipPicture<<0)         |
												(option->forcePicQpEnable<<1)    |
												(option->forcePicQpI<<2)         |
												(option->forcePicQpP<<8)         |
												(option->forcePicQpB<<14)        |
												(option->forcePicTypeEnable<<20) |
												(option->forcePicType<<21)       |
												(option->forcePicQpSrcOrderEnable<<29) |
												(option->forcePicTypeSrcOrderEnable<<30));

	if (option->srcEndFlag == 1)
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_PIC_IDX, 0xFFFFFFFF);               // no more source image.
	else
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_PIC_IDX, option->srcIdx);

	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_ADDR_Y, pSrcFrame->bufY);
	if (pOpenParam->cbcrOrder == CBCR_ORDER_NORMAL) {
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_ADDR_U, pSrcFrame->bufCb);
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_ADDR_V, pSrcFrame->bufCr);
	}
	else {
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_ADDR_U, pSrcFrame->bufCr);
		VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_ADDR_V, pSrcFrame->bufCb);
	}

	if (formatNo == W4_WTL_PIXEL_32BIT) {
		srcStrideC = VPU_ALIGN16(pSrcFrame->stride/2)*(1<<pSrcFrame->cbcrInterleave);
		if ( pSrcFrame->cbcrInterleave == 1)
			srcStrideC = pSrcFrame->stride;
	}
	else {
		srcStrideC = (pSrcFrame->cbcrInterleave == 1) ? pSrcFrame->stride : (pSrcFrame->stride>>1);
	}

	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_STRIDE, (pSrcFrame->stride<<16) | srcStrideC );


	regVal = wave420l_vdi_convert_endian(coreIdx, pOpenParam->sourceEndian);

	bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_FORMAT, (srcFrameFormat<<0)  |
												(srcPixelFormat<<3)  |
												(bsEndian<<6));

	VpuWriteReg(coreIdx, W4_CMD_ENC_PREFIX_SEI_NAL_ADDR, pEncInfo->prefixSeiNalAddr);
	VpuWriteReg(coreIdx, W4_CMD_ENC_PREFIX_SEI_INFO, pEncInfo->prefixSeiDataSize<<16 | pEncInfo->prefixSeiDataEncOrder<<1 | pEncInfo->prefixSeiNalEnable);

	VpuWriteReg(coreIdx, W4_CMD_ENC_SUFFIX_SEI_NAL_ADDR, pEncInfo->suffixSeiNalAddr);
	VpuWriteReg(coreIdx, W4_CMD_ENC_SUFFIX_SEI_INFO, pEncInfo->suffixSeiDataSize<<16 | pEncInfo->suffixSeiDataEncOrder<<1 | pEncInfo->suffixSeiNalEnable);

	VpuWriteReg(coreIdx, W4_CMD_ENC_ROI_ADDR_CTU_MAP, option->ctuOptParam.addrRoiCtuMap);


	VpuWriteReg(coreIdx, W4_CMD_ENC_CTU_OPT_PARAM,  ((option->ctuOptParam.roiEnable) << 0)   |
													(option->ctuOptParam.roiDeltaQp << 1)    |
													(option->ctuOptParam.mapEndian << 12)    |
													(option->ctuOptParam.mapStride << 16));

	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_TIMESTAMP_LOW, 0);  // Wave420L does not support(TELMON-30)
	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_TIMESTAMP_HIGH, 0); // Wave420L does not support(TELMON-30)

	VpuWriteReg(coreIdx, W4_CMD_ENC_LONGTERM_PIC, 0); // Wave420L does not support Long term param. (TELMON-30)

	VpuWriteReg(coreIdx, W4_CMD_ENC_SUB_FRAME_SYNC_CONFIG, 0);    // Wave420L does not support(TELMON-30)

	wave420l_Wave4BitIssueCommand(instance, ENC_PIC);

	return WAVE420L_RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuEncGetResult(CodecInst* instance, EncOutputInfo* result)
{
	Uint32      encodingSuccess, errorReason = 0;
	Uint32      regVal;
	Int32       coreIdx;
	EncInfo*    pEncInfo = VPU_HANDLE_TO_ENCINFO(instance);
	ProductId   productId = (ProductId)instance->productId;

	coreIdx = instance->coreIdx;

	if (instance->loggingEnable) {
		wave420l_vdi_log(coreIdx, ENC_PIC, 0);
	}

	encodingSuccess = VpuReadReg(coreIdx, W4_RET_SUCCESS);
	if (encodingSuccess == FALSE) {
		errorReason = VpuReadReg(coreIdx, W4_RET_FAIL_REASON);

		if (errorReason == WAVE4_SYSERR_WRITEPROTECTION) {
			wave420l_print_vpu_status(coreIdx, (unsigned long )productId);
			return RETCODE_MEMORY_ACCESS_VIOLATION;
		}
		if (errorReason == WAVE4_SYSERR_CP0_EXCEPTION) {
			wave420l_print_vpu_status(coreIdx, (unsigned long )productId);
			return RETCODE_CP0_EXCEPTION;
		}
		if (errorReason == WAVE4_SYSERR_STREAM_BUF_FULL) {
			wave420l_print_vpu_status(coreIdx, (unsigned long )productId);
			return RETCODE_STREAM_BUF_FULL;
		}
		if (errorReason == WAVE4_SYSERR_ACCESS_VIOLATION_HW) {
			wave420l_print_vpu_status(coreIdx, (unsigned long )productId);
			return RETCODE_ACCESS_VIOLATION_HW;
		}

		return WAVE420L_RETCODE_FAILURE;
	}

	result->encPicCnt       = VpuReadReg(coreIdx, W4_RET_ENC_PIC_NUM);
	regVal= VpuReadReg(coreIdx, W4_RET_ENC_PIC_TYPE);
	result->picType         = regVal & 0xFFFF;
	result->encVclNal       = VpuReadReg(coreIdx, W4_RET_ENC_VCL_NUT);
	result->reconFrameIndex = VpuReadReg(coreIdx, W4_RET_ENC_PIC_IDX);

	if (result->reconFrameIndex >= 0) {
		result->reconFrame  = pEncInfo->frameBufPool[result->reconFrameIndex];
	}

	result->numOfSlices     = VpuReadReg(coreIdx, W4_RET_ENC_PIC_SLICE_NUM);
	result->picSkipped      = VpuReadReg(coreIdx, W4_RET_ENC_PIC_SKIP);
	result->numOfIntra      = VpuReadReg(coreIdx, W4_RET_ENC_PIC_NUM_INTRA);
	result->numOfMerge      = VpuReadReg(coreIdx, W4_RET_ENC_PIC_NUM_MERGE);
	result->numOfSkipBlock  = VpuReadReg(coreIdx, W4_RET_ENC_PIC_NUM_SKIP);
	regVal = VpuReadReg(coreIdx, W4_RET_ENC_PIC_FLAG);
	result->bitstreamWrapAround = regVal&0x01;
	result->avgCtuQp        = VpuReadReg(coreIdx, W4_RET_ENC_PIC_AVG_CU_QP);
	result->encPicByte      = VpuReadReg(coreIdx, W4_RET_ENC_PIC_BYTE);
	result->encGopPicIdx    = VpuReadReg(coreIdx, W4_RET_ENC_GOP_PIC_IDX);
	result->encPicPoc       = VpuReadReg(coreIdx, W4_RET_ENC_PIC_POC);
	result->encSrcIdx       = VpuReadReg(coreIdx, W4_RET_ENC_USED_SRC_IDX);
	result->frameCycle      = VpuReadReg(coreIdx, W4_FRAME_CYCLE);
	pEncInfo->streamWrPtr   = VpuReadReg(coreIdx, pEncInfo->streamWrPtrRegAddr);

	//if (pEncInfo->ringBufferEnable == 0)
	{
		result->bitstreamBuffer = VpuReadReg(coreIdx, pEncInfo->streamRdPtrRegAddr);
	}
	result->rdPtr = pEncInfo->streamRdPtr;
	result->wrPtr = pEncInfo->streamWrPtr;

	if (result->reconFrameIndex < 0)
		result->bitstreamSize   = 0;
	else
		result->bitstreamSize   = result->encPicByte;


	return WAVE420L_RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuEncGetHeader(EncHandle instance, EncHeaderParam* encHeaderParam)
{
	EncInfo * pEncInfo;
	EncOpenParam *encOP;
	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	Int32       coreIdx, bsEndian, regVal, int_reason;
	RetCode         ret;

	pEncInfo = VPU_HANDLE_TO_ENCINFO(instance);
	encOP = &(pEncInfo->openParam);
	coreIdx = instance->coreIdx;

	regVal = wave420l_vdi_convert_endian(coreIdx, encOP->streamEndian);

	/* NOTE: When endian mode is 0, SDMA reads MSB first */
	bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

	VpuWriteReg(coreIdx, W4_BS_START_ADDR, encHeaderParam->buf);
	VpuWriteReg(coreIdx, W4_BS_SIZE, encHeaderParam->size);
	pEncInfo->streamRdPtr = encHeaderParam->buf;
	pEncInfo->streamWrPtr = encHeaderParam->buf;
	pEncInfo->streamBufStartAddr = encHeaderParam->buf;
	pEncInfo->streamBufSize = encHeaderParam->size;
	pEncInfo->streamBufEndAddr = encHeaderParam->buf + encHeaderParam->size;

	VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | bsEndian); //VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | (pEncInfo->ringBufferEnable<<4) | bsEndian);
	VpuWriteReg(coreIdx, W4_BS_OPTION, 0);
	/* Set up work-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(coreIdx, W4_WORK_SIZE,      pEncInfo->vbWork.size);
	VpuWriteReg(coreIdx, W4_WORK_PARAM,     0);

	/* Set up temp-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_TEMP_BASE, pEncInfo->vbTemp.phys_addr);
	VpuWriteReg(coreIdx, W4_TEMP_SIZE,      pEncInfo->vbTemp.size);
	VpuWriteReg(coreIdx, W4_TEMP_PARAM,     0);

	VpuWriteReg(coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
	VpuWriteReg(coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);

	/* Secondary AXI */
	VpuWriteReg(coreIdx, W4_ADDR_SEC_AXI, pEncInfo->secAxiInfo.bufBase);
	VpuWriteReg(coreIdx, W4_SEC_AXI_SIZE, pEncInfo->secAxiInfo.bufSize);
	VpuWriteReg(coreIdx, W4_USE_SEC_AXI,  // (pEncInfo->secAxiInfo.u.wave4.useEncImdEnable<<9)    | //Wave420L does not support EncImd secondary AXI (TELMON-28)
										  (pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable<<11)   |
										  (pEncInfo->secAxiInfo.u.wave4.useEncLfEnable<<15));

	VpuWriteReg(coreIdx, W4_CMD_ENC_SRC_PIC_IDX, 0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_CODE_OPTION, encHeaderParam->headerType);

	wave420l_Wave4BitIssueCommand(instance, ENC_PIC);
	//do
	{
		unsigned int use_interrupt_flag = INT_ENABLE_FLAG_CMD_ENC_PIC;
		int_reason = wave420l_vdi_wait_interrupt(0, 0x7FFF/*VPU_ENC_TIMEOUT*/, 0/*W4_VPU_VINT_REASON*/, 0/*W4_VPU_VPU_INT_STS*/, use_interrupt_flag );
	} //while (int_reason == -1 && __VPU_BUSY_TIMEOUT == 0);

	if (int_reason == -1) {
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}
	VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, int_reason);
	VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);

	if (instance->loggingEnable) {
		wave420l_vdi_log(coreIdx, ENC_PIC, 0);
	}

	rdPtr = VpuReadReg(coreIdx, pEncInfo->streamRdPtrRegAddr);
	wrPtr = VpuReadReg(coreIdx, pEncInfo->streamWrPtrRegAddr);
	encHeaderParam->buf  = rdPtr;
	encHeaderParam->size = VpuReadReg(coreIdx, W4_RET_ENC_PIC_BYTE);

	pEncInfo->streamWrPtr = wrPtr;
	pEncInfo->streamRdPtr = rdPtr;

	regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
	if (regVal == 0) {
		encHeaderParam->failReasonCode = VpuReadReg(coreIdx, W4_RET_FAIL_REASON);
		ret = WAVE420L_RETCODE_FAILURE;
	}
	else {
		ret = WAVE420L_RETCODE_SUCCESS;
	}

	return ret;
}

RetCode wave420l_Wave4VpuEncParaChange(EncHandle instance, EncChangeParam* param)
{
	EncInfo * pEncInfo;
	EncOpenParam *encOP;
	Int32       coreIdx, bsEndian, regVal, int_reason=0;

	pEncInfo = VPU_HANDLE_TO_ENCINFO(instance);
	encOP = &(pEncInfo->openParam);
	coreIdx = instance->coreIdx;

	regVal = wave420l_vdi_convert_endian(coreIdx, encOP->streamEndian);

	/* NOTE: When endian mode is 0, SDMA reads MSB first */
	bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

	VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | bsEndian); //VpuWriteReg(coreIdx, W4_BS_PARAM, (pEncInfo->lineBufIntEn<<6) | (pEncInfo->sliceIntEnable<<5) | (pEncInfo->ringBufferEnable<<4) | bsEndian);

	/* Secondary AXI */
	VpuWriteReg(coreIdx, W4_ADDR_SEC_AXI, pEncInfo->secAxiInfo.bufBase);
	VpuWriteReg(coreIdx, W4_SEC_AXI_SIZE, pEncInfo->secAxiInfo.bufSize);
	VpuWriteReg(coreIdx, W4_USE_SEC_AXI,  // (pEncInfo->secAxiInfo.u.wave4.useEncImdEnable<<9)    | //Wave420L does not support EncImd secondary AXI (TELMON-28)
										  (pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable<<11)   |
										  (pEncInfo->secAxiInfo.u.wave4.useEncLfEnable<<15));

	/* Set up work-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(coreIdx, W4_WORK_SIZE,      pEncInfo->vbWork.size);
	VpuWriteReg(coreIdx, W4_WORK_PARAM,     0);

	/* Set up temp-buffer */
	VpuWriteReg(coreIdx, W4_ADDR_TEMP_BASE, pEncInfo->vbTemp.phys_addr);
	VpuWriteReg(coreIdx, W4_TEMP_SIZE,      pEncInfo->vbTemp.size);
	VpuWriteReg(coreIdx, W4_TEMP_PARAM,     0);

	/* Set up reporting-buffer */
	VpuWriteReg(coreIdx, W4_CMD_ENC_ADDR_REPORT_BASE, 0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_REPORT_SIZE, 0);
	VpuWriteReg(coreIdx, W4_CMD_ENC_REPORT_PARAM, 0);

	/* change COMMON parameters */
	if (param->changeParaMode == OPT_COMMON) {
		VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_OPTION, OPT_COMMON);
		VpuWriteReg(coreIdx, W4_CMD_ENC_SET_PARAM_ENABLE, param->enable_option);

		if (param->enable_option & ENC_PIC_PARAM_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_PIC_PARAM,  (param->losslessEnable<<20)         |
									(param->constIntraPredFlag<<21)     |
									((param->chromaCbQpOffset&0x1f)<<22)|
									((param->chromaCrQpOffset&0x1f)<<27));
		}

		if (param->enable_option & ENC_INTRA_PARAM_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INTRA_PARAM, (param->decodingRefreshType<<0) |
				(param->intraQP<<3)             |
				(param->intraPeriod<<16));
		}

		if (param->enable_option & ENC_CONF_WIN_TOP_BOT_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_CONF_WIN_TOP_BOT, param->confWinBot<<16 | param->confWinTop);
		}

		if (param->enable_option & ENC_CONF_WIN_LEFT_RIGHT_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_CONF_WIN_LEFT_RIGHT, param->confWinRight<<16 | param->confWinLeft);
		}

		if (param->enable_option & ENC_FRAME_RATE_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_FRAME_RATE, param->frameRate);
		}

		if (param->enable_option & ENC_RC_INTRA_MIN_MAX_QP_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_INTRA_MIN_MAX_QP, param->intraMaxQp<<6 | param->intraMinQp);
		}
		if (param->enable_option & ENC_NUM_UNITS_IN_TICK_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_NUM_UNITS_IN_TICK, param->numUnitsInTick);
		}

		if (param->enable_option & ENC_TIME_SCALE_CHANGE) {
			VpuWriteReg(coreIdx,W4_CMD_ENC_TIME_SCALE, param->timeScale);
		}

		if (param->enable_option & ENC_INDEPENDENT_SLICE_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INDEPENDENT_SLICE, param->independSliceModeArg<<16 | param->independSliceMode);
		}

		if (param->enable_option & ENC_DEPENDENT_SLICE_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_DEPENDENT_SLICE, param->dependSliceModeArg<<16 | param->dependSliceMode);
		}

		if (param->enable_option & ENC_INTRA_REFRESH_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_SEQ_INTRA_REFRESH, param->intraRefreshArg<<16 | param->intraRefreshMode);
		}

		if (param->enable_option & ENC_PARAM_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_PARAM,  (param->useRecommendEncParam)           |
													(param->scalingListEnable<<3)           |
													(0x7 <<4)                               |   // cuSizeMode should be always 0x7 (should use all cu size (cu8x8, cu16x16, cu32x32))
													(param->tmvpEnable<<7)                  |
													(param->wppEnable<<8)                   |
													(param->maxNumMerge<<9)                 |
													(param->dynamicMerge8x8Enable<<12)      |
													(param->dynamicMerge16x16Enable<<13)    |
													(param->dynamicMerge32x32Enable<<14)    |
													(param->disableDeblk<<15)               |
													(param->lfCrossSliceBoundaryEnable<<16) |
													((param->betaOffsetDiv2&0xF)<<17)       |
													((param->tcOffsetDiv2&0xF)<<21)         |
													(param->skipIntraTrans<<25)             |
													(param->saoEnable<<26)                  |
													(param->intraInInterSliceEnable<<27)    |
													(param->intraNxNEnable<<28));
		}
		if (param->enable_option & ENC_RC_PARAM_CHANGE) {
			{
				VpuWriteReg(coreIdx, W4_CMD_ENC_RC_PARAM,   (param->rcEnable<<0)                |
															(param->cuLevelRCEnable<<1)         |
															(param->hvsQPEnable<<2)             |
															(param->hvsQpScaleEnable<<3)        |
															(param->hvsQpScale<<4)              |
															(param->bitAllocMode<<7)            |
															(param->initBufLevelx8<<9)          |
															(param->initialRcQp<<14)            |
															(param->initialDelay<<20));
			}

		}

		if (param->enable_option & ENC_RC_MIN_MAX_QP_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_MIN_MAX_QP, (param->minQp<<0)        |
														   (param->maxQp<<6)        |
														   (param->maxDeltaQp<<12)  |
														   (param->intraQpOffset << 18)) ;
		}

		if (param->enable_option & ENC_RC_TARGET_RATE_LAYER_0_3_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_BIT_RATIO_LAYER_0_3,   (param->fixedBitRatio[0]<<0)  |
																	  (param->fixedBitRatio[1]<<8)  |
																	  (param->fixedBitRatio[2]<<16) |
																	  (param->fixedBitRatio[3]<<24));
		}

		if (param->enable_option & ENC_RC_TARGET_RATE_LAYER_4_7_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_BIT_RATIO_LAYER_4_7,   (param->fixedBitRatio[4]<<0)  |
																	  (param->fixedBitRatio[5]<<8)  |
																	  (param->fixedBitRatio[6]<<16) |
																	  (param->fixedBitRatio[7]<<24));
		}
		if (param->enable_option & ENC_NR_PARAM_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_NR_PARAM,   (param->nrYEnable<<0)       |
														(param->nrCbEnable<<1)      |
														(param->nrCrEnable<<2)      |
														(param->nrNoiseEstEnable<<3)|
														(param->nrNoiseSigmaY<<4)   |
														(param->nrNoiseSigmaCb<<12) |
														(param->nrNoiseSigmaCr<<20));
		}

		if (param->enable_option & ENC_NR_WEIGHT_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_NR_WEIGHT,  (param->nrIntraWeightY<<0)  |
														(param->nrIntraWeightCb<<5) |
														(param->nrIntraWeightCr<<10)|
														(param->nrInterWeightY<<15) |
														(param->nrInterWeightCb<<20)|
														(param->nrInterWeightCr<<25));
		}

		if (param->enable_option & ENC_RC_TRANS_RATE_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_TRANS_RATE, param->transRate);
		}

		if (param->enable_option & ENC_RC_TARGET_RATE_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_RC_TARGET_RATE, param->bitRate);
		}

		if (param->enable_option & ENC_ROT_PARAM_CHANGE) {
			VpuWriteReg(coreIdx, W4_CMD_ENC_ROT_PARAM,  0);
		}

		wave420l_Wave4BitIssueCommand(instance, SET_PARAM);

		//do
		{
			unsigned int use_interrupt_flag = INT_ENABLE_FLAG_CMD_SET_PARAM;
			int_reason = wave420l_vdi_wait_interrupt(0, 0x7FFF/*VPU_ENC_TIMEOUT*/, 0/*W4_VPU_VINT_REASON*/, 0/*W4_VPU_VPU_INT_STS*/, use_interrupt_flag );
		 } //while (int_reason == -1 && __VPU_BUSY_TIMEOUT == 0);

		if (int_reason == -1) {
			if (instance->loggingEnable) {
				wave420l_vdi_log(coreIdx, SET_PARAM, 0);
			}
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, int_reason);
		VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);

		if (instance->loggingEnable) {
			wave420l_vdi_log(coreIdx, SET_PARAM, 0);
		}

		if (VpuReadReg(coreIdx, W4_RET_SUCCESS) == 0) {
			if (VpuReadReg(coreIdx, W4_RET_FAIL_REASON) == WAVE4_SYSERR_WRITEPROTECTION) {
				return RETCODE_MEMORY_ACCESS_VIOLATION;
			}
			return WAVE420L_RETCODE_FAILURE;
		}
	}

	return WAVE420L_RETCODE_SUCCESS;
}


RetCode wave420l_CheckEncCommonParamValid(EncOpenParam* pop)
{
	RetCode ret = WAVE420L_RETCODE_SUCCESS;
	Int32   low_delay = 0;
	Int32   intra_period_gop_step_size;
	Int32   i;

	EncHevcParam* param     = &pop->EncStdParam.hevcParam;

	// check low-delay gop structure
	if(param->gopPresetIdx == 0) {  // common gop
		Int32 minVal = 0;
		if (param->gopParam.customGopSize > 1) {
			minVal = param->gopParam.picParam[0].pocOffset;
			low_delay = 1;
			for (i = 1; i < param->gopParam.customGopSize; i++) {
				if (minVal > param->gopParam.picParam[i].pocOffset) {
					low_delay = 0;
					break;
				}
				else
					minVal = param->gopParam.picParam[i].pocOffset;
			}
		}
	} else if (param->gopPresetIdx == 2 ||
				param->gopPresetIdx == 6) // low-delay case (IPPP, IBBB)
				low_delay = 1;

	if(low_delay) {
		intra_period_gop_step_size = 1;
	}
	else {
		if (param->gopPresetIdx == PRESET_IDX_CUSTOM_GOP) {
			intra_period_gop_step_size = param->gopParam.customGopSize;
		}
		else {
			intra_period_gop_step_size = presetGopSize[param->gopPresetIdx];
		}
	}

	if (!low_delay && (param->intraPeriod != 0) && ((param->intraPeriod % intra_period_gop_step_size) != 0) && (param->decodingRefreshType != 0) ) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR,"CFG FAIL : Not support intra period[%d] for the gop structure\n", param->intraPeriod);
		VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Intra period = %d\n", intra_period_gop_step_size * (param->intraPeriod / intra_period_gop_step_size));
		#endif
		ret = RETCODE_FAILURE;
	}

	if (!low_delay && (param->intraPeriod != 0) && ((param->intraPeriod % intra_period_gop_step_size) == 1) && param->decodingRefreshType == 0) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR,"CFG FAIL : Not support decoding refresh type[%d] for closed gop structure\n", param->decodingRefreshType );
		VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Decoding refresh type = IDR\n");
		#endif
		ret = RETCODE_FAILURE;
	}

	if (param->gopPresetIdx == PRESET_IDX_CUSTOM_GOP) {
		for(i = 0; i < param->gopParam.customGopSize; i++)
		{
			if (param->gopParam.picParam[i].temporalId >= MAX_NUM_TEMPORAL_LAYER) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : temporalId %d exceeds MAX_NUM_TEMPORAL_LAYER\n", param->gopParam.picParam[i].temporalId);
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Adjust temporal ID under MAX_NUM_TEMPORAL_LAYER(7) in GOP structure\n");
				#endif
				ret = RETCODE_FAILURE;
			}

			if (param->gopParam.picParam[i].temporalId < 0) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : Must be %d-th temporal_id >= 0\n",param->gopParam.picParam[i].temporalId);
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Adjust temporal layer above '0' in GOP structure\n");
				#endif
				ret = RETCODE_FAILURE;
			}
		}
	}

	if (param->useRecommendEncParam == 0)
	{
		// Intra
		if (param->intraInInterSliceEnable == 0 && param->intraRefreshMode != 0) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : If intraInInterSliceEnable is '0', Intra refresh mode must be '0'\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : intraRefreshMode = 0\n");
			#endif
			ret = RETCODE_FAILURE;
		}

		// RDO
		{
			int align_32_width_flag  = pop->picWidth % 32;
			int align_16_width_flag  = pop->picWidth % 16;
			int align_8_width_flag   = pop->picWidth % 8;
			int align_32_height_flag = pop->picHeight % 32;
			int align_16_height_flag = pop->picHeight % 16;
			int align_8_height_flag  = pop->picHeight % 8;

			if (param->cuSizeMode != 0x7) {  // [CEZ-1865]
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"All CU size mode should be enabled.\n");
				#endif
				ret = RETCODE_FAILURE;
			}

			if (((param->cuSizeMode&0x1) == 0) && ((align_8_width_flag != 0) || (align_8_height_flag != 0))) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 8 pixels when enable CU8x8 of cuSizeMode \n");
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x1 (CU8x8)\n");
				#endif
				ret = RETCODE_FAILURE;
			}
			else if (((param->cuSizeMode&0x1) == 0) && ((param->cuSizeMode&0x2) == 0) && ((align_16_width_flag != 0) || (align_16_height_flag != 0))) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 16 pixels when enable CU16x16 of cuSizeMode\n");
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x2 (CU16x16)\n");
				#endif
				ret = RETCODE_FAILURE;
			}
			else if (((param->cuSizeMode&0x1) == 0) && ((param->cuSizeMode&0x2) == 0) && ((param->cuSizeMode&0x4) == 0) && ((align_32_width_flag != 0) || (align_32_height_flag != 0))) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 32 pixels when enable CU32x32 of cuSizeMode\n");
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x4 (CU32x32)\n");
				#endif
				ret = RETCODE_FAILURE;
			}
		}

		// multi-slice & wpp
		if (param->wppEnable == 1 && (param->independSliceMode != 0 || param->dependSliceMode != 0)) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : If WaveFrontSynchro(WPP) '1', the option of multi-slice must be '0'\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : independSliceMode = 0, dependSliceMode = 0\n");
			#endif
			ret = RETCODE_FAILURE;
		}
	}

	// Slice
	{
		if (param->independSliceMode == 0 && param->dependSliceMode != 0) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : If independSliceMode is '0', dependSliceMode must be '0'\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : independSliceMode = 1, independSliceModeArg = TotalCtuNum\n");
			#endif
			ret = RETCODE_FAILURE;
		}
		else if ((param->independSliceMode == 1) && (param->dependSliceMode == 1)) {
			if (param->independSliceModeArg < param->dependSliceModeArg) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : If independSliceMode & dependSliceMode is both '1' (multi-slice with ctu count), must be independSliceModeArg >= dependSliceModeArg\n");
				VLOG(ERR,"RECOMMEND CONFIG PARAMETER : dependSliceMode = 0\n");
				#endif
				ret = RETCODE_FAILURE;
			}
		}

		if (param->independSliceMode != 0) {
			if (param->independSliceModeArg > 65535) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : If independSliceMode is not 0, must be independSliceModeArg <= 0xFFFF\n");
				 #endif
				ret = RETCODE_FAILURE;
			}
		}

		if (param->dependSliceMode != 0) {
			if (param->dependSliceModeArg > 65535) {
				#if !defined(VLOG_DISABLE)
				VLOG(ERR,"CFG FAIL : If dependSliceMode is not 0, must be dependSliceModeArg <= 0xFFFF\n");
				 #endif
				ret = RETCODE_FAILURE;
			}
		}
	}

	if (param->confWinTop % 2) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR, "CFG FAIL : conf_win_top : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinTop);
		 #endif
		ret = RETCODE_FAILURE;
	}

	if (param->confWinBot % 2) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR, "CFG FAIL : conf_win_bot : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinBot);
		 #endif
		ret = RETCODE_FAILURE;
	}

	if (param->confWinLeft % 2) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR, "CFG FAIL : conf_win_left : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinLeft);
		 #endif
		ret = RETCODE_FAILURE;
	}

	if (param->confWinRight % 2) {
		#if !defined(VLOG_DISABLE)
		VLOG(ERR, "CFG FAIL : conf_win_right : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinRight);
		 #endif
		ret = RETCODE_FAILURE;
	}

	return ret;
}

RetCode wave420l_CheckEncRcParamValid(EncOpenParam* pop)
{
	RetCode       ret = RETCODE_SUCCESS;
	EncHevcParam* param     = &pop->EncStdParam.hevcParam;

	if (pop->rcEnable == 1) {
		if ((pop->bitRate > param->transRate) && (param->transRate != 0)) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : Not allowed bitRate > transRate\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : bitRate = transRate (CBR)\n");
			#endif
			ret = RETCODE_FAILURE;
		}

		if (param->minQp > param->maxQp) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : Not allowed MinQP > MaxQP\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : MinQP = MaxQP\n");
			#endif
			ret = RETCODE_FAILURE;
		}

		if (pop->bitRate <= (int) pop->frameRateInfo) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR,"CFG FAIL : Not allowed EncBitRate <= FrameRate\n");
			VLOG(ERR,"RECOMMEND CONFIG PARAMETER : EncBitRate = FrameRate * 10000\n");
			#endif
			ret = RETCODE_FAILURE;
		}
	}

	return ret;
}


