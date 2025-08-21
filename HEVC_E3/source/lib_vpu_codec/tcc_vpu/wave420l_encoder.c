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

#include "wave420l_def.h"

#if defined(CONFIG_HEVC_E3_2)
#include "wave420l_core_2.h"
#else
#include "wave420l_core.h"
#endif

#include "vpuapi.h"
#include "vpuapifunc.h"
#include "wave4.h"
#include "wave4_regdefine.h"
#include "common_regdefine.h"
#include "common_vpuconfig.h"

/******************************************************************************

******************************************************************************/
//#if defined(ARM_LINUX)
//	extern PhysicalAddress gHevcEncVirtualBitWorkAddr;
//	extern PhysicalAddress gHevcEncPhysicalBitWorkAddr;
//	extern PhysicalAddress gHevcEncVirtualRegBaseAddr;
//#endif

#define MIX_BASE                0x1000000
#define MAX_FILE_PATH           256

//static hevc_enc_initial_info_t gsHevcEncInitialInfo;

extern int gHevcEncMaxInstanceNum;

static codec_result_t set_hevc_e3_fw_addr(vpu_hevc_enc_set_fw_addr_t *pstFWAddr) {
	codec_result_t ret = RETCODE_SUCCESS;
	if (pstFWAddr == NULL) {
		return RETCODE_INVALID_PARAM;
	} else {
		gHevcEncFWAddr = pstFWAddr->m_FWBaseAddr;
		DLOGI("gHevcEncFWAddr 0x%x", gHevcEncFWAddr);
	}

	return RETCODE_SUCCESS;
}

/*!
 ***********************************************************************
 * \brief
 *	Set VUI information of SPS
 * \param
 *
 * \param
 *
 * \param
 *
 * \param
 *
 * \return
 *
 ***********************************************************************
 */
//#define TEST_VUI_ENC_CASE 1	//1(all)
Int32 wave420l_setEncOpenVui(EncOpenParam *pEncOP, hevc_enc_init_t *pInitParam)
{
	EncHevcParam *param = &pEncOP->EncStdParam.hevcParam;
	HevcVuiParam *pVui = &param->vuiParam;

	Uint32 vuiParamFlags;
	Uint32 vuiAspectRatioInfoPresentFlag;
	Uint32 vuiOverscanInfoPresentFlag;
	Uint32 vuiVideoSignalTypePresentFlag, vuiColourDescriptionPresentFlag;
	Uint32 vuiChromaLocInfoPresentFlag;
	Uint32 vuiNeutralChromaIndicationFlag;
	Uint32 vuiDefaultDisplayWindowFlag;
	Uint32 vuiTimingInfoPresentFlag;

	vuiParamFlags = 0U;
	pVui->vuiParamFlags = 0U;                // when vuiParamFlags == 0, VPU doesn't encode VUI
#if defined(TEST_VUI_ENC_CASE)
    #if TEST_VUI_ENC_CASE == 1
	pInitParam->m_stVuiParam.aspect_ratio_info_present_flag = 1;
	pInitParam->m_stVuiParam.aspect_ratio_idc = 255;
	pInitParam->m_stVuiParam.sar_width = 256;
	pInitParam->m_stVuiParam.sar_height = 120;
	pInitParam->m_stVuiParam.overscan_info_present_flag = 1;
	pInitParam->m_stVuiParam.overscan_appropriate_flag = 1;
	pInitParam->m_stVuiParam.video_signal_type_present_flag = 1;
	pInitParam->m_stVuiParam.video_format = 5;
	pInitParam->m_stVuiParam.video_full_range_flag = 0;
	pInitParam->m_stVuiParam.colour_description_present_flag = 1;
	pInitParam->m_stVuiParam.colour_primaries = 3;
	pInitParam->m_stVuiParam.transfer_characteristics = 4;
	pInitParam->m_stVuiParam.matrix_coeffs = 1;
	pInitParam->m_stVuiParam.chroma_loc_info_present_flag = 1;
	pInitParam->m_stVuiParam.chroma_sample_loc_type_top_field = 8;
	pInitParam->m_stVuiParam.chroma_sample_loc_type_bottom_field = 8;
	pInitParam->m_stVuiParam.neutral_chroma_indication_flag = 1;
	pInitParam->m_stVuiParam.default_display_window_flag = 1;
	pInitParam->m_stVuiParam.def_disp_win_left_offset = 8;
	pInitParam->m_stVuiParam.def_disp_win_right_offset = 16;
	pInitParam->m_stVuiParam.def_disp_win_top_offset = 24;
	pInitParam->m_stVuiParam.def_disp_win_bottom_offset = 32;
	pInitParam->m_stVuiParam.vui_timing_info_present_flag = 1;
	pInitParam->m_stVuiParam.vui_num_units_in_tick = 1000U;
	pInitParam->m_stVuiParam.vui_time_scale = pEncOP->frameRateInfo * pInitParam->m_stVuiParam.vui_num_units_in_tick;
	pInitParam->m_stVuiParam.vui_poc_proportional_to_timing_flag = 0;
	pInitParam->m_stVuiParam.vui_num_ticks_poc_diff_one_minus1= 0;
    #endif
#endif

	// aspect_ratio_info
	vuiAspectRatioInfoPresentFlag = 0U;
	pVui->vuiAspectRatioIdc = 0U;
	pVui->vuiSarSize = 0U;
	if (pInitParam->m_stVuiParam.aspect_ratio_info_present_flag == 1)
	{
		Uint32 vuiAspectRatioIdc = 0U;
		Uint32 vuiSarSize = 0U;

		if (pInitParam->m_stVuiParam.aspect_ratio_idc < 255)
		{
			vuiAspectRatioInfoPresentFlag = 1U;
			vuiAspectRatioIdc = pInitParam->m_stVuiParam.aspect_ratio_idc;
		}
		else if (pInitParam->m_stVuiParam.aspect_ratio_idc == 255)	//EXTENDED_SAR
		{
			if ((pInitParam->m_stVuiParam.sar_width >0) && (pInitParam->m_stVuiParam.sar_height > 0))
			{
				vuiAspectRatioInfoPresentFlag = 1U;
				vuiAspectRatioIdc = pInitParam->m_stVuiParam.aspect_ratio_idc;
				vuiSarSize  =  (pInitParam->m_stVuiParam.sar_width  & 0x0000FFFF);
				vuiSarSize |= ((pInitParam->m_stVuiParam.sar_height & 0x0000FFFF) << HEVC_VUI_SAR_HEIGHT_POS);
			}
		}
		// [out] vuiAspectRatioInfoPresentFlag, vuiAspectRatioIdc, vuiSarSize
		pVui->vuiAspectRatioIdc = vuiAspectRatioIdc;
		pVui->vuiSarSize = vuiSarSize;
	}

	// overscan_info
	vuiOverscanInfoPresentFlag = 0U;
	pVui->vuiOverScanAppropriate = 0U;
	if (pInitParam->m_stVuiParam.overscan_info_present_flag == 1)
	{
		Uint32 vuiOverScanAppropriate = 0;

		if (pInitParam->m_stVuiParam.overscan_appropriate_flag == 1)
		{
			vuiOverscanInfoPresentFlag = 1U;
			vuiOverScanAppropriate = pInitParam->m_stVuiParam.overscan_appropriate_flag & 0x1;
		}
		// [out] vuiOverscanInfoPresentFlag, vuiOverScanAppropriate
		pVui->vuiOverScanAppropriate = vuiOverScanAppropriate;
	}

	// video_signal_type
	vuiVideoSignalTypePresentFlag = 0U;
	vuiColourDescriptionPresentFlag = 0U;
	pVui->videoSignal = 0U;
	if (pInitParam->m_stVuiParam.video_signal_type_present_flag == 1)
	{
		Uint32 vuiVideoSigal = 0U;
		Uint32 video_signal_type_present_flag, video_format, video_full_range_flag, colour_description_present_flag;
		Uint32 colour_primaries, transfer_characteristics, matrix_coeffs;

		vuiVideoSignalTypePresentFlag = 1U;
		if ((pInitParam->m_stVuiParam.video_format & 0xFFF8) == 0)
			vuiVideoSigal  |= (pInitParam->m_stVuiParam.video_format << HEVC_VUI_VIDEO_FORMAT_POS);	//3bits : [ 2: 0] video_format
		if (pInitParam->m_stVuiParam.video_full_range_flag == 1)
			vuiVideoSigal  |= (pInitParam->m_stVuiParam.video_full_range_flag << HEVC_VUI_VIDEO_FULL_RANGE_FLAG_POS);	// 1bit  : [ 3   ] video_full_range_flag
		if (pInitParam->m_stVuiParam.colour_description_present_flag == 1)
		{
			vuiColourDescriptionPresentFlag = 1;
			if ((pInitParam->m_stVuiParam.colour_primaries & 0xFFFFFF00) == 0)
				vuiVideoSigal  |= (pInitParam->m_stVuiParam.colour_primaries << HEVC_VUI_COLOUR_PRIMARIES_POS);	// 8bits : [11: 4] colour_primaries
			if ((pInitParam->m_stVuiParam.transfer_characteristics & 0xFFFFFF00) == 0)
				vuiVideoSigal  |= (pInitParam->m_stVuiParam.transfer_characteristics << HEVC_VUI_TRANSFER_CHARACTERISTICS_POS);	// 8bits : [19:12] transfer_characteristics
			if ((pInitParam->m_stVuiParam.matrix_coeffs & 0xFFFFFF00) == 0)
				vuiVideoSigal  |= (pInitParam->m_stVuiParam.matrix_coeffs << HEVC_VUI_MATRIX_COEFFS_POS);	// 8bits : [27:20] matrix_coeffs
		}
		// [out] vuiVideoSignalTypePresentFlag, vuiColourDescriptionPresentFlag, vuiVideoSigal
		pVui->videoSignal = vuiVideoSigal;
		//DLOGV("[DBG - VUI] vuiParamFlags = 0x%08X [ex 0x00000060], videoSignal  = 0x%08X [ex: 0x00104035] \n", pVui->vuiParamFlags, pVui->videoSignal);
	}

	// chroma_loc_info
	vuiChromaLocInfoPresentFlag = 0U;
	pVui->vuiChromaSampleLoc = 0U;
	if (pInitParam->m_stVuiParam.chroma_loc_info_present_flag == 1)
	{
		Uint32 vuiChromaSampleLoc = 0U;

		vuiChromaLocInfoPresentFlag = 1U;
		vuiChromaSampleLoc  =  (pInitParam->m_stVuiParam.chroma_sample_loc_type_top_field    & 0x0000FFFF);
		vuiChromaSampleLoc |= ((pInitParam->m_stVuiParam.chroma_sample_loc_type_bottom_field & 0x0000FFFF) << HEVC_VUI_CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD_POS);
			// [out] vuiChromaLocInfoPresentFlag, vuiChromaSampleLoc
		pVui->vuiChromaSampleLoc = vuiChromaSampleLoc;
	}

	// neutral_chroma
	vuiNeutralChromaIndicationFlag = 0U;
	if (pInitParam->m_stVuiParam.neutral_chroma_indication_flag == 1)
	{
		vuiNeutralChromaIndicationFlag = 1U;	// [out] vuiNeutralChromaIndicationFlag
	}

	// default_display_window
	vuiDefaultDisplayWindowFlag = 0U;
	pVui->vuiDispWinLeftRight = 0U;
	pVui->vuiDispWinTopBottom = 0U;
	if (pInitParam->m_stVuiParam.default_display_window_flag == 1)
	{
		Uint32 vuiDispWinLeftRight, vuiDispWinTopBottom;

		vuiDefaultDisplayWindowFlag = 1U;
		vuiDispWinLeftRight  =  (pInitParam->m_stVuiParam.def_disp_win_left_offset  & 0x0000FFFF);
		vuiDispWinLeftRight |= ((pInitParam->m_stVuiParam.def_disp_win_right_offset & 0x0000FFFF) << HEVC_VUI_DEF_DISP_WIN_RIGHT_OFFSET_POS);
		vuiDispWinTopBottom  =  (pInitParam->m_stVuiParam.def_disp_win_top_offset  & 0x0000FFFF);
		vuiDispWinTopBottom |= ((pInitParam->m_stVuiParam.def_disp_win_bottom_offset & 0x0000FFFF) << HEVC_VUI_DEF_DISP_WIN_BOTTOM_OFFSET_POS);

		// [out] vuiDefaultDisplayWindowFlag, vuiDispWinLeftRight, vuiDispWinTopBottom
		pVui->vuiDispWinLeftRight = vuiDispWinLeftRight;
		pVui->vuiDispWinTopBottom = vuiDispWinTopBottom;
	}

	// vui_timing_info
	vuiTimingInfoPresentFlag = 0U;
	if (pInitParam->m_stVuiParam.vui_timing_info_present_flag)
	{
		Uint32 vuiNumUnitsInTick, vuiTimeScale;
		Uint32 vuiPocProportionalToTimeFlag, vuiNumTicksPocDiffOneMinus1;

		vuiTimingInfoPresentFlag = 1;
		vuiPocProportionalToTimeFlag = 0;
		vuiNumTicksPocDiffOneMinus1 = 0;

		vuiNumUnitsInTick = pInitParam->m_stVuiParam.vui_num_units_in_tick;
		vuiTimeScale = pInitParam->m_stVuiParam.vui_time_scale;
		if (pInitParam->m_stVuiParam.vui_poc_proportional_to_timing_flag == 1)
		{
			vuiPocProportionalToTimeFlag = 1;
			if (pInitParam->m_stVuiParam.vui_num_ticks_poc_diff_one_minus1 > 0)
			{
				vuiPocProportionalToTimeFlag = 1;
				vuiNumTicksPocDiffOneMinus1 = pInitParam->m_stVuiParam.vui_num_ticks_poc_diff_one_minus1;
			}
		}
		/* [N/A]
		vui_hrd_parameters_present_flag	u(1)
		if( vui_hrd_parameters_present_flag )
			hrd_parameters( 1, sps_max_sub_layers_minus1 )
		*/

		// [out] vuiTimingInfoPresentFlag, vuiNumUnitsInTick, vuiTimeScale
		// [out] vuiPocProportionalToTimeFlag, vuiNumTicksPocDiffOneMinus1
		param->numUnitsInTick = vuiNumUnitsInTick;
		param->timeScale = vuiTimeScale;
		param->numTicksPocDiffOne = vuiNumTicksPocDiffOneMinus1;
	}
	else
	{
		param->numUnitsInTick     = 1000;
		param->timeScale          = pEncOP->frameRateInfo * 1000;
		param->numTicksPocDiffOne = 0;	//vui_poc_proportional_to_timing_flag = 0
	}

	/* [N/A] bitstream_restriction :
	The "bitstream_restriction_flag" exists in the document, but there is no data (document, reference code) on the details when the flag is enabled.
	bitstream_restriction_flag	u(1) CMD_ENC_VUI_PARAM_FLAGS (0x015C) [ 9   ] BITSTREAM_RESTRICTION_FLAG
	if( bitstream_restriction_flag )
	{
		tiles_fixed_structure_flag	u(1)
		motion_vectors_over_pic_boundaries_flag	u(1)
		restricted_ref_pic_lists_flag	u(1)
		min_spatial_segmentation_idc	ue(v)
		max_bytes_per_pic_denom	ue(v)
		max_bits_per_min_cu_denom	ue(v)
		log2_max_mv_length_horizontal	ue(v)
		log2_max_mv_length_vertical	ue(v)
	}

	if (bitstream_restriction_flag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_BITSTREAM_RESTRICTION_FLAG_POS);
	*/

	if  (vuiTimingInfoPresentFlag == 1)
		vuiParamFlags |= (1 << 30);	//Reserved.
	if  (vuiDefaultDisplayWindowFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_DEFAULT_DISPLAY_WINDOW_FLAG_POS);
	if  (vuiChromaLocInfoPresentFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_CHROMA_LOC_INFO_PRESENT_FLAG_POS);
	if  (vuiColourDescriptionPresentFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_COLOUR_DESCRIPTION_PRESENT_FLAG_POS);
	if  (vuiVideoSignalTypePresentFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_VIDEO_SIGNAL_TYPE_PRESENT_FLAGP_POS);
	if  (vuiOverscanInfoPresentFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_OVERSCAN_INFO_PRESENT_FLAG_POS);
	if  (vuiAspectRatioInfoPresentFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_ASPECT_RATIO_INFO_PRESENT_FLAG_POS);
	if  (vuiNeutralChromaIndicationFlag == 1)
		vuiParamFlags |= (1 << HEVC_VUI_NEUTRAL_CHROMA_INDICATION_FLAG_POS);

	pVui->vuiParamFlags = vuiParamFlags;

	return 1; //success
}

