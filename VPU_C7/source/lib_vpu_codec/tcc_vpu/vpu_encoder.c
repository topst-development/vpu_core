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

#include "vpu_core.h"
#include "TCC_VPU_C7.h"
#include "vpuapi.h"
#include "vpuapifunc.h"
#include "config.h"
#include "jpegtable.h"
#include "vpu_version.h"

#define	MAX_FILE_PATH	256
typedef struct
{
	char yuvFileName[MAX_FILE_PATH];
	char cmdFileName[MAX_FILE_PATH];
	char bitstreamFileName[MAX_FILE_PATH];
	char huffFileName[MAX_FILE_PATH];
	char cInfoFileName[MAX_FILE_PATH];
	char qMatFileName[MAX_FILE_PATH];
	char qpFileName[MAX_FILE_PATH];
	char cfgFileName[MAX_FILE_PATH];
	int stdMode;
	int picWidth;
	int picHeight;
	int kbps;
	int rotAngle;
	int mirDir;
	int useRot;
	int qpReport;
	int saveEncHeader;
	int ringBufferEnable;
	int rcIntraQp;
	int mjpgChromaFormat;
	int outNum;
	int instNum;

	// 2D cache option
	int FrameCacheBypass   ;
	int FrameCacheBurst    ;
	int FrameCacheMerge    ;
	int FrameCacheWayShape ;
} EncConfigParam;

typedef struct {
	FRAME_BUF frameBuf[MAX_FRAME];
	vpu_buffer_t vb_base;
	int instIndex;
	int last_num;
	PhysicalAddress last_addr;
} fb_context;


#if defined(ARM_LINUX) || defined(ARM_WINCE)
extern codec_addr_t gVirtualBitWorkAddr;
extern codec_addr_t gVirtualRegBaseAddr;
extern codec_addr_t gFWAddr;
#endif

extern int gMaxInstanceNum;
extern unsigned int g_nFrameBufferAlignSize;

static int s_last_frame_base = 0;
static FRAME_BUF FrameBufEnc [MAX_FRAME];
static fb_context s_fb[MAX_NUM_INSTANCE];


/**
 * To init EncOpenParam by runtime evaluation
 * IN
 *   EncConfigParam *pEncConfig
 * OUT
 *   EncOpenParam *pEncOP
 */
#if 0
int getEncOpenParamDefault(EncOpenParam *pEncOP, EncConfigParam *pEncConfig)
{
	int bitFormat, ret;

	bitFormat = pEncOP->bitstreamFormat;

	pEncOP->picWidth = pEncConfig->picWidth;
	pEncOP->picHeight = pEncConfig->picHeight;
	pEncOP->frameRateInfo = 30;
	pEncOP->bitRate = pEncConfig->kbps;
	pEncOP->initialDelay = 0;
	pEncOP->vbvBufferSize = 0;			// 0 = ignore
	pEncOP->enableAutoSkip = 1;			// for compare with C-model ( C-model = only 1 )
	pEncOP->gopSize = 30;					// only first picture is I
	pEncOP->slicemode.sliceMode = 1;		// 1 slice per picture
	pEncOP->slicemode.sliceSizeMode = 1;
	pEncOP->slicemode.sliceSize = 115;
	pEncOP->intraRefresh = 0;
	pEncOP->rcIntraQp = -1;				// disable == -1
	pEncOP->userQpMax		= 0;
	pEncOP->userGamma		= (Uint32)(0.75*32768);		//  (0*32768 < gamma < 1*32768)
	pEncOP->RcIntervalMode= 1;						// 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level
	pEncOP->MbInterval	= 0;
	pEncOP->picQpY = 23;
	pEncOP->MEUseZeroPmv = 0;
	pEncOP->IntraCostWeight = 400;

	// Standard specific
	if (bitFormat == STD_AVC) {
		pEncOP->EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
		pEncOP->EncStdParam.avcParam.avc_disableDeblk = 1;
		pEncOP->EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
		pEncOP->EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;
		pEncOP->EncStdParam.avcParam.avc_chromaQpOffset = 10;
		pEncOP->EncStdParam.avcParam.avc_audEnable = 0;
		pEncOP->EncStdParam.avcParam.avc_frameCroppingFlag = 0;
		pEncOP->EncStdParam.avcParam.avc_frameCropLeft = 0;
		pEncOP->EncStdParam.avcParam.avc_frameCropRight = 0;
		pEncOP->EncStdParam.avcParam.avc_frameCropTop = 0;
		pEncOP->EncStdParam.avcParam.avc_frameCropBottom = 0;

		// Update cropping information : Usage example for H.264 frame_cropping_flag
		if (pEncOP->picHeight == 1080)
		{
			// In case of AVC encoder, when we want to use unaligned display width(For example, 1080),
			// frameCroppingFlag parameters should be adjusted to displayable rectangle
			if (pEncConfig->rotAngle != 90 && pEncConfig->rotAngle != 270) // except rotation
			{
				if (pEncOP->EncStdParam.avcParam.avc_frameCroppingFlag == 0)
				{
					pEncOP->EncStdParam.avcParam.avc_frameCroppingFlag = 1;
					// frameCropBottomOffset = picHeight(MB-aligned) - displayable rectangle height
					pEncOP->EncStdParam.avcParam.avc_frameCropBottom = 8;
				}
			}
		}
//		pEncOP->EncStdParam.avcParam.MESearchRange = 3;	//16x16
	}
	else
	{
		//LOG(ERR, "Invalid codec standard mode \n");
		return 0;
	}

	return 1;
}
#endif

static codec_result_t set_vpuc7_enc_fw_addr(vpu_c7_set_fw_addr_t *pstFWAddr) {
	codec_result_t ret = RETCODE_SUCCESS;
	if (pstFWAddr == NULL) {
		return RETCODE_INVALID_PARAM;
	} else {
		gFWAddr = pstFWAddr->m_FWBaseAddr;
		DLOGI("gFWAddr 0x%x", gFWAddr);
	}
	return RETCODE_SUCCESS;
}

static FRAME_BUF *get_frame_buffer_enc(int index)
{
	return &FrameBufEnc[index];
}

static FRAME_BUF *GetFrameBuffer_ENC(int instIdx, int index)
{
	fb_context *fb;
	fb = &s_fb[instIdx];
	return &fb->frameBuf[index];
}

int AllocateFrameBuffer_enc(
			    PhysicalAddress m_FrameBufferStartPAddr,	//unused param.
			    int instIdx,
			    int stdMode,	//unused param.
			    int format, int width, int height,
			    int frameBufNum,
			    int mvCol,  	//unused param.
			    int cbcrinterleave,
			    unsigned int bufAlignSize,
			    unsigned int bufStartAddrAlign)
{
	int chr_hscale, chr_vscale;
	int size_dpb_lum, size_dpb_chr, size_dpb_all;
	int chr_size_y, chr_size_x;
	int  i;
	fb_context *fb;

	fb = &s_fb[instIdx];

	switch (format)
	{
	case FORMAT_420:
		height = (height+1)/2*2;
		width = (width+1)/2*2;
		break;
	case FORMAT_224:
		height = (height+1)/2*2;
		break;
	case FORMAT_422:
		width = (width+1)/2*2;
		break;
	case FORMAT_444:
		break;
	case FORMAT_400:
		height = (height+1)/2*2;
		width = (width+1)/2*2;
		break;
	}

	chr_hscale = format == FORMAT_420 || format == FORMAT_422 ? 2 : 1;
	chr_vscale = format == FORMAT_420 || format == FORMAT_224 ? 2 : 1;

	chr_size_y = (height/chr_hscale);
	chr_size_x = (width/chr_vscale);

	size_dpb_lum   = width * height;
	size_dpb_chr   = chr_size_y * chr_size_x;
	size_dpb_all = format != FORMAT_400 ? (size_dpb_lum + size_dpb_chr*2) : size_dpb_lum;
	size_dpb_all = VALIGN(size_dpb_all, bufAlignSize);

	//size_mvcolbuf = ((size_dpb_lum + 2*size_dpb_chr)+3)/5; // round up by 5
	//size_mvcolbuf = ((size_mvcolbuf + 7)/8)*8 + 1024; // round up by 8

	fb->vb_base.size = size_dpb_all;
	fb->vb_base.size *= frameBufNum;
	fb->vb_base.phys_addr = s_last_frame_base;
	fb->last_addr = fb->vb_base.phys_addr;

	for (i=fb->last_num; i<fb->last_num+frameBufNum; i++)
	{
		fb->frameBuf[i].Format = format;
		fb->frameBuf[i].mapType = LINEAR_FRAME_MAP;
		fb->frameBuf[i].Index  = i;
		fb->frameBuf[i].stride_lum = width;
		fb->frameBuf[i].height  = height;
		fb->frameBuf[i].stride_chr = width/chr_hscale;

		fb->frameBuf[i].vbY.phys_addr = fb->last_addr;
		fb->frameBuf[i].vbY.size = size_dpb_lum;

		fb->last_addr += fb->frameBuf[i].vbY.size;

		fb->frameBuf[i].vbCb.phys_addr = fb->last_addr;
		fb->frameBuf[i].vbCb.size = size_dpb_chr;

		fb->last_addr += fb->frameBuf[i].vbCb.size;

		if (!cbcrinterleave)
		{
			fb->frameBuf[i].vbCr.phys_addr = fb->last_addr;
			fb->frameBuf[i].vbCr.size = size_dpb_chr;
		}
		else
		{
			fb->frameBuf[i].vbCr.phys_addr = 0;
			fb->frameBuf[i].vbCr.size = 0;
		}
		fb->last_addr += size_dpb_chr;
		fb->last_addr = (fb->last_addr+7)/8*8;
	}

	fb->last_num += frameBufNum;
	s_last_frame_base = fb->last_addr;

	return 1;
}