/*!
 ***********************************************************************
 * \brief
 *	EncOpenParam Initialization
 *	To init EncOpenParam by runtime evaluation
 * \param
 *	hevc_enc_init_t* pInitParam
 * \return
 *	EncOpenParam *pEncOP
 ***********************************************************************
 */
Int32 wave420l_GetEncOpenParamDefault(EncOpenParam *pEncOP, hevc_enc_init_t *pInitParam)
{
	//pEncConfig->outNum = 30; //  pEncConfig->outNum == 0 ? 30 : pEncConfig->outNum;  //DEFAULT_ENC_OUTPUT_NUM 30

	//int bitFormat                 = pEncOP->bitstreamFormat = 12; //WAVE420L_STD_HEVC 12 <== Telechips STD_HEVC_ENC 17
	pEncOP->picWidth                = pInitParam->m_iPicWidth;   //pEncConfig->picWidth;
	pEncOP->picHeight               = pInitParam->m_iPicHeight;  //pEncConfig->picHeight;
	pEncOP->frameRateInfo           = pInitParam->m_iFrameRate; //30;

	//pEncOP->frameSkipDisable        = 1;        // for compare with C-model ( C-model = only 1 )
	//pEncOP->gopSize                 = pInitParam->m_iKeyInterval; //30;       // only first picture is I

	pEncOP->frameEndian = VPU_FRAME_ENDIAN; // = VDI_128BIT_LITTLE_ENDIAN;
	pEncOP->streamEndian = VPU_STREAM_ENDIAN; // = VDI_128BIT_LITTLE_ENDIAN;
	pEncOP->sourceEndian = VPU_SOURCE_ENDIAN; // = VDI_128BIT_LITTLE_ENDIAN;

	pEncOP->bitstreamBuffer = pInitParam->m_BitstreamBufferAddr[0];
	pEncOP->bitstreamBufferSize  = pInitParam->m_iBitstreamBufferSize;

	pEncOP->cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
	pEncOP->nv21 = pInitParam->m_bCbCrOrder; //pInitParam->m_bCbCrOrder; //pInitParam->m_bCbCrOrder; // 0: CbCr(NV12), 1: CrCb(NV21)
	pEncOP->srcFormat = pInitParam->m_iSrcFormat;
	if (pInitParam->m_iSrcFormat > 0) {
		switch ( pInitParam->m_iSrcFormat )
		{
		case 1:
			pEncOP->srcFormat = FORMAT_YUYV;
			break;
		case 2:
			pEncOP->srcFormat = FORMAT_YVYU;
			break;
		case 3:
			pEncOP->srcFormat = FORMAT_UYVY;
			break;
		case 4:
			pEncOP->srcFormat = FORMAT_VYUY;
			break;
		default:
			#if !defined(VLOG_DISABLE)
			VLOG(INFO, "VPU Source Format is out of ragne : [%d]\n", pInitParam->m_iSrcFormat);
			#endif
			return RETCODE_INVALID_HANDLE;
			break;
		}
		pEncOP->cbcrInterleave = 0;
		pEncOP->nv21 = 0;
	}

	// Standard specific
	{
		EncHevcParam *param = &pEncOP->EncStdParam.hevcParam;
		Int32   rcBitrate   = pInitParam->m_iTargetKbps* 1000;
		Int32   i = 0;

		pEncOP->bitRate         = rcBitrate;
		param->profile          = HEVC_PROFILE_MAIN;
		//param->level          = 50; //0;
		param->tier             = 0;
		param->internalBitDepth = 8;
		pEncOP->srcBitDepth     = 8;
		param->chromaFormatIdc  = 0;
		param->losslessEnable   = 0;
		param->constIntraPredFlag = 0;

		/* for CMD_ENC_SEQ_GOP_PARAM */
		param->gopPresetIdx     = PRESET_IDX_IPPPP;

		/* for CMD_ENC_SEQ_INTRA_PARAM */
		param->decodingRefreshType = 2;  //0 - Non-IRAP  1 - CRA   2 - IDR
		param->intraPeriod         = 28;
		if( pInitParam->m_iKeyInterval )
			param->intraPeriod         = pInitParam->m_iKeyInterval; //28;
		param->intraQP             = 0;

		/* for CMD_ENC_SEQ_CONF_WIN_TOP_BOT/LEFT_RIGHT */
		param->confWinTop    = 0;
		param->confWinBot    = 0;
		param->confWinLeft   = 0;
		param->confWinRight  = 0;

		/* for CMD_ENC_SEQ_INDEPENDENT_SLICE */
		param->independSliceMode     = 0;
		param->independSliceModeArg  = 0;

		/* for CMD_ENC_SEQ_DEPENDENT_SLICE */
		param->dependSliceMode     = 0;
		param->dependSliceModeArg  = 0;

		/* for CMD_ENC_SEQ_INTRA_REFRESH_PARAM */
		param->intraRefreshMode     = 0;
		param->intraRefreshArg      = 0;
		param->useRecommendEncParam = 1;

		//pEncConfig->seiDataEnc.prefixSeiNalEnable   = 0;
		//pEncConfig->seiDataEnc.suffixSeiNalEnable   = 0;

		pEncOP->encodeHrdRbspInVPS  = 0;
		pEncOP->encodeHrdRbspInVUI  = 0;
		pEncOP->encodeVuiRbsp       = 0;

		//pEncConfig->roi_enable = 0;
		/* for CMD_ENC_PARAM */
		if (param->useRecommendEncParam != 1) {		// 0 : Custom,  2 : Boost mode (normal encoding speed, normal picture quality),  3 : Fast mode (high encoding speed, low picture quality)
			param->scalingListEnable        = 0;
			param->cuSizeMode               = 0x7;
			param->tmvpEnable               = 1;
			param->wppEnable                = 0;
			param->maxNumMerge              = 2;
			param->dynamicMerge8x8Enable    = 1;
			param->dynamicMerge16x16Enable  = 1;
			param->dynamicMerge32x32Enable  = 1;
			param->disableDeblk             = 0;
			param->lfCrossSliceBoundaryEnable= 1;
			param->betaOffsetDiv2           = 0;
			param->tcOffsetDiv2             = 0;
			param->skipIntraTrans           = 1;
			param->saoEnable                = 1;
			param->intraInInterSliceEnable  = 1;
			param->intraNxNEnable           = 1;
		}

		/* for CMD_ENC_RC_PARAM */
		pEncOP->rcEnable             = rcBitrate == 0 ? FALSE : TRUE;
		pEncOP->initialDelay         = 3000;
		param->ctuOptParam.roiEnable = 0;
		param->ctuOptParam.roiDeltaQp= 3;
		param->intraQpOffset         = 0;
		param->initBufLevelx8        = 1;
		param->bitAllocMode          = 0;
		for (i = 0; i < MAX_GOP_NUM; i++)
			param->fixedBitRatio[i] = 1;

		param->level                = 50; //0;  //from cnm
		param->level = wave420l_HevcLevelCalculation(pEncOP->picWidth, pEncOP->picHeight, pEncOP->rcEnable, pEncOP->initialDelay, rcBitrate, pEncOP->frameRateInfo) / 3;

		param->cuLevelRCEnable      = 0;
		param->hvsQPEnable          = 1;
		param->hvsQpScale           = 2;
		param->hvsQpScaleEnable     = (param->hvsQpScale > 0) ? 1: 0;

		/* for CMD_ENC_RC_MIN_MAX_QP */
		param->minQp                = 8;
		param->maxQp                = 51;
		param->maxDeltaQp           = 10;  //If it is less than 10, init. fail(-1) occurs.
		if (pInitParam->m_iUseSpecificRcOption & 1) { //added by 20201208 : 2) min / max QP for inter picture in seq. step
			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0040) { //m_iInterQpMin
				if ( (pInitParam->m_stRcInit.m_iInterQpMin >= 0) && (pInitParam->m_stRcInit.m_iInterQpMin < 52) )
					param->minQp = pInitParam->m_stRcInit.m_iInterQpMin;
			}

			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0080) {	//m_iInterQpMax
				if ((pInitParam->m_stRcInit.m_iInterQpMax > 0) && (pInitParam->m_stRcInit.m_iInterQpMax < 52)) {
					param->maxQp = pInitParam->m_stRcInit.m_iInterQpMax;
				}
			}
		}

		/* for CMD_ENC_CUSTOM_GOP_PARAM */
		param->gopParam.customGopSize = 0;
		param->gopParam.useDeriveLambdaWeight = 0;
		for (i= 0; i<param->gopParam.customGopSize; i++) {
			param->gopParam.picParam[i].picType      = WAVE420L_PIC_TYPE_I;
			param->gopParam.picParam[i].pocOffset    = 1;
			param->gopParam.picParam[i].picQp        = 30;
			param->gopParam.picParam[i].refPocL0     = 0;
			param->gopParam.picParam[i].refPocL1     = 0;
			param->gopParam.picParam[i].temporalId   = 0;
			param->gopParam.gopPicLambda[i]          = 0;
		}
		if (pInitParam->m_iUseSpecificRcOption & 1) {
			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0200) { //m_iInitialInterRcQp
				if ((pInitParam->m_stRcInit.m_iInitialInterRcQp > 0) && (pInitParam->m_stRcInit.m_iInitialInterRcQp < 52)) {
					param->gopPresetIdx = PRESET_IDX_CUSTOM_GOP; //0
					param->gopParam.customGopSize     = 4;
					param->gopParam.useDeriveLambdaWeight = 0;
					for (i= 0; i<param->gopParam.customGopSize; i++)  {
						param->gopParam.picParam[i].picType      = WAVE420L_PIC_TYPE_P; //WAVE420L_PIC_TYPE_I;
						param->gopParam.picParam[i].pocOffset    = (i+1);
						param->gopParam.picParam[i].picQp        = pInitParam->m_stRcInit.m_iInitialInterRcQp;
						param->gopParam.picParam[i].refPocL0     = 0;
						param->gopParam.picParam[i].refPocL1     = 0;
						param->gopParam.picParam[i].temporalId   = 0;
						param->gopParam.gopPicLambda[i]          = 0;
					}
				}
			}
		}

		param->transRate = 0;
		if (pInitParam->m_iUseSpecificRcOption & 1) {
			unsigned bitrateflag = pInitParam->m_stRcInit.m_iUseBitrateOption;

			if ((bitrateflag& 0x3) == 0x1) {
				if ((bitrateflag& 0x2) && (rcBitrate > 0)) {
					int transbitrate = pInitParam->m_stRcInit.m_iTransKbps * 1000;

					// VBR : m_iTransBitrate > m_iTargetKbps or RcControlOff(Bitrate=0)
					// CBR : m_iTransBitrate = m_iTargetKbps
					// ABR : 0 <= m_iTransBitrate < m_iTargetKbps(the input variable is ignored)
					if (transbitrate > rcBitrate)
						param->transRate = transbitrate; //VBR

					if (transbitrate == rcBitrate)
						param->transRate = transbitrate; //CBR
				}
			}
		}

		// for VUI / time information.
		param->timeScale              = pEncOP->frameRateInfo * 1000;
		param->numUnitsInTick         = 1000;
		param->numTicksPocDiffOne     = 0;
		param->vuiParam.vuiParamFlags = 0;	// when vuiParamFlags == 0, VPU doesn't encode VUI

	#if defined(TEST_VUI_ENC_CASE)
		pInitParam->m_uiVuiParamOption = 1;
	#endif
		if (pInitParam->m_uiVuiParamOption == 1)
			wave420l_setEncOpenVui(pEncOP, pInitParam);	// for VUI of SPS

		param->chromaCbQpOffset = 0;
		param->chromaCrQpOffset = 0;

		param->initialRcQp      = 63;       // The value of initial QP by HOST application. This value is meaningless if INITIAL_RC_QP is 63.
		if (pInitParam->m_iUseSpecificRcOption & 1) {
			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0100) {	//m_iInitialIntraRcQp
				int initialRcQp = pInitParam->m_stRcInit.m_iInitialIntraRcQp;

				if ((initialRcQp > 0) && (initialRcQp < 52)) {
					if ((initialRcQp > param->maxQp) || (initialRcQp < param->minQp)) {
						param->initialRcQp      = 63;       // The value of initial QP by HOST application. This value is meaningless if INITIAL_RC_QP is 63.
						//param->intraQP = 0;
					} else {
						param->initialRcQp = param->intraQP = initialRcQp;
					}
				}
			}
		}

		//ENC_NR_PARAM
		param->nrYEnable        = 0;	// It enables noise reduction algorithm to Y component.
		param->nrCbEnable       = 0;	// It enables noise reduction algorithm to Cb component.
		param->nrCrEnable       = 0;	// It enables noise reduction algorithm to Cr component.
		param->nrNoiseEstEnable = 0;	// It enables noise estimation for reduction. When this is disabled, noise estimation is carried out ouside VPU.
		param->nrNoiseSigmaY    = 0;	// It specifies Y noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0).
		param->nrNoiseSigmaCb   = 0;	// It specifies Cb noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0).
		param->nrNoiseSigmaCr   = 0;	// It specifies Cr noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0).

		//ENC_NR_WEIGHT
		param->nrIntraWeightY   = 0;	// A weight to Y noise level for intra picture (0 ~ 31). nrIntraWeight/4 is multiplied to the noise level that has been estimated. This weight is put for intra frame to be filtered more strongly or more weakly than just with the estimated noise level.
		param->nrIntraWeightCb  = 0;	// A weight to Cb noise level for intra picture (0 ~ 31)
		param->nrIntraWeightCr  = 0;	// A weight to Cr noise level for intra picture (0 ~ 31)
		param->nrInterWeightY   = 0;	// A weight to Y noise level for inter picture (0 ~ 31). nrInterWeight/4 is multiplied to the noise level that has been estimated. This weight is put for inter frame to be filtered more strongly or more weakly than just with the estimated noise level.
		param->nrInterWeightCb  = 0;	// A weight to Cb noise level for inter picture (0 ~ 31)
		param->nrInterWeightCr  = 0;	// A weight to Cr noise level for inter picture (0 ~ 31)

		param->intraMinQp       = 8;
		param->intraMaxQp       = 51;
		if (pInitParam->m_iUseSpecificRcOption & 1) {
			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0010) {	//m_iIntraQpMin
				if ((pInitParam->m_stRcInit.m_iIntraQpMin >= 0) && (pInitParam->m_stRcInit.m_iIntraQpMin < 52))
					param->intraMinQp = pInitParam->m_stRcInit.m_iIntraQpMin;
			}

			if (pInitParam->m_stRcInit.m_iUseQpOption & 0x0020) {	//m_iIntraQpMax
				if ((pInitParam->m_stRcInit.m_iIntraQpMax > 0) && (pInitParam->m_stRcInit.m_iIntraQpMax < 52))
					param->intraMaxQp = pInitParam->m_stRcInit.m_iIntraQpMax;
			}
		}
		param->forcedIdrHeaderEnable = 1; // It enables every IDR frame to include VPS/SPS/PPS.
	}
	return 1;
}

/*!
 ***********************************************************************
 * \brief
 *
 * \param
 *
 * \param
 *
 * \return
 *
 ***********************************************************************
 */
int wave420l_enc_reset(wave420l_t *pVpu, int iOption)
{
	int coreIdx = 0;

	if (iOption & 1) {
		VpuWriteReg(pVpu->m_pCodecInst->coreIdx, W4_VPU_VINT_REASON_CLR, 0);
		VpuWriteReg(pVpu->m_pCodecInst->coreIdx, W4_VPU_VINT_CLEAR, 1);
	}

	wave420l_SetPendingInst(coreIdx, 0);

	//reset global var.
	wave420l_reset_global_var((iOption>>16) & 0x0FF);

	return RETCODE_SUCCESS;
}


/*!
 ***********************************************************************
 * \brief
 *
 * \param
 *
 * \param
 *
 * \param
 *
 * \return
 *
 ***********************************************************************
 */
static RetCode wave420l_enc_init(wave420l_t **ppVpu,  hevc_enc_init_t *pInitParam, hevc_enc_initial_info_t *pInitialInfo)
{
	RetCode ret;
	PhysicalAddress codeBase;
	EncHandle handle = { 0 };

	if (gHevcEncFWAddr == 0) {
		return RETCODE_INVALID_COMMAND;
	}

	if (pInitParam->m_iBitstreamFormat != 17)	// Telechips STD_HEVC_ENC=17
		return RETCODE_CODEC_SPECOUT;

	pInitParam->m_iBitstreamFormat = WAVE420L_STD_HEVC; //replace

	if( pInitParam->m_RegBaseAddr[VA] == 0 )
		return RETCODE_MEM_ACCESS_VIOLATION;

	gHevcEncVirtualRegBaseAddr = pInitParam->m_RegBaseAddr[VA];

	//! [0] Global callback func
	{
		wave420l_memcpy = pInitParam->m_Memcpy;
		wave420l_memset = pInitParam->m_Memset;
		wave420l_interrupt = pInitParam->m_Interrupt;
		wave420l_ioremap = pInitParam->m_Ioremap;
		wave420l_iounmap = pInitParam->m_Iounmap;
		wave420l_read_reg = pInitParam->m_reg_read;
		wave420l_write_reg = pInitParam->m_reg_write;
		wave420l_usleep = pInitParam->m_Usleep;

		if( wave420l_memcpy == NULL )
			wave420l_memcpy = wave420l_local_mem_cpy;

		if( wave420l_memset == NULL )
			wave420l_memset = wave420l_local_mem_set;
	}

	codeBase = gHevcEncFWAddr;
	//gHevcEncInitFw = codeBase;

	//! [1] VPU Init
	if (gHevcEncInitFirst == 0)  {  //if( (gHevcEncInitFirst == 0) || (VpuReadReg(0/*coreIdx*/, W4_VCPU_CUR_PC)==0) )
		gHevcEncVirtualBitWorkAddr = pInitParam->m_BitWorkAddr[VA];
		gHevcEncPhysicalBitWorkAddr= pInitParam->m_BitWorkAddr[PA];

		//initialized global variables
		{
			gHevcEncInitFirst = 1;
			gHevcEncInitFirst_exit = 0;
			gpHevcEncHandlePool = &gsHevcEncHandlePool[0];
			gpCodecInstPoolWave420l = &gsCodecInstPoolWave420l[0];
			gHevcEncMaxInstanceNum = VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE;
			gsHevcEncBitCodeSize = VPU_HEVC_ENC_MAX_CODE_BUF_SIZE;

			#ifdef HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_SHIFT
			if ((pInitParam->m_uiEncOptFlags >> HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_SHIFT) & HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_BITS_MASK) {
				gHevcEncMaxInstanceNum = (pInitParam->m_uiEncOptFlags >> HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_SHIFT) & HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_BITS_MASK;
				if (gHevcEncMaxInstanceNum > VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE) {
					gHevcEncMaxInstanceNum = VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE;
				}
			}
			#endif
		}

		ret = wave420l_VPU_InitWithBitcode(0, pInitParam->m_BitWorkAddr[PA], codeBase, gsHevcEncBitCodeSize);




		if (ret != RETCODE_SUCCESS && ret != RETCODE_CALLED_BEFORE) {
			if (ret == RETCODE_CODEC_EXIT) {
				gHevcEncInitFirst = 0;

				#ifdef USE_WAVE420L_SWReset_AT_CODECEXIT
				wave420l_SWReset(0, SW_RESET_SAFETY, handle);
				#endif
				wave420l_enc_reset( NULL, ( 0 | (1<<16) ) );
			}
			return ret;
		}
	}
#ifdef ADD_VPU_INIT_SWRESET
	else {
	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		wave420l_VPU_SWReset(0/*coreIdx*/, SW_RESET_SAFETY, handle);
		ret = wave420l_VPU_SWReset2();//wave420l_VPU_SWReset( SW_RESET_SAFETY );
		if (ret == RETCODE_CODEC_EXIT) {
			wave420l_enc_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
	#else
		wave420l_VPU_SWReset2();//wave420l_VPU_SWReset( SW_RESET_SAFETY );
	#endif
	}
#endif

	//! [2] Get Enc Handle
	ret = wave420l_get_vpu_handle(ppVpu);
	if (ret == RETCODE_FAILURE) {
		*ppVpu = 0;
		return ret;
	}

	(*ppVpu)->m_BitWorkAddr[PA] = pInitParam->m_BitWorkAddr[PA];
	(*ppVpu)->m_BitWorkAddr[VA] = pInitParam->m_BitWorkAddr[VA];

	{
		Uint32 version;
		Uint32 revision;
		Uint32 productId;

		//PrintVpuVersionInfo(coreIdx);
		ret = wave420l_VPU_GetVersionInfo(0, &version, &revision, &productId);

		#if !defined(VLOG_DISABLE)
		VLOG(INFO, "VPU coreNum : [%d]\n", core_idx);
		VLOG(INFO, "Firmware : CustomerCode: %04x | version : %d.%d.%d rev.%d\n", (Uint32)(version>>16), (Uint32)((version>>(12))&0x0f), (Uint32)((version>>(8))&0x0f), (Uint32)((version)&0xff), revision);
		VLOG(INFO, "API      : %d.%d.%d\n\n", API_VERSION_MAJOR, API_VERSION_MINOR, API_VERSION_PATCH);
		#endif

		DLOGI("Firmware : CustomerCode: %04x | version : %d.%d.%d rev.%d\n",
				(Uint32)(version>>16),
				(Uint32)((version>>(12))&0x0f), (Uint32)((version>>(8))&0x0f), (Uint32)((version)&0xff),
				revision);

		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_EXIT) {
				#ifdef USE_WAVE420L_SWReset_AT_CODECEXIT
				wave420l_SWReset(0, SW_RESET_SAFETY, handle);
				#endif
				wave420l_enc_reset( NULL, ( 0 | (1<<16) ) );
			}
			return ret;
		}
		(*ppVpu)->m_iDateRTL = version;
		(*ppVpu)->m_iRevisionRTL = revision;
	}

	//! [3] Open Encoder
	{
		EncOpenParam op = {0,};

		ret = wave420l_GetEncOpenParamDefault(&op, pInitParam);
		(*ppVpu)->m_iBitstreamFormat = op.bitstreamFormat;

	#	if defined(CTRL_3DNR_FEATURE_TEST_DEFAULT)
		{
			//if( pInitParam->m_uiEncOptFlags & (1<<TEST_3DNR_DEFAULT_ON) )
			{
				op.EncStdParam.hevcParam.nrYEnable        = 1;	// EnNoiseReductionY : 0
				op.EncStdParam.hevcParam.nrCbEnable       = 1;	// EnNoiseReductionCb : 0
				op.EncStdParam.hevcParam.nrCrEnable       = 1;	// EnNoiseReductionCr : 0
				op.EncStdParam.hevcParam.nrNoiseEstEnable = 1;	// EnNoiseEst : 1
				op.EncStdParam.hevcParam.nrNoiseSigmaY    = 0;	// NoiseSigmaY : 0
				op.EncStdParam.hevcParam.nrNoiseSigmaCb   = 0;	// NoiseSigmaCb : 0
				op.EncStdParam.hevcParam.nrNoiseSigmaCr   = 0;	// NoiseSigmaCr : 0

				op.EncStdParam.hevcParam.nrIntraWeightY   = 7;	// IntraNoiseWeightY : 7
				op.EncStdParam.hevcParam.nrIntraWeightCb  = 7;	// IntraNoiseWeightCb : 7
				op.EncStdParam.hevcParam.nrIntraWeightCr  = 7;	// IntraNoiseWeightCr : 7
				op.EncStdParam.hevcParam.nrInterWeightY   = 4;	// InterNoiseWeightY : 4
				op.EncStdParam.hevcParam.nrInterWeightCb  = 4;	// InterNoiseWeightCb : 4
				op.EncStdParam.hevcParam.nrInterWeightCr  = 4;	// InterNoiseWeightCr : 4
			}
		}
	#	endif

		//pEncConfig->outNum = 30; //  pEncConfig->outNum == 0 ? 30 : pEncConfig->outNum;  //DEFAULT_ENC_OUTPUT_NUM 30

		// Max resolution: 8192x4096
		// A picture width and height shall be multiple of 8.
		// Min resolution: widthxheight = 256x128
		if (op.picWidth < VPU_HEVC_ENC_MIM_WIDTH)
			return RETCODE_INVALID_STRIDE;

		if (op.picHeight < VPU_HEVC_ENC_MIM_HEIGHT)
			return RETCODE_INVALID_STRIDE;

		if (op.picWidth >= 8192)
			return RETCODE_INVALID_STRIDE;

		if (op.picHeight >= 4096)
			return RETCODE_INVALID_STRIDE;

		{
			PhysicalAddress workbufaddr;

			workbufaddr = pInitParam->m_BitWorkAddr[PA] + VPU_HEVC_ENC_SIZE_BIT_WORK;

			//ADDR_WORK_BASE   W4_ADDR_WORK_BASE       pEncInfo->vbWork.phys_addr  128KB
			op.vbWork.phys_addr = workbufaddr + ((*ppVpu)->m_iVpuInstIndex * (VPU_HEVC_ENC_WORKBUF_SIZE));
			op.vbWork.size = WAVE4ENC_WORKBUF_SIZE * 2;   // 256K (> 128 KB)
			//op.vbWork.size = VPU_HEVC_ENC_WORKBUF_SIZE;

			op.vbTemp.phys_addr = pInitParam->m_BitWorkAddr[PA] + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + VPU_HEVC_ENC_STACK_SIZE;
			op.vbTemp.size = VPU_HEVC_ENC_TEMPBUF_SIZE;  //3840x2160 45K, 1920x1080 22.5K
		}

		// Open an instance and get initial information for encoding.
		ret = wave420l_VPU_EncOpen( &handle, &op );
		if( ret != RETCODE_SUCCESS )
		{
			if( ret == RETCODE_CODEC_EXIT ) {
				#ifdef USE_WAVE420L_SWReset_AT_CODECEXIT
				wave420l_SWReset(0, SW_RESET_SAFETY, handle);
				#endif
				wave420l_enc_reset( (*ppVpu), ( 0 | (1<<16) ) );
			}
			*ppVpu = 0;
			return ret;
		}

		(*ppVpu)->m_pCodecInst = handle;
		(*ppVpu)->m_pCodecInst->productId = PRODUCT_ID_420L;
		(*ppVpu)->m_iBitstreamFormat = op.bitstreamFormat;

		(*ppVpu)->m_iPicWidth  = op.picWidth;
		(*ppVpu)->m_iPicHeight = op.picHeight;

		{
			int srcFrameWidth, srcFrameStride, srcFrameHeight;
			int srcFrameFormat = op.srcFormat;
			int cbcrInterleave = op.cbcrInterleave;
			int maptype = COMPRESSED_FRAME_MAP;

			srcFrameWidth = ALIGNED_BUFF(op.picWidth, 8);  // width = 8-aligned (CU unit)
			srcFrameHeight = ALIGNED_BUFF(op.picHeight, 8); // height = 8-aligned (CU unit)
			srcFrameStride = wave420l_CalcStride(srcFrameWidth, srcFrameHeight, (FrameBufferFormat)srcFrameFormat, cbcrInterleave, (TiledMapType)maptype, FALSE);

			(*ppVpu)->m_iFrameBufWidth = srcFrameWidth;
			(*ppVpu)->m_iFrameBufHeight = srcFrameHeight;
			(*ppVpu)->m_iFrameBufStride = srcFrameStride;
		}

		//(*ppVpu)->m_StreamRdPtrRegAddr = handle->CodecInfo->encInfo.streamRdPtrRegAddr;
		//(*ppVpu)->m_StreamWrPtrRegAddr = handle->CodecInfo->encInfo.streamWrPtrRegAddr;
		(*ppVpu)->m_uiEncOptFlags = pInitParam->m_uiEncOptFlags;
	}

	(*ppVpu)->m_pCodecInst->CodecInfo->encInfo.openParam.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
	//(*ppVpu)->m_pCodecInst->m_iVersionRTL = (*ppVpu)->m_iVersionRTL;

	/* Secondary AXI */
	{
		CodecInst *pCodecInst = handle;
		EncInfo *pEncInfo = &pCodecInst->CodecInfo->encInfo;
		SecAxiUse secAxiUse;
		unsigned int secAxiOptionFlags;

		secAxiOptionFlags = (pInitParam->m_uiEncOptFlags);
		{
			PhysicalAddress secAxiBufBase;
			int use = 1;

			secAxiBufBase = pInitParam->m_BitWorkAddr[PA] + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + VPU_HEVC_ENC_TEMPBUF_SIZE + VPU_HEVC_ENC_STACK_SIZE;

			if( (secAxiOptionFlags >> SEC_AXI_BUS_DISABLE_SHIFT) & 0x1 )
				use = 0;  //SecAXI off

			pInitialInfo->m_Reserved[2] = (use << 30);
			pEncInfo->secAxiInfo.u.wave4.useEncRdoEnable = use;
			pEncInfo->secAxiInfo.u.wave4.useEncLfEnable = use;
			secAxiUse.u.wave4.useEncRdoEnable  = use;  //USE_RDO_INTERNAL_BUF
			secAxiUse.u.wave4.useEncLfEnable   = use;  //USE_LF_INTERNAL_BUF

			pEncInfo->secAxiInfo.bufBase = secAxiBufBase;
			pEncInfo->secAxiInfo.bufSize = VPU_HEVC_ENC_SEC_AXI_BUF_SIZE;
		}
		wave420l_VPU_EncGiveCommand(handle, SET_SEC_AXI, &secAxiUse);

		pEncInfo->mapType = COMPRESSED_FRAME_MAP;
	}

	//! [4] Get Initial Info
	{
		EncInitialInfo initialInfo = {0,};
		ret = wave420l_VPU_EncGetInitialInfo( handle, &initialInfo );
		if( ret != RETCODE_SUCCESS )
		{
			#ifdef ADD_SWRESET_VPU_HANGUP
			if (ret == RETCODE_CODEC_EXIT) 	{
				MACRO_VPU_SWRESET
				wave420l_enc_reset( NULL, ( 0 | (1<<16) ) );
			}
			#endif

			if (ret == RETCODE_CODEC_EXIT) {
				#ifdef USE_WAVE420L_SWReset_AT_CODECEXIT
				wave420l_SWReset(0, SW_RESET_SAFETY, handle);
				#endif
				wave420l_enc_reset( (*ppVpu), ( 0 | (1<<16) ) );
				*ppVpu = 0;
			}
			return ret;
		}
		#if !defined(VLOG_DISABLE)
		VLOG(INFO, "* Enc InitialInfo =>\n instance #%d, \n minframeBuffercount: %u\n minSrcBufferCount: %d\n", instIdx, initialInfo.minFrameBufferCount, initialInfo.minSrcFrameCount);
		VLOG(INFO, " picWidth: %u\n picHeight: %u\n ",encOP.picWidth, encOP.picHeight);
		#endif

		//check alied size & address of buffers (frame buffer....)
		{
			unsigned int bufAlignSize = 0, bufStartAddrAlign = 0;

			if (bufAlignSize < VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN) {
				bufAlignSize = VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN;
			}

			if (bufStartAddrAlign < VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN) {
				bufStartAddrAlign = VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN;
			}

			(*ppVpu)->bufAlignSize = bufAlignSize;
			(*ppVpu)->bufStartAddrAlign = bufStartAddrAlign;
		}

		// output
		(*ppVpu)->m_iNeedFrameBufCount = pInitialInfo->m_iMinFrameBufferCount = initialInfo.minFrameBufferCount;
		{
			int framebufFormat, picWidth, picHeight, mapType, framebufStride;
			BOOL cbcrInterleave;

			cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
			framebufFormat = FORMAT_420;
			mapType = COMPRESSED_FRAME_MAP;
			picWidth   = ((*ppVpu)->m_iPicWidth +7)&~7;;
			picHeight = ((*ppVpu)->m_iPicHeight +7)&~7;;

			framebufStride = wave420l_CalcStride(picWidth, picHeight, (FrameBufferFormat)framebufFormat, cbcrInterleave, (TiledMapType)mapType , FALSE);
			pInitialInfo->m_iMinFrameBufferSize = cal_vpu_hevc_enc_framebuf_size(	framebufStride, picHeight, (*ppVpu)->m_iNeedFrameBufCount, //frame_count =2
												(*ppVpu)->bufAlignSize, (*ppVpu)->bufStartAddrAlign);
		}
		pInitialInfo->m_iMinFrameBufferCount = 1;	//Since the frame count has already been reflected when calculating the size, '1' is returned.
	}

	(*ppVpu)->m_iBitstreamOutBufferPhyVirAddrOffset = pInitParam->m_BitstreamBufferAddr[PA] - pInitParam->m_BitstreamBufferAddr[VA];

	return ret;
}