void FreeFrameBuffer(int instIdx)
{
	fb_context *fb;
	fb = &s_fb[instIdx];

	fb->last_num = 0;
	fb->vb_base.size = 0;
	fb->last_addr = 0;
}

static RetCode
enc_reset(vpu_t *pVpu, int iOption)
{
	#ifdef USE_CODEC_PIC_RUN_INTERRUPT
	if( iOption & (1   ) )
	{
		VpuWriteReg(BIT_INT_REASON, 0);
		VpuWriteReg(BIT_INT_CLEAR, 1);
	}
	#endif

	SetPendingInst(0);

	{
		int i;
		for (i=0 ; i<gMaxInstanceNum ; i++)
			FreeFrameBuffer(i);
	}

	reset_vpu_global_var( (iOption>>16) & 0x0FF );	//reset global var.

	return RETCODE_SUCCESS;
}

static RetCode
enc_init(vpu_t **ppVpu, enc_init_t *pInitParam, enc_initial_info_t *pInitialInfo)
{
	RetCode ret;
	EncHandle handle = { 0 };
	unsigned int bufAlignSize, bufStartAddrAlign;

#ifdef USE_EXTERNAL_FW_WRITER
	int fw_writing_disable = 0;
	PhysicalAddress codeBase;

	fw_writing_disable = (pInitParam->m_uiEncOptFlags >> FW_WRITING_DISABLE_SHIFT) & 0x1;
	codeBase = (PhysicalAddress)gFWAddr;

	if ((fw_writing_disable == 1) && (codeBase == 0)) {
		DLOGE("fw_writing_disable is setted, but codeBase is NULL");
		return RETCODE_WRONG_CALL_SEQUENCE;
	}
#endif

	//! [0] Global callback func
	{
		vpu_memcpy = pInitParam->m_Memcpy;
		vpu_memset = pInitParam->m_Memset;
		vpu_interrupt = pInitParam->m_Interrupt;
		vpu_ioremap = pInitParam->m_Ioremap;
		vpu_iounmap = pInitParam->m_Iounmap;
		vpu_read_reg = pInitParam->m_reg_read;
		vpu_write_reg = pInitParam->m_reg_write;
	#ifdef MEM_FN_REPLACE_INTERNAL_FN
		if (vpu_memcpy == 0)
			vpu_memcpy = vpu_local_mem_cpy;

		if (vpu_memset == 0)
			vpu_memset = (vpu_memset_func*)vpu_local_mem_set;
	#endif
	}

	//! [1] VPU Init
	if ((gInitFirst == 0) || (VpuReadReg(BIT_CUR_PC)==0))
	{
		//gInitFirst = 1;
		gInitFirst = check_aarch_vpu_c7();	//4 for ARM32, 8 for ARM64

	#if defined(ARM_LINUX) || defined(ARM_WINCE)
		gVirtualBitWorkAddr = pInitParam->m_BitWorkAddr[VA];
		gVirtualRegBaseAddr = pInitParam->m_RegBaseVirtualAddr;
	#endif

		gMaxInstanceNum = MAX_NUM_INSTANCE;
	#ifdef USE_MAX_INSTANCE_NUM_CTRL
		if ( (pInitParam->m_uiEncOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF )
			gMaxInstanceNum = (pInitParam->m_uiEncOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF;
		if (gMaxInstanceNum > MAX_NUM_INSTANCE) {
			gMaxInstanceNum = MAX_NUM_INSTANCE;
			return RETCODE_INVALID_PARAM;
		}
	#endif

		//reset the parameter buffer
		{
			char * pInitParaBuf;
		#if defined(ARM_LINUX) || defined(ARM_WINCE)
			pInitParaBuf = (char*)gVirtualBitWorkAddr;
		#else
			pInitParaBuf = (char*)pInitParam->m_BitWorkAddr[PA];
		#endif

			vpu_memset( pInitParaBuf, 0, PARA_BUF_SIZE, ((gInitFirst == 4)?(0):(1)));	//used mem. : <= 2Kbytes,
		}


	#ifndef USE_EXTERNAL_FW_WRITER
		ret = VPU_Init(pInitParam->m_BitWorkAddr[PA]);
	#else
		ret = VPU_Init(pInitParam->m_BitWorkAddr[PA], codeBase, fw_writing_disable);
	#endif
		if ((ret != RETCODE_SUCCESS) && (ret != RETCODE_CALLED_BEFORE))
		{
			if (ret == RETCODE_CODEC_EXIT)
			{
				gInitFirst = 0;
			#ifdef ADD_SWRESET_VPU_HANGUP
				MACRO_VPU_SWRESET
				enc_reset(NULL, (0 | (1 << 16)));
			#endif
				return RETCODE_CODEC_EXIT;
			} else {
				gInitFirst = 0;
			}

			return ret;
		}
	}
#ifdef ADD_VPU_INIT_SWRESET
	else
	{
	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		ret = VPU_SWReset2();//VPU_SWReset( SW_RESET_SAFETY );
		if (ret == RETCODE_CODEC_EXIT)
		{
			enc_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
	#else
		VPU_SWReset2();//VPU_SWReset( SW_RESET_SAFETY );
	#endif
	}
#endif

	//! [2] Get Enc Handle
	ret = get_vpu_handle(ppVpu);
	if (ret == RETCODE_FAILURE)
	{
		*ppVpu = 0;
		return RETCODE_HANDLE_FULL;
	}

	set_register_bit((pInitParam->m_bCbCrInterleaveMode << 2), BIT_FRAME_MEM_CTRL);

	#ifdef USE_HW_AND_FW_VERSION
	{
		unsigned int version = 0, revision = 0, productId = 0;
		unsigned int HwRevision = 0, HwDate = 0;
		int iRet;

		//get F/W version
		iRet = VPU_GetVersionInfo(&version, &revision, &productId);
		if (iRet != RETCODE_SUCCESS)
		{
			if (iRet == RETCODE_CODEC_EXIT)
			{
				enc_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
			ret  = (RetCode)iRet;
			return ret;
		}

		(*ppVpu)->m_iVersionFW = version;
		(*ppVpu)->m_iCodeRevision = revision;
		(*ppVpu)->m_iHwRevision = HwRevision = VpuReadReg(GDI_HW_CFG_REV);
		(*ppVpu)->m_iHwDate = HwDate = VpuReadReg(GDI_CFG_DATE);
		(*ppVpu)->m_iVersionRTL = version = VpuReadReg(GDI_HW_VERINFO);

		//get RTL version
		// val          Description
		//0x0000        v2.1.1 previous Hardware (v1.1.1)
		//0x0211        v2.1.1 Hardware
		//0x0212        v2.1.2 Hardware
		//0x00020103    v2.1.3 Hardware
		//0x0002040a    v2.4.10 Hardware

	#	if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
		if ((*ppVpu)->m_iVersionRTL != 0)
		{
			if (HwDate == 20140717)
				(*ppVpu)->m_iVersionRTL = 3;
			else
				(*ppVpu)->m_iVersionRTL = 2;
		}
	#	else
		(*ppVpu)->m_iVersionRTL = 3;
	#	endif
	}
	#endif

	//! [3] Open Encoder
	{
		// EncOpenParam op = {0,};
		#define op (*ppVpu)->m_openParam

		op.bitstreamBuffer = pInitParam->m_BitstreamBufferAddr;
		op.bitstreamBufferSize = pInitParam->m_iBitstreamBufferSize;
		op.bitstreamFormat = pInitParam->m_iBitstreamFormat;
		op.picWidth = pInitParam->m_iPicWidth;
		op.picHeight = pInitParam->m_iPicHeight;
		op.frameRateInfo = pInitParam->m_iFrameRate;
		op.bitRate = pInitParam->m_iTargetKbps;
		op.initialDelay = 0;
		op.vbvBufferSize = 0;
		op.enableAutoSkip = 0;//1;
		op.gopSize = pInitParam->m_iKeyInterval;
		op.me_blk_mode = 0;
		op.userQpMax = 0;
		op.MbInterval = 0;
		op.me_blk_mode = 0;
		op.rcIntraQp = -1;
		op.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
		op.mapType = 0;
		op.linear2TiledEnable = 0;
		op.enhancedRCmodelEnable = pInitParam->m_uiEncOptFlags & VPU_ENHANCED_RC_MODEL_ENABLE;

	#ifdef H264_ENC_IDR_OPTION
		if (pInitParam->m_iBitstreamFormat == STD_AVC)	//if ((pInitParam->m_iBitstreamFormat == STD_AVC) && (pInitParam->m_uiEncOptFlags & H264_ENC_IDR_OPTION_ENALBE))
			op.h264IdrEncOption = 1;
		else
			op.h264IdrEncOption = 0;
	#endif
		if(pInitParam->m_iUseSpecificRcOption)
		{
			op.slicemode.sliceMode = pInitParam->m_stRcInit.m_iSliceMode;
			op.slicemode.sliceSizeMode = pInitParam->m_stRcInit.m_iSliceSizeMode;
			op.slicemode.sliceSize = pInitParam->m_stRcInit.m_iSliceSize;
			op.intraRefresh = pInitParam->m_stRcInit.m_iIntraMBRefresh;
			op.vbvBufferSize = pInitParam->m_stRcInit.m_iVbvBufferSize;

			op.rcIntraQp = pInitParam->m_stRcInit.m_iPicQpY;
			if ((pInitParam->m_stRcInit.m_iPicQpY> 0) && (pInitParam->m_stRcInit.m_iPicQpY<52)) //Notice : > 0
				op.picQpY = pInitParam->m_stRcInit.m_iPicQpY;

			op.RcIntervalMode = pInitParam->m_stRcInit.m_iRCIntervalMode;
			if(op.RcIntervalMode == 3)
				op.MbInterval	= pInitParam->m_stRcInit.m_iRCIntervalMBNum;
			else
				op.MbInterval	= 0;

			op.MESearchRange = pInitParam->m_stRcInit.m_iSearchRange;
			op.MEUseZeroPmv = pInitParam->m_stRcInit.m_iPVMDisable;
			op.IntraCostWeight = pInitParam->m_stRcInit.m_iWeightIntraCost;

			op.userQpMax = 0;
			if (pInitParam->m_stRcInit.m_iEncQualityLevel > 0) {
				int iPicQpMax, iEncQualityLevel;

				iPicQpMax = ((pInitParam->m_stRcInit.m_iEncQualityLevel >> 16) & 0x0FF);
				if (iPicQpMax > 51)
					iPicQpMax = 0;

				iEncQualityLevel = (pInitParam->m_stRcInit.m_iEncQualityLevel & 0x0FF);
				if (iEncQualityLevel >  35)
					iEncQualityLevel = 0;

				if (op.bitstreamFormat == STD_AVC) {
					if ((iPicQpMax >= 13) && (iPicQpMax <= 51)) { //CNM guide 13 ~ 51   : QP(0 ~ 51)
						op.userQpMax = iPicQpMax;
					} else {
						if (iEncQualityLevel > 0) {
							//1~35 default 11 (QpMax 50~16 defulat 40)
							//if ((iEncQualityLevel >=1)  || (iEncQualityLevel <= 35))
								op.userQpMax = 51 - iEncQualityLevel;
							//else
							//	op.userQpMax = 40; //default (=51 - 11)
						}
					}
				}
			}
		} else {
			op.slicemode.sliceMode = 0;
			op.slicemode.sliceSizeMode = 0;
			op.slicemode.sliceSize = 0;
			op.intraRefresh = 0;
			op.vbvBufferSize = 0;

			op.picQpY = 23;
			op.RcIntervalMode = 1;
			op.MESearchRange = 3;
			op.MEUseZeroPmv = 0;
			op.IntraCostWeight = 400;
			op.userGamma		= (unsigned int)(0.75*32768);		//  (0*32768 < gamma < 1*32768)
			op.RcIntervalMode= 1;						// 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level
		}

		if (op.enhancedRCmodelEnable)
		{
			op.vbvBufferSize = 0;
			op.userGamma = 0;
			op.MbInterval = 0;
			op.RcIntervalMode = 0;
			op.initialDelay = 1000;
		}

		if (op.mapType == TILED_FRAME_MB_RASTER_MAP)
		{
			//op.cbcrInterleave = 1;
			return RETCODE_INVALID_MAP_TYPE;
		}

		(*ppVpu)->m_iBitstreamFormat = op.bitstreamFormat;

		op.ringBufferEnable = 0;

	#ifdef USE_ENC_RESOLUTION_CONSTRAINTS
		if (op.picWidth < VPU_ENC_MIM_HORIZONTAL || (op.picWidth & 0x0F))
		{
			return RETCODE_INVALID_STRIDE;
		}
		if (op.picHeight < VPU_ENC_MIM_VERTICAL || (op.picHeight & 0x0F))
		{
			return RETCODE_INVALID_STRIDE;
		}
	#endif

		if (op.bitstreamFormat == STD_AVC)
		{
			int crL, crR, crT, crB, crFlag;
			int iWidth, iHeight;

			iWidth = op.picWidth;
			iHeight = op.picHeight;
			crL = crR = crT = crB = crFlag = 0;

			op.EncStdParam.avcParam.avc_audEnable = 0;
			if(pInitParam->m_iUseSpecificRcOption)
			{
				op.EncStdParam.avcParam.avc_disableDeblk = pInitParam->m_stRcInit.m_iDeblkDisable;
				op.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = pInitParam->m_stRcInit.m_iDeblkAlpha;
				op.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = pInitParam->m_stRcInit.m_iDeblkBeta;
				op.EncStdParam.avcParam.avc_chromaQpOffset = pInitParam->m_stRcInit.m_iDeblkChQpOffset;
				op.EncStdParam.avcParam.avc_constrainedIntraPredFlag = pInitParam->m_stRcInit.m_iConstrainedIntra;
			}
			else
			{
				op.EncStdParam.avcParam.avc_disableDeblk = 0;//2;
				op.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
				op.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;//-2;
				op.EncStdParam.avcParam.avc_chromaQpOffset = 10;//0;
				op.EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
			}

		#ifdef USE_HOST_PROFILE_LEVEL
			op.EncStdParam.avcParam.avc_level = LevelCalculationAvc(op.picWidth, op.picHeight, op.frameRateInfo, op.bitRate, op.slicemode.sliceSize);
		#endif

		#ifdef USER_OVERRIDE_H264_PROFILE_LEVEL
			if (pInitParam->m_stRcInit.m_iOverrideProfileLevel > 0)
			{
				op.EncStdParam.avcParam.avc_level = pInitParam->m_stRcInit.m_iOverrideProfileLevel;
			}
		#endif

			if (iWidth & 15)
			{
				//op.picWidth  = (iWidth + 15) & ~(15);
				crR = 16 - (iWidth & 15);
				crFlag = 1;
			}

			if (iHeight & 15)
			{
				//op.picHeight = (iHeight + 15) & ~(15);
				crB = 16 - (iHeight & 15);
				crFlag = 1;
			}

			op.EncStdParam.avcParam.avc_frameCropLeft = crL;
			op.EncStdParam.avcParam.avc_frameCropRight = crR;
			op.EncStdParam.avcParam.avc_frameCropTop = crT;
			op.EncStdParam.avcParam.avc_frameCropBottom = crB;
			op.EncStdParam.avcParam.avc_frameCroppingFlag = crFlag;
			#ifdef CMD_ENC_SEQ_264_VUI_PARA //soo_202409
			{
				#define TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT 0	//0, 14, 15
				unsigned int vuiParamOption = 0;
				unsigned char video_signal_type_pres_flag, video_format_present_flag, video_full_range_flag, colour_descrip_pres_flag;
				unsigned char video_format, colour_primaries, transfer_characteristics, matrix_coeff;
				VUI_PARAM *pVuiParam = &op.EncStdParam.avcParam.avc_vui_param;

				// init.
				video_signal_type_pres_flag = 0;
				video_format_present_flag = 0;	// FIXME : API ?
				video_format = 5;		// 5: (default) Unspecified video format (refer to Table E-2 of H.264 or H.265 video spec.)
				video_full_range_flag = 0;	// 0: (default)
				colour_descrip_pres_flag = 0;	// 0: (default)
				colour_primaries = 2;		// 2: (default) Unspecified (refer to Table E-3 of H.264 or H.265 video spec.)
				transfer_characteristics = 2;	// 2: (default) Unspecified (refer to Table E-4 of H.264 or H.265 video spec.)
				matrix_coeff = 2;		// 2: (default) Unspecified (refer to Table E-5 of H.264 or H.265 video spec.)


			#if TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT < 10
				vuiParamOption = pInitParam->m_uiVuiParamOption;
			#else
				vuiParamOption = 1;	//enable test mode
			#endif

				if (vuiParamOption & 1)
				{
					// video_signal_type_present_flag and color_description_present_flag
					//  Bit [0] video_signal_type_present_flag
					//       0 : If the 'video_signal_type_present_flag' equals zero, there's no VUI in SPS.
					//       1 : encode vui info.
					//  Bit [2] color_description_present_flag
					//       0 : If the 'color_description_present_flag' equals zero, there's no color description info.
					//           (color_primaries, transfer_characteristics, matrix_coeffs).
					//       1 : encode color description info.
				#	if TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT < 10
					video_signal_type_pres_flag = pInitParam->m_stVuiParam.m_iAvcVuiVideoSignalPresentFlags & 0x1;
					video_format_present_flag = 1;	//(pInitParam->m_stVuiParam.m_iAvcVuiVideoSignalPresentFlags >> 1)  & 0x1;
					colour_descrip_pres_flag = (pInitParam->m_stVuiParam.m_iAvcVuiVideoSignalPresentFlags >> 2)  & 0x1;	//1 bit
					video_full_range_flag = pInitParam->m_stVuiParam.m_iAvcVuiVideoFullRangeFlag & 0x1;	// 1 bit
				#	else
					video_signal_type_pres_flag = 1;
					video_format_present_flag = 1;
					#if TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT == 14
					video_full_range_flag = 1;
					#elif  TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT == 15
					video_full_range_flag = 0;
					#endif
					colour_descrip_pres_flag = 1;
				#	endif
					if (video_signal_type_pres_flag)
					{
						if (video_format_present_flag)
						{
						#	if TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT < 10
							video_format = pInitParam->m_stVuiParam.m_iAvcVuiVideoFormat & 0x7;	//3bits
						#	else
							video_format = 5;
						#	endif
						}

						if (colour_descrip_pres_flag)
						{
						#	if TEST_AVC_VUI_VIDEO_SIGNAL_INFO_ENC_INIT < 10
							colour_primaries = (unsigned char)(pInitParam->m_stVuiParam.m_iAvcVuiColourPrimaries & 0x0FF);	//8bits
							transfer_characteristics = (unsigned char)(pInitParam->m_stVuiParam.m_iAvcVuiTransferCharacteristics & 0x0FF);	//8bits
							matrix_coeff = (unsigned char)(pInitParam->m_stVuiParam.m_iAvcVuiMatrixCoefficients & 0x0FF);	//8bits
						#	else
							colour_primaries = 0xa1;
							transfer_characteristics = 0xa2;
							matrix_coeff = 0xa3;
						#	endif
						}
					} else {
						video_format_present_flag = 0;	// FIXME : API ?
						video_format = 5;		// 5: (default) Unspecified video format (refer to Table E-2 of H.264 or H.265 video spec.)
						video_full_range_flag = 0;	// 0: (default)
						colour_descrip_pres_flag = 0;	// 0: (default)
						colour_primaries = 2;		// 2: (default) Unspecified (refer to Table E-3 of H.264 or H.265 video spec.)
						transfer_characteristics = 2;	// 2: (default) Unspecified (refer to Table E-4 of H.264 or H.265 video spec.)
						matrix_coeff = 2;		// 2: (default) Unspecified (refer to Table E-5 of H.264 or H.265 video spec.)
					}
				}

				if (colour_primaries != 2)
					colour_descrip_pres_flag = 1;
				if (transfer_characteristics != 2)
					colour_descrip_pres_flag = 1;
				if (matrix_coeff != 2)
					colour_descrip_pres_flag = 1;

				if (video_format != 5)
					video_signal_type_pres_flag = 1;
				if (video_full_range_flag != 0)
					video_signal_type_pres_flag = 1;
				if (colour_descrip_pres_flag)
					video_signal_type_pres_flag = 1;

				pVuiParam->video_signal_type_pres_flag = video_signal_type_pres_flag;
				pVuiParam->video_format = video_format;
				pVuiParam->video_full_range_flag = video_full_range_flag;
				pVuiParam->colour_descrip_pres_flag = colour_descrip_pres_flag;
				pVuiParam->colour_primaries = colour_primaries;
				pVuiParam->transfer_characteristics = transfer_characteristics;
				pVuiParam->matrix_coeff = matrix_coeff;

				op.EncStdParam.avcParam.avc_vui_present_flag = video_signal_type_pres_flag;	//FIXME : if need to encode more vui info.
			}
			#endif
		}

		{
			PhysicalAddress workbufaddr;
			workbufaddr = pInitParam->m_BitWorkAddr[PA] + TEMP_BUF_SIZE + PARA_BUF_SIZE + CODE_BUF_SIZE + SEC_AXI_BUF_SIZE;
			op.workBuffer = workbufaddr+((*ppVpu)->m_iVpuInstIndex*(WORK_BUF_SIZE));
			op.workBufferSize = WORK_BUF_SIZE;
		}

		if( op.bitstreamFormat != STD_AVC )
			op.userQpMax = 0;

		ret = VPU_EncOpen( &handle, &op );

		if (ret != RETCODE_SUCCESS)
			return ret;

		//VPU_EncGiveCommand(handle, ENC_SET_SLICE_INFO, &op.slicemode); //soo before startOneFrame

		FreeFrameBuffer(handle->instIndex);	// soo

		(*ppVpu)->m_pCodecInst = handle;
		(*ppVpu)->m_iPicWidth  = op.picWidth;
		(*ppVpu)->m_iPicHeight = op.picHeight;

		(*ppVpu)->m_iFrameBufWidth = (op.picWidth + 15) & ~15;
		(*ppVpu)->m_iFrameBufHeight = (op.picHeight + 15) & ~15;

		(*ppVpu)->m_StreamRdPtrRegAddr = handle->CodecInfo.encInfo.streamRdPtrRegAddr;
		(*ppVpu)->m_StreamWrPtrRegAddr = handle->CodecInfo.encInfo.streamWrPtrRegAddr;
		(*ppVpu)->m_uiEncOptFlags = pInitParam->m_uiEncOptFlags;

		#undef op
	}

	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useBitEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufBitUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useIpEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufIpAcDcUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useDbkYEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufDbkYUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useDbkCEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufDbkCUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useOvlEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufOvlUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.useBtpEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.encInfo.secAxiUse.bufBtpUse = 0;
	(*ppVpu)->m_pCodecInst->m_iVersionRTL = (*ppVpu)->m_iVersionRTL;

#ifdef USE_SEC_AXI_SRAM
	if ((*ppVpu)->m_pCodecInst->codecMode == STD_AVC)
	{
		PhysicalAddress secAxiFlag, secAXIaddr;

		secAxiFlag = (*ppVpu)->m_uiEncOptFlags & SEC_AXI_ENABLE_CTRL;

		if (1) //secAxiFlag
		{
			secAxiFlag >>= SEC_AXI_ENABLE_CTRL_SHIFT;
		//#ifdef USE_SEC_AXI_SRAM_BASE_ADDR
		//	secAXIaddr = USE_SEC_AXI_SRAM_BASE_ADDR;
		//#endif

			//if( secAxiFlag == 3 ) //test
			{
				secAXIaddr = (*ppVpu)->m_BitWorkAddr[PA] + TEMP_BUF_SIZE + PARA_BUF_SIZE + CODE_BUF_SIZE;// + (WORK_BUF_SIZE*MAX_NUM_INSTANCE);
			}

			if( secAXIaddr&0x0FF ) //256 byte aligned
			{
				secAXIaddr = (secAXIaddr+0x0FF)& ~(0x0FF);
			}

		#ifdef USE_SEC_AXI_BIT
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useBitEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBitUse = secAXIaddr;
			secAXIaddr += BIT_INTERNAL_USE_BUF_SIZE_FULLHD;
		#endif

		#ifdef USE_SEC_AXI_IPACDC
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useIpEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufIpAcDcUse = secAXIaddr;
			secAXIaddr += IPACDC_INTERNAL_USE_BUF_SIZE_FULLHD;
		#endif

		#ifdef USE_SEC_AXI_DBKY
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkYEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkYUse = secAXIaddr;
			secAXIaddr += DBKY_INTERNAL_USE_BUF_SIZE_FULLHD;
		#endif

			//if( secAxiFlag & 1 )
			{
			#ifdef USE_SEC_AXI_DBKC
				(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkCEnable = 1;
				(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkCUse = secAXIaddr;
				secAXIaddr += DBKC_INTERNAL_USE_BUF_SIZE_FULLHD;
			#endif
				if( (*ppVpu)->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamFormat == STD_VC1 )
				{
				#ifdef USE_SEC_AXI_OVL
					(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useOvlEnable = 1;
					(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufOvlUse = secAXIaddr;
					secAXIaddr += OVL_INTERNAL_USE_BUF_SIZE_FULLHD;
				#endif

				#ifdef USE_SEC_AXI_BTP
					(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useBtpEnable = 1;
					(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBtpUse = secAXIaddr;
					secAXIaddr += BTP_INTERNAL_USE_BUF_SIZE_FULLHD;
				#endif
				}
			}
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufSize = USE_SEC_AXI_SIZE;
		}
	}
#endif //USE_SEC_AXI_SRAM

	// [4] Update Buffer aligned info. - check aligned size & address of buffers (frame buffer....)
	{
		bufAlignSize = g_nFrameBufferAlignSize;
		bufStartAddrAlign = g_nFrameBufferAlignSize;

		if (bufAlignSize < VPU_C7_MIN_BUF_SIZE_ALIGN) {
			bufAlignSize = VPU_C7_MIN_BUF_SIZE_ALIGN;
		}

		if (bufStartAddrAlign < VPU_C7_MIN_BUF_START_ADDR_ALIGN) {
			bufStartAddrAlign = VPU_C7_MIN_BUF_START_ADDR_ALIGN;
		}

		(*ppVpu)->m_pCodecInst->bufAlignSize = bufAlignSize;
		(*ppVpu)->m_pCodecInst->bufStartAddrAlign = bufStartAddrAlign;
	}

	//! [5] Get Initial Info
	{
		EncInitialInfo initialInfo = {0,};
		int addFrameBufCount;

		ret = VPU_EncGetInitialInfo(handle, &initialInfo);
		if( ret != RETCODE_SUCCESS )
		{
		#ifdef ADD_SWRESET_VPU_HANGUP
			if (ret == RETCODE_CODEC_EXIT)
			{
				MACRO_VPU_SWRESET
				enc_reset(NULL, (0 | (1<<16)));
			}
		#endif
			return ret;
		}

		// output
		addFrameBufCount = 3;
		if ( (*ppVpu)->m_pCodecInst->codecMode == MJPG_ENC )
			addFrameBufCount = 2;

		(*ppVpu)->m_iNeedFrameBufCount = pInitialInfo->m_iMinFrameBufferCount = initialInfo.minFrameBufferCount;
		pInitialInfo->m_iMinFrameBufferCount += addFrameBufCount;

		{
			int framebufFormat, picWidth, picHeight, mapType;
			int framebufsize;

			if ( (*ppVpu)->m_pCodecInst->codecMode == MJPG_ENC )
				framebufFormat = (*ppVpu)->m_pCodecInst->CodecInfo.encInfo.jpgInfo.format;
			else
				framebufFormat = FORMAT_420;

			picWidth  = (*ppVpu)->m_iPicWidth;
			picHeight = (*ppVpu)->m_iPicHeight;
			mapType = (*ppVpu)->m_pCodecInst->CodecInfo.encInfo.openParam.mapType;
			framebufsize = EncGetFrameBufSize(framebufFormat, picWidth, picHeight, mapType);

			// fb[0] = frame[0]
			// fb[1] = frame[1]
			// fb[2] = subSampBaseA
			// fb[3] = subSampBaseB
			// fb[4] = scatch buffer for MPEG-4

		#if 1	//case minFrameBufferCount = 1 with total_size
			framebufsize *= pInitialInfo->m_iMinFrameBufferCount;
			pInitialInfo->m_iMinFrameBufferCount = 1;	//Since the frame count has already been reflected when calculating the size, '1' is returned.
		#else	//case minFrameBufferCount = 5 with  one frame_buffer_size
		#endif

			framebufsize += bufStartAddrAlign;
			framebufsize = VALIGN(framebufsize, bufAlignSize);
			pInitialInfo->m_iMinFrameBufferSize = framebufsize;
		}
	}

	(*ppVpu)->m_iBitstreamOutBufferPhyVirAddrOffset = pInitParam->m_BitstreamBufferAddr - pInitParam->m_BitstreamBufferAddr_VA;

	return ret;
}


static RetCode
enc_register_frame_buffer(vpu_t *pVpu, enc_buffer_t *pBufferParam)
{
	RetCode ret;
	EncHandle handle = pVpu->m_pCodecInst;

	int stride = pVpu->m_iPicWidth;
	int needFrameBufCount = pVpu->m_iNeedFrameBufCount;
	int addFrameBufCount;
	int i, pic_width, pic_height, framebufFormat;
	unsigned int bufAlignSize = 0, bufStartAddrAlign = 0;

	FrameBuffer *p_frame_buf_array = pVpu->m_stFrameBuffer[PA];

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_FrameBufferStartAddr[PA];
	pVpu->m_FrameBufferStartVAddr = pBufferParam->m_FrameBufferStartAddr[VA];

	bufAlignSize = pVpu->m_pCodecInst->bufAlignSize;
	bufStartAddrAlign = pVpu->m_pCodecInst->bufStartAddrAlign;

	framebufFormat = 0;	//FORMAT_420
	{
		pic_width = pVpu->m_iPicWidth;
		pic_height = pVpu->m_iPicHeight;

		pic_width  = (pic_width  + 15) & (~0x0F);	//16 bytes alig.
		pic_height = (pic_height + 15) & (~0x0F);	//16 bytes alig.

		// Initialize frame buffers for encoding and source frame
		{
			PhysicalAddress Addr = (PhysicalAddress)pBufferParam->m_FrameBufferStartAddr[PA];

			Addr = VALIGN(Addr, bufStartAddrAlign);
			s_last_frame_base = Addr;
			//s_last_frame_base = pVpu->m_FrameBufferStartPAddr;
		}

		addFrameBufCount = 3;

		if ( pVpu->m_pCodecInst->CodecInfo.encInfo.openParam.mapType != LINEAR_FRAME_MAP )
			return RETCODE_INVALID_MAP_TYPE;

		AllocateFrameBuffer_enc(	pVpu->m_FrameBufferStartPAddr, pVpu->m_iVpuInstIndex, pVpu->m_iBitstreamFormat,
						framebufFormat, pic_width, pic_height,
						(needFrameBufCount+addFrameBufCount),
						0,
						pVpu->m_pCodecInst->CodecInfo.encInfo.openParam.cbcrInterleave,
						bufAlignSize, bufStartAddrAlign);

		for( i = 0; i < (needFrameBufCount + addFrameBufCount); ++i )
		{
			FRAME_BUF * pFrame = GetFrameBuffer_ENC(pVpu->m_iVpuInstIndex, i);

			pVpu->m_stFrameBuffer[PA][i].myIndex = i;
			pVpu->m_stFrameBuffer[PA][i].stride = pFrame->stride_lum;
			pVpu->m_stFrameBuffer[PA][i].bufY  = pFrame->vbY.phys_addr;
			pVpu->m_stFrameBuffer[PA][i].bufCb = pFrame->vbCb.phys_addr;
			pVpu->m_stFrameBuffer[PA][i].bufCr = pFrame->vbCr.phys_addr;
		}
	}

	{
		PhysicalAddress	subSampBaseA, subSampBaseB;
		ExtBufCfg scratchBuf;

		subSampBaseA = pVpu->m_stFrameBuffer[PA][needFrameBufCount + 0].bufY;
		subSampBaseB = pVpu->m_stFrameBuffer[PA][needFrameBufCount + 1].bufY;
		scratchBuf.bufferBase = pVpu->m_stFrameBuffer[PA][needFrameBufCount + 2].bufY;
		scratchBuf.bufferSize = VPU_MP4ENC_DATA_PARTITION_SIZE;

		// Register frame buffers requested by the encoder.
		ret = VPU_EncRegisterFrameBuffer(handle, p_frame_buf_array, needFrameBufCount, stride,
							subSampBaseA, subSampBaseB, &scratchBuf);

	}

	if (ret != RETCODE_SUCCESS)
	{
	#ifdef ADD_SWRESET_VPU_HANGUP
		if (ret == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_SWRESET
			enc_reset( NULL, (0 | (1<<16)));
		}
	#endif

		return ret;
	}

	return ret;
}

static int
enc_encode(vpu_t *pVpu, enc_input_t *pInputParam, enc_output_t *pOutParam)
{
	RetCode ret;
	EncHandle handle = pVpu->m_pCodecInst;
	int iMp4HeaderWriteSize = 0;

	if (pInputParam->m_iChangeRcParamFlag & 1) //0: disable, 3:enable(change a bitrate), 5: enable(change a framerate)
	{
		if( pInputParam->m_iChangeRcParamFlag & 0x2 ) //change a bitrate
		{
			int iBitrate = pInputParam->m_iChangeTargetKbps;
			VPU_EncGiveCommand(handle, ENC_SET_BITRATE, &iBitrate);
		}
		else if (pInputParam->m_iChangeRcParamFlag & 0x4) //change a framerate
		{
			CodecInst * pCodecInst;
			EncInfo * pEncInfo;
			int iFramerate = pInputParam->m_iChangeFrameRate;

			pCodecInst = handle;
			pEncInfo = &pCodecInst->CodecInfo.encInfo;

			VPU_EncGiveCommand(handle, ENC_SET_FRAME_RATE, &iFramerate);

		}

		if (pInputParam->m_iChangeRcParamFlag & 0x8) //change KeyInterval
		{
			int newGopNum = pInputParam->m_iChangeKeyInterval;
			VPU_EncGiveCommand(handle, ENC_SET_GOP_NUMBER, &newGopNum);
		}
	}

	//! Encoding
	{
		EncInfo *pEncInfo = &handle->CodecInfo.encInfo;
		EncOpenParam * pOpenParam = &pEncInfo->openParam;
		EncParam enc_param = {0,};
		FrameBuffer src_frame = {0,};
		src_frame.bufY  = pInputParam->m_PicYAddr;
		src_frame.bufCb = pInputParam->m_PicCbAddr;
		src_frame.bufCr = pInputParam->m_PicCrAddr;
		src_frame.bufMvCol = 0;
		enc_param.sourceFrame = &src_frame;
		src_frame.myIndex = pVpu->m_iNeedFrameBufCount;
		src_frame.stride = pVpu->m_iFrameBufWidth;

		//Add a routine to check if the YCbCr buffer address is a valid address in encoding process
		if ( src_frame.bufY == 0)
			return RETCODE_INVALID_PARAM;

		if (src_frame.bufCb == 0)
			return RETCODE_INVALID_PARAM;

		if (pOpenParam->cbcrInterleave == 0)
		{
			if( src_frame.bufCr == 0 )
				return RETCODE_INVALID_PARAM;
		}

		enc_param.picStreamBufferAddr = pInputParam->m_BitstreamBufferAddr;
		enc_param.picStreamBufferSize = pInputParam->m_iBitstreamBufferSize;
		enc_param.picStreamBufferAddr += iMp4HeaderWriteSize;
		enc_param.quantParam = pInputParam->m_iQuantParam;
		enc_param.forceIPicture = pInputParam->m_iForceIPicture;

		if (pVpu->m_pCodecInst->codecMode == MJPG_ENC)
		{
			EncParamSet encHeaderParam = {0};
			codec_addr_t BitstreamBuffer_VA;

			//BitstreamBuffer_VA = pInputParam->m_BitstreamBufferAddr - pVpu->m_iBitstreamOutBufferPhyVirAddrOffset;
			//encHeaderParam.pParaSet = (BYTE*)BitstreamBuffer_VA;
			encHeaderParam.pParaSet = (BYTE*)pInputParam->m_BitstreamBufferAddr_VA;
			encHeaderParam.size = pInputParam->m_iBitstreamBufferSize;

			ret = VPU_EncGiveCommand(handle, ENC_GET_JPEG_HEADER, &encHeaderParam);
			if( ret != RETCODE_SUCCESS )
				return ret;
			enc_param.picStreamBufferAddr = pInputParam->m_BitstreamBufferAddr+encHeaderParam.size;
		}

		if (pInputParam->m_iChangeRcParamFlag & 1)
		{
			if( pInputParam->m_iChangeRcParamFlag & 0x10 )
			{
				int newIntraQp = pInputParam->m_iQuantParam;
				VPU_EncGiveCommand(handle, ENC_SET_INTRA_QP, &newIntraQp);
			}
		}

		ret = VPU_EncStartOneFrame(handle, &enc_param);
		if( ret != RETCODE_SUCCESS )
		{
			return ret;
		}
		while( 1 ) //while( VPU_IsBusy() )
		{
			int iRet = 0;

		#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
			if (VpuReadReg(BIT_CUR_PC) == 0x0)
			{
				enc_reset( pVpu, ( 0 | (1<<16) ) );
				return RETCODE_CODEC_EXIT;
			}
		#endif
			if (vpu_interrupt != NULL)
				iRet = vpu_interrupt();
			if( iRet == RETCODE_CODEC_EXIT )
			{
			#ifdef ADD_SWRESET_VPU_HANGUP
				MACRO_VPU_SWRESET
				enc_reset( pVpu, ( 0 | (1<<16) ) );
			#endif
				VpuWriteReg(BIT_INT_REASON, 0);
				VpuWriteReg(BIT_INT_CLEAR, 1);
				return RETCODE_CODEC_EXIT;
			}

			iRet = VpuReadReg(BIT_INT_REASON);
			if (iRet)
			{
				if (iRet & (1<<INT_BIT_PIC_RUN))
				{
					break;
				}
				else
				{
				#ifdef USE_CODEC_PIC_RUN_INTERRUPT
					VpuWriteReg(BIT_INT_REASON, 0);
					VpuWriteReg(BIT_INT_CLEAR, 1);
				#endif
					if (iRet & (1<<INT_BIT_BIT_BUF_EMPTY))
					{
						/*
						CodecInst * pCodecInst;
						pCodecInst = pVpu->m_pCodecInst;
						VpuWriteReg(BIT_INT_ENABLE, (1<<INT_BIT_SEQ_END) );
						BitIssueCommand(pCodecInst, SEQ_END);
						continue;
						*/
						SetPendingInst(0);
						return RETCODE_FRAME_NOT_COMPLETE;
					}
					else if (iRet & (1<<INT_BIT_BIT_BUF_FULL))
					{
						return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
					}
					else if (iRet & (	 (1<<INT_BIT_SEQ_END)
								|(1<<INT_BIT_SEQ_INIT)
								|(1<<INT_BIT_FRAMEBUF_SET)
								|(1<<INT_BIT_ENC_HEADER)
								|(1<<INT_BIT_ENC_PARA_SET)
								//|(1<<INT_BIT_DEC_BUF_FLUSH)
								|(1<<INT_BIT_USERDATA)))
					{
						return RETCODE_FAILURE;
					}
				}
			}
		}
	}
	#ifdef USE_CODEC_PIC_RUN_INTERRUPT
		VpuWriteReg(BIT_INT_REASON, 0);
		VpuWriteReg(BIT_INT_CLEAR, 1);
	#endif

	//! Output
	{
		EncOutputInfo out_info = {0,};

		ret = VPU_EncGetOutputInfo(handle, &out_info);
		if (ret != RETCODE_SUCCESS)
		{
			return ret;
		}

		if (iMp4HeaderWriteSize)
		{
			out_info.bitstreamBuffer = pInputParam->m_BitstreamBufferAddr;
			out_info.bitstreamSize += iMp4HeaderWriteSize;
		}

		pOutParam->m_BitstreamOut[PA] = out_info.bitstreamBuffer;
		//pOutParam->m_BitstreamOut[VA] = pOutParam->m_BitstreamOut[PA] - pVpu->m_iBitstreamOutBufferPhyVirAddrOffset;
		pOutParam->m_BitstreamOut[VA] = pInputParam->m_BitstreamBufferAddr_VA + (pOutParam->m_BitstreamOut[PA] - pInputParam->m_BitstreamBufferAddr);

		pOutParam->m_iBitstreamOutSize = out_info.bitstreamSize;
		pOutParam->m_iPicType = out_info.picType;
		pOutParam->m_Reserved = out_info.frameCycle; // Cycle Log

		pOutParam->m_Reserved1 = 0;
		pOutParam->m_Reserved2 = 0;
		pOutParam->m_ReservedAddr1 = NULL;
		pOutParam->m_Reserved3 = 0;
		pOutParam->m_ReservedAddr2 = NULL;

		if ((out_info.AvgQp < 0) || (out_info.AvgQp > 51)) {
			//For QP, it holds a value between 0 and 51, and returns -1 for any other values.
			pOutParam->m_iAvgQp = -1;
		} else {
			pOutParam->m_iAvgQp = out_info.AvgQp;
		}

		if( out_info.bitstreamWrapAround )
		{
			return RETCODE_WRAP_AROUND;
		}
	}

	return ret;
}


#ifdef USE_AVC_ENC_VUI_ADDITION

#define VPU_H264_VUI_TBL_FPS	6
#define VPU_H264_VUI_TBL_UNIT	8 //4
#define VPU_H264_VUI_TBL_SIZE	13
#define VPU_H264_VUI_TBL_FIXFPS	0// fixed_frame_rate_flag  : 0 or 1

static const unsigned char vpu_h264_vui_header_table[ VPU_H264_VUI_TBL_SIZE*VPU_H264_VUI_TBL_UNIT ] [28] =
{
	//176x144  - 	11, 12 : origin size, 23, 24 : new size
	{	0x0b/*size*/ ,
		0x18/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe4 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x0e , 0xa9 , 0xc3 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x17 , 0x76 , 0x03 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x1d , 0x53 , 0x83 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x2e , 0xec , 0x03 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x3a , 0xa7 , 0x03 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x75 , 0x4e , 0x03 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x0e , 0xa9 , 0xe3 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x17 , 0x76 , 0x23 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x1d , 0x53 , 0xa3 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x2e , 0xec , 0x23 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x3a , 0xa7 , 0x23 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x82 , 0xc4 , 0xe8 , 0x40 , 0x00 , 0x00 , 0xfa , 0x40 , 0x00 , 0x75 , 0x4e , 0x23 , 0x68 , 0x22 , 0x11 , 0xa8 , 0x00 , 0x00 , 0x00 },// 120
	//#endif

	//320x240  - 	11, 12 : origin size, 23, 24 : new size
	{	0x0b/*size*/ ,
		0x18/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xf9 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x03 , 0xaa , 0x70 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x05 , 0xdd , 0x80 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x07 , 0x54 , 0xe0 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0b , 0xbb , 0x00 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0e , 0xa9 , 0xc0 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x1d , 0x53 , 0x80 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x03 , 0xaa , 0x78 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x05 , 0xdd , 0x88 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x07 , 0x54 , 0xe8 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0b , 0xbb , 0x08 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0e , 0xa9 , 0xc8 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x41 , 0xfa , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x1d , 0x53 , 0x88 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 , 0x00 },// 120
	//#endif

	//352x288  - 	12, 13 : origin size, 24, 25 : new size
	{	0x0c/*size*/ ,
		0x19/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x40 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9c , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x60 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 30
 	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x70 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9e , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x62 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x3a , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x72 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x14 , 0xa6 , 0x81 , 0x60 , 0x96 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 , 0x00 },// 120
	//#endif

	//640x480  - 	12, 13 : origin size, 25, 26 : new size
	{	0x0c/*size*/ ,
		0x1a/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0x90 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x3a , 0xa7 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x5d , 0xd8 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x75 , 0x4e , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xbb , 0xb0 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xea , 0x9c , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x3a , 0xa7 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x5d , 0xd8 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x75 , 0x4e , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xbb , 0xb0 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xea , 0x9c , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xa0 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 120
	//#endif

	//720x480  - 	12, 13 : origin size, 25, 26 : new size
	{	0x0c/*size*/ ,
		0x1a/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0x90 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x3a , 0xa7 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x5d , 0xd8 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x75 , 0x4e , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xbb , 0xb0 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xea , 0x9c , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x0d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x3a , 0xa7 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x5d , 0xd8 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0x75 , 0x4e , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xbb , 0xb0 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x00 , 0xea , 0x9c , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1e , 0xa6 , 0x80 , 0xb4 , 0x3d , 0xa1 , 0x00 , 0x00 , 0x03 , 0x03 , 0xe9 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x8d , 0xa0 , 0x88 , 0x46 , 0xa0 , 0x00 },// 120
	//#endif

	//1280x720 - 	12, 13 : origin size, 25, 26 : new size
	{	0x0c/*size*/ ,
		0x19/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xb9 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x03 , 0xaa , 0x70 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x05 , 0xdd , 0x80 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x07 , 0x54 , 0xe0 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0b , 0xbb , 0x00 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0e , 0xa9 , 0xc0 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x1d , 0x53 , 0x80 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x03 , 0xaa , 0x78 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x05 , 0xdd , 0x88 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x07 , 0x54 , 0xe8 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0b , 0xbb , 0x08 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x0e , 0xa9 , 0xc8 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x1f , 0xa6 , 0x80 , 0x50 , 0x05 , 0xba , 0x10 , 0x00 , 0x00 , 0x3e , 0x90 , 0x00 , 0x1d , 0x53 , 0x88 , 0xda , 0x08 , 0x84 , 0x6a , 0x00 , 0x00 },// 120
	//#endif

	//1920x1080 - 	14, 15 : origin size, 27, 28 :  new size
	{	0x0e/*size*/ ,
		0x1b  /*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x40 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },// without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9c , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x60 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x70 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9e , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x62 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x3a , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x72 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x27 , 0xe5 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 },// 120
	//#endif
	//1920x1088 - 	13, 14 : origin size, 26, 27 : new size
	{	0x0d/*size*/ ,
		0x1a/*new size*/ ,
		0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x40 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 },//without VUI
	//#if VPU_H264_VUI_TBL_FIXFPS == 0
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9c , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x60 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x38 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x70 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe0 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 }, // 120
	//#else //#elif VPU_H264_VUI_TBL_FIXFPS == 1
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x00 , 0xea , 0x9e , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 15
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0x77 , 0x62 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 24
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x01 , 0xd5 , 0x3a , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 30
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x02 , 0xee , 0xc2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 48
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x03 , 0xaa , 0x72 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 },// 60
	{	0x00 , 0x00 , 0x00 , 0x01 , 0x67 , 0x42 , 0x40 , 0x28 , 0xa6 , 0x80 , 0x78 , 0x02 , 0x26 , 0x84 , 0x00 , 0x00 , 0x0f , 0xa4 , 0x00 , 0x07 , 0x54 , 0xe2 , 0x36 , 0x82 , 0x21 , 0x1a , 0x80 , 0x00 } // 120
	//#endif
};


static int enc_h264_sps_with_vui( int sps_size, int width, int height, int framerate, int fixed_frame_rate_flag, unsigned char *pAddr)
{
	const unsigned int vpu_h264_vui_framerate[VPU_H264_VUI_TBL_FPS] = { 15, 24, 30, 48, 60, 120 };
	int check_resolution = 0;
	int check_fps = 0;
	int iRet = 0;
	int i, table_size, start_code_size;
	int fixed_framerate_flag_offset;

	unsigned char * pSrc, *pDst;
	const unsigned char *ptbl;

	fixed_framerate_flag_offset = 0;
	if( fixed_frame_rate_flag == 1 )
		fixed_framerate_flag_offset = VPU_H264_VUI_TBL_FPS;

	if( sps_size < 4 )
		return iRet;

	//check framerate
	switch( framerate )
	{
		case  15: check_fps = 1; break;
		case  24: check_fps = 2; break;
		case  30: check_fps = 3; break;
		case  48: check_fps = 4; break;
		case  60: check_fps = 5; break;
		case 120: check_fps = 6; break;
		default: return iRet;; //not supported.
	}

	//check a resolution
	if( (width ==  176) && (height ==  144) )		check_resolution = 1; // QCIF
	else if( (width ==  320) && (height ==  240) )	check_resolution = 2; // QVGA
	else if( (width ==  352) && (height ==  288) )	check_resolution = 3; // CIF
	else if( (width ==  640) && (height ==  480) )	check_resolution = 4; // VGA
	else if( (width ==  720) && (height ==  480) )	check_resolution = 5; // 480P
	else if( (width == 1280) && (height ==  720) )	check_resolution = 6; //720P
	else if( (width == 1920) && (height == 1080) )	check_resolution = 7; //1080P
	else if( (width == 1920) && (height == 1088) )	check_resolution = 8; //1080P
	else	return iRet;; //not supported resolution

	pSrc = pAddr;
	ptbl = vpu_h264_vui_header_table[ VPU_H264_VUI_TBL_SIZE*(check_resolution-1) ];
	table_size = *ptbl++;

	if( table_size == sps_size ) start_code_size = 3;
	else	start_code_size = 4;

	//read new table size and skip
	table_size = *ptbl++;
	if( start_code_size == 3)
		ptbl++;
	else
		table_size += 1;

	//compare data until level_idc
	for(i=0;i<start_code_size+3;i++)
	{
		if( *ptbl++ != *pSrc++ )
			return iRet; //not supported (without VUI)
	}

	//skip the log2_max_frame_num_minus4
	ptbl++;
	pSrc++;
	i++;
	for(;i<sps_size; i++)
	{
		if( *ptbl++ != *pSrc++ )
			return iRet; //not supported (without VUI)
	}

	//write SPS data from num_ref_frames or max-num_ref_frames
	pDst = pAddr;
	ptbl = vpu_h264_vui_header_table[ VPU_H264_VUI_TBL_SIZE*(check_resolution-1) + fixed_framerate_flag_offset + check_fps ];
	if( start_code_size == 3)
		ptbl++;

	i = (4+start_code_size);
	pDst += i;
	ptbl += i;
	for(;i<table_size; i++)
		*pDst++ = *ptbl++;

	return table_size;
}
#endif //USE_AVC_ENC_VUI_ADDITION

int enc_check_handle_instance(vpu_t *pVpu, CodecInst *pCodecInst)
{
	//check global variables
	int ret = 0;
	int bitstreamformat;
	unsigned int uret = 0;

	if ((pVpu == NULL) || (pCodecInst == NULL)) {
		return RETCODE_INVALID_HANDLE;
	}

	ret = check_vpu_handle_addr(pVpu);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}

	ret = check_vpu_handle_use(pVpu);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}

	bitstreamformat = pVpu->m_iBitstreamFormat;

	ret = checkCodecInstanceAddr((void *)pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}

	ret = checkCodecInstanceUse((void *)pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}

	uret = checkCodecType(bitstreamformat, (void *)pCodecInst);
	if (uret == 0) {
		return RETCODE_NOT_INITIALIZED;
	} else {
		unsigned int isEncoder = (uret >> 8) & 0x00FF;	//0: Decoder(default), 1: Encoder
		unsigned int isCheckCodec = uret & 0x00FF;	//0: not yet, 1: confirmed, 2: codec mis-match

		if (isEncoder == 0) {
			return RETCODE_NOT_INITIALIZED;
		}

		if ((isCheckCodec == 0) || (isCheckCodec == 2)) {
			return RETCODE_NOT_INITIALIZED;
		}
	}
	return 0;
}

codec_result_t
TCC_VPU_ENC( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	codec_result_t ret = RETCODE_SUCCESS;
	vpu_t *p_vpu_enc = NULL;

	if (pHandle != NULL) {
		p_vpu_enc = (vpu_t *)(*pHandle);
	}

	if (Op == VPU_CTRL_LOG_STATUS) {
		vpu_dec_ctrl_log_status_t *pst_ctrl_log = (vpu_dec_ctrl_log_status_t *)pParam1;
		if (pst_ctrl_log == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			g_pfPrintCb_VpuC7 = (void (*)(const char *, ...))pst_ctrl_log->pfLogPrintCb;

			g_bLogVerbose_VpuC7 = pst_ctrl_log->stLogLevel.bVerbose;
			g_bLogDebug_VpuC7 = pst_ctrl_log->stLogLevel.bDebug;
			g_bLogInfo_VpuC7 = pst_ctrl_log->stLogLevel.bInfo;
			g_bLogWarn_VpuC7 = pst_ctrl_log->stLogLevel.bWarn;
			g_bLogError_VpuC7 = pst_ctrl_log->stLogLevel.bError;
			g_bLogAssert_VpuC7 = pst_ctrl_log->stLogLevel.bAssert;
			g_bLogFunc_VpuC7 = pst_ctrl_log->stLogLevel.bFunc;
			g_bLogTrace_VpuC7 = pst_ctrl_log->stLogLevel.bTrace;

			g_bLogDecodeSuccess_VpuC7 = pst_ctrl_log->stLogCondition.bDecodeSuccess;

			V_PRINTF(PRT_PREFIX "VPU_CTRL_LOG_STATUS: V/D/I/W/E/A/F/T/=%d/%d/%d/%d/%d/%d/%d/%d\n",
					 g_bLogVerbose_VpuC7, g_bLogDebug_VpuC7, g_bLogInfo_VpuC7, g_bLogWarn_VpuC7,
					 g_bLogError_VpuC7, g_bLogAssert_VpuC7, g_bLogFunc_VpuC7, g_bLogTrace_VpuC7);
		}
	}
	else if (Op == VPU_C7_SET_FW_ADDRESS)
	{
		if (pParam1 == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			vpu_c7_set_fw_addr_t *pst_fw_addr = (vpu_c7_set_fw_addr_t *)pParam1;
			ret = set_vpuc7_enc_fw_addr(pst_fw_addr);
		}
	}
	else if (Op == VPU_ENC_INIT)
	{
		enc_init_t* p_init_param	= (enc_init_t*)pParam1;
		enc_initial_info_t* p_initial_info_param	= (enc_initial_info_t*)pParam2;

		{
			char sz_ver[64];
			char sz_date[64];
			ret = get_version_vpu(&sz_ver[0], &sz_date[0]);
			if (ret != 0)
			{
				V_PRINTF(PRT_PREFIX"VPU C7 Version Failed\n");
			}
			else
			{
				V_PRINTF(PRT_PREFIX"VPU C7 Version   : %s\n", sz_ver);
				V_PRINTF(PRT_PREFIX"VPU C7 Build Date: %s\n", sz_date);
			}
			ret = 0;
		}

		ret = enc_init(&p_vpu_enc, p_init_param, p_initial_info_param);
		if (ret != RETCODE_SUCCESS)
		{
			return ret;
		}

		*pHandle = (codec_handle_t)p_vpu_enc;
	}
	else if (Op == VPU_GET_VERSION)
	{
		vpu_get_version_t *pst_getversion = (vpu_get_version_t *)pParam1;

		if (pst_getversion == NULL)
		{
			return RETCODE_INVALID_PARAM;
		}

		//1. check version of api header
		if (pst_getversion->pszHeaderApiVersion != NULL)
		{
			const char *vpu_api_version = VPU_CODEC_API_VERSION;
			V_PRINTF(PRT_PREFIX"[VPU_GET_VERSION] API Version = %s \n", vpu_api_version);
			if (vpu_strcmp(pst_getversion->pszHeaderApiVersion, vpu_api_version) != 0)
			{
				V_PRINTF(PRT_PREFIX"[VPU_GET_VERSION] API Version must be %s \n", vpu_api_version);
				return RETCODE_API_VERSION_MISMATCH;
			}
		}

		//2. get version
		return get_version_vpu(pst_getversion->szGetVersion, pst_getversion->szGetBuildDate);
	}
	else if( Op == VPU_ENC_REG_FRAME_BUFFER )
	{
		enc_buffer_t* p_buffer_param = (enc_buffer_t*)pParam1;

		if ((p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL))
		{
			return RETCODE_INVALID_HANDLE;
		}

		//check handle and instance information
		{
			int res;
			res = enc_check_handle_instance(p_vpu_enc, p_vpu_enc->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = enc_register_frame_buffer(p_vpu_enc, p_buffer_param);
		if (ret != RETCODE_SUCCESS)
		{
			return ret;
		}
	}
	else if( Op == VPU_ENC_PUT_HEADER )
	{
		EncHeaderParam header = {0,};
		EncHandle handle;
		enc_header_t* p_header = (enc_header_t*)pParam1;

		header.buf = p_header->m_HeaderAddr;
		header.size = p_header->m_iHeaderSize;

		if (p_vpu_enc == NULL)
		{
			return RETCODE_INVALID_HANDLE;
		}

		handle = p_vpu_enc->m_pCodecInst;

		switch( p_header->m_iHeaderType )
		{
		case MPEG4_VOL_HEADER:
			header.headerType = VOL_HEADER;
			ret = VPU_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &header);
			break;
		case MPEG4_VOS_HEADER:
			header.headerType = VOS_HEADER;
			ret = VPU_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &header);
			break;
		case MPEG4_VIS_HEADER:
			header.headerType = VIS_HEADER;
			ret = VPU_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &header);
			break;
		case AVC_SPS_RBSP:
			header.headerType = SPS_RBSP;
			ret = VPU_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &header);
		#ifdef USE_AVC_ENC_VUI_ADDITION
			if( (ret == RETCODE_SUCCESS) && (header.size > 3) )
			{
				unsigned char *ptr_data  = (unsigned char *)p_header->m_HeaderAddr_VA;
				unsigned int framerate_ctrl;
				int fixed_framerate_flag;

				framerate_ctrl = (p_vpu_enc->m_uiEncOptFlags & USE_AVC_ENC_VUI_ADDITION)>>USE_AVC_ENC_VUI_ADDITION_RSHIFT; //3 bits : include fixed_frame_rate_flag info.
				fixed_framerate_flag = (framerate_ctrl>>2); //MSB 1bits
				framerate_ctrl = (framerate_ctrl& 0x3); //LSB 2bits.

				if ((ptr_data != 0) && framerate_ctrl)
				{
					int width, height, framerate;
					EncInfo * pEncInfo;
					int tmp_ret = 0;

					pEncInfo = &p_vpu_enc->m_pCodecInst->CodecInfo.encInfo;
					height = pEncInfo->openParam.picHeight;
					width = pEncInfo->openParam.picWidth;
					framerate = pEncInfo->openParam.frameRateInfo; //pEncInfo->openParam.gopSize;
					tmp_ret = enc_h264_sps_with_vui( header.size, width, height, (framerate*framerate_ctrl), fixed_framerate_flag, ptr_data);
					if (tmp_ret > header.size)
					{
						header.size = tmp_ret;
					}
				}
			}
		#endif //USE_AVC_ENC_VUI_ADDITION
			break;
		case AVC_PPS_RBSP:
			header.headerType = PPS_RBSP;
			ret = VPU_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &header);
			break;
		default:
			return RETCODE_INVALID_PARAM;
		}

		if (ret != RETCODE_SUCCESS)
		{
			if (ret == RETCODE_CODEC_EXIT)
			{
				enc_reset(NULL, (0 | (1<<16)));
			}
			return ret;
		}

		// output
		p_header->m_HeaderAddr = header.buf;
		p_header->m_iHeaderSize = header.size;
	}
	else if (Op == VPU_ENC_ENCODE)
	{
		enc_input_t* p_input_param	= (enc_input_t*)pParam1;
		enc_output_t* p_output_param	= (enc_output_t*)pParam2;

		if ((p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL))
		{
			return RETCODE_INVALID_HANDLE;
		}

		//check handle and instance information
		{
			int res;
			res = enc_check_handle_instance(p_vpu_enc, p_vpu_enc->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = enc_encode(p_vpu_enc, p_input_param, p_output_param);
		if( ret != RETCODE_SUCCESS )
		{
			return ret;
		}
	}
	else if (Op == VPU_ENC_CLOSE)
	{
		EncHandle handle;
		//enc_output_t* p_out_info = (enc_output_t*)pParam2;

		if ((p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL))
		{
			return RETCODE_INVALID_HANDLE;
		}

		//check handle and instance information
		{
			int res;
			res = enc_check_handle_instance(p_vpu_enc, p_vpu_enc->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		handle = p_vpu_enc->m_pCodecInst;

		if (gInitFirst == 0)
		{
			return RETCODE_NOT_INITIALIZED;
		}

		ret = VPU_EncClose( handle );
	#ifdef ADD_SWRESET_VPU_HANGUP
		if (ret == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_SWRESET
			enc_reset(NULL, (0 | (1<<16)));
		}
	#endif

		if( ret == RETCODE_FRAME_NOT_COMPLETE )
		{
		#if 0
			EncOutputInfo out_info = {0,};
			VPU_EncGetOutputInfo(handle, &out_info);

			p_out_info->m_BitstreamOut[PA] = out_info.bitstreamBuffer;
			p_out_info->m_BitstreamOut[VA] = p_out_info->m_BitstreamOut[PA] - p_vpu_enc->m_iBitstreamOutBufferPhyVirAddrOffset;
			p_out_info->m_iBitstreamOutSize = out_info.bitstreamSize;
			p_out_info->m_iPicType = out_info.picType;
			if( out_info.bitstreamWrapAround )
			{
				DPRINTF( "Warning!! BitStream buffer wrap arounded. prepare more large buffer = %d \n", ret );
				return RETCODE_WRAP_AROUND;
			}
		#else
			VPU_SWReset2(); //VPU_SWReset(SW_RESET_SAFETY);
		#endif
			VPU_EncClose(handle); //VPU_EncClose( handle, 0 );
		}

		FreeFrameBuffer(p_vpu_enc->m_pCodecInst->instIndex);
		free_vpu_handle( p_vpu_enc );
	}
	else if( Op == VPU_DEC_SWRESET )
	{
		if (GetPendingInst())
		{
			ClearPendingInst();
		}
		MACRO_VPU_SWRESET
		return RETCODE_SUCCESS;
	}
	else if( Op == VPU_RESET_SW )
	{
		if (p_vpu_enc == NULL)
		{
			return RETCODE_INVALID_HANDLE;
		}
		enc_reset( p_vpu_enc, ( 1 | (1<<16) ) );
		free_vpu_handle( p_vpu_enc );
	}
	else
	{
		return RETCODE_INVALID_COMMAND;
	}

	return ret;
}