/*!
 ***********************************************************************
 * \brief
 *
 * \param
 *
 * \param
 *
 * \return
 *
 ***********************************************************************
 */
static RetCode
wave420l_enc_register_frame_buffer(wave420l_t *pVpu, hevc_enc_buffer_t *pBufferParam)
{
	RetCode ret;
	EncHandle handle = pVpu->m_pCodecInst;
	EncInfo *pEncInfo = &handle->CodecInfo->encInfo;
	EncOpenParam *pOpenParam = &pEncInfo->openParam;
	EncInitialInfo *pInitInfo = &pEncInfo->initialInfo;
	int mapType = COMPRESSED_FRAME_MAP;
	int framebufWidth = pVpu->m_iPicWidth;
	int framebufHeight = pVpu->m_iPicHeight;
	int framebufStride;
	int cbcrInterleave = pOpenParam->cbcrInterleave;
	int srcFormat = pOpenParam->srcFormat;
	int FrameBufSize;
	int bufStartAddrAlign;
	int bufAlignSize;
	int regFrameBufCount = 0;
	static FrameBuffer fbRecon[MAX_REG_FRAME];
	unsigned int offset = 0;
	unsigned int saddr_offset = 0;	//start address aligned offset

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_FrameBufferStartAddr[PA];

	bufAlignSize = pVpu->bufAlignSize;
	bufStartAddrAlign = pVpu->bufStartAddrAlign;

	//  Allocate framebuffers for recon.
	{
		int i;
		PhysicalAddress start_addr = (unsigned long)pVpu->m_FrameBufferStartPAddr;

		mapType = COMPRESSED_FRAME_MAP; //LINEAR_FRAME_MAP;
		framebufWidth  = (framebufWidth +7)&~7;
		framebufHeight = (framebufHeight +7)&~7;
		framebufStride = wave420l_CalcStride(framebufWidth, framebufHeight, (FrameBufferFormat)srcFormat, cbcrInterleave, (TiledMapType)mapType , FALSE);
		FrameBufSize   = wave420l_VPU_GetFrameBufSize(0, framebufStride, framebufHeight, mapType, srcFormat, cbcrInterleave, NULL);
		FrameBufSize = VPU_ALIGN(FrameBufSize, bufAlignSize);
		wave420l_memset(&fbRecon[0], 0x00, sizeof(fbRecon),0);

		if (bufStartAddrAlign > 0)
		{
			if (start_addr & (bufStartAddrAlign-1)) {	//need to aligned address
				saddr_offset = start_addr & (bufStartAddrAlign-1);
				saddr_offset = bufStartAddrAlign - saddr_offset;
			}
		}

		regFrameBufCount = pInitInfo->minFrameBufferCount;
		for (i = 0; i < regFrameBufCount; i++) {
			fbRecon[i].bufY  = (pVpu->m_FrameBufferStartPAddr + saddr_offset + i*FrameBufSize);
			fbRecon[i].bufCb = fbRecon[i].bufY + (framebufStride * framebufHeight);	//fbArr[i].bufCb = fbArr[i].bufY + (sizeLuma >> fieldFrame);
			fbRecon[i].bufCr = (PhysicalAddress)-1;
			fbRecon[i].size  = FrameBufSize;
			fbRecon[i].updateFbInfo = TRUE;
		}

		offset = regFrameBufCount * FrameBufSize + saddr_offset;
		pEncInfo->vbMV.base = 0;
		pEncInfo->vbMV.phys_addr = pBufferParam->m_FrameBufferStartAddr[PA] + offset;
		pEncInfo->vbMV.size = regFrameBufCount * pVpu->m_iMinFrameBufferSize - offset;
		if( pBufferParam->m_FrameBufferStartAddr[VA]  != 0 )
			pEncInfo->vbMV.virt_addr  = pBufferParam->m_FrameBufferStartAddr[VA] + offset;
		else
			pEncInfo->vbMV.virt_addr = 0;
	}

	{
		ret = wave420l_VPU_EncRegisterFrameBuffer(handle, fbRecon, regFrameBufCount, framebufStride, framebufHeight, mapType);
		if (ret != RETCODE_SUCCESS) {
			#if !defined(VLOG_DISABLE)
			VLOG(ERR, "VPU_EncRegisterFrameBuffer failed Error code is 0x%x \n", ret );
			#endif
			//goto ERR_ENC_OPEN;
			return ret;
		}
	}

	return ret;
}

#ifdef HEVCENC_REPORT_MVINFO

#define VPU_CHECK_BIT(_data, _pos) ((_data) & (0x01 << (_pos)))

typedef struct {
    // CU16MVData[24:0] = {cu16_flagl0, cu16_mvxl0, cu16_mvyl0}
    Uint8   idx;
    Uint8   cu16_flagl0 : 1;    //1bit
    Int16   cu16_mvxl0  : 12;   //12bit
    Int16   cu16_mvyl0  : 12;   //12bit
} CU16MVData;

typedef struct {
    // CU32MVData[127:0]   = {28'b0, Cu16MVData[0], Cu16MVData[1], Cu16MVData[2], Cu16MVData[3]}
    CU16MVData Cu16MVData[4];
} CU32MVData;

static void wave420l_enc_GetCU32MVData(Uint8 *buf, CU32MVData *Cu32MVData, Uint32 bufPos)
{
    Uint32   cu32_idx = 0, cu16_idx = 0;
    Int16    temp = 0;

    for ( cu32_idx = 0; cu32_idx < 4; cu32_idx++ ) {
        cu16_idx = 0;
        while ( cu16_idx < 4 ) {
            // {28'b0, Cu16MVData[0], Cu16MVData[1]}
            // CU16MVData[49:0] = {cu16_flagl0, cu16_mvxl0, cu16_mvyl0, cu16_flagl1, cu16_mvxl1, cu16_mvyl1}

            // 28bit = 24bit + 4bit
            // 24bit = 3 * 8bit
            bufPos += 3;

            // cu16_flagl0 = 1bit
            if ( VPU_CHECK_BIT(buf[bufPos], 3) ) {
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 1;

                // cu16_mvxl0 = 12bit
                // 3bit
                temp = buf[bufPos] & 0x7;

                // 8bit
                bufPos += 1;
                temp = (temp << 8) | buf[bufPos];

                // 1bit
                bufPos += 1;
                temp = (temp << 1) | ((buf[bufPos] >> 7) & 0x1);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = temp;

                // cu16_mvyl0 = 12bit
                // 7bit
                temp = buf[bufPos] & 0x7F;

                // 5bit
                bufPos += 1;
                temp = (temp << 5) | ((buf[bufPos] >> 3) & 0x1F);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = temp;
            }
            else {
                bufPos += 3;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
            }

            Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = cu16_idx + cu32_idx*4;
            cu16_idx++;

            //cu16_flagl0 = 1bit
            if ( VPU_CHECK_BIT(buf[bufPos], 2) ) {
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 1;

                // cu16_mvxl1 = 12bit
                // 2bit
                temp = buf[bufPos] & 0x3;

                // 8bit
                bufPos += 1;
                temp = (temp << 8) | buf[bufPos];

                // 2bit
                bufPos += 1;
                temp = (temp << 2) | ((buf[bufPos] >> 6) & 0x3);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = temp;

                // cu16_mvyl0 = 12bit
                // 6bit
                temp = buf[bufPos] & 0x3F;

                // 6bit
                bufPos += 1;
                temp = (temp << 6) | ((buf[bufPos] >> 2) & 0x3F);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = temp;
            }
            else {
                bufPos += 3;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
            }

            Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = cu16_idx + cu32_idx*4;
            cu16_idx++;

            // cu16_flagl0 = 1bit
            if ( VPU_CHECK_BIT(buf[bufPos], 1) ) {
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 1;

                // cu16_mvxl0 = 12bit
                // 1bit
                temp = buf[bufPos] & 0x1;

                // 8bit
                bufPos += 1;
                temp = (temp << 8) | buf[bufPos];

                // 3bit
                bufPos += 1;
                temp = (temp << 3) | ((buf[bufPos] >> 5) & 0x7);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = temp;

                // cu16_mvyl0 = 12bit
                // 5bit
                temp = buf[bufPos] & 0x1F;

                // 7bit
                bufPos += 1;
                temp = (temp << 7) | ((buf[bufPos] >> 1) & 0x7F);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = temp;
            }
            else {
                bufPos += 3;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
            }

            Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = cu16_idx + cu32_idx*4;
            cu16_idx++;
            // cu16_flagl0 = 1bit
            if ( VPU_CHECK_BIT(buf[bufPos], 0) ) {
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 1;

                // cu16_mvxl1 = 12bit
                // 8bit
                bufPos += 1;
                temp = buf[bufPos];

                // 4bit
                bufPos += 1;
                temp = (temp << 4) | ((buf[bufPos] >> 4) & 0xF);
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = temp;

                // cu16_mvyl1 = 12bit
                // 4bit
                temp = buf[bufPos] & 0xF;

                // 8bit
                bufPos += 1;
                temp = (temp << 8) | buf[bufPos];
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = temp;

            }
            else {
                bufPos += 3;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
                Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
            }

            Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = cu16_idx + cu32_idx*4;
            cu16_idx++;
            bufPos++;
        }
    }
};

#endif	//HEVCENC_REPORT_MVINFO

static void wave420l_enc_ConvertMVData(
	Uint8    *buf,
	Uint32    width,
	Uint32    height,
	Uint8    *outbuf,
	int       outformat
)
{
	Uint32     bufPos   = 0;
	Uint32     cu16_idx = 0, cu32_idx = 0;
	Uint32     ctbx, ctby, max_ctbx, max_ctby, x0, y0;

	max_ctbx = (((width+127)/128)*128)/64;
	max_ctby = (height+63)/64;

	if (outformat == 2)	//   2 : copy (big endian, 12bits per X or Y)
	{
		for (ctby = 0; ctby < max_ctby; ctby++)
		{
			for (ctbx = 0; ctbx < max_ctbx; ctbx++)
			{
				wave420l_memcpy((outbuf+bufPos), (buf+bufPos), 16*4, 2);
				bufPos += 16*4; // 16byte = Cu16MVData * 2
			}
		}
	} else if (outformat == 1)	//   1 : convert (little endian, 16bits per X or Y)
	{
		short *poutbuf = (short *)outbuf;
		CU32MVData Cu32MVData[4];
		unsigned char cdata16[16*4];

		for (ctby = 0; ctby < max_ctby; ctby++)
		{
			for (ctbx = 0; ctbx < max_ctbx; ctbx++)
			{
				wave420l_memcpy(cdata16, (buf+bufPos), 16*4, 2);
				wave420l_swap_endian(0, cdata16, 16*4, VDI_BIG_ENDIAN);	//008

				for (cu32_idx = 0; cu32_idx < 4; cu32_idx++)
				{
					for (cu16_idx = 0; cu16_idx < 4; cu16_idx++)
					{
						Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = 0;
						Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
						Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
						Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
					}
				}
				wave420l_enc_GetCU32MVData(cdata16, Cu32MVData, 0);	// Get data for CTB unit. CU32 * 4.

				//16x16 {MvX:16bits, MvY:16bits}
				for (cu32_idx = 0; cu32_idx < 4; cu32_idx++)
				{
					for (cu16_idx = 0; cu16_idx < 4; cu16_idx++)
					{
						x0 = (ctbx * 64) + ((cu32_idx%2) * 32) + ((cu16_idx%2) * 16);
						y0 = (ctby * 64) + ((cu32_idx/2) * 32) + ((cu16_idx/2) * 16);

						if (((x0+7) < width) && ((y0+7) < height))
						{
						} else
						{
							Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].idx = -1;
							Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_flagl0 = 0;
							Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0 = 0;
							Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0 = 0;
						}
						*poutbuf = (short)Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvxl0;
						poutbuf++;
						*poutbuf = (short)Cu32MVData[cu32_idx].Cu16MVData[cu16_idx].cu16_mvyl0;
						poutbuf++;
					}
				}
				bufPos += 16*4; // 16byte = Cu16MVData * 2
			}
		}
	} else
	{
		//nothing
	}
}

/*!
 ***********************************************************************
 * \brief
 *
 * \param
 *
 * \param
 *
 * \param
 *
 * \return
 *
 ***********************************************************************
 */

static int
wave420l_enc_encode( wave420l_t *pVpu, hevc_enc_input_t *pInputParam, hevc_enc_output_t *pOutParam )
{
	RetCode ret;
	EncHandle handle = pVpu->m_pCodecInst;
	int iHeaderWriteSize = 0;

	//! Encoding
	{
		EncInfo *pEncInfo = &handle->CodecInfo->encInfo;
		EncOpenParam * pOpenParam = &pEncInfo->openParam;
		//EncInitialInfo *pInitInfo = &pEncInfo->initialInfo;
		EncParam enc_param = {0,};
		FrameBuffer src_frame = {0,};

	#ifdef HEVCENC_REPORT_MVINFO
		if (pInputParam->mvInfoBufSize > 0)
		{
			int w, h, mvColSize;

			w  = VPU_ALIGN8(pEncInfo->openParam.picWidth);
			h  = VPU_ALIGN8(pEncInfo->openParam.picHeight);
			mvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(w, h);
			if (pInputParam->mvInfoBufSize < mvColSize)
			{
				pInputParam->mvInfoBufSize = 0;
			}
		}

		if ((pInputParam->mvInfoBufAddr[VA] == NULL) ||
		    (pInputParam->mvInfoBufSize == 0) ||
		    (pInputParam->mvInfoFormat == 0))
		{
			pOutParam->mvInfoBufAddr[PA] = NULL;
			pOutParam->mvInfoBufAddr[VA] = NULL;
			pOutParam->mvInfoBufSize = 0;
			pOutParam->mvInfoFormat = 0;
		} else
		{
			pOutParam->mvInfoBufAddr[PA] = pInputParam->mvInfoBufAddr[PA];
			pOutParam->mvInfoBufAddr[VA] = pInputParam->mvInfoBufAddr[VA];
			pOutParam->mvInfoBufSize = pInputParam->mvInfoBufSize;
			pOutParam->mvInfoFormat = pInputParam->mvInfoFormat;
		}
	#endif

		src_frame.srcBufState = SRC_BUFFER_USE_ENCODE; // 3     //!< source buffer was sent to VPU. but it was not used for encoding.
		src_frame.bufY  = pInputParam->m_PicYAddr;
		src_frame.bufCb = pInputParam->m_PicCbAddr;
		src_frame.bufCr = pInputParam->m_PicCrAddr;
		src_frame.endian = pOpenParam->sourceEndian;

		if( src_frame.bufY == 0 )
			return RETCODE_INVALID_PARAM;

		if( src_frame.bufCb == 0 )
			return RETCODE_INVALID_PARAM;

		if (pOpenParam->cbcrInterleave)  //When CbCrInterleaved mode is enable, VPU uses the same Cr buffer address as the Cb buffer address.
			src_frame.bufCr = src_frame.bufCb;

		if( src_frame.bufCr == 0 )
			return RETCODE_INVALID_PARAM;

		src_frame.lumaBitDepth = 8;
		src_frame.chromaBitDepth = 8;
		src_frame.cbcrInterleave = pOpenParam->cbcrInterleave;
		src_frame.myIndex = pVpu->m_iNeedFrameBufCount;
		//src_frame.stride = pVpu->m_iFrameBufStride;
		src_frame.stride = pEncInfo->stride;

		enc_param.sourceFrame = &src_frame;
		enc_param.picStreamBufferAddr = pInputParam->m_BitstreamBufferAddr[PA];
		enc_param.picStreamBufferSize = pInputParam->m_iBitstreamBufferSize;
		enc_param.srcEndFlag = 0;
		enc_param.picStreamBufferAddr += iHeaderWriteSize;

		if( enc_param.picStreamBufferAddr == 0 )
			return RETCODE_INVALID_PARAM;

		if( enc_param.picStreamBufferSize == 0 )
			return RETCODE_INVALID_PARAM;

		wave420l_memset(&enc_param.ctuOptParam, 0, sizeof(HevcCtuOptParam), 0);

		{
			enc_param.ctuOptParam.addrRoiCtuMap = 0;
			enc_param.ctuOptParam.mapEndian = VDI_128BIT_LITTLE_ENDIAN;
			enc_param.ctuOptParam.mapStride = ((pOpenParam->picWidth+63)&~63)>>6;
			enc_param.ctuOptParam.roiDeltaQp = 0;
			enc_param.ctuOptParam.roiEnable = 0;
		}

		//encParam.codeOption.encodeAUD   = encConfig.encAUD;
		//encParam.codeOption.encodeEOS   = 0;

		enc_param.codeOption.implicitHeaderEncode = 1;      // FW will encode header data implicitly when changing the header syntaxes

		// If this value is 0, the encoder encodes a picture as normal.
		// If this value is 1, the encoder ignores sourceFrame and generates a skipped picture. In this case, the reconstructed image at decoder side is a duplication of the previous picture. The skipped picture is encoded as P-type regardless of the GOP size.
		if (pInputParam->m_iSkipPicture) {
			pInputParam->m_iForceIPicture = 0;
			enc_param.forceIPicture = 0;
			enc_param.forcePicTypeEnable = 0;
			enc_param.forcePicType = 0;
			enc_param.skipPicture = 1;  //fix bug 2020-06-19
		} else {
			// If this value is 0, the picture type is determined by VPU according to the various parameters such as encoded frame number and GOP size.
			// If this value is 1, the frame is encoded as an I-picture regardless of the frame number or GOP size, and I-picture period calculation is reset to initial state.
			// This value is ignored if m_iSkipPicture is 1.
			switch (pInputParam->m_iForceIPicture)
			{
			case 1 :  //1 : IDR(Instantaneous Decoder Refresh) picture
				enc_param.forceIPicture = 1;
				enc_param.forcePicTypeEnable = 1;
				enc_param.forcePicType = 3;  //refer to W4_CMD_ENC_PIC_PARAM register
				break;
			case 2 : //2 : I picture
				enc_param.forceIPicture = 1;
				enc_param.forcePicTypeEnable = 1;
				enc_param.forcePicType = 0;  //refer to W4_CMD_ENC_PIC_PARAM register
				break;
			case 3: //3 : CRA(Clean Random Access) picture
				enc_param.forceIPicture = 1;
				enc_param.forcePicTypeEnable = 1;
				enc_param.forcePicType = 4;  //refer to W4_CMD_ENC_PIC_PARAM register
				break;
			case 4: //4 : P picture
				enc_param.forceIPicture = 1;
				enc_param.forcePicTypeEnable = 1;
				enc_param.forcePicType = 1;  //refer to W4_CMD_ENC_PIC_PARAM register
				break;
			default:  //0 :  disable
				enc_param.forceIPicture = 0;
				enc_param.forcePicTypeEnable = 0;
				enc_param.forcePicType = 0;  //refer to W4_CMD_ENC_PIC_PARAM register
				break;
			}
		}

		enc_param.forcePicQpEnable = 0;
		enc_param.forcePicQpI         = 0;
		enc_param.forcePicQpP        = 0;
		enc_param.forcePicQpB        = 0;
		enc_param.forcePicQpSrcOrderEnable = 0;
		enc_param.quantParam = pInputParam->m_iQuantParam;
		if ((0 < enc_param.quantParam) && (enc_param.quantParam <= 51)) {	//(0 ~ 51)
			enc_param.forcePicQpEnable = 1;
			enc_param.forcePicQpI         = enc_param.quantParam;
			enc_param.forcePicQpP        = enc_param.quantParam;
		}

		if (pInputParam->m_iChangeRcParamFlag & 1) {
			static EncChangeParam changeParam;

			wave420l_memset(&changeParam, 0x00, sizeof(EncChangeParam), 0);
			changeParam.changeParaMode    = OPT_COMMON;

			if(pInputParam->m_iChangeRcParamFlag & 0x2) {	//change a bitrate
				changeParam.enable_option |= ENC_RC_TARGET_RATE_CHANGE;
				changeParam.bitRate  = pInputParam->m_iChangeTargetKbps  * 1000;
				//changeParam.enable_option |= ENC_NUM_UNITS_IN_TICK_CHANGE;
			}

			if (pInputParam->m_iChangeRcParamFlag & 0x4) {	//change a framerate
				changeParam.enable_option |= ENC_FRAME_RATE_CHANGE;
				changeParam.frameRate = pInputParam->m_iChangeFrameRate;
			}

			if (pInputParam->m_iChangeRcParamFlag & 0x8) {	//change KeyInterval
				changeParam.enable_option |= ENC_INTRA_PARAM_CHANGE;
				if( pInputParam->m_iChangeKeyInterval > 1 )
					changeParam.intraPeriod = pInputParam->m_iChangeKeyInterval;  //-1
				else
					changeParam.intraPeriod = pInputParam->m_iChangeKeyInterval;
				changeParam.decodingRefreshType = 2;   //0 - Non-IRAP  1 - CRA   2 - IDR
			}

			if (pInputParam->m_iChangeRcParamFlag & (0x08 | 0x04 | 0x02)) {
				wave420l_VPU_EncGiveCommand(handle, ENC_SET_PARA_CHANGE, &changeParam);
			}
		}

		ret = wave420l_VPU_EncStartOneFrame( handle, &enc_param );
		if(ret != RETCODE_SUCCESS)
			return ret;

		{
			int int_reason;

			int_reason = wave420l_VPU_WaitInterrupt(0, 10/*VPU_WAIT_TIME_OUT*/);
			if (int_reason == -1) {
				VLOG(ERR, "Error : encoder timeout happened\n");
				wave420l_VPU_SWReset(0, SW_RESET_SAFETY, handle);
			}

			if (int_reason & (1<<INT_BIT_BIT_BUF_FULL)) {
				VLOG(WARN,"INT_BIT_BIT_BUF_FULL \n");
				VLOG(ERR, "Error : INT_BIT_BIT_BUF_FULL \n");
				return RETCODE_INSUFFICIENT_BITSTREAM_BUF;
			}

			if (int_reason)
			{
				extern void wave420l_VPU_ClearInterrupt(Uint32 coreIdx);
				wave420l_VPU_ClearInterrupt(0);
				//if (int_reason & (1<<INT_WAVE_ENC_PIC)) break;
			}
		}
	}

	//! Output
	{
		EncOutputInfo out_info = {0,};
		EncInfo *pEncInfo = &handle->CodecInfo->encInfo;

		ret = wave420l_VPU_EncGetOutputInfo( handle, &out_info );
		if (ret != RETCODE_SUCCESS) {
			VLOG(ERR, "VPU_EncGetOutputInfo failed Error code is 0x%x \n", ret );
			if (ret == RETCODE_STREAM_BUF_FULL) 	{
				VLOG(ERR, "RETCODE_STREAM_BUF_FULL\n");
				return RETCODE_INSUFFICIENT_BITSTREAM_BUF;
			} else {
				if ( ret == RETCODE_MEMORY_ACCESS_VIOLATION ||
				   ret == RETCODE_CP0_EXCEPTION ||
				   ret == RETCODE_ACCESS_VIOLATION_HW) {
					wave420l_VPU_SWReset(0, SW_RESET_SAFETY, handle);
					return RETCODE_VPU_HEVC_ENC_MEMORY_ACCESS_VIOLATION;
				}
			}

			return RETCODE_FAILURE;
		}

		if (iHeaderWriteSize) {
			out_info.bitstreamBuffer = pInputParam->m_BitstreamBufferAddr[PA];
			out_info.bitstreamSize += iHeaderWriteSize;
		}

		pOutParam->m_BitstreamOutAddr[PA] = out_info.bitstreamBuffer;
		pOutParam->m_BitstreamOutAddr[VA] = pInputParam->m_BitstreamBufferAddr[VA] + (pOutParam->m_BitstreamOutAddr[PA] - pInputParam->m_BitstreamBufferAddr[PA]);

		pOutParam->m_iBitstreamOutSize = out_info.bitstreamSize;
		pOutParam->m_iPicType = out_info.picType;

		pOutParam->m_iAvgQp = out_info.avgCtuQp;

	#ifdef HEVCENC_REPORT_MVINFO
		{
			if (pOutParam->mvInfoBufAddr[VA] != NULL)	//mvReportEnable
			{
				PhysicalAddress binaryAddr;
				void *binaryAddrVA;
				int h, w, mvColSize, totalMvColSize;
				int reconFrameIndex = out_info.reconFrameIndex;

				w = VPU_ALIGN8(pEncInfo->openParam.picWidth);
				h = VPU_ALIGN8(pEncInfo->openParam.picHeight);
				mvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(w, h);
				mvColSize = VPU_ALIGN16(mvColSize);
				totalMvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(w, h);
						// vbBuffer.size      = ((mvColSize*count+4095)&~4095)+4096;   /* 4096 is a margin */

				if (pOutParam->m_iPicType == PIC_TYPE_I)
				{
					pOutParam->mvInfoBufAddr[PA] = NULL;
					pOutParam->mvInfoBufAddr[VA] = NULL;
					pOutParam->mvInfoBufSize = 0;
					pOutParam->mvInfoFormat = 0;
				}
				else if (pOutParam->m_iPicType == PIC_TYPE_P)
				{
					binaryAddr = (pEncInfo->vbMV.phys_addr + totalMvColSize * reconFrameIndex);
					if (wave420l_ioremap != NULL)
					{
						binaryAddrVA = wave420l_ioremap(binaryAddr, totalMvColSize);
						if (binaryAddrVA != NULL)
						{
							wave420l_enc_ConvertMVData((Uint8 *)binaryAddrVA, w, h, (Uint8 *)pOutParam->mvInfoBufAddr[VA], pOutParam->mvInfoFormat);
						}
					}
				}
			}
		}
	#endif

		if (out_info.bitstreamWrapAround)
			return RETCODE_WRAP_AROUND;
	}
	return ret;
}



/*!
 ***********************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC	: main api function of libvpuhevcenc
 * \param
 *		[in]Op			: encoder operation
 * \param
 *		[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: init or input parameter
 * \param
 *		[in]pParam2		: output or info parameter
 * \return
 *		If successful, TCC_VPU_ENC returns 0 or plus. Otherwise, it returns a minus value.
 ***********************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
   	codec_result_t ret = RETCODE_SUCCESS;
	wave420l_t *p_vpu_enc = NULL;

	if (pHandle != NULL)
		p_vpu_enc = (wave420l_t *)(*pHandle);

	if (Op == VPU_HEVC_ENC_CTRL_LOG_STATUS) {
		vpu_hevc_enc_ctrl_log_status_t *pst_ctrl_log = (vpu_hevc_enc_ctrl_log_status_t *)pParam1;
		if (pst_ctrl_log == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			g_pfPrintCb = (void (*)(const char *, ...))pst_ctrl_log->pfLogPrintCb;

			g_bLogVerbose = pst_ctrl_log->stLogLevel.bVerbose;
			g_bLogDebug = pst_ctrl_log->stLogLevel.bDebug;
			g_bLogInfo = pst_ctrl_log->stLogLevel.bInfo;
			g_bLogWarn = pst_ctrl_log->stLogLevel.bWarn;
			g_bLogError = pst_ctrl_log->stLogLevel.bError;
			g_bLogAssert = pst_ctrl_log->stLogLevel.bAssert;
			g_bLogFunc = pst_ctrl_log->stLogLevel.bFunc;
			g_bLogTrace = pst_ctrl_log->stLogLevel.bTrace;

			g_bLogEncodeSuccess = pst_ctrl_log->stLogCondition.bEncodeSuccess;

			V_PRINTF(PRT_PREFIX "VPU_HEVC_ENC_CTRL_LOG_STATUS: V/D/I/W/E/A/F/T/=%d/%d/%d/%d/%d/%d/%d/%d\n",
					 g_bLogVerbose, g_bLogDebug, g_bLogInfo, g_bLogWarn, g_bLogError, g_bLogAssert, g_bLogFunc, g_bLogTrace);
		}
	} else if (Op == VPU_HEVC_ENC_SET_FW_ADDRESS) {
		if (pParam1 == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			vpu_hevc_enc_set_fw_addr_t *pst_fw_addr = (vpu_hevc_enc_set_fw_addr_t *)pParam1;
			ret = set_hevc_e3_fw_addr(pst_fw_addr);
		}
	} else if (Op == VPU_ENC_INIT) {
		hevc_enc_init_t *p_init_param = (hevc_enc_init_t *)pParam1;
		hevc_enc_initial_info_t *p_initial_info_param = (hevc_enc_initial_info_t *)pParam2;

		ret = wave420l_enc_init(&p_vpu_enc, p_init_param, p_initial_info_param);
		if( ret != RETCODE_SUCCESS ) {
			return ret;
		}

		#if defined(ENABLE_FORCE_ESCAPE)
		if( gHevcEncInitFirst_exit > 0 ) {
			#if defined(ENABLE_DISABLE_AXI_TRANSACTION_TELPIC_147)
			wave420l_enc_disable_axi_transaction(0, 0x7FFF);
			#endif
			wave420l_enc_reset( NULL, ( 0 | (1<<16) ) );
			return RETCODE_CODEC_EXIT;
		}
		#endif

		V_PRINTF(VPU_KERN_ERR PRT_PREFIX "handle:%p(%x), RevisionFW: %d, RTL(Date: %d, revision: %d)\n",
				 p_vpu_enc, (codec_handle_t)p_vpu_enc,
				 p_vpu_enc->m_iRevisionFW, p_vpu_enc->m_iDateRTL, p_vpu_enc->m_iRevisionRTL);

		*pHandle = (codec_handle_t)p_vpu_enc;

	} else if (Op == VPU_ENC_REG_FRAME_BUFFER) {
		hevc_enc_buffer_t *p_buffer_param = (hevc_enc_buffer_t *)pParam1;

		//DLOGV(DEBUG_LEVEL_LOW, "VPU_ENC_REG_FRAME_BUFFER");
		if ((p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		if (p_buffer_param->m_FrameBufferStartAddr[PA] == 0) {
			//DLOGV(DEBUG_LEVEL_LOW, "m_FrameBufferStartAddr is NULL ");
			return RETCODE_INVALID_FRAME_BUFFER;
		}

		ret = wave420l_enc_register_frame_buffer(p_vpu_enc, p_buffer_param);
		if( ret != RETCODE_SUCCESS )
			return ret;

	} else if (Op == VPU_ENC_PUT_HEADER) {
		EncHandle handle;
		EncHeaderParam encHeaderParam = {0,};
		hevc_enc_header_t *p_header = (hevc_enc_header_t *)pParam1;
		int headerType, headerTypeMask;
		//EncOpenParam
		//EncParam encParam;

		if (p_vpu_enc == NULL)
			return RETCODE_INVALID_HANDLE;

		handle = p_vpu_enc->m_pCodecInst;
		//DLOGV(DEBUG_LEVEL_LOW, "VPU_ENC_PUT_HEADER");

		//VPU_ENC_PUT_HEADER
		if (p_header->m_HeaderAddr[PA] == 0) {
			VLOG(ERR, "HeaderAddr is NULL  \n");
			encHeaderParam.failReasonCode = 1; //NOTICE: Error return values ​​may be adjusted depending on the version.
			return RETCODE_INVALID_PARAM; //goto ERR_ENC_OPEN;
		}

		//This is a type of header that HOST wants to generate and have values as VPS, SPS, and PPS.
		headerType = p_header->m_iHeaderType;
		headerTypeMask = (HEVC_ENC_CODEOPT_ENC_VPS |
				  HEVC_ENC_CODEOPT_ENC_SPS |
				  HEVC_ENC_CODEOPT_ENC_PPS);
		if (headerType & headerTypeMask) {
			int headerTypeRes = 0;

			if (headerType & CODEOPT_ENC_VPS)
				headerTypeRes |= CODEOPT_ENC_VPS;

			if (headerType & CODEOPT_ENC_SPS)
				headerTypeRes |= CODEOPT_ENC_SPS;

			if (headerType & CODEOPT_ENC_PPS)
				headerTypeRes |= CODEOPT_ENC_PPS;

			headerType = headerTypeRes;
		} else {
			headerType = CODEOPT_ENC_VPS | CODEOPT_ENC_SPS | CODEOPT_ENC_PPS;
		}
		encHeaderParam.headerType = headerType;

		encHeaderParam.buf = p_header->m_HeaderAddr[PA];       //encOP.bitstreamBuffer;        //A physical address pointing the generated stream location
		//encHeaderParam.pBuf = p_header->m_HeaderAddr[VA]; //The virtual address according to buf. This address is needed when a HOST wants to access encoder header bitstream buffer.
		encHeaderParam.size = p_header->m_iHeaderSize;       //encOP.bitstreamBufferSize;   //The size of the generated stream in bytes

		encHeaderParam.zeroPaddingEnable = 0; //It enables header to be padded at the end with zero for byte alignment.
		encHeaderParam.failReasonCode = 0 ; //Not defined yet

		if(encHeaderParam.buf == 0) {
			//DLOGV(DEBUG_LEVEL_LOW, "encHeaderParam.buf = 0x%x   p_header->m_HeaderAddr[PA] =0x%lx  p_header->m_iHeaderSize=%d\n", encHeaderParam.buf, p_header->m_HeaderAddr[PA], p_header->m_iHeaderSize);
			VLOG(ERR, "encHeaderParam.buf = NULL\n");
			encHeaderParam.failReasonCode = 1; //NOTICE: Error return values ​​may be adjusted depending on the version.
			return RETCODE_INVALID_PARAM; //goto ERR_ENC_OPEN;
		}

		if( encHeaderParam.size < 1 ) { //check the min. header size
			//DLOGV(DEBUG_LEVEL_LOW, "encHeaderParam.size = %d \n", encHeaderParam.size);
			VLOG(ERR, "encHeaderParam.size=0\n");
			encHeaderParam.failReasonCode = 2; //NOTICE: Error return values ​​may be adjusted depending on the version.
			return RETCODE_INVALID_PARAM; //goto ERR_ENC_OPEN;
		}

		ret = wave420l_VPU_EncGiveCommand(handle, ENC_PUT_VIDEO_HEADER, &encHeaderParam);
		if (ret != RETCODE_SUCCESS) {
			VLOG(ERR, "wave420l_VPU_EncGiveCommand ( ENC_PUT_VIDEO_HEADER ) for VPS/SPS/PPS failed Error Reason code : 0x%x \n", ret);
			//DLOGV(DEBUG_LEVEL_LOW, "wave420l_VPU_EncGiveCommand ( ENC_PUT_VIDEO_HEADER ) for VPS/SPS/PPS failed Error Reason code : 0x%x \n", ret);
			encHeaderParam.failReasonCode = 8; //NOTICE: Error return values ​​may be adjusted depending on the version.
			return RETCODE_INVALID_PARAM; //goto ERR_ENC_OPEN;
		}

		if (encHeaderParam.size == 0) {
			//DLOGV(DEBUG_LEVEL_LOW, "Error : encHeaderParam.size = %d \n", encHeaderParam.size);
			return RETCODE_INVALID_PARAM;   //goto ERR_ENC_OPEN;}
		} else {
			p_header->m_iHeaderSize = encHeaderParam.size;
		}
	} else if (Op == VPU_ENC_ENCODE) {
		hevc_enc_input_t *p_input_param = (hevc_enc_input_t *)pParam1;
		hevc_enc_output_t *p_output_param = (hevc_enc_output_t *)pParam2;

		//DLOGV(DEBUG_LEVEL_LOW, "VPU_ENC_ENCODE");

		if( (p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL) )
			return RETCODE_INVALID_HANDLE;

		ret = wave420l_enc_encode(p_vpu_enc, p_input_param, p_output_param);
		if( ret != RETCODE_SUCCESS )
			return ret;
	} else if (Op == VPU_ENC_GET_VERSION) {
		char *ppsz_version = (char *)pParam1;
		char *ppsz_build_date = (char *)pParam2;

		if( (ppsz_version == NULL) ||  (ppsz_build_date==NULL) )
			return RETCODE_INVALID_PARAM;

		wave420l_memcpy(ppsz_version, WAVE420L_CODEC_NAME_STR, WAVE420L_CODEC_NAME_LEN, 2);
		wave420l_memcpy(ppsz_build_date, WAVE420L_CODEC_BUILD_DATE_STR, WAVE420L_CODEC_BUILD_DATE_LEN, 2);

		if( p_vpu_enc == NULL )
			return RETCODE_INVALID_HANDLE;

	} else if (Op == VPU_ENC_CLOSE) 	{
		EncHandle handle;

		if( (p_vpu_enc == NULL) || (p_vpu_enc->m_pCodecInst == NULL) ) {
			return RETCODE_INVALID_HANDLE;
		}

		handle = p_vpu_enc->m_pCodecInst;

		if (gHevcEncInitFirst == 0) {
			return RETCODE_NOT_INITIALIZED;
		}

		ret = wave420l_VPU_EncClose(handle);
		#ifdef ADD_SWRESET_VPU_HANGUP   //ADD_VPU_CLOSE_SWRESET
		if (ret == RETCODE_CODEC_EXIT) {
			MACRO_VPU_SWRESET
			wave420l_enc_reset( NULL, ( 0 | (1<<16) ) );
		}
		#endif

		if (ret == RETCODE_FRAME_NOT_COMPLETE) 	{
			//wave420l_VPU_SWReset2();
			wave420l_VPU_SWReset(0, SW_RESET_SAFETY, handle);
			wave420l_VPU_EncClose( handle );
		}

		wave420l_free_vpu_handle( p_vpu_enc );

	} else if (Op == VPU_RESET_SW) {
		//DLOGV(DEBUG_LEVEL_LOW, "VPU_RESET_SW");
		wave420l_enc_reset( p_vpu_enc, ( 1 | (1<<16) ) );
		wave420l_free_vpu_handle( p_vpu_enc );

	} else {
		//DLOGV(DEBUG_LEVEL_LOW,"INVALID COMMAND");
		return RETCODE_INVALID_COMMAND;
	}
	return ret;
}

/*!
 ***********************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC_ESC	: exit api function of libvpuhevcenc
 * \param
 *		[in]Op			: encoder exit operation, 1 (only)
 * \param
 *		[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: NULL
 * \param
 *		[in]pParam2		: NULL
 * \return
 *		TCC_VPU_HEVC_ENC_ESC returns 0.
 ***********************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_ESC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if( Op == 1 )
		gHevcEncInitFirst_exit = 2;

	return 0;
}

/*!
 ***********************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC_EXT	: exit api function of libvpuhevcenc
 * \param
 *		[in]Op			: encoder exit operation, 1 (only)
 * \param
 *		[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: NULL
 * \param
 *		[in]pParam2		: NULL
 * \return
 *		TCC_VPU_HEVC_ENC_EXT returns 0.
 ***********************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_EXT(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if( Op == 1 )
		gHevcEncInitFirst_exit = 1;

	return 0;
}
