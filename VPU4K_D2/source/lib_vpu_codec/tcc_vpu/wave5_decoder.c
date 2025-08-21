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

#include "wave5api.h"
#include "wave5_core.h"
#include "wave5error.h"
#include "TCC_VPU4K_D2.h"
#include "wave5apifunc.h"
#include "wave5.h"
#include "wave5_regdefine.h"
#include "wave5_pre_define.h"
#include "vpu4k_d2_version.h"
#include "vpu4k_d2_log.h"

#define CHECK_HANDLE(A, B)	do{ if( (A == NULL) || (B == NULL) ) return RETCODE_INVALID_HANDLE; }while(0);
								//CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst)
								//if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
								//	return RETCODE_INVALID_HANDLE;

#ifdef CHECK_SEQ_INIT_SUCCESS
#define CHECK_HANDLE_AND_SEQINIT(A, B, C) do{ if( (A == NULL) || (B == NULL) ) return RETCODE_INVALID_HANDLE; if( !C ) return RETCODE_WRONG_CALL_SEQUENCE; }while(0);
#else
#define CHECK_HANDLE_AND_SEQINIT(A, B, C) do{ if( (A == NULL) || (B == NULL) ) return RETCODE_INVALID_HANDLE; }while(0);
#endif

#if defined(ARM_LINUX) || defined(ARM_WINCE)
	extern codec_addr_t gWave5VirtualBitWorkAddr;
	extern PhysicalAddress gWave5PhysicalBitWorkAddr;
	extern codec_addr_t gWave5VirtualRegBaseAddr;
#endif

extern int gWave5MaxInstanceNum;

extern unsigned int g_nFrameBufferAlignSize;

static vpu_4K_D2_dec_initial_info_t gsHevcDecInitialInfo;

char afbc_blkorder[20][3] =
{	{0,4,4},  {0,0,4},  {1,0,0}, {0,0,0},   {0,4,0},
	{0,8,0},  {0,12,0}, {1,4,0}, {0,12,4},  {0,8,4},
	{0,8,8},  {0,12,8}, {1,4,4}, {0,12,12}, {0,8,12},
	{0,4,12}, {0,0,12}, {1,0,4}, {0,0,8},   {0,4,8}
};

unsigned int g_uiBitCodeSize;
// Define global variables for log messages
DEFINE_VPU_LOG_VARS(Vpu4K_D2)
// for profiling...
void (*g_pfPrintCb_Vpu4K_D2_forProfile)(const char *, ...) = NULL; /**< printk callback */
static int gs_iDbgCqCount = 0;

const char *get_pic_type_str(int iPicType) {
	if (iPicType == PIC_TYPE_I) {
		return "I  ";
	} else if (iPicType == PIC_TYPE_P) {
		return "P  ";
	} else if (iPicType == PIC_TYPE_B) {
		return "B  ";
	} else if (iPicType == PIC_TYPE_IDR) {
		return "IDR";
	} else {
		return "U  ";
	}
}

const char *getDecodingStatusString(int status) {
	switch (status) {
	case VPU_DEC_SUCCESS:
		return "SUCCESS";
	case VPU_DEC_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF:
		return "NOT_SUFFICIENT_SPS_PPS_BUFF";
	case VPU_DEC_INFO_NOT_SUFFICIENT_SLICE_BUFF:
		return "NOT_SUFFICIENT_SLICE_BUFF";
	case VPU_DEC_BUF_FULL:
		return "BUF_FULL";
	case VPU_DEC_SUCCESS_FIELD_PICTURE:
		return "SUCCESS_FIELD_PICTURE";
	case VPU_DEC_DETECT_RESOLUTION_CHANGE:
		return "DETECT_RESOLUTION_CHANGE";
	case VPU_DEC_INVALID_INSTANCE:
		return "INVALID_INSTANCE";
	case VPU_DEC_DETECT_DPB_CHANGE:
		return "DETECT_DPB_CHANGE";
	case VPU_DEC_QUEUEING_FAIL:
		return "QUEUEING_FAIL";
	case VPU_DEC_VP9_SUPER_FRAME:
		return "VP9_SUPER_FRAME";
	case VPU_DEC_CQ_EMPTY:
		return "CQ_EMPTY";
	case VPU_DEC_REPORT_NOT_READY:
		return "REPORT_NOT_READY";
	default:
		return "UNKNOWN_STATUS";
	}
}

#define DLOG_VP9 DLOGD

#ifdef PROFILE_DECODING_TIME_INCLUDE_TICK

#define LOG_PROFILE(fmt, ...) g_pfPrintCb_Vpu4K_D2_forProfile(fmt, ##__VA_ARGS__)

// If 'AVOID_STALL_ERROR' is not defined, an error known as 'RCU stall' may occur.
// However, the cause of this error is currently unknown.
#if defined(__ANDROID__)
#define AVOID_STALL_ERROR
#endif

static
void DisplayDecodedInformationForHevc(
    DecHandle      handle,
    Uint32         frameNo,
    DecOutputInfo* decodedInfo
    )
{
	if ((handle == NULL) || (decodedInfo == NULL)) {
		if (frameNo <= 1) {
			LOG_PROFILE("invalid handle\n");
		}
		return;
	} else {
		Int32 logLevel;
		DecQueueStatusInfo queueStatus;

		WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&queueStatus);

		if (frameNo <= 1) {
			LOG_PROFILE("     | |          |      |      |        |        |        |        |        |        |         |    |  FRAME   |   HOST   |   SEEK_S     SEEK_E     SEEK   |   PARSE_S    PARSE_E    PARSE  |   DEC_S      DEC_E      DEC    |\n");
			LOG_PROFILE("  No |T|    POC   | DECO | DISP |DISPFLAG| RD_PTR | WR_PTR | FRM_STA| FRM_END|FRM_SIZE|   WxH   | SEQ|  CYCLE   |   TICK   |   TICK       TICK       CYCLE  |   TICK       TICK       CYCLE  |   TICK       TICK       CYCLE  | IQ\n");
			LOG_PROFILE("-----|-|----------|------|------|--------|--------|--------|--------|--------|--------|---------|----|----------|----------|--------------------------------|--------------------------------|--------------------------------|---\n");
		}

		logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? ERR : TRACE;

		// Print informations
	#ifndef AVOID_STALL_ERROR
		LOG_PROFILE("%5d|%d|%4d(%4d)|%2d(%2d)|%2d(%2d)|%08x|%08x|%08x|%08x|%08x|%8d|%4dx%-4d|%4u|"\
					"%10u|%10u|%10u %10u %10u|%10u %10u %10u|%10u %10u %10u|  %d\n"
					, frameNo, decodedInfo->picType
					, decodedInfo->h265Info.decodedPOC, decodedInfo->h265Info.displayPOC
					, decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled
					, decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled
					, decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr
					, decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd
					, decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart
					, decodedInfo->dispPicWidth, decodedInfo->dispPicHeight
					, decodedInfo->sequenceNo
					, decodedInfo->frameCycle
					, decodedInfo->decHostCmdTick
					, decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle
					, decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle
					, decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->decodeCycle
					, queueStatus.instanceQueueCount);
	#else
		LOG_PROFILE("%5d|%d|%4d(%4d)|%2d(%2d)|%2d(%2d)|"
					, frameNo, decodedInfo->picType
					, decodedInfo->h265Info.decodedPOC, decodedInfo->h265Info.displayPOC
					, decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled
					, decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled);
		LOG_PROFILE("%08x|%08x|%08x|%08x|%08x|%8d|%4dx%-4d|%4u|"
					, decodedInfo->frameDisplayFlag, decodedInfo->rdPtr, decodedInfo->wrPtr
					, decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd
					, decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart
					, decodedInfo->dispPicWidth, decodedInfo->dispPicHeight
					, decodedInfo->sequenceNo);
		LOG_PROFILE("%10u|%10u|%10u %10u %10u|%10u %10u %10u|%10u %10u %10u|  %d\n"
					, decodedInfo->frameCycle
					, decodedInfo->decHostCmdTick
					, decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle
					, decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle
					, decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->decodeCycle
					, queueStatus.instanceQueueCount);
	#endif

		if (logLevel == ERR) {
			LOG_PROFILE("\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
		}

		if (decodedInfo->numOfErrMBs) {
			LOG_PROFILE("\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
		}
	}
}

static
void DisplayDecodedInformationForVP9(DecHandle handle, Uint32 frameNo, DecOutputInfo* decodedInfo)
{
	if ((handle == NULL) || (decodedInfo == NULL)) {
		if (frameNo <= 1) {
			LOG_PROFILE("invalid handle\n");
		}
		return;
	} else {
		Int32 logLevel;
		DecQueueStatusInfo queueStatus;

		WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&queueStatus);

		if (frameNo <= 1) {
			// Print header
			LOG_PROFILE("     | |      |      |        |        |        |        |        |        |         |    |  FRAME   |   HOST   |   SEEK_S     SEEK_E     SEEK   |   PARSE_S    PARSE_E    PARSE  |   DEC_S      DEC_E      DEC    |\n");
			LOG_PROFILE("  No |T| DECO | DISP |DISPFLAG| RD_PTR | WR_PTR | FRM_STA| FRM_END|FRM_SIZE|   WxH   | SEQ|  CYCLE   |   TICK   |   TICK       TICK       CYCLE  |   TICK       TICK       CYCLE  |   TICK       TICK       CYCLE  | IQ\n");
			LOG_PROFILE("-----|-|------|------|--------|--------|--------|--------|--------|--------|---------|----|----------|----------|--------------------------------|--------------------------------|--------------------------------|---\n");
		}

		logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? ERR : TRACE;

	#ifndef AVOID_STALL_ERROR
		LOG_PROFILE("%5d|%d|%2d(%2d)|%2d(%2d)|%08x|%08x|%08x|%08x|%08x|%8d|%4dx%-4d|%4u|"\
					"%10u|%10u|%10u %10u %10u|%10u %10u %10u|%10u %10u %10u|  %d\n"
					, frameNo, decodedInfo->picType
					, decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled
					, decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled
					, decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr
					, decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd
					, decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart
					, decodedInfo->dispPicWidth, decodedInfo->dispPicHeight
					, decodedInfo->sequenceNo
					, decodedInfo->frameCycle
					, decodedInfo->decHostCmdTick
					, decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle
					, decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle
					, decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->decodeCycle
					, queueStatus.instanceQueueCount);
	#else
		LOG_PROFILE("%5d|%d|%2d(%2d)|%2d(%2d)|"
					, frameNo, decodedInfo->picType
					, decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled
					, decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled);
		LOG_PROFILE("%08x|%08x|%08x|%08x|%08x|%8d|%4dx%-4d|%4u|"
					, decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr
					, decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd
					, decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart
					, decodedInfo->dispPicWidth, decodedInfo->dispPicHeight
					, decodedInfo->sequenceNo);
		LOG_PROFILE("%10u|%10u|%10u %10u %10u|%10u %10u %10u|%10u %10u %10u|  %d\n"
					, decodedInfo->frameCycle
					, decodedInfo->decHostCmdTick
					, decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle
					, decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle
					, decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->decodeCycle
					, queueStatus.instanceQueueCount);
	#endif

		if (logLevel == ERR) {
			LOG_PROFILE("\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
		}

		if (decodedInfo->numOfErrMBs) {
			LOG_PROFILE("\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
		}
	}
}

/**
* \brief                   Print out decoded information such like RD_PTR, WR_PTR, PIC_TYPE, ..
* \param   decodedInfo     If this parameter is not NULL then print out decoded informations
*                          otherwise print out header.
*/
static void
    DisplayDecodedInformation(
    DecHandle      handle,
    Uint32         bitstreamFormat,
    Uint32         frameNo,
    DecOutputInfo* decodedInfo
    )
{
	#if TCC_HEVC___DEC_SUPPORT == 1
	if (bitstreamFormat == STD_HEVC)
		DisplayDecodedInformationForHevc(handle, frameNo, decodedInfo);
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (bitstreamFormat == STD_VP9)
		DisplayDecodedInformationForVP9(handle, frameNo, decodedInfo);
	#endif

	return;
}
#endif	//PROFILE_DECODING_TIME_INCLUDE_TICK

static char *wave5_strcpy(char *dest, const char *source) {
	char *p;
	const char *psz_string = source;
	char *psz_dest = dest;

	if (psz_dest == NULL)
		return NULL;

	if (source == NULL)
		return NULL;

	p = dest;
	while (*psz_string != '\0') {
		*psz_dest = *psz_string;
		psz_dest++;
		psz_string++;
	}
	*psz_dest = '\0';
	return p;
}

static int wave5_strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static codec_result_t get_version_4kd2(char *pszVersion, char *pszBuildDate) {
	codec_result_t ret = RETCODE_SUCCESS;

	if (pszVersion == NULL) {
		return RETCODE_INVALID_HANDLE;
	}
	if (pszBuildDate == NULL) {
		return RETCODE_INVALID_HANDLE;
	}

	wave5_strcpy(pszVersion, vpu4k_d2_get_version());
	pszVersion[VPU4K_D2_VERSION_LEN] = '\0';
	wave5_strcpy(pszBuildDate, vpu4k_d2_get_build_date());
	pszBuildDate[VPU4K_D2_BUILD_DATE_LEN] = '\0';
	return ret;
}

static int set_options_4kd2(wave5_t *pstVpu, vpu_4K_D2_dec_set_options_t *pstVpuOptions) {
	codec_result_t ret = RETCODE_SUCCESS;

	if (pstVpuOptions->iReserved[0] == VPU4K_D2_OPT_SET_CQ_COUNT) {
		DLOGE("VPU_4KD2_SET_OPTIONS - Set CQ Count to %d\n", pstVpuOptions->iReserved[1]);
		// change CQ count - must be set before VDEC_INIT
		gs_iDbgCqCount = pstVpuOptions->iReserved[1];
	} else {
		DLOGE("VPU_4KD2_SET_OPTIONS - iReserved[0] = %d\n", pstVpuOptions->iReserved[0]);

		if (pstVpu == NULL)
			return RETCODE_INVALID_PARAM;

		// [option] use bitstream offset
		if (pstVpuOptions->iUseBitstreamOffset > 0) {
			DLOGE("VPU_4KD2_SET_OPTIONS - Use BitstreamOffset \n");
			pstVpu->stSetOptions.iUseBitstreamOffset = pstVpuOptions->iUseBitstreamOffset;
		}

		// [option] measure decoder performance
		if (pstVpuOptions->iMeasureDecPerf == 1) {
			DLOGE("VPU_4KD2_SET_OPTIONS - Measure Decoder Performance \n");
			pstVpu->stSetOptions.iMeasureDecPerf = pstVpuOptions->iMeasureDecPerf;
			if (pstVpuOptions->pfPrintCb != NULL) {
				pstVpu->stSetOptions.pfPrintCb = pstVpuOptions->pfPrintCb;
				g_pfPrintCb_Vpu4K_D2_forProfile = pstVpu->stSetOptions.pfPrintCb;

				DLOGE("Measure decoder performance \n");
				g_pfPrintCb_Vpu4K_D2_forProfile("%s, %s\n", VPU4K_D2_VERSION_STR, VPU4K_D2_BUILD_DATE_STR);
				g_pfPrintCb_Vpu4K_D2_forProfile("RevisionFW: %d, RTL(Date: %d, revision: %d)\n",
												pstVpu->m_iRevisionFW, pstVpu->m_iDateRTL, pstVpu->m_iRevisionRTL);
			} else {
				DLOGE("VPU_4KD2_SET_OPTIONS - Print callback is not set. \n");
			}
		}
	}
	return ret;
}

int WAVE5_dec_reset(wave5_t *pVpu, int iOption)
{
	int coreIdx = 0;
	if ((iOption & 1) && (pVpu != NULL)) {
		Wave5WriteReg(pVpu->m_pCodecInst->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
		Wave5WriteReg(pVpu->m_pCodecInst->coreIdx, W5_VPU_VINT_CLEAR, 1);
	}

	WAVE5_SetPendingInst((Uint32)coreIdx, 0);

	//reset global var.
	wave5_reset_global_var( (iOption>>16) & 0x0FF );

	return RETCODE_SUCCESS;
}

static void
WAVE5_dec_buffer_flag_clear_ex(wave5_t *pstVpu, unsigned int dispFlag)
{
	#ifdef TEST_BUFFERFULL_POLLTIMEOUT
	if (pstVpu->m_pCodecInst->CodecInfo.decInfo.prev_buffer_full == 1)
		pstVpu->m_pCodecInst->CodecInfo.decInfo.fb_cleared = 1;
	else
		pstVpu->m_pCodecInst->CodecInfo.decInfo.fb_cleared = 0;
	#endif

	WAVE5_DecClrDispFlagEx(pstVpu->m_pCodecInst, dispFlag);
}

static void
WAVE5_dec_buffer_flag_clear(wave5_t *pstVpu, int iIndexDisplay)
{
	if (iIndexDisplay >= 0)
		WAVE5_dec_buffer_flag_clear_ex(pstVpu, (1 << iIndexDisplay));
}

static void WAVE5_dec_hold_all_framebuffer(wave5_t *pstVpu)
{
	DecHandle handle = pstVpu->m_pCodecInst;
	unsigned int setFlag = 0;
	int framebufcnt;

	handle->CodecInfo.decInfo.previous_frameDisplayFlag = handle->CodecInfo.decInfo.frameDisplayFlag;	//Wave5ReadReg(handle->coreIdx, handle->CodecInfo.decInfo.frameDisplayFlagRegAddr);

	// hold all display output from framebuffer
	DLOGV("hold all display framebuffer [frameDisplayFlag:0x%08X,prev:0x%08X][clearDisplayIndexes:0x%08X] set flag\n",
		  handle->CodecInfo.decInfo.frameDisplayFlag,
		  handle->CodecInfo.decInfo.previous_frameDisplayFlag,
		  handle->CodecInfo.decInfo.clearDisplayIndexes);
	for (framebufcnt = 0; framebufcnt < pstVpu->m_iNeedFrameBufCount; framebufcnt++) {
		setFlag |= (1 << framebufcnt);
		DLOGV("\tSet Frame Buffer:%d/%d, displayFlag=0x%08x\n", framebufcnt, pstVpu->m_iNeedFrameBufCount, setFlag);
	}
	WAVE5_DecGiveCommand(handle, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
	DLOGV("hold all display framebuffer [frameDisplayFlag:0x%08X,set:0x%08X][clearDisplayIndexes:0x%08X] result\n",
		  handle->CodecInfo.decInfo.frameDisplayFlag,
		  setFlag,
		  handle->CodecInfo.decInfo.clearDisplayIndexes);
}

static void WAVE5_dec_clear_framebuffer(wave5_t *pstVpu)
{
#ifdef TEST_CQ2_CLEAR_INTO_DECODE
	DecHandle handle = pstVpu->m_pCodecInst;

	int idx_cnt;
	unsigned int clrFlag = 0;
	for (idx_cnt = 0; idx_cnt < MAX_GDI_IDX; idx_cnt++) {
		if ((handle->CodecInfo.decInfo.clearDisplayIndexes >> idx_cnt) & 1) {
			clrFlag |= (1 << idx_cnt);
			DLOGV("Clear frame buffer - Buffer Index: %d\n", idx_cnt);
		}
	}

	if (clrFlag > 0) {
		DLOGV("\t[frameDisplayFlag:0x%08X][clearDisplayIndexes:0x%08X] clear:0x%08X\n",
			handle->CodecInfo.decInfo.frameDisplayFlag,
			handle->CodecInfo.decInfo.clearDisplayIndexes,
			clrFlag);

		WAVE5_dec_buffer_flag_clear_ex(pstVpu, handle->CodecInfo.decInfo.clearDisplayIndexes);

		DLOGV("\t[frameDisplayFlag:0x%08X][clearDisplayIndexes:0x%08X] result\n",
			handle->CodecInfo.decInfo.frameDisplayFlag,
			handle->CodecInfo.decInfo.clearDisplayIndexes);
	}
	handle->CodecInfo.decInfo.clearDisplayIndexes = 0;
#endif
}

#define VDI_128BIT_BUS_SYSTEM_ENDIAN 	VDI_128BIT_LITTLE_ENDIAN
static void byte_swap(unsigned char *data, int len)
{
	Uint8 temp;
	Int32 i;

	for (i=0; i<len; i+=2) {
		temp      = data[i];
		data[i]   = data[i+1];
		data[i+1] = temp;
	}
}

static void word_swap(unsigned char *data, int len)
{
	Uint16  temp;
	Uint16* ptr = (Uint16*)data;
	Int32   i, size = len/sizeof(Uint16);

	for (i=0; i<size; i+=2) {
		temp      = ptr[i];
		ptr[i]   = ptr[i+1];
		ptr[i+1] = temp;
	}
}

static void dword_swap(unsigned char *data, int len)
{
	Uint32  temp;
	Uint32* ptr = (Uint32*)data;
	Int32   i, size = len/sizeof(Uint32);

	for (i=0; i<size; i+=2) {
		temp      = ptr[i];
		ptr[i]   = ptr[i+1];
		ptr[i+1] = temp;
	}
}

static void lword_swap(unsigned char *data, int len)
{
	unsigned long long  temp;
	unsigned long long* ptr = (unsigned long long*)data;
	Int32   i, size = len/sizeof(unsigned long long);

	for (i=0; i<size; i+=2) {
		temp      = ptr[i];
		ptr[i]   = ptr[i+1];
		ptr[i+1] = temp;
	}
}

int vdi_convert_endian(unsigned int coreIdx, unsigned int endian)
{
	switch (endian)
	{
		case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
		case VDI_BIG_ENDIAN:          endian = 0x0f; break;
		case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
		case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
	}

    return (endian & 0x0f);
}

int WAVE5_swap_endian(unsigned int coreIdx, unsigned char *data, int len, int endian)
{
	int changes;
	int sys_endian;
	BOOL byteChange, wordChange, dwordChange, lwordChange;

	sys_endian = VDI_128BIT_BUS_SYSTEM_ENDIAN;

	endian     = vdi_convert_endian(coreIdx, endian);
	sys_endian = vdi_convert_endian(coreIdx, sys_endian);

	if (endian == sys_endian)
		return 0;

	changes     = endian ^ sys_endian;
	byteChange  = changes&0x01;
	wordChange  = ((changes&0x02) == 0x02);
	dwordChange = ((changes&0x04) == 0x04);
	lwordChange = ((changes&0x08) == 0x08);

	if (byteChange)  byte_swap(data, len);
	if (wordChange)  word_swap(data, len);
	if (dwordChange) dword_swap(data, len);
	if (lwordChange) lword_swap(data, len);

	return 1;
}

static void
WAVE5_dec_out_info(wave5_t *pstVpu, vpu_4K_D2_dec_output_info_t *pOutInfo, DecOutputInfo *pInpInfo, RetCode ret_info)
{
	DecHandle handle = pstVpu->m_pCodecInst;

	if (pstVpu->iApplyPrevOutInfo == 2 && handle->CodecInfo.decInfo.prev_sequencechanged == 0) {
		//(replace) discard current output info (for Alt Ref.) frame and apply previous output info
		DLOG_VP9("[VP9SUPERFRAME][F:%d][index:%d, nFrames:%d] apply PrevOutInfo, (Cur:%d/%d, dispFlag:%x) => (Idx:%d/%d, dispFlag:%x)\n",
				 handle->CodecInfo.decInfo.frameNumCount,
				 handle->CodecInfo.decInfo.superframe.currentIndex,
				 handle->CodecInfo.decInfo.superframe.nframes,
				 pInpInfo->indexFrameDecoded, pInpInfo->indexFrameDisplay, pInpInfo->frameDisplayFlag,
				 pstVpu->stPrevDecOutputInfo.indexFrameDecoded, pstVpu->stPrevDecOutputInfo.indexFrameDisplay, pstVpu->stPrevDecOutputInfo.frameDisplayFlag);
		wave5_local_mem_cpy(pInpInfo, &pstVpu->stPrevDecOutputInfo, sizeof(*pInpInfo), 0);
		pstVpu->iApplyPrevOutInfo = 0; //completed
	}

	pOutInfo->m_iPicType = pInpInfo->picType;
	pOutInfo->m_iNumOfErrMBs = pInpInfo->numOfErrMBs;
	pOutInfo->m_iDecodingStatus = pInpInfo->decodingSuccess;

	if ((pInpInfo->sequenceChanged == 0) && (pInpInfo->indexFrameDecoded == DECODED_IDX_FLAG_NO_FB)) {
		pOutInfo->m_iDecodingStatus = VPU_DEC_BUF_FULL;
		pstVpu->m_pCodecInst->CodecInfo.decInfo.prev_buffer_full = 1;
	} else {
		pstVpu->m_pCodecInst->CodecInfo.decInfo.prev_buffer_full = 0;
	}

	if (pInpInfo->indexFrameDisplay >= 0)
		pOutInfo->m_iOutputStatus = VPU_DEC_OUTPUT_SUCCESS;
	else
		pOutInfo->m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;

	if (ret_info == RETCODE_REPORT_NOT_READY)
		pOutInfo->m_iDecodingStatus = VPU_DEC_REPORT_NOT_READY;

	pOutInfo->m_iDispOutIdx = pInpInfo->indexFrameDisplay;
	pOutInfo->m_iDecodedIdx = pInpInfo->indexFrameDecoded;

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (pstVpu->m_iBitstreamFormat == STD_HEVC) {
		pOutInfo->m_Reserved[5] = pInpInfo->h265Info.decodedPOC;
		pOutInfo->m_Reserved[6] = pInpInfo->h265Info.displayPOC;
	}
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (pstVpu->m_iBitstreamFormat == STD_VP9) {
		pOutInfo->m_VP9ColorInfo.color_space = pInpInfo->vp9PicInfo.color_space;
		pOutInfo->m_VP9ColorInfo.color_range = pInpInfo->vp9PicInfo.color_range;
	}
	#endif

	pOutInfo->m_Reserved[7] = pInpInfo->refMissingFrameFlag;

	DLOGV("[Dec(res:%d)][%3d|PicType(%s:%d)|(%4dx%-4d)][CQ(%d/%d)][Dec(%d)|Idx(%2d)][Out(%d)|Idx(%2d)][ResChanged(prev:%d/%d)][bufferFull:%d]\n",
		ret_info, handle->CodecInfo.decInfo.frameNumCount,
		get_pic_type_str(pInpInfo->picType), pInpInfo->picType, pInpInfo->decPicWidth, pInpInfo->decPicHeight,
		handle->CodecInfo.decInfo.instanceQueueCount, handle->CodecInfo.decInfo.totalQueueCount,
		pOutInfo->m_iDecodingStatus, pOutInfo->m_iDecodedIdx,
		pOutInfo->m_iOutputStatus, pOutInfo->m_iDispOutIdx,
		handle->CodecInfo.decInfo.prev_sequencechanged, pInpInfo->sequenceChanged,
		handle->CodecInfo.decInfo.prev_buffer_full);

	#ifndef TEST_OUTPUTINFO
	#if TCC_VP9____DEC_SUPPORT == 1
	if (pstVpu->m_iBitstreamFormat == STD_VP9) {
		if (handle->CodecInfo.decInfo.superframe.nframes > 0) {
			if (pstVpu->m_pCodecInst->CodecInfo.decInfo.openParam.CQ_Depth > 1) {
				pstVpu->iApplyPrevOutInfo = 1; // hold and release

				DLOG_VP9("[VP9SUPERFRAME][F:%d][index:%d,nFrames:%d] hold and release for CQ2, (Idx:%d/%d)=>(-1,-3)\n",
					handle->CodecInfo.decInfo.frameNumCount,
					handle->CodecInfo.decInfo.superframe.currentIndex,
					handle->CodecInfo.decInfo.superframe.nframes,
					pOutInfo->m_iDecodedIdx, pOutInfo->m_iDispOutIdx);

				pOutInfo->m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
				pOutInfo->m_iDecodingStatus = VPU_DEC_VP9_SUPER_FRAME;
				pOutInfo->m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
				pOutInfo->m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;
			} else {
				DLOG_VP9("[VP9SUPERFRAME][F:%d][index:%d,nFrames:%d] retry for superframe, (DecIdx:%d => -1|OutIdx:%d)\n",
					handle->CodecInfo.decInfo.frameNumCount,
					handle->CodecInfo.decInfo.superframe.currentIndex,
					handle->CodecInfo.decInfo.superframe.nframes,
					pOutInfo->m_iDecodedIdx, pOutInfo->m_iDispOutIdx);

				pOutInfo->m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
				pOutInfo->m_iDecodingStatus = VPU_DEC_VP9_SUPER_FRAME;
				#ifdef TEST_SUPERFRAME_DECIDX_OUT
				if (pOutInfo->m_iDispOutIdx >= 0) {
					pOutInfo->m_iDecodedIdx = pOutInfo->m_iDispOutIdx;
					DLOG_VP9("[VP9SUPERFRAME] (DecIdx:-1 => %d)\n", pOutInfo->m_iDecodedIdx);
				}
				#endif
			}
		}
	}
	#endif
	#endif

	pOutInfo->m_iDecodedWidth = pInpInfo->decPicWidth;
	pOutInfo->m_iDecodedHeight = pInpInfo->decPicHeight;

	pOutInfo->m_iDisplayWidth = pInpInfo->dispPicWidth;
	pOutInfo->m_iDisplayHeight = pInpInfo->dispPicHeight;

	pOutInfo->m_DecodedCropInfo.m_iCropTop = pInpInfo->rcDecoded.top;
	pOutInfo->m_DecodedCropInfo.m_iCropLeft = pInpInfo->rcDecoded.left;
	pOutInfo->m_DecodedCropInfo.m_iCropRight = pInpInfo->rcDecoded.right;
	pOutInfo->m_DecodedCropInfo.m_iCropBottom = pInpInfo->rcDecoded.bottom;

	pOutInfo->m_DisplayCropInfo.m_iCropTop = pInpInfo->rcDisplay.top;
	pOutInfo->m_DisplayCropInfo.m_iCropLeft = pInpInfo->rcDisplay.left;
	pOutInfo->m_DisplayCropInfo.m_iCropRight = pInpInfo->rcDisplay.right;
	pOutInfo->m_DisplayCropInfo.m_iCropBottom = pInpInfo->rcDisplay.bottom;
	pOutInfo->m_iAspectRateInfo = pInpInfo->aspectRateInfo;

	if (pstVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {
		if (ret_info == VPU_DEC_SUCCESS) {
			if (pInpInfo->indexFrameDecoded >= 0 || pInpInfo->indexFrameDecoded == DECODED_IDX_FLAG_SKIP_OR_EOS) {
				if (pstVpu->m_pCodecInst->CodecInfo.decInfo.superframe.doneIndex > 0) {
					pstVpu->m_pCodecInst->CodecInfo.decInfo.superframe.doneIndex--;
					if (pstVpu->m_pCodecInst->CodecInfo.decInfo.superframe.doneIndex == 0) {
						if (wave5_memset) {
							wave5_memset(&pstVpu->m_pCodecInst->CodecInfo.decInfo.superframe, 0x00, sizeof(VP9Superframe), 0);
						}
					}
				}
			}
		}
	}

	pstVpu->m_pCodecInst->CodecInfo.decInfo.sequencechange_detected = 0;
	if (pInpInfo->sequenceChanged) {
		unsigned int seqChangeInfo = pInpInfo->sequenceChanged;

		if (seqChangeInfo & SEQ_CHANGE_ENABLE_DPB_COUNT)
			pOutInfo->m_iDecodingStatus = VPU_DEC_DETECT_DPB_CHANGE;

		if (seqChangeInfo & SEQ_CHANGE_ENABLE_SIZE)
			pOutInfo->m_iDecodingStatus = VPU_DEC_DETECT_RESOLUTION_CHANGE;

		DLOGI("[seq.Changed:%d] %dx%-d, (%d) %s => %s\n",
			handle->CodecInfo.decInfo.frameNumCount,
			handle->CodecInfo.decInfo.newSeqInfo.picWidth, handle->CodecInfo.decInfo.newSeqInfo.picHeight,
			pOutInfo->m_iDecodingStatus,
			(pOutInfo->m_iDecodingStatus == VPU_DEC_DETECT_DPB_CHANGE) ? "DPB.Changed" :
			(pOutInfo->m_iDecodingStatus == VPU_DEC_DETECT_RESOLUTION_CHANGE) ? "Res.Changed" : "\n",
			(handle->CodecInfo.decInfo.openParam.FullSizeFB) ? "Buffer Full" : "Re-init");

		pstVpu->m_pCodecInst->CodecInfo.decInfo.sequencechange_detected = 1;

		pOutInfo->m_iDecodedWidth = handle->CodecInfo.decInfo.newSeqInfo.picWidth;
		pOutInfo->m_iDecodedHeight = handle->CodecInfo.decInfo.newSeqInfo.picHeight;

		if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
			pOutInfo->m_iDecodingStatus = VPU_DEC_BUF_FULL;
			handle->CodecInfo.decInfo.prev_sequencechanged = 1;
		}

		#ifndef DISABLE_RC_DPB_FLUSH
		Wave5VpuDecFlush(pstVpu->m_pCodecInst, NULL, 0);
		#endif
	} else {
		handle->CodecInfo.decInfo.prev_sequencechanged = 0;
	}

	if (pInpInfo->rcDecoded.right)
		pOutInfo->m_DecodedCropInfo.m_iCropRight = (pInpInfo->decPicWidth - pInpInfo->rcDecoded.right);

	if (pInpInfo->rcDecoded.bottom)
		pOutInfo->m_DecodedCropInfo.m_iCropBottom = (pInpInfo->decPicHeight - pInpInfo->rcDecoded.bottom);

	if (pInpInfo->rcDisplay.right)
		pOutInfo->m_DisplayCropInfo.m_iCropRight = (pInpInfo->dispPicWidth - pInpInfo->rcDisplay.right);

	if (pInpInfo->rcDisplay.bottom)
		pOutInfo->m_DisplayCropInfo.m_iCropBottom = (pInpInfo->dispPicHeight - pInpInfo->rcDisplay.bottom);

	pOutInfo->m_iConsumedBytes = (int)pInpInfo->consumedByte;

	if (pstVpu->iApplyPrevOutInfo == 1) {
		DLOG_VP9("[VP9SUPERFRAME][F:%d][index:%d,nFrames:%d] backup OutInfo Dec:%d,Out:%d\n",
				 handle->CodecInfo.decInfo.frameNumCount,
				 handle->CodecInfo.decInfo.superframe.currentIndex,
				 handle->CodecInfo.decInfo.superframe.nframes,
				 pInpInfo->indexFrameDecoded, pInpInfo->indexFrameDisplay);
		wave5_local_mem_cpy(&pstVpu->stPrevDecOutputInfo, pInpInfo, sizeof(*pInpInfo), 0);
		pstVpu->iApplyPrevOutInfo = 2; // store output info for current decoded frame
	}
}

static void
GetSeqInitUserData( wave5_t *pVpu, vpu_4K_D2_dec_initial_info_t *pOutParam, DecInitialInfo info )
{
	DecHandle handle = pVpu->m_pCodecInst;
	user_data_entry_t* pEntry;
	Uint8 *pBase;
	Int32 coreIdx = pVpu->m_pCodecInst->coreIdx;
	Uint16 userdataNum = 0;
	//Uint32 userdataSize = 0;
	Uint16 userdataTotalSize = 0;
	Uint32 t35_pre = 0;
	Uint32 t35_suf = 0;
	Uint32 t35_pre_1 = 0;
	Uint32 t35_pre_2 = 0;
	Uint32 t35_suf_1 = 0;
	Uint32 t35_suf_2 = 0;

	pBase = (Uint8 *)handle->CodecInfo.decInfo.vbUserData.virt_addr;
	pEntry = (user_data_entry_t*)pBase;

	pOutParam->m_uiUserData = 0;

	WAVE5_swap_endian(coreIdx, pBase, 8*18, VDI_LITTLE_ENDIAN);

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_MASTERING_COLOR_VOL)) {
		h265_mastering_display_colour_volume_t *mastering;
		int i;

		mastering = (h265_mastering_display_colour_volume_t*)(pBase + pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].offset, pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.min_display_mastering_luminance = mastering->min_display_mastering_luminance;

		for (i = 0 ; i < 3 ; i++)
			pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.display_primaries_x[i] = mastering->display_primaries_x[i];

		for (i = 0 ; i < 3 ; i++)
			pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.display_primaries_y[i] = mastering->display_primaries_y[i];

		pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.white_point_x = mastering->white_point_x;
		pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.white_point_y = mastering->white_point_y;
		pOutParam->m_UserDataInfo.m_MasteringDisplayColorVolume.max_display_mastering_luminance = mastering->max_display_mastering_luminance;
	}

	if(info.userDataHeader&(1<<H265_USERDATA_FLAG_VUI)) {
		h265_vui_param_t* vui;
		int i, j;

		vui = (h265_vui_param_t*)(pBase + pEntry[H265_USERDATA_FLAG_VUI].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_VUI].offset, pEntry[H265_USERDATA_FLAG_VUI].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;

		pOutParam->m_UserDataInfo.m_VuiParam.aspect_ratio_info_present_flag = vui->aspect_ratio_info_present_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.aspect_ratio_info_present_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.aspect_ratio_idc = vui->aspect_ratio_idc;
			if ( pOutParam->m_UserDataInfo.m_VuiParam.aspect_ratio_idc == 255 ) {
				pOutParam->m_UserDataInfo.m_VuiParam.sar_width = vui->sar_width;
				pOutParam->m_UserDataInfo.m_VuiParam.sar_height = vui->sar_height;
			}
		}

		pOutParam->m_UserDataInfo.m_VuiParam.overscan_info_present_flag = vui->overscan_info_present_flag;
		pOutParam->m_UserDataInfo.m_VuiParam.overscan_appropriate_flag = vui->overscan_appropriate_flag;

		pOutParam->m_UserDataInfo.m_VuiParam.video_signal_type_present_flag = vui->video_signal_type_present_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.video_signal_type_present_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.video_format = vui->video_format;
			pOutParam->m_UserDataInfo.m_VuiParam.video_full_range_flag = vui->video_full_range_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.colour_description_present_flag = vui->colour_description_present_flag;
			if ( pOutParam->m_UserDataInfo.m_VuiParam.colour_description_present_flag ) {
				pOutParam->m_UserDataInfo.m_VuiParam.colour_primaries = vui->colour_primaries;
				pOutParam->m_UserDataInfo.m_VuiParam.transfer_characteristics = vui->transfer_characteristics;
				pOutParam->m_UserDataInfo.m_VuiParam.matrix_coefficients = vui->matrix_coefficients;
			}
		}

		pOutParam->m_UserDataInfo.m_VuiParam.chroma_loc_info_present_flag = vui->chroma_loc_info_present_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.chroma_loc_info_present_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.chroma_sample_loc_type_top_field = vui->chroma_sample_loc_type_top_field;
			pOutParam->m_UserDataInfo.m_VuiParam.chroma_sample_loc_type_bottom_field = vui->chroma_sample_loc_type_bottom_field;
		}

		pOutParam->m_UserDataInfo.m_VuiParam.neutral_chroma_indication_flag = vui->neutral_chroma_indication_flag;
		pOutParam->m_UserDataInfo.m_VuiParam.field_seq_flag = vui->field_seq_flag;
		pOutParam->m_UserDataInfo.m_VuiParam.frame_field_info_present_flag = vui->frame_field_info_present_flag;

		pOutParam->m_UserDataInfo.m_VuiParam.default_display_window_flag = vui->default_display_window_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.default_display_window_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.def_disp_win.sBottom = vui->def_disp_win.bottom;
			pOutParam->m_UserDataInfo.m_VuiParam.def_disp_win.sLeft = vui->def_disp_win.left;
			pOutParam->m_UserDataInfo.m_VuiParam.def_disp_win.sRight = vui->def_disp_win.right;
			pOutParam->m_UserDataInfo.m_VuiParam.def_disp_win.sTop = vui->def_disp_win.top;
		}

		pOutParam->m_UserDataInfo.m_VuiParam.vui_timing_info_present_flag = vui->vui_timing_info_present_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.vui_timing_info_present_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.vui_num_units_in_tick = vui->vui_num_units_in_tick;
			pOutParam->m_UserDataInfo.m_VuiParam.vui_poc_proportional_to_timing_flag = vui->vui_poc_proportional_to_timing_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.vui_num_ticks_poc_diff_one_minus1 = vui->vui_num_ticks_poc_diff_one_minus1;
			pOutParam->m_UserDataInfo.m_VuiParam.vui_time_scale = vui->vui_time_scale;
		}

		pOutParam->m_UserDataInfo.m_VuiParam.vui_hrd_parameters_present_flag = vui->vui_hrd_parameters_present_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.vui_hrd_parameters_present_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_hrd_param_present_flag = vui->hrd_param.nal_hrd_param_present_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_hrd_param_present_flag = vui->hrd_param.vcl_hrd_param_present_flag;
			if (pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_hrd_param_present_flag || pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_hrd_param_present_flag) {
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_hrd_params_present_flag = vui->hrd_param.sub_pic_hrd_params_present_flag;
				if (pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_hrd_params_present_flag) {
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.tick_divisor_minus2 = vui->hrd_param.tick_divisor_minus2;
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.du_cpb_removal_delay_inc_length_minus1 = vui->hrd_param.du_cpb_removal_delay_inc_length_minus1;
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_cpb_params_in_pic_timing_sei_flag = vui->hrd_param.sub_pic_cpb_params_in_pic_timing_sei_flag;
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.dpb_output_delay_du_length_minus1 = vui->hrd_param.dpb_output_delay_du_length_minus1;
				}
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.bit_rate_scale = vui->hrd_param.bit_rate_scale;
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.cpb_size_scale = vui->hrd_param.cpb_size_scale;
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.initial_cpb_removal_delay_length_minus1 = vui->hrd_param.initial_cpb_removal_delay_length_minus1;
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.cpb_removal_delay_length_minus1 = vui->hrd_param.cpb_removal_delay_length_minus1;
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.dpb_output_delay_length_minus1 = vui->hrd_param.dpb_output_delay_length_minus1;
			}

			for (i = 0 ; i < H265_MAX_NUM_SUB_LAYER ; i++) {
				pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_gen_flag[i] = vui->hrd_param.fixed_pic_rate_gen_flag[i];
				if (!pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_gen_flag[i])
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_within_cvs_flag[i] = vui->hrd_param.fixed_pic_rate_within_cvs_flag[i];
				if (vui->hrd_param.fixed_pic_rate_within_cvs_flag[i])
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.elemental_duration_in_tc_minus1[i] = vui->hrd_param.elemental_duration_in_tc_minus1[i];
				else
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.low_delay_hrd_flag[i] = vui->hrd_param.low_delay_hrd_flag[i];
				if (!vui->hrd_param.low_delay_hrd_flag[i])
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.cpb_cnt_minus1[i] = vui->hrd_param.cpb_cnt_minus1[i];

				if (vui->hrd_param.nal_hrd_param_present_flag) {
					for (j = 0 ; j < H265_MAX_CPB_CNT ; j++) {
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_bit_rate_value_minus1[i][j] = vui->hrd_param.nal_bit_rate_value_minus1[i][j];
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_cpb_size_value_minus1[i][j] = vui->hrd_param.nal_cpb_size_value_minus1[i][j];
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_cbr_flag[i][j] = vui->hrd_param.nal_cbr_flag[i][j];
					}
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_cpb_size_du_value_minus1[i] = vui->hrd_param.nal_cpb_size_du_value_minus1[i];
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.nal_bit_rate_du_value_minus1[i] = vui->hrd_param.nal_bit_rate_du_value_minus1[i];
				}

				if (vui->hrd_param.vcl_hrd_param_present_flag) {
					for (j = 0 ; j < H265_MAX_CPB_CNT ; j++) {
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_bit_rate_value_minus1[i][j] = vui->hrd_param.vcl_bit_rate_value_minus1[i][j];
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_cpb_size_value_minus1[i][j] = vui->hrd_param.vcl_cpb_size_value_minus1[i][j];
						pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_cbr_flag[i][j] = vui->hrd_param.vcl_cbr_flag[i][j];
					}
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_cpb_size_du_value_minus1[i] = vui->hrd_param.vcl_cpb_size_du_value_minus1[i];
					pOutParam->m_UserDataInfo.m_VuiParam.hrd_param.vcl_bit_rate_du_value_minus1[i] = vui->hrd_param.vcl_bit_rate_du_value_minus1[i];
				}


			}
		}

		pOutParam->m_UserDataInfo.m_VuiParam.bitstream_restriction_flag = vui->bitstream_restriction_flag;
		if (pOutParam->m_UserDataInfo.m_VuiParam.bitstream_restriction_flag) {
			pOutParam->m_UserDataInfo.m_VuiParam.tiles_fixed_structure_flag = vui->tiles_fixed_structure_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.motion_vectors_over_pic_boundaries_flag = vui->motion_vectors_over_pic_boundaries_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.restricted_ref_pic_lists_flag = vui->restricted_ref_pic_lists_flag;
			pOutParam->m_UserDataInfo.m_VuiParam.min_spatial_segmentation_idc = vui->min_spatial_segmentation_idc;
			pOutParam->m_UserDataInfo.m_VuiParam.max_bytes_per_pic_denom = vui->max_bytes_per_pic_denom;
			pOutParam->m_UserDataInfo.m_VuiParam.max_bits_per_mincu_denom = vui->max_bits_per_mincu_denom;
			pOutParam->m_UserDataInfo.m_VuiParam.log2_max_mv_length_horizontal = vui->log2_max_mv_length_horizontal;
			pOutParam->m_UserDataInfo.m_VuiParam.log2_max_mv_length_vertical = vui->log2_max_mv_length_vertical;
		}
	}

	if (info.userDataHeader & (1<<H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO)) {
		h265_content_light_level_info_t* content_light_level;
		content_light_level = (h265_content_light_level_info_t*)(pBase + pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].offset, pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_ContentLightLevelInfo.max_content_light_level = content_light_level->max_content_light_level;
		pOutParam->m_UserDataInfo.m_ContentLightLevelInfo.max_pic_average_light_level = content_light_level->max_pic_average_light_level;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS)) {
		h265_alternative_transfer_characteristics_info_t* alternative_transfer_characteristics;
		alternative_transfer_characteristics = (h265_alternative_transfer_characteristics_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].offset, pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_AlternativeTransferCharacteristicsInfo.preferred_transfer_characteristics = alternative_transfer_characteristics->preferred_transfer_characteristics;
	}

	#ifndef USER_DATA_SKIP
	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT)) {
		h265_chroma_resampling_filter_hint_t* c_resampleing_filter_hint;
		int i,j;

		c_resampleing_filter_hint = (h265_chroma_resampling_filter_hint_t*)(pBase + pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].offset, pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.ver_chroma_filter_idc = c_resampleing_filter_hint->ver_chroma_filter_idc;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.hor_chroma_filter_idc = c_resampleing_filter_hint->hor_chroma_filter_idc;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.ver_filtering_field_processing_flag = c_resampleing_filter_hint->ver_filtering_field_processing_flag;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.target_format_idc = c_resampleing_filter_hint->target_format_idc;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.num_vertical_filters = c_resampleing_filter_hint->num_vertical_filters;
		pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.num_horizontal_filters = c_resampleing_filter_hint->num_horizontal_filters;
		for (i = 0 ; i < H265_MAX_NUM_VERTICAL_FILTERS ; i++)
			pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.ver_tap_length_minus1[i] = c_resampleing_filter_hint->ver_tap_length_minus1[i];

		for (i = 0 ; i < H265_MAX_NUM_HORIZONTAL_FILTERS ; i++)
			pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.hor_tap_length_minus1[i] = c_resampleing_filter_hint->hor_tap_length_minus1[i];

		for (i = 0 ; i < H265_MAX_NUM_VERTICAL_FILTERS ; i++) {
			for (j = 0 ; j < H265_MAX_TAP_LENGTH ; j++)
				pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.ver_filter_coeff[i][j] = c_resampleing_filter_hint->ver_filter_coeff[i][j];
		}

		for (i = 0 ; i < H265_MAX_NUM_HORIZONTAL_FILTERS ; i++) {
			for (j = 0 ; j < H265_MAX_TAP_LENGTH ; j++)
				pOutParam->m_UserDataInfo.m_ChromaResamplingFilterHint.hor_filter_coeff[i][j] = c_resampleing_filter_hint->hor_filter_coeff[i][j];
		}
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_KNEE_FUNCTION_INFO)) {
		h265_knee_function_info_t* knee_function;
		int i;

		knee_function = (h265_knee_function_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].offset, pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.knee_function_id = knee_function->knee_function_id;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.knee_function_cancel_flag = knee_function->knee_function_cancel_flag;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.knee_function_persistence_flag = knee_function->knee_function_persistence_flag;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.input_disp_luminance = knee_function->input_disp_luminance;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.input_d_range = knee_function->input_d_range;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.output_d_range = knee_function->output_d_range;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.output_disp_luminance = knee_function->output_disp_luminance;
		pOutParam->m_UserDataInfo.m_KneeFunctionInfo.num_knee_points_minus1 = knee_function->num_knee_points_minus1;
		for (i = 0 ; i < H265_MAX_NUM_KNEE_POINT ; i++)
			pOutParam->m_UserDataInfo.m_KneeFunctionInfo.input_knee_point[i] = knee_function->input_knee_point[i];

		for (i = 0 ; i < H265_MAX_NUM_KNEE_POINT ; i++)
			pOutParam->m_UserDataInfo.m_KneeFunctionInfo.output_knee_point[i] = knee_function->output_knee_point[i];
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_TONE_MAPPING_INFO)) {
		h265_tone_mapping_info_t* tone_mapping;
		int i;

		tone_mapping = (h265_tone_mapping_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].offset, pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.tone_map_id = tone_mapping->tone_map_id;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.tone_map_cancel_flag = tone_mapping->tone_map_cancel_flag;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.tone_map_persistence_flag = tone_mapping->tone_map_persistence_flag;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.coded_data_bit_depth = tone_mapping->coded_data_bit_depth;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.target_bit_depth = tone_mapping->target_bit_depth;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.tone_map_model_id = tone_mapping->tone_map_model_id;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.min_value = tone_mapping->min_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.max_value = tone_mapping->max_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.sigmoid_midpoint = tone_mapping->sigmoid_midpoint;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.sigmoid_width = tone_mapping->sigmoid_width;
		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++)
			pOutParam->m_UserDataInfo.m_ToneMappingInfo.start_of_coded_interval[i] = tone_mapping->start_of_coded_interval[i];

		pOutParam->m_UserDataInfo.m_ToneMappingInfo.num_pivots = tone_mapping->num_pivots;

		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++)
			pOutParam->m_UserDataInfo.m_ToneMappingInfo.coded_pivot_value[i] = tone_mapping->coded_pivot_value[i];

		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++)
			pOutParam->m_UserDataInfo.m_ToneMappingInfo.target_pivot_value[i] = tone_mapping->target_pivot_value[i];

		pOutParam->m_UserDataInfo.m_ToneMappingInfo.camera_iso_speed_idc = tone_mapping->camera_iso_speed_idc;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.camera_iso_speed_value = tone_mapping->camera_iso_speed_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.exposure_index_idc = tone_mapping->exposure_index_idc;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.exposure_index_value = tone_mapping->exposure_index_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_sign_flag = tone_mapping->exposure_compensation_value_sign_flag;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_numerator = tone_mapping->exposure_compensation_value_numerator;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_denom_idc = tone_mapping->exposure_compensation_value_denom_idc;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.ref_screen_luminance_white = tone_mapping->ref_screen_luminance_white;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.extended_range_white_level = tone_mapping->extended_range_white_level;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.nominal_black_level_code_value = tone_mapping->nominal_black_level_code_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.nominal_white_level_code_value = tone_mapping->nominal_white_level_code_value;
		pOutParam->m_UserDataInfo.m_ToneMappingInfo.extended_white_level_code_value = tone_mapping->extended_white_level_code_value;
	}
	#endif

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_PIC_TIMING)) {
		h265_sei_pic_timing_t* pic_timing;
		pic_timing = (h265_sei_pic_timing_t*)(pBase + pEntry[H265_USERDATA_FLAG_PIC_TIMING].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_PIC_TIMING].offset, pEntry[H265_USERDATA_FLAG_PIC_TIMING].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;

		pOutParam->m_UserDataInfo.m_SeiPicTiming.status = pic_timing->status;
		pOutParam->m_UserDataInfo.m_SeiPicTiming.pic_struct = pic_timing->pic_struct;
		pOutParam->m_UserDataInfo.m_SeiPicTiming.source_scan_type = pic_timing->source_scan_type;
		pOutParam->m_UserDataInfo.m_SeiPicTiming.duplicate_flag = pic_timing->duplicate_flag;
	}

	#ifndef USER_DATA_SKIP
	if (info.userDataHeader & (1<<H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO)) {
		h265_film_grain_characteristics_t* film_grain_characteristics;
		int i, j, k;
		film_grain_characteristics = (h265_film_grain_characteristics_t*)(pBase + pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].offset, pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_characteristics_cancel_flag = film_grain_characteristics->film_grain_characteristics_cancel_flag;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_model_id = film_grain_characteristics->film_grain_model_id;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.separate_colour_description_present_flag = film_grain_characteristics->separate_colour_description_present_flag;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_bit_depth_luma_minus8 = film_grain_characteristics->film_grain_bit_depth_luma_minus8;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_bit_depth_chroma_minus8 = film_grain_characteristics->film_grain_bit_depth_chroma_minus8;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_full_range_flag = film_grain_characteristics->film_grain_full_range_flag;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_colour_primaries = film_grain_characteristics->film_grain_colour_primaries;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_transfer_characteristics = film_grain_characteristics->film_grain_transfer_characteristics;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_matrix_coeffs = film_grain_characteristics->film_grain_matrix_coeffs;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.blending_mode_id = film_grain_characteristics->blending_mode_id;
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.log2_scale_factor = film_grain_characteristics->log2_scale_factor;
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++)
			pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.comp_model_present_flag[i] = film_grain_characteristics->comp_model_present_flag[i];
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++)
			pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.num_intensity_intervals_minus1[i] = film_grain_characteristics->num_intensity_intervals_minus1[i];
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++)
			pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.num_model_values_minus1[i] = film_grain_characteristics->num_model_values_minus1[i];
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++)
				pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.intensity_interval_lower_bound[i][j] = film_grain_characteristics->intensity_interval_lower_bound[i][j];
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++)
				pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.intensity_interval_upper_bound[i][j] = film_grain_characteristics->intensity_interval_upper_bound[i][j];
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++) {
				for (k = 0 ; k < H265_MAX_NUM_MODEL_VALUES ; k++)
					pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.comp_model_value[i][j][k] = film_grain_characteristics->comp_model_value[i][j][k];
			}
		}
		pOutParam->m_UserDataInfo.m_FilmGrainCharInfo.film_grain_characteristics_persistence_flag = film_grain_characteristics->film_grain_characteristics_persistence_flag;
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.userDataHeader & (1<<H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO)) {
		h265_colour_remapping_info_t* colour_remapping;
		int i, j, k;
		colour_remapping = (h265_colour_remapping_info_t*)(pBase + pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].offset);

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].offset, pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_cancel_flag = colour_remapping->colour_remap_cancel_flag;

		if (!pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_cancel_flag) {
			pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_persistence_flag = colour_remapping->colour_remap_persistence_flag;
			pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_video_signal_info_present_flag = colour_remapping->colour_remap_video_signal_info_present_flag;

			if (pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_video_signal_info_present_flag) {
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_full_range_flag = colour_remapping->colour_remap_full_range_flag;
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_primaries = colour_remapping->colour_remap_primaries;
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_transfer_function = colour_remapping->colour_remap_transfer_function;
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_coefficients = colour_remapping->colour_remap_matrix_coefficients;
			}
			pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_input_bit_depth = colour_remapping->colour_remap_input_bit_depth;
			pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_bit_depth = colour_remapping->colour_remap_bit_depth;

			for(i = 0 ; i < H265_MAX_LUT_NUM_VAL ; i++) {
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i] = colour_remapping->pre_lut_num_val_minus1[i];
				if (pOutParam->m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i] > 0)
				{
					for( j = 0; j <= pOutParam->m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i]; j++ )
					{
						pOutParam->m_UserDataInfo.m_ColourRemappingInfo.pre_lut_coded_value[i][j] = colour_remapping->pre_lut_coded_value[i][j];
						pOutParam->m_UserDataInfo.m_ColourRemappingInfo.pre_lut_target_value[i][j] = colour_remapping->pre_lut_target_value[i][j];
					}
				}
			}

			pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_present_flag = colour_remapping->colour_remap_matrix_present_flag;
			if (pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_present_flag) {
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.log2_matrix_denom = colour_remapping->log2_matrix_denom;
				for( i = 0; i < H265_MAX_COLOUR_REMAP_COEFFS; i++ )
					for( j = 0; j < H265_MAX_COLOUR_REMAP_COEFFS; j++ )
						pOutParam->m_UserDataInfo.m_ColourRemappingInfo.colour_remap_coeffs[i][j] = colour_remapping->colour_remap_coeffs[i][j];
			}

			for(i = 0; i < H265_MAX_LUT_NUM_VAL; i++) {
				pOutParam->m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i] = colour_remapping->post_lut_num_val_minus1[i];
				if (pOutParam->m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i] > 0) {
					for(j = 0; j <= pOutParam->m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i]; j++) {
						pOutParam->m_UserDataInfo.m_ColourRemappingInfo.post_lut_coded_value[i][j] = colour_remapping->post_lut_coded_value[i][j];
						pOutParam->m_UserDataInfo.m_ColourRemappingInfo.post_lut_target_value[i][j] = colour_remapping->post_lut_target_value[i][j];
					}
				}
			}
		}
	}
	#endif

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE)) {
		int pre_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset, pre_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_pre = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF)) {
		int suf_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].offset, suf_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_suf = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_1)) {
		int pre_1_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset, pre_1_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_pre_1 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_2)) {
		int pre_2_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset, pre_2_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_pre_2 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_1)) {
		int suf_1_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].offset, suf_1_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_suf_1 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size;
	}

	if (info.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_2)) {
		int suf_2_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size + 15) / 16 ) *16;
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].offset, suf_2_size, VDI_LITTLE_ENDIAN);

		pOutParam->m_uiUserData = 1;
		userdataNum++;
		t35_suf_2 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size;
	}

	if ( userdataNum > 0 ) {
		Uint32 offset_user = 0;
		wave5_memcpy(pBase, &userdataNum, sizeof(Uint16), 2);
		wave5_memcpy(pBase+2, &userdataTotalSize, sizeof(Uint16), 2);

		if (t35_pre) 	{
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf) 	{
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_pre_1) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_pre_2) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf_1) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf_2) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}
	}
}

static RetCode
WAVE5_dec_seq_header(wave5_t *pVpu, vpu_4K_D2_dec_initial_info_t *pInitialInfo)
{
	RetCode ret = 0;
	int coreIdx = 0;
	DecHandle handle = pVpu->m_pCodecInst;

	// Seq Init & Get Initial Info
	{
		DecInitialInfo initial_info = {0,};

		if ((pVpu->m_pCodecInst->codecMode != W_HEVC_DEC) && (pVpu->m_pCodecInst->codecMode != W_VP9_DEC)) {
			pInitialInfo->m_iReportErrorReason = RETCODE_CODEC_SPECOUT;
			ret = RETCODE_CODEC_SPECOUT;
		}

		#if TCC_HEVC___DEC_SUPPORT == 1
		#else
		if (pVpu->m_pCodecInst->codecMode == W_HEVC_DEC) {
			pInitialInfo->m_iReportErrorReason = RETCODE_CODEC_SPECOUT;
			ret = RETCODE_CODEC_SPECOUT;
		}
		#endif

		#if TCC_VP9____DEC_SUPPORT == 1
		#else
		if (pVpu->m_pCodecInst->codecMode == W_VP9_DEC) {
			pInitialInfo->m_iReportErrorReason = RETCODE_CODEC_SPECOUT;
			ret = RETCODE_CODEC_SPECOUT;
		}
		#endif

		do {
			ret = WAVE5_DecIssueSeqInit(handle);
		} while (ret == RETCODE_QUEUEING_FAILURE);
		if (ret != RETCODE_SUCCESS)
			return ret;

		do {
			ret = WAVE5_DecCompleteSeqInit(handle, &initial_info);
		} while (ret == RETCODE_REPORT_NOT_READY);

		if (initial_info.seqInitErrReason)
			pInitialInfo->m_iReportErrorReason = initial_info.seqInitErrReason;

		if (initial_info.warnInfo)
			pInitialInfo->m_iReportErrorReason = initial_info.warnInfo;

		if (ret != RETCODE_SUCCESS) {
			volatile int seqErrReason;

			seqErrReason = initial_info.seqInitErrReason;
			pInitialInfo->m_iReportErrorReason = seqErrReason;
			if(seqErrReason) {
				if (seqErrReason == WAVE5_SPECERR_OVER_BIT_DEPTH) {
					pInitialInfo->m_iReportErrorReason = RETCODE_VPUERR_PROFILE;
					ret = RETCODE_CODEC_SPECOUT;
				}

				if (seqErrReason == WAVE5_SPECERR_OVER_PICTURE_WIDTH_SIZE) {
					pInitialInfo->m_iReportErrorReason = RETCODE_VPUERR_MAX_RESOLUTION;
					ret = RETCODE_CODEC_SPECOUT;
				}

				if (seqErrReason == WAVE5_SPECERR_OVER_PICTURE_HEIGHT_SIZE) {
					pInitialInfo->m_iReportErrorReason = RETCODE_VPUERR_MAX_RESOLUTION;
					ret = RETCODE_CODEC_SPECOUT;
				}

				if (seqErrReason == WAVE5_SPECERR_OVER_CHROMA_FORMAT) {
					pInitialInfo->m_iReportErrorReason = RETCODE_VPUERR_CHROMA_FORMAT;
					ret = RETCODE_CODEC_SPECOUT;
				}

				if (seqErrReason == WAVE5_ETCERR_INIT_SEQ_SPS_NOT_FOUND) {
					pInitialInfo->m_iReportErrorReason = RETCODE_VPUERR_SEQ_HEADER_NOT_FOUND;
				}
			}
			return ret;
		}

		if (ret != RETCODE_SUCCESS) {
			return ret;
		}

	#ifdef CQ_BUFFER_RESILIENCE
		// CQ2 error-resilience: Addresses a bug where buffer availability becomes insufficient.
		// If the command queue count (CQ count) exceeds 1 and there is a frame buffer delay,
		// increment the minimum frame buffer count to prevent buffer underflow or queueing_failure
		// caused by additional CQ buffer.
		if ((pVpu->iCqCount > 1) && (initial_info.frameBufDelay > 0)) {
			/*int add_count = pVpu->iCqCount;*/
			int add_count = 1;
			DLOGD("CQ2 error-resilience: Adjusting minBuffer(%d => %d)",
					initial_info.minFrameBufferCount,
					initial_info.minFrameBufferCount + add_count);
			initial_info.minFrameBufferCount += add_count;
		}
	#endif

		// RETCODE_SUCCESS
		if (handle->CodecInfo.decInfo.openParam.FullSizeFB == 1) {
			int crop_l = initial_info.picCropRect.left;
			int crop_t = initial_info.picCropRect.top;
			int crop_r = (initial_info.picWidth - initial_info.picCropRect.right);
			int crop_b = (initial_info.picHeight - initial_info.picCropRect.bottom);

			int real_width  = initial_info.picWidth  - crop_l - crop_r;
			int real_height = initial_info.picHeight - crop_b - crop_t;

			if (real_width < real_height) { // vertical video
				int vertical_width = handle->CodecInfo.decInfo.openParam.maxheight;
				int vertical_height = handle->CodecInfo.decInfo.openParam.maxwidth;

				handle->CodecInfo.decInfo.openParam.maxwidth = WAVE5_W_ALIGN(handle->CodecInfo.decInfo.openParam, vertical_width);
				handle->CodecInfo.decInfo.openParam.maxheight = WAVE5_H_ALIGN(handle->CodecInfo.decInfo.openParam, vertical_height);
				DLOGE("MaxFB(Vertical) Video - L, R, T, B = %d, %d, %d, %d\n", crop_l, crop_r, crop_t, crop_b);
				DLOGE("MaxFB(Vertical) Video - W x H = %d x %d, real = %d x %d, MaxFb = %d x %d"
					, initial_info.picWidth, initial_info.picHeight
					, real_width, real_height
					, handle->CodecInfo.decInfo.openParam.maxwidth
					, handle->CodecInfo.decInfo.openParam.maxheight);
			}
		}

		{
			int compressedFbCount = 0;
			int linearFbCount = 0;
			int totalFbCount = 0;
			Uint32 framebufSize;
			Uint32 framebufSize_Linear = 0;
			Int32 framebufSizeCompressed = 0;
			Int32 framebufStride = 0;
			size_t framebufHeight = 0;
			Uint32 mvColSize = 0, fbcYTblSize = 0, fbcCTblSize = 0;
			Int32 width, height;
			size_t  picWidth = 0, picHeight = 0;
			unsigned int bufAlignSize = 0, bufStartAddrAlign = 0;

		#ifdef ADD_FBREG_3
			int sizeLuma_comp = 0;
			int sizeChroma_comp = 0;
			int sizeFb_comp = 0;
			int sizeLuma_linear = 0;
			int sizeChroma_linear = 0;
			int sizeFb_linear = 0;
		#endif
			FrameBufferFormat format = handle->CodecInfo.decInfo.wtlFormat;

			bufAlignSize = pVpu->m_pCodecInst->bufAlignSize;
			bufStartAddrAlign = pVpu->m_pCodecInst->bufStartAddrAlign;

			compressedFbCount = initial_info.minFrameBufferCount;

			if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1)) {
				linearFbCount = initial_info.minFrameBufferCount;
			}

			totalFbCount = compressedFbCount + linearFbCount;

			if (handle->CodecInfo.decInfo.enableAfbce) {
				if ((initial_info.lumaBitdepth > 8 || initial_info.chromaBitdepth > 8)) {
					handle->CodecInfo.decInfo.openParam.afbceFormat = AFBCE_FORMAT_420_P10_16BIT_LSB;
				}
				else {
					handle->CodecInfo.decInfo.openParam.afbceFormat = AFBCE_FORMAT_420;
				}

				handle->CodecInfo.decInfo.afbceFormat = handle->CodecInfo.decInfo.openParam.afbceFormat;
			}

			if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
				width = handle->CodecInfo.decInfo.openParam.maxwidth;
				height = handle->CodecInfo.decInfo.openParam.maxheight;
				format = (initial_info.lumaBitdepth > 8 || initial_info.chromaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;

				if (format == FORMAT_420_P10_16BIT_LSB) {
					pInitialInfo->m_iBitDepth = 10;
				}
				else {
					pInitialInfo->m_iBitDepth = 8;
				}

				format = (handle->CodecInfo.decInfo.openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;

				if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1)) {
					if (initial_info.lumaBitdepth > 8 || initial_info.chromaBitdepth > 8) {
						FrameBufferFormat yuvFormat = FORMAT_420;
						if (!handle->CodecInfo.decInfo.openParam.TenBitDisable) {
							yuvFormat = FORMAT_420_P10_16BIT_LSB;
						}
						WAVE5_DecGiveCommand(handle, DEC_SET_WTL_FRAME_FORMAT, &yuvFormat);
					}
				}

				#if TCC_HEVC___DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_HEVC) {
					framebufStride = width;
					framebufHeight = WAVE5_ALIGN32(height);
				}
				#endif
				#if TCC_VP9____DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_VP9) {
					framebufStride = WAVE5_ALIGN64(width);
					framebufHeight = WAVE5_ALIGN64(height);
				}
				#endif

				framebufStride = CalcStride(framebufStride, height, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
				framebufSizeCompressed = WAVE5_GetFrameBufSize(handle->coreIdx, framebufStride, framebufHeight,
							COMPRESSED_FRAME_MAP, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, NULL);
				framebufSizeCompressed = WAVE5_ALIGN(framebufSizeCompressed, bufAlignSize);

			#ifdef ADD_FBREG_3
				sizeLuma_comp   = CalcLumaSize(pVpu->m_pCodecInst->productId, framebufStride, framebufHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
				sizeChroma_comp = CalcChromaSize(pVpu->m_pCodecInst->productId, framebufStride, framebufHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
			#endif

				if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1) || (linearFbCount != 0))	{
					size_t  linearStride = 0;
					size_t  fbHeight = 0;
					Uint32  mapType = LINEAR_FRAME_MAP;

					format = handle->CodecInfo.decInfo.wtlFormat;

					if (handle->CodecInfo.decInfo.openParam.TenBitDisable) {
						format = FORMAT_420;
					}

					if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1)) {
						WAVE5_DecGiveCommand(handle, DEC_SET_WTL_FRAME_FORMAT, &format);
					}

					if (handle->CodecInfo.decInfo.enableAfbce) {
						mapType = ARM_COMPRESSED_FRAME_MAP;
						format = handle->CodecInfo.decInfo.afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					}

					#if TCC_HEVC___DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						picWidth  = width;
						picHeight = WAVE5_ALIGN16(height);
						fbHeight  = picHeight;
					}
					#endif

					#if TCC_VP9____DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_VP9) {
						picWidth = WAVE5_ALIGN64(width);
						picHeight = height;
						fbHeight = WAVE5_ALIGN64(height);
					}
					#endif

					linearStride = CalcStride(picWidth, picHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, FALSE);	//isVP9=FALSE not
					framebufSize_Linear = WAVE5_GetFrameBufSize(coreIdx, linearStride, fbHeight, mapType, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, NULL);
					framebufSize_Linear = WAVE5_ALIGN(framebufSize_Linear, bufAlignSize);

				#ifdef ADD_FBREG_3
					sizeLuma_linear   = CalcLumaSize(pVpu->m_pCodecInst->productId, linearStride, fbHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, NULL);
					sizeChroma_linear = CalcChromaSize(pVpu->m_pCodecInst->productId, linearStride, fbHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, NULL);
				#endif
				}

				framebufSize = framebufSize_Linear + framebufSizeCompressed + bufStartAddrAlign;
				framebufSize = WAVE5_ALIGN(framebufSize, bufAlignSize);
				handle->CodecInfo.decInfo.framebufSizeCompressedLinear = framebufSize;

				#if TCC_HEVC___DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_HEVC) {
					mvColSize = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(width, height);
					fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(width, height);
					fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(width, height);
				}
				#endif

				#if TCC_VP9____DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_VP9) {
					mvColSize = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(width, height);
					fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(width), WAVE5_ALIGN64(height));
					fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(width), WAVE5_ALIGN64(height));
				}
				#endif

				mvColSize = WAVE5_ALIGN16(mvColSize);
				mvColSize = ((mvColSize + 4095) & ~4095) + 4096;		// 4096 is a margin

				fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
				fbcYTblSize = ((fbcYTblSize + 4095) & ~4095) + 4096;	// 4096 is a margin

				fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
				fbcCTblSize = ((fbcCTblSize + 4095) & ~4095) + 4096;	// 4096 is a margin

				framebufSize = (framebufSize + mvColSize + fbcYTblSize + fbcCTblSize);
				framebufSize = WAVE5_ALIGN(framebufSize, bufAlignSize);

			#ifdef ADD_FBREG_3
				pInitialInfo->m_uiBufSizeLinearLuma = sizeLuma_linear;
				pInitialInfo->m_uiBufSizeLinearChroma = sizeChroma_linear;
				pInitialInfo->m_uiBufSizeCompressedLuma = sizeLuma_comp;
				pInitialInfo->m_uiBufSizeCompressedChroma = sizeChroma_comp;
				pInitialInfo->m_uiBufSizeMVCol = mvColSize;
				pInitialInfo->m_uiBufSizeFBCYTable = fbcYTblSize;
				pInitialInfo->m_uiBufSizeFBCCTable = fbcCTblSize;
				DLOGI("FullSizeFB BufSize, LinearLuma:%u, LinearChroma:%u, CompressedLuma:%u, CompressedChroma:%u, MVCol:%u, FBCYTable:%u, FBCCTable:%u",
					sizeLuma_linear, sizeChroma_linear, sizeLuma_comp, sizeChroma_comp, mvColSize, fbcYTblSize, fbcCTblSize);
			#endif

			}
			else {	//!FullSizeFB
				width = initial_info.picWidth;
				height = initial_info.picHeight;

				format = (initial_info.lumaBitdepth > 8 || initial_info.chromaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
				if (format == FORMAT_420_P10_16BIT_LSB)
					pInitialInfo->m_iBitDepth = 10;
				else
					pInitialInfo->m_iBitDepth = 8;

				if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1)) {
					if (initial_info.lumaBitdepth > 8 || initial_info.chromaBitdepth > 8) {
						FrameBufferFormat yuvFormat = FORMAT_420;
						if (!handle->CodecInfo.decInfo.openParam.TenBitDisable) {
							yuvFormat = FORMAT_420_P10_16BIT_LSB;
						}
						WAVE5_DecGiveCommand(handle, DEC_SET_WTL_FRAME_FORMAT, &yuvFormat);
					}
				}

				#if TCC_HEVC___DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_HEVC) {
					framebufStride = width;
					framebufHeight = WAVE5_ALIGN32(height);
				}
				#endif

				#if TCC_VP9____DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_VP9) {
					framebufStride = WAVE5_ALIGN64(width);
					framebufHeight = WAVE5_ALIGN64(height);
				}
				#endif

				framebufStride = CalcStride(framebufStride, height, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
				framebufSizeCompressed = WAVE5_GetFrameBufSize(handle->coreIdx, framebufStride, framebufHeight,
							COMPRESSED_FRAME_MAP, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, NULL);
				framebufSizeCompressed = WAVE5_ALIGN(framebufSizeCompressed, bufAlignSize);

			#ifdef ADD_FBREG_3
				sizeLuma_comp   = CalcLumaSize(pVpu->m_pCodecInst->productId, framebufStride, framebufHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
				sizeChroma_comp = CalcChromaSize(pVpu->m_pCodecInst->productId, framebufStride, framebufHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, NULL);
			#endif

				if (handle->CodecInfo.decInfo.openParam.TenBitDisable) {
					format = FORMAT_420;
				}

				if ((handle->CodecInfo.decInfo.wtlEnable == 1) || (handle->CodecInfo.decInfo.enableAfbce == 1) || (linearFbCount != 0)) {
					size_t  linearStride = 0;
					size_t  fbHeight = 0;
					Uint32  mapType = LINEAR_FRAME_MAP;

					format = handle->CodecInfo.decInfo.wtlFormat;

					if (handle->CodecInfo.decInfo.openParam.TenBitDisable) {
						format = FORMAT_420;
					}

					if (handle->CodecInfo.decInfo.enableAfbce) {
						mapType = ARM_COMPRESSED_FRAME_MAP;
						format = handle->CodecInfo.decInfo.afbceFormat == AFBCE_FORMAT_420 ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					}

					#if TCC_HEVC___DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						picWidth  = width;
						picHeight = WAVE5_ALIGN16(height);
						fbHeight  = picHeight;
					}
					#endif

					#if TCC_VP9____DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_VP9) {
						picWidth = WAVE5_ALIGN64(width);
						picHeight = height;
						fbHeight = WAVE5_ALIGN64(height);
					}
					#endif
					linearStride = CalcStride(picWidth, picHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, FALSE);
					framebufSize_Linear = WAVE5_GetFrameBufSize(coreIdx, linearStride, fbHeight, mapType, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, NULL);
					framebufSize_Linear = WAVE5_ALIGN(framebufSize_Linear, bufAlignSize);

					#ifdef ADD_FBREG_3
					sizeLuma_linear   = CalcLumaSize(pVpu->m_pCodecInst->productId, linearStride, fbHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, NULL);
					sizeChroma_linear = CalcChromaSize(pVpu->m_pCodecInst->productId, linearStride, fbHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, mapType, NULL);
					#endif
				}

				framebufSize = framebufSize_Linear + framebufSizeCompressed + bufStartAddrAlign;
				framebufSize = WAVE5_ALIGN(framebufSize, bufAlignSize);
				handle->CodecInfo.decInfo.framebufSizeCompressedLinear = framebufSize;

				#if TCC_HEVC___DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_HEVC) {
					mvColSize = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(width, height);
					fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(width, height);
					fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(width, height);
				}
				#endif

				#if TCC_VP9____DEC_SUPPORT == 1
				if (pVpu->m_iBitstreamFormat == STD_VP9) {
					mvColSize = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(width, height);
					fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(width), WAVE5_ALIGN64(height));
					fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(width), WAVE5_ALIGN64(height));
				}
				#endif

				mvColSize = WAVE5_ALIGN16(mvColSize);
				mvColSize = ((mvColSize + 4095) & ~4095) + 4096;	// 4096 is a margin

				fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
				fbcYTblSize = ((fbcYTblSize + 4095) & ~4095) + 4096;	// 4096 is a margin

				fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
				fbcCTblSize = ((fbcCTblSize + 4095) & ~4095) + 4096;	// 4096 is a margin

				framebufSize = (framebufSize + mvColSize + fbcYTblSize + fbcCTblSize);
				framebufSize = WAVE5_ALIGN(framebufSize, bufAlignSize);

			#ifdef ADD_FBREG_3
				pInitialInfo->m_uiBufSizeLinearLuma = sizeLuma_linear;
				pInitialInfo->m_uiBufSizeLinearChroma = sizeChroma_linear;
				pInitialInfo->m_uiBufSizeCompressedLuma = sizeLuma_comp;
				pInitialInfo->m_uiBufSizeCompressedChroma = sizeChroma_comp;
				pInitialInfo->m_uiBufSizeMVCol = mvColSize;
				pInitialInfo->m_uiBufSizeFBCYTable = fbcYTblSize;
				pInitialInfo->m_uiBufSizeFBCCTable = fbcCTblSize;
				DLOGI("BufSize, LinearLuma:%u, LinearChroma:%u, CompressedLuma:%u, CompressedChroma:%u, MVCol:%u, FBCYTable:%u, FBCCTable:%u",
					sizeLuma_linear, sizeChroma_linear, sizeLuma_comp, sizeChroma_comp, mvColSize, fbcYTblSize, fbcCTblSize);
			#endif
			}

			handle->CodecInfo.decInfo.framebufSizeCompressed = framebufSizeCompressed;
			handle->CodecInfo.decInfo.framebufSizeLinear = framebufSize_Linear;
			handle->CodecInfo.decInfo.mvColSize = mvColSize;
			handle->CodecInfo.decInfo.fbcYTblSize = fbcYTblSize;
			handle->CodecInfo.decInfo.fbcCTblSize = fbcCTblSize;
			//handle->CodecInfo.decInfo.framebufSizeCompressedLinear = framebufSize;

			initial_info.framebufferformat = format;
			pInitialInfo->m_iFrameBufferFormat = format;
			pInitialInfo->m_iMinFrameBufferSize = WAVE5_ALIGN(framebufSize, bufAlignSize);
			pVpu->m_iMinFrameBufferSize = pInitialInfo->m_iMinFrameBufferSize;

			pVpu->m_iFrameBufWidth  = framebufStride;
			pVpu->m_iFrameBufHeight = framebufHeight;
		}

		pInitialInfo->m_iMinFrameBufferCount = initial_info.minFrameBufferCount;
		pVpu->m_iNeedFrameBufCount = initial_info.minFrameBufferCount;

		pInitialInfo->m_iAspectRateInfo = initial_info.aspectRateInfo;
		pInitialInfo->m_iFrameBufDelay = initial_info.frameBufDelay;
		pInitialInfo->m_iLevel = initial_info.level;

		if (initial_info.picCropRect.left || initial_info.picCropRect.right ||
			initial_info.picCropRect.top  || initial_info.picCropRect.bottom) {
			pInitialInfo->m_PicCrop.m_iCropLeft = initial_info.picCropRect.left;
			pInitialInfo->m_PicCrop.m_iCropTop = initial_info.picCropRect.top;
			pInitialInfo->m_PicCrop.m_iCropRight  = (initial_info.picWidth - initial_info.picCropRect.right);
			pInitialInfo->m_PicCrop.m_iCropBottom = (initial_info.picHeight - initial_info.picCropRect.bottom);
		}

		pInitialInfo->m_iPicHeight = initial_info.picHeight;
		pInitialInfo->m_iPicWidth = initial_info.picWidth;
		pInitialInfo->m_iProfile = initial_info.profile;
		pInitialInfo->m_uiFrameRateRes = initial_info.fRateNumerator;
		pInitialInfo->m_uiFrameRateDiv = initial_info.fRateDenominator;
		pInitialInfo->m_iTier = initial_info.tier;
		pInitialInfo->m_iInterlace = initial_info.interlace;

		pInitialInfo->m_uiRtlDate = pVpu->m_iDateRTL;
		pInitialInfo->m_uiRtlRevision = pVpu->m_iRevisionRTL;

		#if TCC_VP9____DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_VP9) {
			pInitialInfo->m_VP9ColorInfo.color_space = initial_info.vp9PicInfo.color_space;
			pInitialInfo->m_VP9ColorInfo.color_range = initial_info.vp9PicInfo.color_range;
		}
		#endif

		if ((initial_info.userDataHeader != 0) && (handle->CodecInfo.decInfo.userDataEnable != 0)) {
			GetSeqInitUserData( pVpu, pInitialInfo, initial_info );
		}

		WAVE5_SetPendingInst(pVpu->m_pCodecInst->coreIdx, 0);

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		WAVE5_ClearInterrupt(coreIdx);
		#endif
	}

	return ret;
}

static RetCode check_bitstreamformat(int m_iBitstreamFormat)
{
	RetCode ret = RETCODE_SUCCESS;

	if ((m_iBitstreamFormat != STD_HEVC) && (m_iBitstreamFormat != STD_VP9)) {
		ret = RETCODE_CODEC_SPECOUT;
	}

	#if TCC_HEVC___DEC_SUPPORT == 1
	#else
	if (m_iBitstreamFormat == STD_HEVC) {
		ret = RETCODE_CODEC_SPECOUT;
	}
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	#else
	if (m_iBitstreamFormat == STD_VP9) {
		ret = RETCODE_CODEC_SPECOUT;
	}
	#endif

	return ret;
}

static RetCode
WAVE5_dec_init(wave5_t **ppVpu, vpu_4K_D2_dec_init_t *pInitParam, vpu_4K_D2_dec_initial_info_t *pInitialInfoParam)
{
	int coreIdx = 0;
	int cq_count;
	PhysicalAddress codeBase = 0;
	RetCode ret;
	DecHandle handle = { 0 };
	DecOpenParam op = {0,};

	ret = check_bitstreamformat(pInitParam->m_iBitstreamFormat);

	if (ret != RETCODE_SUCCESS)
		return RETCODE_CODEC_SPECOUT;

	if (gWave5FWAddr == 0) {
		return RETCODE_INVALID_COMMAND;
	}

	//! [0] Global callback func
	{
		wave5_memcpy	= pInitParam->m_Memcpy;
		wave5_memset	= pInitParam->m_Memset;
		wave5_interrupt	= pInitParam->m_Interrupt;
		wave5_ioremap	= pInitParam->m_Ioremap;
		wave5_iounmap	= pInitParam->m_Iounmap;
		wave5_read_reg	= pInitParam->m_reg_read;
		wave5_write_reg	= pInitParam->m_reg_write;
		wave5_usleep	= pInitParam->m_Usleep;

	#ifdef ENABLE_WAIT_POLLING_USLEEP
	#if DBG_PROCESS_LEVEL >= 501
		if (wave5_usleep != NULL) {
			wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
		}
	#endif
	#endif

		if (wave5_memcpy == 0) {
			wave5_memcpy = wave5_local_mem_cpy;
		}

		if (wave5_memset == 0) {
			wave5_memset = wave5_local_mem_set;
		}
	}

	//version info
	{
		char sz_ver[64];
		char sz_date[64];
		ret = get_version_4kd2(&sz_ver[0], &sz_date[0]);
		if (ret != 0) {
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX"Wave5 VPU Version Failed\n");
		}
		else {
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX"Wave5 VPU Version   : %s\n", sz_ver);
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX"Wave5 VPU Build Date: %s\n", sz_date);
		}
		ret = 0;

		// Initialize global values before calling WAVE5_dec_init()
		g_uiBitCodeSize = WAVE5_MAX_CODE_BUF_SIZE;
	}


	// CQ Settings
	{
		// Extract CQ Count from DecOptFlags for older versions
		cq_count = (pInitParam->m_uiDecOptFlags >> WAVE5_CQ_DEPTH_SHIFT) & 0xF;

		// Apply initialization parameter if set
		if (pInitParam->m_iCQCount > 0) {
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX
					"Setting CQ Count from %d to %d based on init parameters\n",
					cq_count, pInitParam->m_iCQCount);
			cq_count = pInitParam->m_iCQCount;
		}

		// Apply debug settings if set
		if (gs_iDbgCqCount > 0) {
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX
					"Overriding CQ Count from %d to %d based on debug settings\n",
					cq_count, gs_iDbgCqCount);
			cq_count = gs_iDbgCqCount;
			gs_iDbgCqCount = 0;  // Reset debug CQ count after applying it
		}

		// Validate CQ Count and set default if needed
		if (cq_count < 1 || cq_count > 2) {
			V_PRINTF(VPU_KERN_ERR PRT_PREFIX
					"Invalid CQ Count (%d), setting to default value: 2\n", cq_count);
			cq_count = 2;
		}

		// Log the final CQ Count setting
		//V_PRINTF(VPU_KERN_ERR PRT_PREFIX "CQ Count set to %d\n", cq_count);
	}

	codeBase = gWave5FWAddr;

	//! [1] VPU Init
	if ((gWave5InitFirst == 0) || (Wave5ReadReg(coreIdx, W5_VCPU_CUR_PC) == 0)) {
		gWave5InitFirst = 1;
		gWave5InitFirst_exit = 0;

	#if defined(ARM_LINUX) || defined(ARM_WINCE)
		gWave5VirtualBitWorkAddr = pInitParam->m_BitWorkAddr[VA];
		gWave5PhysicalBitWorkAddr= pInitParam->m_BitWorkAddr[PA];
		gWave5VirtualRegBaseAddr = pInitParam->m_RegBaseVirtualAddr;
	#endif

		gWave5MaxInstanceNum = WAVE5_MAX_NUM_INSTANCE;
	#ifdef USE_MAX_INSTANCE_NUM_CTRL
		if ((pInitParam->m_uiDecOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF) {
			gWave5MaxInstanceNum = (pInitParam->m_uiDecOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF;
		}

		if (gWave5MaxInstanceNum > 4) {
			return RETCODE_INVALID_PARAM;
		}
	#endif

		//reset the parameter buffer
		{
			char * pInitParaBuf;

		#if defined(ARM_LINUX) || defined(ARM_WINCE)
			pInitParaBuf = (char*)gWave5VirtualBitWorkAddr;
		#else
			pInitParaBuf = (char*)pInitParam->m_BitWorkAddr[PA];
		#endif

			if (wave5_memset) {
				if (pInitParaBuf != NULL) {
				#ifndef TEST_ADAPTIVE_CQ_NUM
					if( wave5_memset )
						wave5_memset( pInitParaBuf, 0, WAVE5_WORK_CODE_BUF_SIZE, 1);
				#else
					int work_code_buf_size;

					work_code_buf_size = ((WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE + (cq_count*WAVE5_ONE_TASKBUF_SIZE_FOR_CQ)) + (WAVE5_WORKBUF_SIZE*WAVE5_MAX_NUM_INSTANCE));
					if (wave5_memset) {
						wave5_memset(pInitParaBuf, 0, work_code_buf_size, 1);
					}
				#endif
				}
			}
		}

		ret = WAVE5_Init( coreIdx, pInitParam->m_BitWorkAddr[PA], codeBase, cq_count);

		if ((ret != RETCODE_SUCCESS) && (ret != RETCODE_CALLED_BEFORE)) {
			if (ret == RETCODE_CODEC_EXIT) {
			#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, cq_count);
			#endif
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
			return ret;
		}
	}
	else {
		if (WAVE5_ResetRegisters(coreIdx, 1) != RETCODE_SUCCESS) {
			return RETCODE_FAILURE;
		}
	}

	//! [2] Get Dec Handle
	ret = wave5_get_vpu_handle(ppVpu);
	if (ret == RETCODE_FAILURE) {
		*ppVpu = 0;
		return ret;
	}

	(*ppVpu)->m_BitWorkAddr[PA] = pInitParam->m_BitWorkAddr[PA];
	(*ppVpu)->m_BitWorkAddr[VA] = pInitParam->m_BitWorkAddr[VA];

	//explicit initialization
	(*ppVpu)->stSetOptions.iUseBitstreamOffset = 0;
	(*ppVpu)->stSetOptions.iMeasureDecPerf = 0;
	(*ppVpu)->stSetOptions.pfPrintCb = NULL;

	(*ppVpu)->iCqCount = cq_count;

	{
		Uint32 version;
		Uint32 revision;
		Uint32 productId;

		ret = WAVE5_GetVersionInfo(coreIdx, &version, &revision, &productId);

		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_EXIT) {
				#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, cq_count);
				#endif
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
			return ret;
		}

		(*ppVpu)->m_iRevisionFW = revision;
		(*ppVpu)->m_iDateRTL = Wave5ReadReg(coreIdx, W5_RET_CONF_DATE);	// 0x130
		(*ppVpu)->m_iRevisionRTL = Wave5ReadReg(coreIdx, W5_RET_CONF_REVISION);	// 0x134
	}

	//! [3] Open Decoder
	{
		op.bitstreamFormat = pInitParam->m_iBitstreamFormat;
		op.coreIdx = 0;
		op.tiled2LinearEnable = 0;
		op.fbc_mode = 0x0c;

		if (pInitParam->m_uiDecOptFlags & WAVE5_WTL_ENABLE) {
			op.wtlEnable = 1;
		}
		if (op.wtlEnable == 1) {
			op.wtlMode = FF_FRAME;
		}

		if (pInitParam->m_uiDecOptFlags & WAVE5_RESOLUTION_CHANGE) {
			op.FullSizeFB = 1;
		}
		else {
			op.FullSizeFB = 0;
		}

		op.bitstreamBuffer = pInitParam->m_BitstreamBufAddr[PA];
		op.bitstreamBufferSize = pInitParam->m_iBitstreamBufSize;

		op.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
		op.frameEndian    = VDI_128BIT_LITTLE_ENDIAN;
		op.streamEndian   = VDI_128BIT_LITTLE_ENDIAN;

		op.bitstreamMode  = 0;

		op.TenBitDisable = 0;
		op.TenBitDisable = (pInitParam->m_uiDecOptFlags >> WAVE5_10BITS_DISABLE_SHIFT) & 0x1;

		op.nv21 = 0;
		op.nv21 = (pInitParam->m_uiDecOptFlags >> WAVE5_NV21_ENABLE_SHIFT) & 0x1;

		op.afbceEnable = 0;
		op.afbceEnable = (pInitParam->m_uiDecOptFlags >> WAVE5_AFBC_ENABLE_SHIFT) & 0x1;
		op.afbceFormat = 0;

		op.CQ_Depth = cq_count;
		op.seq_init_bsbuff_change = (pInitParam->m_uiDecOptFlags & WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE)>>WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE_SHIFT;
		if (pInitParam->m_iFilePlayEnable == 0) {
			op.seq_init_bsbuff_change = 0;
		}

		{
			PhysicalAddress workbufaddr;

			workbufaddr = pInitParam->m_BitWorkAddr[PA] + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE + (op.CQ_Depth*WAVE5_ONE_TASKBUF_SIZE_FOR_CQ);
			op.vbWork.phys_addr = workbufaddr + ((*ppVpu)->m_iVpuInstIndex*(WAVE5_WORKBUF_SIZE));
			op.vbWork.size = WAVE5_WORKBUF_SIZE;

			op.vbTemp.phys_addr = pInitParam->m_BitWorkAddr[PA] + WAVE5_MAX_CODE_BUF_SIZE;
			op.vbTemp.size = WAVE5_TEMPBUF_SIZE;
		}

		if (op.FullSizeFB == 1) {
			op.maxwidth = pInitParam->m_Reserved[3];	// max width
			op.maxheight = pInitParam->m_Reserved[4];	// max height
			op.maxbit = pInitParam->m_Reserved[5];		// max bit depth

			// Due to performance limitations in the linear frame map,
			// the VPU may only support up to a maximum resolution of FHD 1080p.
			// This means that resolutions above 1080p may not be supported, or
			// may require additional optimization to ensure optimal performance.
			if ((op.maxwidth * op.maxheight) > (4096 * 4096)) {
				return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
			}

			if (op.maxwidth < op.maxheight) { //vertical video
			    int vertical_width = op.maxheight;
				int vertical_height = op.maxwidth;
				op.maxwidth = vertical_width;
				op.maxheight = vertical_height;
			}

			if (op.maxbit != 8) {
				op.maxbit = 10;
			}

			op.maxwidth = WAVE5_W_ALIGN(op, op.maxwidth);
			op.maxheight = WAVE5_H_ALIGN(op, op.maxheight);
		}

		ret = WAVE5_DecOpen(&handle, &op);
		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_EXIT) {
				#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, cq_count);
				#endif
				WAVE5_dec_reset( (*ppVpu), ( 0 | (1<<16) ) );
				*ppVpu = 0;
				return RETCODE_CODEC_EXIT;
			}
			*ppVpu = 0;
			return ret;
		}

		#ifdef ENABLE_VDI_LOG
		WAVE5_DecGiveCommand(handle, ENABLE_LOGGING, 1);
		#endif
		(*ppVpu)->m_pCodecInst = handle;
		(*ppVpu)->m_iBitstreamFormat = op.bitstreamFormat;
		(*ppVpu)->m_StreamRdPtrRegAddr = handle->CodecInfo.decInfo.streamRdPtrRegAddr;
		(*ppVpu)->m_StreamWrPtrRegAddr = handle->CodecInfo.decInfo.streamWrPtrRegAddr;
		(*ppVpu)->m_uiDecOptFlags = pInitParam->m_uiDecOptFlags;
		(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;
		if (wave5_memset) {
			wave5_memset(&(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.vp9SuperFrame, 0x00, sizeof(VP9Superframe), 0);
		}

		#ifdef USE_SEC_AXI
		if ((pInitParam->m_uiDecOptFlags >> SEC_AXI_BUS_DISABLE_SHIFT) & 0x1) {
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useBitEnable = 0;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useIpEnable = 0;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useLfRowEnable = 0;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclEnable = 0;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclPackedModeEnable = 0;
			handle->CodecInfo.decInfo.secAxiInfo.bufBase = 0;
			handle->CodecInfo.decInfo.secAxiInfo.bufSize = 0;
		}
		else {
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useBitEnable = 1;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useIpEnable = 1;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useLfRowEnable = 1;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclEnable = 1;
			handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclPackedModeEnable = 1;
			#ifndef TEST_USE_SECAXI_SRAM
			if ((pInitParam->m_uiDecOptFlags >> SEC_AXI_BUS_ENABLE_SRAM_SHIFT) & 0x1) {
				handle->CodecInfo.decInfo.secAxiInfo.bufBase = USE_SEC_AXI_SRAM_BASE_ADDR;
				handle->CodecInfo.decInfo.secAxiInfo.bufSize = WAVE5_SEC_AXI_BUF_SIZE;
			}
			else {
				handle->CodecInfo.decInfo.secAxiInfo.bufBase = pInitParam->m_BitWorkAddr[PA] + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE;
				handle->CodecInfo.decInfo.secAxiInfo.bufSize = WAVE5_SEC_AXI_BUF_SIZE;
			}
			#else
			handle->CodecInfo.decInfo.secAxiInfo.bufBase = USE_SEC_AXI_SRAM_BASE_ADDR;
			handle->CodecInfo.decInfo.secAxiInfo.bufSize = WAVE5_SEC_AXI_BUF_SIZE;
			#endif
		}
		#else
		handle->CodecInfo.decInfo.secAxiInfo.u.wave.useBitEnable = 0;
		handle->CodecInfo.decInfo.secAxiInfo.u.wave.useIpEnable = 0;
		handle->CodecInfo.decInfo.secAxiInfo.u.wave.useLfRowEnable = 0;
		handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclEnable = 0;
		handle->CodecInfo.decInfo.secAxiInfo.u.wave.useSclPackedModeEnable = 0;
		handle->CodecInfo.decInfo.secAxiInfo.bufBase = 0;
		handle->CodecInfo.decInfo.secAxiInfo.bufSize = 0;
		#endif

		if (handle->CodecInfo.decInfo.openParam.afbceEnable == 1) {
			WAVE5_DecGiveCommand(handle, ENABLE_AFBCE, NULL);
		}
		else {
			WAVE5_DecGiveCommand(handle, DISABLE_AFBCE, NULL);
		}

		handle->CodecInfo.decInfo.userDataEnable = pInitParam->m_bEnableUserData;

		if (handle->CodecInfo.decInfo.userDataEnable) {
			#ifndef USER_DATA_SKIP
			handle->CodecInfo.decInfo.userDataEnable = (1<<H265_USERDATA_FLAG_MASTERING_COLOR_VOL) | (1<<H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT)
													 | (1<<H265_USERDATA_FLAG_KNEE_FUNCTION_INFO) | (1<<H265_USERDATA_FLAG_TONE_MAPPING_INFO) | (1<<H265_USERDATA_FLAG_VUI) | (1<<H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS)
													 | (1<<H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO) | (1<<H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO) | (1<<H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO) | (1<<H265_USERDATA_FLAG_PIC_TIMING)
													 | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF)
													 | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_1) | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_2) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_1) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_2);
			#else
			handle->CodecInfo.decInfo.userDataEnable = (1<<H265_USERDATA_FLAG_MASTERING_COLOR_VOL) | (1<<H265_USERDATA_FLAG_VUI) | (1<<H265_USERDATA_FLAG_PIC_TIMING)
													 | (1<<H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO) | (1<<H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS)
													 | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF)
													 | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_1) | (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_2) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_1) | (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_2);
			#endif
		}

		WAVE5_DecGiveCommand(handle, SET_ADDR_REP_USERDATA, (void*)&pInitParam->m_UserDataAddr[PA]);
		WAVE5_DecGiveCommand(handle, SET_SIZE_REP_USERDATA, (void*)&pInitParam->m_iUserDataBufferSize);
		WAVE5_DecGiveCommand(handle, SET_VIRT_ADDR_REP_USERDATA, (void*)&pInitParam->m_UserDataAddr[VA]);
	}

	//! [4] Get & Update Stream Buffer
	{
		int bitstreamMode = pInitParam->m_iFilePlayEnable;

		switch(bitstreamMode)
		{
		case 0: //Ring buffer mode
			bitstreamMode = BS_MODE_INTERRUPT;	break;
		case 1: //File play mode
		default:
			bitstreamMode = BS_MODE_PIC_END;	break;
		}

		(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode = bitstreamMode;
		(*ppVpu)->m_bFilePlayEnable = bitstreamMode;
	}

	// [5] Update Buffer aligned info. - check alied size & address of buffers (frame buffer....)
	{
		unsigned int bufAlignSize, bufStartAddrAlign;

		bufAlignSize = g_nFrameBufferAlignSize;
		bufStartAddrAlign = g_nFrameBufferAlignSize;

		if (bufAlignSize < VPU_WAVE5_HEVC_MIN_BUF_SIZE_ALIGN) {
			bufAlignSize = VPU_WAVE5_HEVC_MIN_BUF_SIZE_ALIGN;
		}

		if (bufStartAddrAlign < VPU_WAVE5_HEVC_MIN_BUF_START_ADDR_ALIGN) {
			bufStartAddrAlign = VPU_WAVE5_HEVC_MIN_BUF_START_ADDR_ALIGN;
		}

		(*ppVpu)->m_pCodecInst->bufAlignSize = bufAlignSize;
		(*ppVpu)->m_pCodecInst->bufStartAddrAlign = bufStartAddrAlign;
	}

	return ret;
}

static RetCode
WAVE5_dec_register_frame_buffer(wave5_t *pVpu, vpu_4K_D2_dec_buffer_t *pBufferParam)
{
	RetCode ret = RETCODE_SUCCESS;
	DecHandle handle = pVpu->m_pCodecInst;
	long pv_offset = 0;
	int mapType;
	//unsigned int numWTL = 0;
	int framebufStride;
	int i;
	#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
	int coreIdx=0;
	#endif
	int linearFbCount = 0;
	Uint32 format;
	FrameBuffer *pUserFrame = NULL;

	if (pBufferParam->m_iFrameBufferCount > MAX_GDI_IDX) { // if the frameBufferCount exceeds the maximum buffer count number 31..
		return RETCODE_INVALID_PARAM;
	}

	if (pBufferParam->m_iFrameBufferCount < pVpu->m_iNeedFrameBufCount) { // if the frameBufferCount exceeds the maximum buffer count number 31..
		return RETCODE_INVALID_PARAM;
	}

	if (pBufferParam->m_iFrameBufferCount > pVpu->m_iNeedFrameBufCount) {
		pVpu->m_iNeedFrameBufCount = pBufferParam->m_iFrameBufferCount;
	}

	if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
		format = (handle->CodecInfo.decInfo.openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_HEVC) {
			framebufStride  = WAVE5_ALIGN32(handle->CodecInfo.decInfo.openParam.maxwidth);
		}
		#endif
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_VP9) {
			framebufStride  = CalcStride(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), handle->CodecInfo.decInfo.openParam.maxheight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
		}
		#endif
	}
	else {
		format = (handle->CodecInfo.decInfo.initialInfo.lumaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
		#if TCC_HEVC___DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_HEVC) {
			framebufStride  = WAVE5_ALIGN32(handle->CodecInfo.decInfo.initialInfo.picWidth);
		}
		#endif
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_VP9) {
			framebufStride  = CalcStride(WAVE5_ALIGN64(handle->CodecInfo.decInfo.initialInfo.picWidth), handle->CodecInfo.decInfo.initialInfo.picHeight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
		}
		#endif
	}
	mapType = handle->CodecInfo.decInfo.mapType;

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_FrameBufferStartAddr[PA];
	pv_offset = pBufferParam->m_FrameBufferStartAddr[VA] - pBufferParam->m_FrameBufferStartAddr[PA];

	if ((handle->CodecInfo.decInfo.wtlEnable == TRUE) || (handle->CodecInfo.decInfo.enableAfbce == TRUE)) {
		linearFbCount = pVpu->m_iNeedFrameBufCount;
	}

	ret = WAVE5_DecRegisterFrameBuffer(handle, pUserFrame, pVpu->m_FrameBufferStartPAddr, pVpu->m_iNeedFrameBufCount, linearFbCount, framebufStride, handle->CodecInfo.decInfo.initialInfo.picHeight, COMPRESSED_FRAME_MAP);
	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
			WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth);
			#endif
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
		}
		return ret;
	}

	pVpu->m_iFrameBufferCount = pBufferParam->m_iFrameBufferCount;
	pVpu->m_iDecodedFrameCount = 0;

	{
		int fbnum;

		if ((handle->CodecInfo.decInfo.wtlEnable == TRUE) || (handle->CodecInfo.decInfo.enableAfbce == TRUE)) {
			fbnum = pVpu->m_iNeedFrameBufCount * 2;
		}
		else {
			fbnum = pVpu->m_iNeedFrameBufCount;
		}

		for (i = 0; i < fbnum; ++i) {
			pVpu->m_stFrameBuffer[PA][i].bufY  = handle->CodecInfo.decInfo.frameBufPool[i].bufY;
			pVpu->m_stFrameBuffer[PA][i].bufCb = handle->CodecInfo.decInfo.frameBufPool[i].bufCb;
			pVpu->m_stFrameBuffer[PA][i].bufCr = handle->CodecInfo.decInfo.frameBufPool[i].bufCr;
			pVpu->m_stFrameBuffer[PA][i].myIndex = i;
			pVpu->m_stFrameBuffer[PA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[PA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[PA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[PA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[PA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[PA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[PA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			// for output addr
			pVpu->m_stFrameBuffer[VA][i].bufY = handle->CodecInfo.decInfo.frameBufPool[i].bufY + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCb = handle->CodecInfo.decInfo.frameBufPool[i].bufCb + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCr = handle->CodecInfo.decInfo.frameBufPool[i].bufCr + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].myIndex = i;
			pVpu->m_stFrameBuffer[VA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[VA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[VA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[VA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[VA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[VA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[VA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			handle->CodecInfo.decInfo.vbFbcYTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcYTbl[i].phys_addr + pv_offset;
			handle->CodecInfo.decInfo.vbFbcCTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcCTbl[i].phys_addr + pv_offset;
		}
	}

	return ret;
}

static RetCode
WAVE5_dec_register_frame_buffer_FullFB_SC(wave5_t *pVpu)
{
	RetCode ret = RETCODE_SUCCESS;
	DecHandle handle = pVpu->m_pCodecInst;
	long pv_offset = 0;
	int mapType;
	//unsigned int numWTL = 0;
	int framebufStride;
	int i;
	#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
	int coreIdx=0;
	#endif
	int linearFbCount = 0;
	Uint32 format;
	FrameBuffer *pUserFrame = NULL;

	if (pVpu->m_iNeedFrameBufCount > MAX_GDI_IDX) { // if the frameBufferCount exceeds the maximum buffer count number 31..
		return RETCODE_INVALID_PARAM;
	}

	format = (handle->CodecInfo.decInfo.openParam.maxbit > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (pVpu->m_iBitstreamFormat == STD_HEVC) {
		framebufStride  = WAVE5_ALIGN32(handle->CodecInfo.decInfo.openParam.maxwidth);
	}
	#endif
	#if TCC_VP9____DEC_SUPPORT == 1
	if (pVpu->m_iBitstreamFormat == STD_VP9) {
		framebufStride  = CalcStride(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), handle->CodecInfo.decInfo.openParam.maxheight, format, handle->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE);
	}
	#endif

	mapType = handle->CodecInfo.decInfo.mapType;

	if ((handle->CodecInfo.decInfo.wtlEnable == TRUE) || (handle->CodecInfo.decInfo.enableAfbce == TRUE)) {
		linearFbCount = pVpu->m_iNeedFrameBufCount;
	}

	ret = WAVE5_DecRegisterFrameBuffer(handle, pUserFrame, pVpu->m_FrameBufferStartPAddr, pVpu->m_iNeedFrameBufCount, linearFbCount, framebufStride, handle->CodecInfo.decInfo.initialInfo.picHeight, COMPRESSED_FRAME_MAP);
	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
			WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth, handle->CodecInfo.decInfo.openParam.fw_writing_disable);
			#endif
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
		}
		return ret;
	}

	pVpu->m_iDecodedFrameCount = 0;

	{
		int fbnum;

		if ((handle->CodecInfo.decInfo.wtlEnable == TRUE) || (handle->CodecInfo.decInfo.enableAfbce == TRUE)) {
			fbnum = pVpu->m_iNeedFrameBufCount * 2;
		}
		else {
			fbnum = pVpu->m_iNeedFrameBufCount;
		}

		for (i = 0; i < fbnum; ++i) {
			pVpu->m_stFrameBuffer[PA][i].bufY  = handle->CodecInfo.decInfo.frameBufPool[i].bufY;
			pVpu->m_stFrameBuffer[PA][i].bufCb = handle->CodecInfo.decInfo.frameBufPool[i].bufCb;
			pVpu->m_stFrameBuffer[PA][i].bufCr = handle->CodecInfo.decInfo.frameBufPool[i].bufCr;
			pVpu->m_stFrameBuffer[PA][i].myIndex = i;
			pVpu->m_stFrameBuffer[PA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[PA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[PA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[PA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[PA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[PA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[PA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			// for output addr
			pVpu->m_stFrameBuffer[VA][i].bufY		= handle->CodecInfo.decInfo.frameBufPool[i].bufY + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCb		= handle->CodecInfo.decInfo.frameBufPool[i].bufCb + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCr		= handle->CodecInfo.decInfo.frameBufPool[i].bufCr + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].myIndex = i;
			pVpu->m_stFrameBuffer[VA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[VA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[VA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[VA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[VA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[VA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[VA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			handle->CodecInfo.decInfo.vbFbcYTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcYTbl[i].phys_addr + pv_offset;
			handle->CodecInfo.decInfo.vbFbcCTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcCTbl[i].phys_addr + pv_offset;
		}
	}

	return ret;
}

#ifdef ADD_FBREG_2
static RetCode
WAVE5_dec_register_frame_buffer2( wave5_t *pVpu, vpu_4K_D2_dec_buffer2_t *pBufferParam )
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	long pv_offset = 0;
	int mapType;
	unsigned int numWTL = 0;
	int framebufStride, stride;
	int i;
	#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
	int coreIdx=0;
	#endif
	int framebufHeight;
	FrameBuffer *pUserFrame = NULL;

	if (pBufferParam->m_iFrameBufferCount > MAX_GDI_IDX) { // if the frameBufferCount exceeds the maximum buffer count number 31..
		return RETCODE_INVALID_PARAM;
	}

	if (pBufferParam->m_iFrameBufferCount < pVpu->m_iNeedFrameBufCount) { // if the frameBufferCount exceeds the maximum buffer count number 31..
		return RETCODE_INVALID_PARAM;
	}

	handle->CodecInfo.decInfo.openParam.bufferRegMode = 2;

	if (pBufferParam->m_iFrameBufferCount > pVpu->m_iNeedFrameBufCount) {
		pVpu->m_iNeedFrameBufCount = pBufferParam->m_iFrameBufferCount;
	}

	if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
		stride  = WAVE5_ALIGN32(handle->CodecInfo.decInfo.openParam.maxwidth);
		framebufHeight = WAVE5_ALIGN16(handle->CodecInfo.decInfo.openParam.maxheight);
		framebufStride = stride;
	}
	else {
		stride  = WAVE5_ALIGN32(handle->CodecInfo.decInfo.initialInfo.picWidth);
		framebufHeight = WAVE5_ALIGN16(handle->CodecInfo.decInfo.initialInfo.picHeight);
		framebufStride = stride;
	}
	mapType = handle->CodecInfo.decInfo.mapType;

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_addrFrameBuffer[PA][0];
	pv_offset = pBufferParam->m_addrFrameBuffer[VA][0] - pBufferParam->m_addrFrameBuffer[PA][0];

	if (handle->CodecInfo.decInfo.wtlEnable == TRUE) {
		numWTL = pVpu->m_iNeedFrameBufCount;
	}

	ret = WAVE5_DecRegisterFrameBuffer2(handle, pUserFrame, pBufferParam->m_addrFrameBuffer[PA], pVpu->m_iNeedFrameBufCount, numWTL, stride, framebufHeight, mapType);
	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
			WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth);
			#endif
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
		}
		return ret;
	}

	pVpu->m_iFrameBufferCount = pBufferParam->m_iFrameBufferCount;
	pVpu->m_iDecodedFrameCount = 0;

	{
		int fbnum;

		if (handle->CodecInfo.decInfo.wtlEnable) {
			fbnum = pVpu->m_iNeedFrameBufCount * 2;
		}
		else {
			fbnum = pVpu->m_iNeedFrameBufCount;
		}
		for (i = 0; i < fbnum; ++i) {
			pVpu->m_stFrameBuffer[PA][i].bufY  = handle->CodecInfo.decInfo.frameBufPool[i].bufY;
			pVpu->m_stFrameBuffer[PA][i].bufCb = handle->CodecInfo.decInfo.frameBufPool[i].bufCb;
			pVpu->m_stFrameBuffer[PA][i].bufCr = handle->CodecInfo.decInfo.frameBufPool[i].bufCr;
			pVpu->m_stFrameBuffer[PA][i].myIndex = i;
			pVpu->m_stFrameBuffer[PA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[PA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[PA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[PA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[PA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[PA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[PA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[PA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			// for output addr
			pVpu->m_stFrameBuffer[VA][i].bufY  = handle->CodecInfo.decInfo.frameBufPool[i].bufY + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCb = handle->CodecInfo.decInfo.frameBufPool[i].bufCb + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].bufCr = handle->CodecInfo.decInfo.frameBufPool[i].bufCr + pv_offset;
			pVpu->m_stFrameBuffer[VA][i].myIndex = i;
			pVpu->m_stFrameBuffer[VA][i].cbcrInterleave = handle->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
			pVpu->m_stFrameBuffer[VA][i].nv21 = handle->CodecInfo.decInfo.frameBufPool[i].nv21;
			pVpu->m_stFrameBuffer[VA][i].endian = handle->CodecInfo.decInfo.frameBufPool[i].endian;
			pVpu->m_stFrameBuffer[VA][i].mapType = handle->CodecInfo.decInfo.frameBufPool[i].mapType;
			pVpu->m_stFrameBuffer[VA][i].stride = handle->CodecInfo.decInfo.frameBufPool[i].stride;
			pVpu->m_stFrameBuffer[VA][i].height = handle->CodecInfo.decInfo.frameBufPool[i].height;
			pVpu->m_stFrameBuffer[VA][i].lumaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].chromaBitDepth = handle->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
			pVpu->m_stFrameBuffer[VA][i].format = handle->CodecInfo.decInfo.frameBufPool[i].format;

			handle->CodecInfo.decInfo.vbFbcYTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcYTbl[i].phys_addr + pv_offset;
			handle->CodecInfo.decInfo.vbFbcCTbl[i].virt_addr = handle->CodecInfo.decInfo.vbFbcCTbl[i].phys_addr + pv_offset;
		}
	}

	return ret;
}
#endif

#ifdef ADD_FBREG_3
static RetCode
WAVE5_dec_register_frame_buffer3(wave5_t *pVpu, vpu_4K_D2_dec_buffer3_t *pBufferParam)
{
	RetCode ret = RETCODE_SUCCESS;
	DecHandle handle;
	int mapType;
	int framebufStride;
	int i;
	Uint32 format;
	int bitperdepth;
	int pic_width;
	int pic_height;
	int is_wtl_mode;
	int framebuffer_cnt = 0;
	int result_fbnum;

	if ((pVpu == NULL) || (pBufferParam == NULL))
		return RETCODE_INVALID_PARAM;

	handle = pVpu->m_pCodecInst;
	is_wtl_mode = (pVpu->m_pCodecInst->CodecInfo.decInfo.wtlEnable == TRUE) || (pVpu->m_pCodecInst->CodecInfo.decInfo.enableAfbce == TRUE);

	if (is_wtl_mode != 1) {
		if (pBufferParam->m_iFrameBufferCount > MAX_GDI_IDX) {
			return RETCODE_INVALID_PARAM;
		}

		if (pBufferParam->m_iFrameBufferCount < pVpu->m_iNeedFrameBufCount) {
			return RETCODE_INVALID_PARAM;
		}

		framebuffer_cnt = pBufferParam->m_iFrameBufferCount;
	}
	else {
		//if the output is linear, the required number of buffers doubles
		if (pBufferParam->m_iFrameBufferCount > (MAX_GDI_IDX * 2)) {
			return RETCODE_INVALID_PARAM;
		}

		if (pBufferParam->m_iFrameBufferCount < (pVpu->m_iNeedFrameBufCount * 2)) {
			return RETCODE_INVALID_PARAM;
		}

		if (pBufferParam->m_iFrameBufferCount % 2 != 0) {
			DLOGE("WTL mode is enabled, it must be an even number. m_iFrameBufferCount:%d", pBufferParam->m_iFrameBufferCount);
			return RETCODE_INVALID_PARAM;
		}

		framebuffer_cnt = pBufferParam->m_iFrameBufferCount / 2;
	}

	if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.FullSizeFB) {
		bitperdepth = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.maxbit;
		pic_width = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.maxwidth;
		pic_height = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.maxheight;
	}
	else {
		bitperdepth = pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.lumaBitdepth;
		pic_width = pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picWidth;
		pic_height = pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picHeight;
	}
	format = (bitperdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
	framebufStride = (pVpu->m_iBitstreamFormat == STD_VP9) ?
		CalcStride(WAVE5_ALIGN64(pic_width), pic_height, format, pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave, COMPRESSED_FRAME_MAP, TRUE) :
		WAVE5_ALIGN32(pic_width);

	mapType = pVpu->m_pCodecInst->CodecInfo.decInfo.mapType;

	ret = WAVE5_DecRegisterFrameBuffer3(handle, (codec_addr_t (*)[9])(pBufferParam->m_addrFrameBuffer[PA]),
							is_wtl_mode ? framebuffer_cnt : 0, framebuffer_cnt,
							framebufStride, pic_height, COMPRESSED_FRAME_MAP);

	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			DLOGE("[DBG_W5][%s-%d] exited since WAVE5_DecRegisterFrameBuffer\n", __func__, __LINE__);
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
		}
		return ret;
	}

	pVpu->m_FrameBufferStartPAddr = NULL;
	pVpu->m_iFrameBufferCount = framebuffer_cnt;
	pVpu->m_iDecodedFrameCount = 0;
	pVpu->m_iNeedFrameBufCount = pBufferParam->m_iFrameBufferCount;

	result_fbnum = (is_wtl_mode ? framebuffer_cnt * 2 : framebuffer_cnt);
	for(i = 0; i < result_fbnum; ++i) {
		pVpu->m_stFrameBuffer[PA][i].bufY  = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].bufY;
		pVpu->m_stFrameBuffer[PA][i].bufCb = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].bufCb;
		pVpu->m_stFrameBuffer[PA][i].bufCr = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].bufCr;
		pVpu->m_stFrameBuffer[PA][i].myIndex = i;
		pVpu->m_stFrameBuffer[PA][i].cbcrInterleave = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].cbcrInterleave;
		pVpu->m_stFrameBuffer[PA][i].nv21 = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].nv21;
		pVpu->m_stFrameBuffer[PA][i].endian = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].endian;
		pVpu->m_stFrameBuffer[PA][i].mapType = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].mapType;
		pVpu->m_stFrameBuffer[PA][i].stride = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].stride;
		pVpu->m_stFrameBuffer[PA][i].height = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].height;
		pVpu->m_stFrameBuffer[PA][i].lumaBitDepth = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].lumaBitDepth;
		pVpu->m_stFrameBuffer[PA][i].chromaBitDepth = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].chromaBitDepth;
		pVpu->m_stFrameBuffer[PA][i].format = pVpu->m_pCodecInst->CodecInfo.decInfo.frameBufPool[i].format;
	}
	return ret;
}
#endif

static void
GetDecUserData(wave5_t *pVpu, vpu_4K_D2_dec_output_t *pOutParam, DecOutputInfo info)
{
	DecHandle handle = pVpu->m_pCodecInst;
	user_data_entry_t *pEntry;
	Uint8 *pBase;
	Int32 coreIdx = pVpu->m_pCodecInst->coreIdx;
	Uint16 userdataNum = 0;
	//Uint32 userdataSize = 0;
	Uint16 userdataTotalSize = 0;
	Uint32 t35_pre = 0;
	Uint32 t35_suf = 0;
	Uint32 t35_pre_1 = 0;
	Uint32 t35_pre_2 = 0;
	Uint32 t35_suf_1 = 0;
	Uint32 t35_suf_2 = 0;

	pBase = (Uint8 *)handle->CodecInfo.decInfo.vbUserData.virt_addr;
	pEntry = (user_data_entry_t*)pBase;

	pOutParam->m_DecOutInfo.m_uiUserData = 0;

	WAVE5_swap_endian(coreIdx, pBase, 8*18, VDI_LITTLE_ENDIAN);

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_MASTERING_COLOR_VOL)) {
		h265_mastering_display_colour_volume_t *mastering;
		int i;

		mastering = (h265_mastering_display_colour_volume_t *)(pBase + pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].offset, pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.min_display_mastering_luminance = mastering->min_display_mastering_luminance;

		for (i = 0 ; i < 3 ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.display_primaries_x[i] = mastering->display_primaries_x[i];
		}

		for (i = 0 ; i < 3 ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.display_primaries_y[i] = mastering->display_primaries_y[i];
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.white_point_x = mastering->white_point_x;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.white_point_y = mastering->white_point_y;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_MasteringDisplayColorVolume.max_display_mastering_luminance = mastering->max_display_mastering_luminance;
	}

	if(info.decOutputExtData.userDataHeader&(1<<H265_USERDATA_FLAG_VUI)) {
		h265_vui_param_t* vui;
		int i, j;

		vui = (h265_vui_param_t*)(pBase + pEntry[H265_USERDATA_FLAG_VUI].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_VUI].offset, pEntry[H265_USERDATA_FLAG_VUI].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.aspect_ratio_info_present_flag = vui->aspect_ratio_info_present_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.aspect_ratio_info_present_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.aspect_ratio_idc = vui->aspect_ratio_idc;
			if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.aspect_ratio_idc == 255) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.sar_width = vui->sar_width;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.sar_height = vui->sar_height;
			}
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.overscan_info_present_flag = vui->overscan_info_present_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.overscan_appropriate_flag = vui->overscan_appropriate_flag;

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.video_signal_type_present_flag = vui->video_signal_type_present_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.video_signal_type_present_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.video_format = vui->video_format;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.video_full_range_flag = vui->video_full_range_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.colour_description_present_flag = vui->colour_description_present_flag;
			if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.colour_description_present_flag) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.colour_primaries = vui->colour_primaries;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.transfer_characteristics = vui->transfer_characteristics;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.matrix_coefficients = vui->matrix_coefficients;
			}
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.chroma_loc_info_present_flag = vui->chroma_loc_info_present_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.chroma_loc_info_present_flag ) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.chroma_sample_loc_type_top_field = vui->chroma_sample_loc_type_top_field;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.chroma_sample_loc_type_bottom_field = vui->chroma_sample_loc_type_bottom_field;
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.neutral_chroma_indication_flag = vui->neutral_chroma_indication_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.field_seq_flag = vui->field_seq_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.frame_field_info_present_flag = vui->frame_field_info_present_flag;

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.default_display_window_flag = vui->default_display_window_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.default_display_window_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.def_disp_win.sBottom = vui->def_disp_win.bottom;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.def_disp_win.sLeft = vui->def_disp_win.left;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.def_disp_win.sRight = vui->def_disp_win.right;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.def_disp_win.sTop = vui->def_disp_win.top;
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_timing_info_present_flag = vui->vui_timing_info_present_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_timing_info_present_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_num_units_in_tick = vui->vui_num_units_in_tick;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_poc_proportional_to_timing_flag = vui->vui_poc_proportional_to_timing_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_num_ticks_poc_diff_one_minus1 = vui->vui_num_ticks_poc_diff_one_minus1;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_time_scale = vui->vui_time_scale;
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_hrd_parameters_present_flag = vui->vui_hrd_parameters_present_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.vui_hrd_parameters_present_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_hrd_param_present_flag = vui->hrd_param.nal_hrd_param_present_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_hrd_param_present_flag = vui->hrd_param.vcl_hrd_param_present_flag;

			if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_hrd_param_present_flag || pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_hrd_param_present_flag) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_hrd_params_present_flag = vui->hrd_param.sub_pic_hrd_params_present_flag;
				if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_hrd_params_present_flag) {
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.tick_divisor_minus2 = vui->hrd_param.tick_divisor_minus2;
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.du_cpb_removal_delay_inc_length_minus1 = vui->hrd_param.du_cpb_removal_delay_inc_length_minus1;
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.sub_pic_cpb_params_in_pic_timing_sei_flag = vui->hrd_param.sub_pic_cpb_params_in_pic_timing_sei_flag;
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.dpb_output_delay_du_length_minus1 = vui->hrd_param.dpb_output_delay_du_length_minus1;
				}
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.bit_rate_scale = vui->hrd_param.bit_rate_scale;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.cpb_size_scale = vui->hrd_param.cpb_size_scale;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.initial_cpb_removal_delay_length_minus1 = vui->hrd_param.initial_cpb_removal_delay_length_minus1;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.cpb_removal_delay_length_minus1 = vui->hrd_param.cpb_removal_delay_length_minus1;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.dpb_output_delay_length_minus1 = vui->hrd_param.dpb_output_delay_length_minus1;
			}

			for (i = 0 ; i < H265_MAX_NUM_SUB_LAYER ; i++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_gen_flag[i] = vui->hrd_param.fixed_pic_rate_gen_flag[i];
				if (!pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_gen_flag[i])
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.fixed_pic_rate_within_cvs_flag[i] = vui->hrd_param.fixed_pic_rate_within_cvs_flag[i];

				if (vui->hrd_param.fixed_pic_rate_within_cvs_flag[i]) {
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.elemental_duration_in_tc_minus1[i] = vui->hrd_param.elemental_duration_in_tc_minus1[i];
				}
				else {
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.low_delay_hrd_flag[i] = vui->hrd_param.low_delay_hrd_flag[i];
				}

				if (!vui->hrd_param.low_delay_hrd_flag[i]) {
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.cpb_cnt_minus1[i] = vui->hrd_param.cpb_cnt_minus1[i];
				}

				if (vui->hrd_param.nal_hrd_param_present_flag) {
					for (j = 0 ; j < H265_MAX_CPB_CNT ; j++)
					{
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_bit_rate_value_minus1[i][j] = vui->hrd_param.nal_bit_rate_value_minus1[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_cpb_size_value_minus1[i][j] = vui->hrd_param.nal_cpb_size_value_minus1[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_cbr_flag[i][j] = vui->hrd_param.nal_cbr_flag[i][j];
					}

					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_cpb_size_du_value_minus1[i] = vui->hrd_param.nal_cpb_size_du_value_minus1[i];
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.nal_bit_rate_du_value_minus1[i] = vui->hrd_param.nal_bit_rate_du_value_minus1[i];
				}

				if (vui->hrd_param.vcl_hrd_param_present_flag) {
					for (j = 0 ; j < H265_MAX_CPB_CNT ; j++) {
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_bit_rate_value_minus1[i][j] = vui->hrd_param.vcl_bit_rate_value_minus1[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_cpb_size_value_minus1[i][j] = vui->hrd_param.vcl_cpb_size_value_minus1[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_cbr_flag[i][j] = vui->hrd_param.vcl_cbr_flag[i][j];
					}

					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_cpb_size_du_value_minus1[i] = vui->hrd_param.vcl_cpb_size_du_value_minus1[i];
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.hrd_param.vcl_bit_rate_du_value_minus1[i] = vui->hrd_param.vcl_bit_rate_du_value_minus1[i];
				}
			}
		}

		pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.bitstream_restriction_flag = vui->bitstream_restriction_flag;
		if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.bitstream_restriction_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.tiles_fixed_structure_flag = vui->tiles_fixed_structure_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.motion_vectors_over_pic_boundaries_flag = vui->motion_vectors_over_pic_boundaries_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.restricted_ref_pic_lists_flag = vui->restricted_ref_pic_lists_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.min_spatial_segmentation_idc = vui->min_spatial_segmentation_idc;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.max_bytes_per_pic_denom = vui->max_bytes_per_pic_denom;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.max_bits_per_mincu_denom = vui->max_bits_per_mincu_denom;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.log2_max_mv_length_horizontal = vui->log2_max_mv_length_horizontal;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_VuiParam.log2_max_mv_length_vertical = vui->log2_max_mv_length_vertical;
		}
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO))  {
		h265_content_light_level_info_t *content_light_level;

		content_light_level = (h265_content_light_level_info_t*)(pBase + pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].offset, pEntry[H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ContentLightLevelInfo.max_content_light_level = content_light_level->max_content_light_level;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ContentLightLevelInfo.max_pic_average_light_level = content_light_level->max_pic_average_light_level;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS)) {
		h265_alternative_transfer_characteristics_info_t *alternative_transfer_characteristics;

		alternative_transfer_characteristics = (h265_alternative_transfer_characteristics_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].offset, pEntry[H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_AlternativeTransferCharacteristicsInfo.preferred_transfer_characteristics = alternative_transfer_characteristics->preferred_transfer_characteristics;
	}

	#ifndef USER_DATA_SKIP
	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT)) {
		h265_chroma_resampling_filter_hint_t *c_resampleing_filter_hint;
		int i,j;

		c_resampleing_filter_hint = (h265_chroma_resampling_filter_hint_t*)(pBase + pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].offset, pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.ver_chroma_filter_idc = c_resampleing_filter_hint->ver_chroma_filter_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.hor_chroma_filter_idc = c_resampleing_filter_hint->hor_chroma_filter_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.ver_filtering_field_processing_flag = c_resampleing_filter_hint->ver_filtering_field_processing_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.target_format_idc = c_resampleing_filter_hint->target_format_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.num_vertical_filters = c_resampleing_filter_hint->num_vertical_filters;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.num_horizontal_filters = c_resampleing_filter_hint->num_horizontal_filters;
		for (i = 0 ; i < H265_MAX_NUM_VERTICAL_FILTERS ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.ver_tap_length_minus1[i] = c_resampleing_filter_hint->ver_tap_length_minus1[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_HORIZONTAL_FILTERS ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.hor_tap_length_minus1[i] = c_resampleing_filter_hint->hor_tap_length_minus1[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_VERTICAL_FILTERS ; i++) {
			for (j = 0 ; j < H265_MAX_TAP_LENGTH ; j++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.ver_filter_coeff[i][j] = c_resampleing_filter_hint->ver_filter_coeff[i][j];
			}
		}
		for (i = 0 ; i < H265_MAX_NUM_HORIZONTAL_FILTERS ; i++) {
			for (j = 0 ; j < H265_MAX_TAP_LENGTH ; j++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ChromaResamplingFilterHint.hor_filter_coeff[i][j] = c_resampleing_filter_hint->hor_filter_coeff[i][j];
			}
		}
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_KNEE_FUNCTION_INFO)) {
		h265_knee_function_info_t *knee_function;
		int i;

		knee_function = (h265_knee_function_info_t *)(pBase + pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].offset, pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.knee_function_id = knee_function->knee_function_id;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.knee_function_cancel_flag = knee_function->knee_function_cancel_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.knee_function_persistence_flag = knee_function->knee_function_persistence_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.input_disp_luminance = knee_function->input_disp_luminance;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.input_d_range = knee_function->input_d_range;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.output_d_range = knee_function->output_d_range;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.output_disp_luminance = knee_function->output_disp_luminance;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.num_knee_points_minus1 = knee_function->num_knee_points_minus1;
		for (i = 0 ; i < H265_MAX_NUM_KNEE_POINT ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.input_knee_point[i] = knee_function->input_knee_point[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_KNEE_POINT ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_KneeFunctionInfo.output_knee_point[i] = knee_function->output_knee_point[i];
		}
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_TONE_MAPPING_INFO)) {
		h265_tone_mapping_info_t* tone_mapping;
		int i;

		tone_mapping = (h265_tone_mapping_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].offset, pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.tone_map_id = tone_mapping->tone_map_id;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.tone_map_cancel_flag = tone_mapping->tone_map_cancel_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.tone_map_persistence_flag = tone_mapping->tone_map_persistence_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.coded_data_bit_depth = tone_mapping->coded_data_bit_depth;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.target_bit_depth = tone_mapping->target_bit_depth;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.tone_map_model_id = tone_mapping->tone_map_model_id;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.min_value = tone_mapping->min_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.max_value = tone_mapping->max_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.sigmoid_midpoint = tone_mapping->sigmoid_midpoint;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.sigmoid_width = tone_mapping->sigmoid_width;
		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.start_of_coded_interval[i] = tone_mapping->start_of_coded_interval[i];
		}
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.num_pivots = tone_mapping->num_pivots;
		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.coded_pivot_value[i] = tone_mapping->coded_pivot_value[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_TONE_VALUE ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.target_pivot_value[i] = tone_mapping->target_pivot_value[i];
		}
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.camera_iso_speed_idc = tone_mapping->camera_iso_speed_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.camera_iso_speed_value = tone_mapping->camera_iso_speed_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.exposure_index_idc = tone_mapping->exposure_index_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.exposure_index_value = tone_mapping->exposure_index_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_sign_flag = tone_mapping->exposure_compensation_value_sign_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_numerator = tone_mapping->exposure_compensation_value_numerator;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.exposure_compensation_value_denom_idc = tone_mapping->exposure_compensation_value_denom_idc;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.ref_screen_luminance_white = tone_mapping->ref_screen_luminance_white;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.extended_range_white_level = tone_mapping->extended_range_white_level;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.nominal_black_level_code_value = tone_mapping->nominal_black_level_code_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.nominal_white_level_code_value = tone_mapping->nominal_white_level_code_value;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ToneMappingInfo.extended_white_level_code_value = tone_mapping->extended_white_level_code_value;
	}
	#endif

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_PIC_TIMING)) {
		h265_sei_pic_timing_t *pic_timing;

		pic_timing = (h265_sei_pic_timing_t*)(pBase + pEntry[H265_USERDATA_FLAG_PIC_TIMING].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_PIC_TIMING].offset, pEntry[H265_USERDATA_FLAG_PIC_TIMING].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_SeiPicTiming.status = pic_timing->status;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_SeiPicTiming.pic_struct = pic_timing->pic_struct;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_SeiPicTiming.source_scan_type = pic_timing->source_scan_type;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_SeiPicTiming.duplicate_flag = pic_timing->duplicate_flag;
	}

	#ifndef USER_DATA_SKIP
	if (info.decOutputExtData.userDataHeader & (1<<H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO)) {
		h265_film_grain_characteristics_t *film_grain_characteristics;
		int i, j, k;

		film_grain_characteristics = (h265_film_grain_characteristics_t*)(pBase + pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].offset, pEntry[H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_characteristics_cancel_flag = film_grain_characteristics->film_grain_characteristics_cancel_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_model_id = film_grain_characteristics->film_grain_model_id;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.separate_colour_description_present_flag = film_grain_characteristics->separate_colour_description_present_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_bit_depth_luma_minus8 = film_grain_characteristics->film_grain_bit_depth_luma_minus8;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_bit_depth_chroma_minus8 = film_grain_characteristics->film_grain_bit_depth_chroma_minus8;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_full_range_flag = film_grain_characteristics->film_grain_full_range_flag;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_colour_primaries = film_grain_characteristics->film_grain_colour_primaries;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_transfer_characteristics = film_grain_characteristics->film_grain_transfer_characteristics;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_matrix_coeffs = film_grain_characteristics->film_grain_matrix_coeffs;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.blending_mode_id = film_grain_characteristics->blending_mode_id;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.log2_scale_factor = film_grain_characteristics->log2_scale_factor;
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.comp_model_present_flag[i] = film_grain_characteristics->comp_model_present_flag[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.num_intensity_intervals_minus1[i] = film_grain_characteristics->num_intensity_intervals_minus1[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.num_model_values_minus1[i] = film_grain_characteristics->num_model_values_minus1[i];
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.intensity_interval_lower_bound[i][j] = film_grain_characteristics->intensity_interval_lower_bound[i][j];
				}
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.intensity_interval_upper_bound[i][j] = film_grain_characteristics->intensity_interval_upper_bound[i][j];
				}
		}
		for (i = 0 ; i < H265_MAX_NUM_FILM_GRAIN_COMPONENT ; i++) {
			for (j = 0 ; j < H265_MAX_NUM_INTENSITY_INTERVALS ; j++) {
				for (k = 0 ; k < H265_MAX_NUM_MODEL_VALUES ; k++) {
					pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.comp_model_value[i][j][k] = film_grain_characteristics->comp_model_value[i][j][k];
				}
			}
		}
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_FilmGrainCharInfo.film_grain_characteristics_persistence_flag = film_grain_characteristics->film_grain_characteristics_persistence_flag;
	}
	#endif

	#ifndef USER_DATA_SKIP
	if (info.decOutputExtData.userDataHeader & (1<<H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO)) {
		h265_colour_remapping_info_t *colour_remapping;
		int i, j, k;

		colour_remapping = (h265_colour_remapping_info_t*)(pBase + pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].offset);
		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].offset, pEntry[H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO].size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_cancel_flag = colour_remapping->colour_remap_cancel_flag;

		if (!pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_cancel_flag) {
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_persistence_flag = colour_remapping->colour_remap_persistence_flag;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_video_signal_info_present_flag = colour_remapping->colour_remap_video_signal_info_present_flag;

			if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_video_signal_info_present_flag) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_full_range_flag = colour_remapping->colour_remap_full_range_flag;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_primaries = colour_remapping->colour_remap_primaries;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_transfer_function = colour_remapping->colour_remap_transfer_function;
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_coefficients = colour_remapping->colour_remap_matrix_coefficients;
			}
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_input_bit_depth = colour_remapping->colour_remap_input_bit_depth;
			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_bit_depth = colour_remapping->colour_remap_bit_depth;

			for (i = 0 ; i < H265_MAX_LUT_NUM_VAL ; i++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i] = colour_remapping->pre_lut_num_val_minus1[i];
				if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i] > 0) {
					for (j = 0; j <= pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.pre_lut_num_val_minus1[i]; j++) {
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.pre_lut_coded_value[i][j] = colour_remapping->pre_lut_coded_value[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.pre_lut_target_value[i][j] = colour_remapping->pre_lut_target_value[i][j];
					}
				}
			}

			pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_present_flag = colour_remapping->colour_remap_matrix_present_flag;
			if (pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_matrix_present_flag) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.log2_matrix_denom = colour_remapping->log2_matrix_denom;
				for (i = 0; i < H265_MAX_COLOUR_REMAP_COEFFS; i++) {
					for (j = 0; j < H265_MAX_COLOUR_REMAP_COEFFS; j++) {
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.colour_remap_coeffs[i][j] = colour_remapping->colour_remap_coeffs[i][j];
					}
				}
			}

			for (i = 0; i < H265_MAX_LUT_NUM_VAL; i++) {
				pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i] = colour_remapping->post_lut_num_val_minus1[i];
				if(pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i] > 0) {
					for( j = 0; j <= pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.post_lut_num_val_minus1[i]; j++) {
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.post_lut_coded_value[i][j] = colour_remapping->post_lut_coded_value[i][j];
						pOutParam->m_DecOutInfo.m_UserDataInfo.m_ColourRemappingInfo.post_lut_target_value[i][j] = colour_remapping->post_lut_target_value[i][j];
					}
				}
			}
		}
	}
	#endif

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE)) {
		int pre_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset, pre_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_pre = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF)) {
		int suf_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].offset, suf_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_suf = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_1)) {
		int pre_1_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset, pre_1_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_pre_1 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_PRE_2)) {
		int pre_2_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset, pre_2_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_pre_2 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_1)) {
		int suf_1_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].offset, suf_1_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_suf_1 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size;
	}

	if (info.decOutputExtData.userDataHeader & (1<<H265_USERDATA_FLAG_ITU_T_T35_SUF_2)) {
		int suf_2_size = ( (pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size + 15) / 16 ) * 16;

		WAVE5_swap_endian(coreIdx, pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].offset, suf_2_size, VDI_LITTLE_ENDIAN);
		pOutParam->m_DecOutInfo.m_uiUserData = 1;
		userdataNum++;
		t35_suf_2 = 1;
		userdataTotalSize += pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size;
	}

	if (userdataNum > 0) {
		Uint32 offset_user = 0;

		wave5_memcpy(pBase, &userdataNum, sizeof(Uint16), 2);
		wave5_memcpy(pBase+2, &userdataTotalSize, sizeof(Uint16), 2);

		if (t35_pre) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_pre_1) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_pre_2) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf_1) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_1].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}

		if (t35_suf_2) {
			wave5_memcpy(pBase+8+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].size, sizeof(Uint32), 1);
			wave5_memcpy(pBase+8+4+offset_user, &pEntry[H265_USERDATA_FLAG_ITU_T_T35_SUF_2].offset, sizeof(Uint32), 1);
			offset_user += 8;
		}
	}
}

void SetAfbceFrameInfo( AfbcFrameInfo* frameinfo, DecOutputInfo* decodedInfo, Uint8 topCrop )
{
	Int32   i;
	afbc_block4x4_order blkorder[20] =
	{{0,4,4},  {0,0,4},  {1,0,0}, {0,0,0},   {0,4,0},
	{0,8,0},  {0,12,0}, {1,4,0}, {0,12,4},  {0,8,4},
	{0,8,8},  {0,12,8}, {1,4,4}, {0,12,12}, {0,8,12},
	{0,4,12}, {0,0,12}, {1,0,4}, {0,0,8},   {0,4,8}};

	frameinfo->version = AFBC_VERSION;
	frameinfo->width   = decodedInfo->dispPicWidth;
	frameinfo->height = decodedInfo->dispPicHeight;
	frameinfo->left_crop=0;
	frameinfo->top_crop=topCrop;
	frameinfo->mbw=(frameinfo->left_crop+frameinfo->width+15)>>4;
	frameinfo->mbh=(frameinfo->top_crop+frameinfo->height+15)>>4;
	frameinfo->subsampling = AFBC_SUBSAMPLING_420;
	frameinfo->yuv_transform = 0;
	frameinfo->sbs_multiplier[0] = 1;
	frameinfo->sbs_multiplier[1] = 1;
	frameinfo->inputbits[0] = decodedInfo->dispFrame.lumaBitDepth;
	frameinfo->inputbits[1] = frameinfo->inputbits[2] = decodedInfo->dispFrame.chromaBitDepth;
	frameinfo->nsubblocks = 20; //16 luma + 4 chroma
	//frameinfo->nsubblocks = 20; // 16 luma + 4 chroma
	frameinfo->nplanes=2;          // luma and chroma
	frameinfo->ncomponents[0]=1;   // Y
	frameinfo->ncomponents[1]=2;   // UV
	frameinfo->first_component[0]=0;     // Y
	frameinfo->first_component[1]=1;     // U
	frameinfo->body_base_ptr_bits=28;
	frameinfo->subblock_size_bits=5;
	frameinfo->uncompressed_size[0]=(16*frameinfo->inputbits[0]+7)>>3;
	frameinfo->uncompressed_size[1]=(16*(frameinfo->inputbits[1]+frameinfo->inputbits[2])+7)>>3;
	//frameinfo->sbs_multiplier[0] = 1;
	if (frameinfo->inputbits[1] <= 8) {
		frameinfo->sbs_multiplier[1] = 1;
	}
	else {
		frameinfo->sbs_multiplier[1] = 2;
	}
	for (i = 0; i < 3; i++) {
		frameinfo->compbits[i]= frameinfo->inputbits[i];
	}
	frameinfo->subblock_order = blkorder;
	frameinfo->file_message  = 0;
	frameinfo->format = decodedInfo->dispFrame.format;
}

void GetAfbceData(DecHandle handle, DecOutputInfo *decodedInfo, VpuRect rcDisplay, Uint8 lfEnableflag,
					Uint8 *pAfbce, AfbcFrameInfo *afbcframeinfo, Int32 compareType)
{
	//EndianMode endian;
	Uint8    topCrop;

	//endian = (EndianMode)decodedInfo->dispFrame.endian;
	topCrop = lfEnableflag ? 4 : 0;
	SetAfbceFrameInfo(afbcframeinfo, decodedInfo, topCrop);
}


#ifdef TEST_OUTPUTINFO
static RetCode
WAVE5_dec_outputinfo(wave5_t *pVpu, vpu_4K_D2_dec_output_t *pOutParam)
{
	RetCode ret, ret_outinfo;
	DecHandle handle = pVpu->m_pCodecInst;
	int coreIdx = 0;
	int instanceQcount = handle->CodecInfo.decInfo.instanceQueueCount;

	{
		Uint32 iRet;
		if (Wave5ReadReg(coreIdx, W5_VCPU_CUR_PC) == 0x0) {
			DLOGA("RETCODE_CODEC_EXIT: pc is 0 \n");
			WAVE5_dec_reset( pVpu, ( 0 | (1<<16) ) );
			return RETCODE_CODEC_EXIT;
		}
		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			WAVE5_ClearInterrupt(coreIdx);
			#endif

			if (iRet & (1<<INT_WAVE5_BSBUF_EMPTY)) {
				if (handle->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
					WAVE5_SetPendingInst(coreIdx, 0);
					ret = RETCODE_FRAME_NOT_COMPLETE;
				}
				else {
					int read_ptr, write_ptr, room;

					ret = WAVE5_DecGetBitstreamBuffer(pVpu->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (int *)&room);
					if (ret != RETCODE_SUCCESS) {
						return ret;
					}

					handle->CodecInfo.decInfo.prev_streamWrPtr = write_ptr;
					WAVE5_SetPendingInst(coreIdx, handle);
					handle->CodecInfo.decInfo.m_iPendingReason = (1<<INT_WAVE5_BSBUF_EMPTY);
					ret = RETCODE_INSUFFICIENT_BITSTREAM;
				}
			}
		}
	}

	{
		int disp_idx = 0;
		DecOutputInfo info = {0,};

		ret_outinfo = WAVE5_DecGetOutputInfo(handle, &info);
		pOutParam->m_DecOutInfo.m_Reserved[20] = ret_outinfo;
		if (ret_outinfo != RETCODE_SUCCESS) {
			if (handle->CodecInfo.decInfo.superframe.doneIndex > 0) {
				handle->CodecInfo.decInfo.superframe.doneIndex--;
				pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiDoneIdx = handle->CodecInfo.decInfo.superframe.doneIndex;
				if (handle->CodecInfo.decInfo.superframe.doneIndex == 0) {
					if (wave5_memset) {
						wave5_memset(&handle->CodecInfo.decInfo.superframe, 0x00, sizeof(VP9Superframe), 0);
					}
				}
			}
		}

		WAVE5_dec_out_info( pVpu, &pOutParam->m_DecOutInfo, &info, ret_outinfo );

		if (instanceQcount == 0) {
			if (ret_outinfo == RETCODE_REPORT_NOT_READY) {
				pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_CQ_EMPTY;
			}
		}

		if (ret_outinfo == RETCODE_REPORT_NOT_READY) {
			return ret_outinfo;
		}

		if (info.decOutputExtData.userDataNum > 0) {
			GetDecUserData(pVpu, pOutParam, info);
		}

		if (pOutParam->m_DecOutInfo.m_iDispOutIdx > MAX_GDI_IDX) {
			DLOGA("RETCODE_CODEC_EXIT: m_iDispOutIdx is %d \n", pOutParam->m_DecOutInfo.m_iDispOutIdx);
			return RETCODE_CODEC_EXIT;
		}

		if (pOutParam->m_DecOutInfo.m_iDecodedIdx > MAX_GDI_IDX) {
			DLOGA("RETCODE_CODEC_EXIT: m_iDecodedIdx is %d \n", pOutParam->m_DecOutInfo.m_iDispOutIdx);
			return RETCODE_CODEC_EXIT;
		}

		//! Store Image
		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
			#ifdef PLATFORM_FPGA
			int stride  = (( pVpu->m_iPicWidth  + 15 ) & ~15);
			#endif
			int disp_idx;
			int disp_idx_comp = pOutParam->m_DecOutInfo.m_iDispOutIdx;

			//! Display Buffer
			pVpu->m_iDecodedFrameCount++;
			pVpu->m_iDecodedFrameCount = pVpu->m_iDecodedFrameCount % pVpu->m_iFrameBufferCount;
			disp_idx= pOutParam->m_DecOutInfo.m_iDispOutIdx;

			if (disp_idx > MAX_GDI_IDX) {
				DLOGA("RETCODE_CODEC_EXIT: disp_idx is %d \n", disp_idx);
				return RETCODE_CODEC_EXIT;
			}

			if (pVpu->m_iIndexFrameDisplay >= 0) {
				pVpu->m_iIndexFrameDisplayPrev = pVpu->m_iIndexFrameDisplay;
			}
			pVpu->m_iIndexFrameDisplay = disp_idx;

			if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					disp_idx_comp = pOutParam->m_DecOutInfo.m_iDispOutIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
				}
				else {
					disp_idx = pOutParam->m_DecOutInfo.m_iDispOutIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
				}
				pVpu->m_iIndexFrameDisplayPrev = pVpu->m_iIndexFrameDisplay;
				pVpu->m_iIndexFrameDisplay = disp_idx;
			}

			if (disp_idx >= 0) {
				//Physical Address
				pOutParam->m_pDispOut[PA][COMP_Y] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pDispOut[PA][COMP_U] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pDispOut[PA][COMP_V] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);
				//Virtual Address
				pOutParam->m_pDispOut[VA][COMP_Y] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
				pOutParam->m_pDispOut[VA][COMP_U] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);
				pOutParam->m_pDispOut[VA][COMP_V] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCr);

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufCb;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufCb;

				if (handle->CodecInfo.decInfo.enableAfbce)
				{
					DecOutputInfo dispInfo = {0};
					VpuRect     rcDisplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedY[PA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedCb[PA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedY[VA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedCb[VA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);

					rcDisplay.left   = 0;
					rcDisplay.top    = 0;
					rcDisplay.right  = info.dispPicWidth;
					rcDisplay.bottom = info.dispPicHeight;
					dispInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					dispInfo.dispFrame.cbcrInterleave = 0;
					dispInfo.dispFrame.nv21 = 0;
					dispInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					dispInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					//dispInfo.dispFrame.lumaBitDepth =  (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : info.dispFrame.orgLumaBitDepth  ;
					//dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.orgChromaBitDepth;
					dispInfo.dispFrame.lumaBitDepth   = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : 10;
					dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispPicWidth = info.dispPicWidth;
					dispInfo.dispPicHeight = info.dispPicHeight;
					dispInfo.dispFrame.endian = info.dispFrame.endian;
					dispInfo.dispFrame.bufY = info.dispFrame.bufY;

					GetAfbceData(handle, &dispInfo, rcDisplay, info.dispFrame.lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					for (compnum = 0 ; compnum < 3 ; compnum++) {
						pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];
					}

					{
						int i, j;
						for (i = 0 ; i < 20 ; i++) {
							for (j = 0 ; j < 3 ; j++) {
								pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							}
							pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCY;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCY;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCC;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCC;
				}
				else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
						#endif
					}
					else {
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDisplayWidth, pOutParam->m_DecOutInfo.m_iDisplayHeight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDisplayWidth, pOutParam->m_DecOutInfo.m_iDisplayHeight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayHeight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayHeight));
						}
						#endif
					}
					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize+4095)&~4095)+4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize+4095)&~4095)+4096;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].virt_addr;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth;

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					#if TCC_HEVC___DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDisplayWidth)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDisplayWidth/2)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					#endif
					#if TCC_VP9____DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_VP9) {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth/2)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiLumaStride   = lumaStride;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiChromaStride = chromaStride;
					#endif
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian)
					{
						case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
						case VDI_BIG_ENDIAN:          endian = 0x0f; break;
						case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
						case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
						default:                      endian = pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian; break;
					}
					endian = endian&0xf;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiFrameEndian = 0;
				}
			}
			else {
				pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
				pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
				pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;
			}

		}
		else {
			pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
			pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
			pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[VA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight = NULL;
		}

		if (pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS) {
			int decoded_idx;
			int decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx;

			//! Decoded Buffer
			decoded_idx= pOutParam->m_DecOutInfo.m_iDecodedIdx;

			if (decoded_idx > MAX_GDI_IDX) {
				DLOGA("RETCODE_CODEC_EXIT: decoded_idx is %d \n", decoded_idx);
				return RETCODE_CODEC_EXIT;
			}

			if (decoded_idx >= 0) {
				if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
					if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
						decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
					else {
						decoded_idx = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
				}

				//Physical Address
				pOutParam->m_pCurrOut[PA][COMP_Y] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx].bufY;
				pOutParam->m_pCurrOut[PA][COMP_U] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCb;
				pOutParam->m_pCurrOut[PA][COMP_V] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCr;
				//Virtual Address
				pOutParam->m_pCurrOut[VA][COMP_Y] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx].bufY;
				pOutParam->m_pCurrOut[VA][COMP_U] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCb;
				pOutParam->m_pCurrOut[VA][COMP_V] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCr;

				if (handle->CodecInfo.decInfo.enableAfbce) {
					DecOutputInfo decpInfo = {0};
					VpuRect     rcDecplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedY[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx].bufY;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedCb[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCb;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedY[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx].bufY;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedCb[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCb;

					rcDecplay.left   = 0;
					rcDecplay.top    = 0;
					rcDecplay.right  = info.decPicWidth;
					rcDecplay.bottom = info.decPicHeight;
					decpInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					decpInfo.dispFrame.cbcrInterleave = 0;
					decpInfo.dispFrame.nv21 = 0;
					decpInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					decpInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					//decpInfo.dispFrame.lumaBitDepth =  (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : info.dispFrame.orgLumaBitDepth  ;
					//decpInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.orgChromaBitDepth;
					decpInfo.dispFrame.lumaBitDepth   = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : 10;
					decpInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					decpInfo.dispPicWidth = info.decPicWidth;
					decpInfo.dispPicHeight = info.decPicHeight;
					decpInfo.dispFrame.endian =	pVpu->m_stFrameBuffer[PA][decoded_idx].endian;//handle->CodecInfo.decInfo.frameBufPool[decoded_idx].endian;
					decpInfo.dispFrame.bufY = pVpu->m_stFrameBuffer[PA][decoded_idx].bufY;

					GetAfbceData(handle, &decpInfo, rcDecplay, handle->CodecInfo.decInfo.decOutInfo[decoded_idx].lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					for (compnum = 0 ; compnum < 3 ; compnum++) {
						pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];
					}

					{
						int i, j;
						for (i = 0 ; i < 20 ; i++) {
							for (j = 0 ; j < 3 ; j++) {
								pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							}
							pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufY;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufCb;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufY;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufCb;

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufFBCY;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufFBCY;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufFBCC;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufFBCC;
				}
				else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB)
					{
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
						#endif
					}
					else {
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
							fbcYTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
							fbcYTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
						}
						#endif
					}

					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize+4095)&~4095)+4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize+4095)&~4095)+4096;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[decoded_idx_compressed].phys_addr;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[decoded_idx_compressed].virt_addr;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[decoded_idx_compressed].phys_addr;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[decoded_idx_compressed].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth;

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					#if TCC_HEVC___DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth)*(pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth/2)*(pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					#endif
					#if TCC_VP9____DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_VP9) {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth)*(pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth/2)*(pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					#endif

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiLumaStride   = lumaStride;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiChromaStride = chromaStride;
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].endian)
					{
						case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
						case VDI_BIG_ENDIAN:          endian = 0x0f; break;
						case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
						case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
						default:                      endian = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].endian; break;
					}
					endian = endian&0xf;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiFrameEndian = 0;
				}
			}
			else {
				pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;
			}
		}
		else {
			pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
			pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
			pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[VA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropTop = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropBottom = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropLeft = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropRight = NULL;
		}

		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
			int disp_idx_comp = pVpu->m_iIndexFrameDisplayPrev;

			//! Previously Displayed Buffer
			disp_idx = pVpu->m_iIndexFrameDisplayPrev;
			if (disp_idx >= 0) {
				if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
					if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
						disp_idx_comp = pVpu->m_iIndexFrameDisplayPrev + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
					else {
						disp_idx_comp = pVpu->m_iIndexFrameDisplayPrev - handle->CodecInfo.decInfo.numFbsForDecoding;
					}
				}

				//Physical Address
				pOutParam->m_pPrevOut[PA][COMP_Y] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pPrevOut[PA][COMP_U] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[PA][COMP_V] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);
				//Virtual Address
				pOutParam->m_pPrevOut[VA][COMP_Y] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
				pOutParam->m_pPrevOut[VA][COMP_U] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[VA][COMP_V] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCr);

				if (handle->CodecInfo.decInfo.enableAfbce) {
					DecOutputInfo dispInfo = {0};
					VpuRect     rcDisplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedY[PA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedCb[PA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedY[VA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedCb[VA] = (unsigned char*)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);

					rcDisplay.left   = 0;
					rcDisplay.top    = 0;
					rcDisplay.right  = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicWidth;
					rcDisplay.bottom = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicHeight;
					dispInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					dispInfo.dispFrame.cbcrInterleave = 0;
					dispInfo.dispFrame.nv21 = 0;
					dispInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					dispInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					dispInfo.dispFrame.lumaBitDepth   = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : 10;
					dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispPicWidth = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicWidth;
					dispInfo.dispPicHeight = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicHeight;
					dispInfo.dispFrame.endian = pVpu->m_stFrameBuffer[PA][disp_idx].endian;
					dispInfo.dispFrame.bufY = pVpu->m_stFrameBuffer[PA][disp_idx].bufY;

					GetAfbceData(handle, &dispInfo, rcDisplay, handle->CodecInfo.decInfo.decOutInfo[disp_idx].lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					for (compnum = 0 ; compnum < 3 ; compnum++) {
						pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];
					}

					{
						int i, j;
						for (i = 0 ; i < 20 ; i++) {
							for (j = 0 ; j < 3 ; j++) {
								pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							}
							pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufCb;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufCb;

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCY;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCY;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = (unsigned char*)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCC;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = (unsigned char*)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCC;
				}
				else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
						#endif
					}
					else {
						#if TCC_HEVC___DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
						}
						#endif
						#if TCC_VP9____DEC_SUPPORT == 1
						if (pVpu->m_iBitstreamFormat == STD_VP9) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
						}
						#endif
					}
					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize+4095)&~4095)+4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize+4095)&~4095)+4096;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].virt_addr;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth;

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					#if TCC_HEVC___DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth/2)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					#endif
					#if TCC_VP9____DEC_SUPPORT == 1
					if (pVpu->m_iBitstreamFormat == STD_VP9) {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth>8?5:4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth/2)*(pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth>8?5:4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}
					#endif

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiLumaStride   = lumaStride;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiChromaStride = chromaStride;
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian) {
						case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
						case VDI_BIG_ENDIAN:          endian = 0x0f; break;
						case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
						case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
						default:                      endian = pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian; break;
					}
					endian = endian&0xf;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiFrameEndian = 0;
				}
			}
			else {
				pOutParam->m_pPrevOut[PA][COMP_Y] = pOutParam->m_pPrevOut[VA][COMP_Y] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_U] = pOutParam->m_pPrevOut[VA][COMP_U] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_V] = pOutParam->m_pPrevOut[VA][COMP_V] = NULL;
			}
		}
		else {
			pOutParam->m_pPrevOut[PA][COMP_Y] = pOutParam->m_pPrevOut[VA][COMP_Y] = NULL;
			pOutParam->m_pPrevOut[PA][COMP_U] = pOutParam->m_pPrevOut[VA][COMP_U] = NULL;
			pOutParam->m_pPrevOut[PA][COMP_V] = pOutParam->m_pPrevOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[VA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;
		}

		if (info.decodingSuccess == 0) {
			return ret;
		}

		if (info.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END) {
			return RETCODE_CODEC_FINISH;
		}
		else if (info.indexFrameDisplay > pVpu->m_iNeedFrameBufCount) {
			return RETCODE_INVALID_FRAME_BUFFER;
		}
	}

	return ret_outinfo;
}
#endif //#ifdef TEST_OUTPUTINFO


#ifdef ENABLE_VDI_STATUS_LOG
static unsigned int _vpu_4k_d2mgr_FIORead(unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data  = 0xffffffff;

	ctrl  = (addr&0xffff);
	ctrl |= (0<<16);    /* read operation */
	Wave5WriteReg(0, W5_VPU_FIO_CTRL_ADDR, ctrl);
	count = 10000;
	while (count--) {
		ctrl = Wave5ReadReg(0, W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			data = Wave5ReadReg(0, W5_VPU_FIO_DATA);
			break;
		}
	}
	return data;
}

static int _vpu_4k_d2mgr_FIOWrite(unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	Wave5WriteReg(0, W5_VPU_FIO_DATA, data);
	ctrl  = (addr&0xffff);
	ctrl |= (1<<16);    /* write operation */
	Wave5WriteReg(0, W5_VPU_FIO_CTRL_ADDR, ctrl);
	return 1;
}

static unsigned int _vpu_4k_d2mgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308
	int     vcpu_reg_addr;
	int     udata;
	//int     vce_core_base = 0x8000 + 0x1000*vce_core_idx;
	int     vce_core_base = 0x8000;

	_vpu_4k_d2mgr_FIOWrite(VCORE_DBG_READY, 0);

	vcpu_reg_addr = vce_addr >> 2;

	_vpu_4k_d2mgr_FIOWrite(VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

	while (1) {
		if (_vpu_4k_d2mgr_FIORead(VCORE_DBG_READY) == 1) {
			udata= _vpu_4k_d2mgr_FIORead(VCORE_DBG_DATA);
			break;
		}
	}

    return udata;
}

static void _vpu_4k_d2_dump_status(void)
{
	int rd, wr;
	unsigned int tq, ip, mc, lf;
	unsigned int avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
	unsigned int ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy, lf_fsm, bs_data, bbusy, fv;
	unsigned int reg_val;
	unsigned int index;
	unsigned int vcpu_reg[31]= {0,};

	DLOGV("-------------------------------------------------------------------------------\n");
	DLOGV("------                            VCPU STATUS                             -----\n");
	DLOGV("-------------------------------------------------------------------------------\n");
	rd = Wave5ReadReg(0, W5_BS_RD_PTR);
	wr = Wave5ReadReg(0, W5_BS_WR_PTR);
	DLOGV("RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x\n",
	rd, wr, Wave5ReadReg(0, W5_BS_OPTION), Wave5ReadReg(0, W5_CMD_BS_PARAM));

	// --------- VCPU register Dump
	DLOGV("[+] VCPU REG Dump\n");
	for (index = 0; index < 25; index++) {
		Wave5WriteReg(0, 0x14, (1<<9) | (index & 0xff));
		vcpu_reg[index] = Wave5ReadReg(0, 0x1c);

		if (index < 16) {
			DLOGV("0x%08x\t",  vcpu_reg[index]);
			if ((index % 4) == 3) DLOGV("\n");
		}
		else {
			switch (index) {
				case 16: DLOGV("CR0: 0x%08x\t", vcpu_reg[index]); break;
				case 17: DLOGV("CR1: 0x%08x\n", vcpu_reg[index]); break;
				case 18: DLOGV("ML:  0x%08x\t", vcpu_reg[index]); break;
				case 19: DLOGV("MH:  0x%08x\n", vcpu_reg[index]); break;
				case 21: DLOGV("LR:  0x%08x\n", vcpu_reg[index]); break;
				case 22: DLOGV("PC:  0x%08x\n", vcpu_reg[index]);break;
				case 23: DLOGV("SR:  0x%08x\n", vcpu_reg[index]);break;
				case 24: DLOGV("SSP: 0x%08x\n", vcpu_reg[index]);break;
			}
		}
	}
	DLOGV("[-] VCPU REG Dump\n");
	// --------- BIT register Dump
	DLOGV("[+] BPU REG Dump\n");
	DLOGV("BITPC = 0x%08x\n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x18)));
	DLOGV("BIT START=0x%08x, BIT END=0x%08x\n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x11c)),
	_vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x120)) );

	DLOGV("CODE_BASE			%x \n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x18)));
	DLOGV("VCORE_REINIT_FLAG	%x \n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x0C)));

	// --------- BIT HEVC Status Dump
	ctu_fsm		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x48));
	nb_fsm		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x4c));
	cabac_fsm	= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x50));
	cu_info		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x54));
	mvp_fsm		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x58));
	tc_busy		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x5c));
	lf_fsm		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x60));
	bs_data		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x64));
	bbusy		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x68));
	fv		= _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x6C));

	DLOGV("[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d\n",  _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x40)), _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x44)));
	DLOGV("[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x\n", ((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff), ((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
	DLOGV("[DEBUG-BPUHEVC] NB_FSM:	0x%02x\n", nb_fsm & 0xff);
	DLOGV("[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x\n", ((cabac_fsm>>25) & 0x3f), ((cabac_fsm>>19) & 0x3f), ((cabac_fsm>>13) & 0x3f), ((cabac_fsm>>6) & 0x7f), (cabac_fsm & 0x3f));
	DLOGV("[DEBUG-BPUHEVC] CU_INFO value = 0x%04x \n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)\n", cu_info,
			((cu_info>> 16) & 0x3), ((cu_info>> 13) & 0x7), ((cu_info>> 10) & 0x7), ((cu_info>> 9) & 0x3), ((cu_info>> 8) & 0x1), ((cu_info>> 6) & 0x3), ((cu_info>> 4) & 0x3), ((cu_info>> 2) & 0x3), (cu_info & 0x3));
	DLOGV("[DEBUG-BPUHEVC] MVP_FSM> 0x%02x\n", mvp_fsm & 0xf);
	DLOGV("[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x\n", ((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
	DLOGV("[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x\n", ((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
	DLOGV("[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x\n", ((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff), (bs_data & 0xfff));
	DLOGV("[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d\n", ((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1) , (bbusy & 0x1));
	DLOGV("[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d\n\n", ((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1), ((fv >> 1) & 0x1), (fv & 0x1));
	DLOGV("[-] BPU REG Dump\n");

	// --------- VCE register Dump
	DLOGV("[+] VCE REG Dump\n");
	tq = _vpu_4k_d2mgr_ReadRegVCE(0xd0);
	ip = _vpu_4k_d2mgr_ReadRegVCE(0xd4);
	mc = _vpu_4k_d2mgr_ReadRegVCE(0xd8);
	lf = _vpu_4k_d2mgr_ReadRegVCE(0xdc);
	avail_cu = (_vpu_4k_d2mgr_ReadRegVCE(0x11C)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x110)>>16);
	avail_tu = (_vpu_4k_d2mgr_ReadRegVCE(0x11C)&0xFFFF) - (_vpu_4k_d2mgr_ReadRegVCE(0x110)&0xFFFF);
	avail_tc = (_vpu_4k_d2mgr_ReadRegVCE(0x120)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x114)>>16);
	avail_lf = (_vpu_4k_d2mgr_ReadRegVCE(0x120)&0xFFFF) - (_vpu_4k_d2mgr_ReadRegVCE(0x114)&0xFFFF);
	avail_ip = (_vpu_4k_d2mgr_ReadRegVCE(0x124)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x118)>>16);
	DLOGV("       TQ            IP              MC             LF      GDI_EMPTY          ROOM \n");
	DLOGV("------------------------------------------------------------------------------------------------------------\n");
	DLOGV("| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)\n",
		(tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
		(ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
		(mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
		(lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
		_vpu_4k_d2mgr_FIORead(0x88f4),                      /* GDI empty */
		avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);

	/* CU/TU Queue count */
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x12C);
	DLOGV("[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A0);
	DLOGV("TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A4);
	DLOGV("LF(%5d)\n", (reg_val>>16)&0xffff);
	DLOGV("VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
		"               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d) \n",
		(reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1, (reg_val>>3)&1,
		(reg_val>>2)&1, (reg_val>>1)&1, (reg_val>>0)&1,
		(reg_val>>10)&1, (reg_val>>9)&1, (reg_val>>8)&1, (reg_val>>7)&1);

	DLOGV("State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x\n",
		_vpu_4k_d2mgr_ReadRegVCE(0xd0), _vpu_4k_d2mgr_ReadRegVCE(0xd4), _vpu_4k_d2mgr_ReadRegVCE(0xd8), _vpu_4k_d2mgr_ReadRegVCE(0xdc));
	DLOGV("BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _vpu_4k_d2mgr_ReadRegVCE(0x194), _vpu_4k_d2mgr_ReadRegVCE(0x198));
	DLOGV("BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _vpu_4k_d2mgr_ReadRegVCE(0x194), _vpu_4k_d2mgr_ReadRegVCE(0x198));
	DLOGV("DCI INFO\n");
	DLOGV("READ_CNT_0 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x110));
	DLOGV("READ_CNT_1 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x114));
	DLOGV("READ_CNT_2 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x118));
	DLOGV("WRITE_CNT_0: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x11c));
	DLOGV("WRITE_CNT_1: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x120));
	DLOGV("WRITE_CNT_2: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x124));
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x128);
	DLOGV("LF_DEBUG_PT: 0x%08x\n", reg_val & 0xffffffff);
	DLOGV("cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d\n",
		(reg_val >> 16) & 0x1f,
		(reg_val >> 15) & 0x1,
		(reg_val >> 14) & 0x1);
	DLOGV("para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d\n",
		(reg_val >> 13) & 0x1,
		(reg_val >> 12) & 0x1,
		(reg_val >> 11) & 0x1,
		(reg_val >> 10) & 0x1);
	DLOGV("lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d\n",
		(reg_val >> 9) & 0x1,
		(reg_val >> 8) & 0x1,
		(reg_val >> 6) & 0x1,
		(reg_val >> 4) & 0x1,
		reg_val & 0xf);
	DLOGV("[-] VCE REG Dump\n");
	DLOGV("[-] VCE REG Dump\n");

	DLOGV("-------------------------------------------------------------------------------\n");
	{
		unsigned int q_cmd_done_inst, stage0_inst_info, stage1_inst_info, stage2_inst_info, dec_seek_cycle, dec_parsing_cycle, dec_decoding_cycle;

		q_cmd_done_inst = Wave5ReadReg(0, 0x01E8);
		stage0_inst_info = Wave5ReadReg(0, 0x01EC);
		stage1_inst_info = Wave5ReadReg(0, 0x01F0);
		stage2_inst_info = Wave5ReadReg(0, 0x01F4);
		dec_seek_cycle = Wave5ReadReg(0, 0x01C0);
		dec_parsing_cycle = Wave5ReadReg(0, 0x01C4);
		dec_decoding_cycle = Wave5ReadReg(0, 0x01C8);
		DLOGV("W5_RET_QUEUE_CMD_DONE_INST : 0x%08x\n", q_cmd_done_inst);
		DLOGV("W5_RET_STAGE0_INSTANCE_INFO : 0x%08x\n", stage0_inst_info);
		DLOGV("W5_RET_STAGE1_INSTANCE_INFO : 0x%08x\n", stage1_inst_info);
		DLOGV("W5_RET_STAGE2_INSTANCE_INFO : 0x%08x\n", stage2_inst_info);
		DLOGV("W5_RET_DEC_SEEK_CYCLE : 0x%08x\n", dec_seek_cycle);
		DLOGV("W5_RET_DEC_PARSING_CYCLE : 0x%08x\n", dec_parsing_cycle);
		DLOGV("W5_RET_DEC_DECODING_CYCLE : 0x%08x\n", dec_decoding_cycle);
	}
	DLOGV("-------------------------------------------------------------------------------\n");

	/* SDMA & SHU INFO */
	// -----------------------------------------------------------------------------
	// SDMA registers
	// -----------------------------------------------------------------------------

	{
		//DECODER SDMA INFO
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5000);
		DLOGV("SDMA_LOAD_CMD    = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5004);
		DLOGV("SDMA_AUTO_MOD  = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5008);
		DLOGV("SDMA_START_ADDR  = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x500C);
		DLOGV("SDMA_END_ADDR   = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5010);
		DLOGV("SDMA_ENDIAN     = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5014);
		DLOGV("SDMA_IRQ_CLEAR  = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5018);
		DLOGV("SDMA_BUSY       = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x501C);
		DLOGV("SDMA_LAST_ADDR  = 0x%x\n",reg_val);
		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5020);
		DLOGV("SDMA_SC_BASE_ADDR  = 0x%x\n",reg_val);

		// -----------------------------------------------------------------------------
		// SHU registers
		// -----------------------------------------------------------------------------

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5400);
		DLOGV("SHU_INIT = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5404);
		DLOGV("SHU_SEEK_NXT_NAL = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5408);
		DLOGV("SHU_RD_NAL_ADDR = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x540c);
		DLOGV("SHU_STATUS = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5410);
		DLOGV("SHU_GBYTE_1 = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5414);
		DLOGV("SHU_GBYTE_2 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5418);
        DLOGV("SHU_GBYTE_3 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x541c);
        DLOGV("SHU_GBYTE_4 = 0x%x\n",reg_val);


        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5420);
        DLOGV("SHU_GBYTE_5 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5424);
        DLOGV("SHU_GBYTE_6 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5428);
        DLOGV("SHU_GBYTE_7 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x542c);
        DLOGV("SHU_GBYTE_8 = 0x%x\n",reg_val);


        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5430);
        DLOGV("SHU_SBYTE_LOW = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5434);
        DLOGV("SHU_SBYTE_HIGH = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5438);
        DLOGV("SHU_ST_PAT_DIS = 0x%x\n",reg_val);


        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5440);
        DLOGV("SHU_SHU_NBUF0_0 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5444);
        DLOGV("SHU_SHU_NBUF0_1 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5448);
        DLOGV("SHU_SHU_NBUF0_2 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x544c);
        DLOGV("SHU_SHU_NBUF0_3 = 0x%x\n",reg_val);


        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5450);
        DLOGV("SHU_SHU_NBUF1_0 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5454);
        DLOGV("SHU_SHU_NBUF1_1 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5458);
        DLOGV("SHU_SHU_NBUF1_2 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x545c);
        DLOGV("SHU_SHU_NBUF1_3 = 0x%x\n",reg_val);


        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5460);
        DLOGV("SHU_SHU_NBUF2_0 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5464);
        DLOGV("SHU_SHU_NBUF2_1 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5468);
        DLOGV("SHU_SHU_NBUF2_2 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x546c);
        DLOGV("SHU_SHU_NBUF2_3 = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5470);
        DLOGV("SHU_NBUF_RPTR = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5474);
        DLOGV("SHU_NBUF_WPTR = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5478);
        DLOGV("SHU_REMAIN_BYTE = 0x%x\n",reg_val);

        // -----------------------------------------------------------------------------
        // GBU registers
        // -----------------------------------------------------------------------------

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5800);
        DLOGV("GBU_INIT = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5804);
        DLOGV("GBU_STATUS = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5808);
        DLOGV("GBU_TCNT = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x580c);
        DLOGV("GBU_TCNT = 0x%x\n",reg_val);

		reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c10);
        DLOGV("GBU_WBUF0_LOW = 0x%x\n",reg_val);
        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c14);
        DLOGV("GBU_WBUF0_HIGH = 0x%x\n",reg_val);
        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c18);

        DLOGV("GBU_WBUF1_LOW = 0x%x\n",reg_val);
        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c1c);
        DLOGV("GBU_WBUF1_HIGH = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c20);
        DLOGV("GBU_WBUF2_LOW = 0x%x\n",reg_val);
        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c24);
        DLOGV("GBU_WBUF2_HIGH = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c30);
        DLOGV("GBU_WBUF_RPTR = 0x%x\n",reg_val);
        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c34);
        DLOGV("GBU_WBUF_WPTR = 0x%x\n",reg_val);

        reg_val = Wave5ReadReg(0, W5_REG_BASE + 0x5c28);
        DLOGV("GBU_REMAIN_BIT = 0x%x\n",reg_val);
	}
}
#endif

#ifdef ENABLE_VDI_LOG
void vdi_make_log(unsigned long core_idx, const char *str, int step)
{
    int val;

    val = Wave5ReadReg(core_idx, W5_CMD_INSTANCE_INFO);
    val &= 0xffff;
    if (step == 1)
        DLOGV("\n**%s start(%d)\n", str, val);
    else if (step == 2)	//
        DLOGV("\n**%s timeout(%d)\n", str, val);
    else
        DLOGV("\n**%s end(%d)\n", str, val);
}

void vdi_log(unsigned long core_idx, int cmd, int step)
{
	switch(cmd) {
	case W5_INIT_VPU:
		(core_idx, "INIT_VPU", step);
		break;
        case W5_INIT_SEQ:
		vdi_make_log(core_idx, "DEC INIT_SEQ", step);
		break;
        case W5_DESTROY_INSTANCE:
		vdi_make_log(core_idx, "DESTROY_INSTANCE", step);
		break;
        case W5_DEC_PIC:
		vdi_make_log(core_idx, "DEC_PIC(ENC_PIC)", step);
		break;
	case W5_SET_FB:
		vdi_make_log(core_idx, "SET_FRAMEBUF", step);
		break;
	case W5_FLUSH_INSTANCE:
		vdi_make_log(core_idx, "FLUSH INSTANCE", step);
		break;
	case W5_QUERY:
		vdi_make_log(core_idx, "QUERY", step);
		break;
	case W5_SLEEP_VPU:
		vdi_make_log(core_idx, "SLEEP_VPU", step);
		break;
        case W5_WAKEUP_VPU:
		vdi_make_log(core_idx, "WAKEUP_VPU", step);
		break;
        case W5_UPDATE_BS:
		vdi_make_log(core_idx, "UPDATE_BS", step);
		break;
	case W5_CREATE_INSTANCE:
		vdi_make_log(core_idx, "CREATE_INSTANCE", step);
		break;
	case W5_RESET_VPU:
		vdi_make_log(core_idx, "RESET_VPU", step);
		break;
	default:
		vdi_make_log(core_idx, "ANY_CMD", step);
		break;
	}

#ifdef ENABLE_VDI_LOG_HALF
	#ifdef ENABLE_VDI_LOG_ONE_LINE
		DLOGV("0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,      Wave5ReadReg(core_idx,0x100), Wave5ReadReg(core_idx, 0x100+4), Wave5ReadReg(core_idx, 0x100+8), Wave5ReadReg(core_idx, 0x100+0xc) );
	#else
	for (i=0x100; i<0x180; i=i+128) { // host IF register 0x100 ~ 0x200
		DLOGV("0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			i,      Wave5ReadReg(core_idx, i     ), Wave5ReadReg(core_idx, i+4   ), Wave5ReadReg(core_idx, i+8   ), Wave5ReadReg(core_idx, i+0xc ),
			i+0x10, Wave5ReadReg(core_idx, i+0x10), Wave5ReadReg(core_idx, i+0x14), Wave5ReadReg(core_idx, i+0x18), Wave5ReadReg(core_idx, i+0x1c),
			i+0x20, Wave5ReadReg(core_idx, i+0x20), Wave5ReadReg(core_idx, i+0x24), Wave5ReadReg(core_idx, i+0x28), Wave5ReadReg(core_idx, i+0x2c),
			i+0x30, Wave5ReadReg(core_idx, i+0x30), Wave5ReadReg(core_idx, i+0x34), Wave5ReadReg(core_idx, i+0x38), Wave5ReadReg(core_idx, i+0x3c),
			i+0x40, Wave5ReadReg(core_idx, i+0x40), Wave5ReadReg(core_idx, i+0x44), Wave5ReadReg(core_idx, i+0x48), Wave5ReadReg(core_idx, i+0x4c),
			i+0x50, Wave5ReadReg(core_idx, i+0x50), Wave5ReadReg(core_idx, i+0x54), Wave5ReadReg(core_idx, i+0x58), Wave5ReadReg(core_idx, i+0x5c),
			i+0x60, Wave5ReadReg(core_idx, i+0x60), Wave5ReadReg(core_idx, i+0x64), Wave5ReadReg(core_idx, i+0x68), Wave5ReadReg(core_idx, i+0x6c),
			i+0x70, Wave5ReadReg(core_idx, i+0x70), Wave5ReadReg(core_idx, i+0x74), Wave5ReadReg(core_idx, i+0x78), Wave5ReadReg(core_idx, i+0x7c)
			);
	}
	#endif
#else
	for (i=0x100; i<0x200; i=i+16) { // host IF register 0x100 ~ 0x200
		DLOGV("0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
		Wave5ReadReg(core_idx, i), Wave5ReadReg(core_idx, i+4),
		Wave5ReadReg(core_idx, i+8), Wave5ReadReg(core_idx, i+0xc));
	}
#endif

	#ifdef ENABLE_VDI_STATUS_LOG
	if (cmd == W5_INIT_VPU || cmd == W5_RESET_VPU || cmd == W5_CREATE_INSTANCE) {
		_vpu_4k_d2_dump_status();
	}
	#endif

}
#endif

static RetCode
WAVE5_dec_decode(wave5_t *pVpu, vpu_4K_D2_dec_input_t *pInputParam, vpu_4K_D2_dec_output_t *pOutParam)
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	DecParam dec_param = {0,};
	int coreIdx = 0;
	int seqchange_reinterrupt = 0;
	DecQueueStatusInfo qStatus;
	int i_instance_queue_count_before_dec = -1;

	if (pOutParam != NULL) {
		if (wave5_memset)
			wave5_memset(&pOutParam->m_DecOutInfo, 0, sizeof(pOutParam->m_DecOutInfo), 0);
	}

	DLOGV("[INP]====>[VPU:%p][F:%4d][Size:%6d][Offset:%6d][skipFrameMode:%d][CQ:%d/%d][prevBufFull:%d][fb_cleared:%d][codec:%d][SkipFrameMode:%d]\n",
		  pVpu, handle->CodecInfo.decInfo.frameNumCount,
		  pInputParam->m_iBitstreamDataSize, pInputParam->m_iBitstreamDataOffset,
		  pInputParam->m_iSkipFrameMode,
		  handle->CodecInfo.decInfo.instanceQueueCount, handle->CodecInfo.decInfo.totalQueueCount,
		  handle->CodecInfo.decInfo.prev_buffer_full,
		  handle->CodecInfo.decInfo.fb_cleared,
		  pVpu->m_pCodecInst->codecMode,
		  pInputParam->m_iSkipFrameMode);

	if (handle->CodecInfo.decInfo.openParam.CQ_Depth > 1) {
		i_instance_queue_count_before_dec = handle->CodecInfo.decInfo.instanceQueueCount;
		if (handle->CodecInfo.decInfo.watchdog_timeout == 1) {
			// check cq status
			DLOGA("[QStatus] count(%d)/total(%d)/set(%d)",
				handle->CodecInfo.decInfo.instanceQueueCount,
				handle->CodecInfo.decInfo.totalQueueCount,
				handle->CodecInfo.decInfo.openParam.CQ_Depth);

			handle->CodecInfo.decInfo.watchdog_timeout = 0;
		} else {
			DLOGT("[QStatus] count(%d)/total(%d)/set(%d)",
				handle->CodecInfo.decInfo.instanceQueueCount,
				handle->CodecInfo.decInfo.totalQueueCount,
				handle->CodecInfo.decInfo.openParam.CQ_Depth);
		}

	#ifdef CQ_BUFFER_RESILIENCE
		// Check if the current instance queue count has reached or exceeded the maximum CQ depth
		if (handle->CodecInfo.decInfo.instanceQueueCount >= handle->CodecInfo.decInfo.openParam.CQ_Depth) {
			DLOGE("[CQ:%d/Total:%d/Depth:%d] CQ is full and cannot accept any more commands \n",
					handle->CodecInfo.decInfo.instanceQueueCount,
					handle->CodecInfo.decInfo.totalQueueCount,
					handle->CodecInfo.decInfo.openParam.CQ_Depth);

			// Handling steps for a full CQ:
			// - Call the interrupt handler and wait for the result.
			// - Retrieve output information from the result queue.
			// - Add the current command to the queue.

			// TODO: Process the output and handle the next steps
			// Temporary code to retrieve and log the output info
			pVpu->iApplyPrevOutInfo = 10;//special code for test
			goto SEQ_CHANGE_OUTINFO_CQ1;
		}
	#endif
	}

#ifdef TEST_FULLFB_SEQCHANGE_REMAINING
	handle->CodecInfo.decInfo.decRemainingDispNum = 0;
#endif

	//! [2] Run Decoder
	{
	#ifdef SUPPORT_NON_IRAP_DELAY_ENABLE
		dec_param.skipframeMode = (pInputParam->m_iSkipFrameMode & 0x0F);

		if ((pVpu->m_pCodecInst->codecMode == W_HEVC_DEC) && (pInputParam->m_iSkipFrameMode & (1 << 4))) {
			dec_param.skipframeDelayEnable = 1;
			if (dec_param.skipframeMode == 1) {
				DLOGI("Enable DelayedOutput on I-frame search Mode\n");
			}
		} else {
			dec_param.skipframeDelayEnable = 0;
		}
		// dec_param.skipframeMode =  0 ;
	#else
		dec_param.skipframeMode = pInputParam->m_iSkipFrameMode;
	#endif

	#if TCC_HEVC___DEC_SUPPORT == 1
		if (handle->codecMode == W_HEVC_DEC) {
			if (dec_param.skipframeMode == 1) {
				DLOGD("Skip non-RAP pictures. all pictures that are not IDR, CRA, or BLA are skipped.\n");
			} else if (dec_param.skipframeMode == 2) {
				DLOGD("Skip non-reference pictures.\n");
			} else {
			#ifdef TEST_CQ2_DEC_SKIPFRAME
				handle->CodecInfo.decInfo.skipframeUsed = 0;
			#endif
			}
		}
	#endif

		handle->CodecInfo.decInfo.frameskipmode = dec_param.skipframeMode;

	#ifdef TEST_BUFFERFULL_POLLTIMEOUT
		if ((handle->CodecInfo.decInfo.prev_buffer_full == 1) && (handle->CodecInfo.decInfo.fb_cleared == 1)) {
			handle->CodecInfo.decInfo.fb_cleared = 0;
			goto SEQ_CHANGE_OUTINFO_CQ1;
		}
	#endif
		if (dec_param.skipframeMode == 1) {
			unsigned int timeoutcount = 0;
			int index;
			int iRet = 0;

			// clear superframe info
			if (handle->CodecInfo.decInfo.superframe.doneIndex > 0) {
				DLOGV("If there is any remaining superframe information, clear it before starting the I-Frame search.");
				if (wave5_memset) {
					wave5_memset(&handle->CodecInfo.decInfo.superframe, 0x00, sizeof(VP9Superframe), 0);
				}
			}

		#ifdef TEST_CQ2_DEC_SKIPFRAME
			handle->CodecInfo.decInfo.frameNumCount = 0;
			handle->CodecInfo.decInfo.skipframeUsed = 1;
		#endif

			/*
			 * [20191203] do not to all frame buffers & Flush ring buffer while skipframeMode is set to 1 in S** project
			 */
			if (handle->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
				unsigned int clrFlag = 0;

				DLOGD("Clear All Frame Buffer\n");
				for (index = 0; index < MAX_GDI_IDX; index++) {
					clrFlag |= (1 << index);
				}
				if (clrFlag > 0) {
					iRet = WAVE5_DecClrDispFlagEx(handle, clrFlag);
					if (iRet == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecClrDispFlagEx function. \n");
						WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
						return RETCODE_CODEC_EXIT;
					}
				}

			#ifdef ENABLE_FORCE_ESCAPE
				if (gWave5InitFirst_exit > 0) {
					DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}
			#endif

				DLOGD("Flush All Frame Buffer\n");
				while (1) {
					iRet = WAVE5_DecFrameBufferFlush(handle, NULL, NULL);
					if (iRet != RETCODE_VPU_STILL_RUNNING) {
						if (iRet == RETCODE_CODEC_EXIT) {
							DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecFrameBufferFlush function. \n");
							WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
							return RETCODE_CODEC_EXIT;
						}
						break;
					}
					timeoutcount++;
				#ifdef ENABLE_WAIT_POLLING_USLEEP
					if (wave5_usleep != NULL) {
						wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
						if (timeoutcount > VPU_BUSY_CHECK_USLEEP_CNT) {
							DLOGA("RETCODE_CODEC_EXIT: timeoutcount > VPU_BUSY_CHECK_USLEEP_CNT \n");
							WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
							return RETCODE_CODEC_EXIT;
						}
					}
				#endif
				#ifdef ENABLE_FORCE_ESCAPE_2
					if (gWave5InitFirst_exit > 0) {
						DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
						WAVE5_dec_reset(NULL, (0 | (1 << 16)));
						return RETCODE_CODEC_EXIT;
					}
				#endif
					if (timeoutcount > VPU_BUSY_CHECK_TIMEOUT) {
						DLOGA("RETCODE_CODEC_EXIT: timeoutcount > VPU_BUSY_CHECK_TIMEOUT \n");
						return RETCODE_CODEC_EXIT;
					}
				}
			}

			#if TCC_HEVC___DEC_SUPPORT == 1
			if (handle->codecMode == W_HEVC_DEC) {
				if (handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {	// BS_MODE_INTERRUPT
					unsigned int reason_flush;
					DecOutputInfo info_flush = {0,};

					reason_flush = Wave5ReadReg(0, W5_VPU_VINT_REASON);
					if (reason_flush) {
						Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, reason_flush);
						Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					}
				#ifdef ENABLE_FORCE_ESCAPE
					{
						int iRet = 0;

						iRet = WAVE5_DecGetOutputInfo(handle, &info_flush);
						if (iRet == RETCODE_CODEC_EXIT) {
							DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecGetOutputInfo function. \n");
							return RETCODE_CODEC_EXIT;
						}
					}
				#else
						WAVE5_DecGetOutputInfo(handle, &info_flush);
				#endif
				}
			}
			#endif
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) {
			int bsBufSize;
			PhysicalAddress rdptr, wrptr;

			ret = WAVE5_DecGetBitstreamBuffer(handle, &rdptr, &wrptr, (unsigned int *)&bsBufSize);
			if (ret != RETCODE_SUCCESS) {
			#ifdef ENABLE_FORCE_ESCAPE_2
				if ((ret == RETCODE_CODEC_EXIT) || (gWave5InitFirst_exit > 0)) {
					DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecGetBitstreamBuffer function. %d \n", ret);
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}
			#endif
				return ret;
			}

			{
				long long size;

				if (rdptr < wrptr) {
					size = (long long)(wrptr - rdptr);
				} else {
					long long start_addr, end_addr;
					start_addr = (long long)(handle->CodecInfo.decInfo.openParam.bitstreamBuffer & 0x0FFFFFFFF);
					end_addr = start_addr + handle->CodecInfo.decInfo.openParam.bitstreamBufferSize;
					size = (end_addr - rdptr);
					size += wrptr;
					size -= start_addr;
				}
				size -= 1;

				if (size < 1024) {
					return RETCODE_INSUFFICIENT_BITSTREAM;
				}
			}

			if (WAVE5_PokePendingInstForSimInts()) {
				WAVE5_SetPendingInst(coreIdx, pVpu->m_pCodecInst);
				WAVE5_PeekPendingInstForSimInts(0);
			}

			if ((handle->CodecInfo.decInfo.isFirstFrame == 0) && (pInputParam->m_iBitstreamDataSize != 0)) {
				if ((WAVE5_GetPendingInst(coreIdx) == handle) && (handle->CodecInfo.decInfo.m_iPendingReason & (1 << INT_WAVE5_BSBUF_EMPTY))) {
					int read_ptr, write_ptr, room;

					ret = WAVE5_DecGetBitstreamBuffer(pVpu->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (unsigned int *)&room);
					if (ret != RETCODE_SUCCESS) {
					#ifdef ENABLE_FORCE_ESCAPE_2
						if ((ret == RETCODE_CODEC_EXIT) || (gWave5InitFirst_exit > 0)) {
							DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecGetBitstreamBuffer function. %d \n", ret);
							WAVE5_dec_reset(NULL, (0 | (1 << 16)));
							return RETCODE_CODEC_EXIT;
						}
					#endif
						return ret;
					}

					if (handle->CodecInfo.decInfo.prev_streamWrPtr == write_ptr) {
						pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
						pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_SKIP_OR_EOS;
						pOutParam->m_DecOutInfo.m_iDecodingStatus = 0;
						pOutParam->m_DecOutInfo.m_iOutputStatus = 0;
						pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
						pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
						pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;
						pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
						pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
						pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;

						return RETCODE_INSUFFICIENT_BITSTREAM;
					}
				}
			}
		} else { // BS_MODE_PIC_END
			#if TCC_HEVC___DEC_SUPPORT == 1
			if (pVpu->m_pCodecInst->codecMode == W_HEVC_DEC) {
				ret = WAVE5_DecSetRdPtr(handle, pInputParam->m_BitstreamDataAddr[PA], 1);
				if (ret == RETCODE_CODEC_EXIT) {
					DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecSetRdPtr function. ");
					return RETCODE_CODEC_EXIT;
				}

				ret = WAVE5_DecUpdateBitstreamBuffer(pVpu->m_pCodecInst, pInputParam->m_iBitstreamDataSize); // If size = 0, set stream end flag.
				if (ret == RETCODE_CODEC_EXIT) {
					DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecUpdateBitstreamBuffer function. \n");
					return RETCODE_CODEC_EXIT;
				}
			}
			#endif

			#if TCC_VP9____DEC_SUPPORT == 1
			if (pVpu->m_pCodecInst->codecMode == W_VP9_DEC) {
				if (handle->CodecInfo.decInfo.superframe.nframes > 0) {
					PhysicalAddress superframe_addr;
					unsigned int superframe_size;

					superframe_addr = handle->CodecInfo.decInfo.superframe.frames[handle->CodecInfo.decInfo.superframe.currentIndex];
					superframe_size = handle->CodecInfo.decInfo.superframe.frameSize[handle->CodecInfo.decInfo.superframe.currentIndex];
					WAVE5_DecSetRdPtr(handle, superframe_addr, TRUE);
					ret = WAVE5_DecUpdateBitstreamBuffer(handle, superframe_size);
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecUpdateBitstreamBuffer function. \n");
						return RETCODE_CODEC_EXIT;
					}

					DLOG_VP9("[VP9SUPERFRAME][INP-%d-%d]====>[VPU:%p][F:%4d][Size:%6d/%6d][Addr:%8x][skipFrameMode:%d][index:%d, nFrames:%d]\n",
							 handle->CodecInfo.decInfo.frameNumCount,
							 handle->CodecInfo.decInfo.superframe.currentIndex,
							 pVpu, handle->CodecInfo.decInfo.frameNumCount,
							 superframe_size, pInputParam->m_iBitstreamDataSize, superframe_addr,
							 pInputParam->m_iSkipFrameMode,
							 handle->CodecInfo.decInfo.superframe.currentIndex,
							 handle->CodecInfo.decInfo.superframe.nframes);
				} else {
					int offset = 0;

					if (pVpu->stSetOptions.iUseBitstreamOffset != 0) {
						offset = pInputParam->m_iBitstreamDataOffset;
						DLOGV("framesize: %6d, offset: %d\n", pInputParam->m_iBitstreamDataSize, offset);
					}
					ret = WAVE5_DecSetRdPtr(handle, pInputParam->m_BitstreamDataAddr[PA] + offset, 1);
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecSetRdPtr function. \n");
						return RETCODE_CODEC_EXIT;
					}
					ret = WAVE5_DecUpdateBitstreamBuffer(pVpu->m_pCodecInst, pInputParam->m_iBitstreamDataSize);
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecUpdateBitstreamBuffer function. \n");
						return RETCODE_CODEC_EXIT;
					}
				}
			}
			#endif

			if (handle->CodecInfo.decInfo.isFirstFrame != 0) {
				DLOGD("Detected First Frame !!!\n");
				handle->CodecInfo.decInfo.isFirstFrame = 0;
			}
		}

		if (pInputParam->m_iBitstreamDataSize == 0) {
			int iRet = 0;

			iRet = WAVE5_DecUpdateBitstreamBuffer(handle, 0);
			if (iRet == RETCODE_CODEC_EXIT) {
				DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecUpdateBitstreamBuffer function. \n");
				return RETCODE_CODEC_EXIT;
			}

			if (handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) {
				PhysicalAddress wrptr;
				wrptr = handle->CodecInfo.decInfo.streamWrPtr;
				handle->CodecInfo.decInfo.streamRdPtr = wrptr;
				handle->CodecInfo.decInfo.streamWrPtr = wrptr;
			}
		}

		//. User Data Buffer Setting if it's required.
		if (handle->CodecInfo.decInfo.userDataEnable) {
			handle->CodecInfo.decInfo.userDataBufAddr = pInputParam->m_UserDataAddr[PA];
			handle->CodecInfo.decInfo.userDataBufSize = pInputParam->m_iUserDataBufferSize;
			pOutParam->m_DecOutInfo.m_UserDataAddress[0] = pInputParam->m_UserDataAddr[0];
			pOutParam->m_DecOutInfo.m_UserDataAddress[1] = pInputParam->m_UserDataAddr[1];
		}

		// CQ2
		if (handle->CodecInfo.decInfo.openParam.CQ_Depth > 1) {
			if (handle->CodecInfo.decInfo.skipframeUsed == 0) {
				if ((handle->CodecInfo.decInfo.frameNumCount == 0) || (handle->CodecInfo.decInfo.frameNumCount == 1)) {
					int framebufcnt;

					// frameNumCount is 0
					if (handle->CodecInfo.decInfo.frameNumCount == 0) {
						unsigned int setFlag = 0;
						for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
							setFlag |= (1 << framebufcnt);
						}

						setFlag = (setFlag & ~handle->CodecInfo.decInfo.frameDisplayFlag); // [20191203] frameDisplayFlag is not set again if frameDisplayFlag already set.

						if (setFlag > 0) {
							WAVE5_DecGiveCommand(handle, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
						}
						#ifdef ENABLE_FORCE_ESCAPE
						if (gWave5InitFirst_exit > 0) {
							DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
							return RETCODE_CODEC_EXIT;
						}
						#endif
					}
					// frameNumCount is 1
					else if (handle->CodecInfo.decInfo.frameNumCount == 1) {
						int iRet = 0;
						unsigned int clrFlag = 0;
						for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
							clrFlag |= (1 << framebufcnt);
						}

						if (clrFlag > 0) {
							iRet = WAVE5_DecClrDispFlagEx(handle, clrFlag);
						}
						if (iRet == RETCODE_CODEC_EXIT) {
							DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecClrDispFlagEx function. \n");
							return RETCODE_CODEC_EXIT;
						}
					}
				}
			} else {
			#ifndef TEST_CQ2_DEC_SKIPFRAME_CQCOUNT
				if (handle->CodecInfo.decInfo.frameNumCount == 0) {
					pVpu->iRepeatCntForFBNum1 = 0;
					pVpu->iRepeatCntForFBNum2 = 0;
					DLOGV("frameNumCount is 0 \n");
				} else if (handle->CodecInfo.decInfo.frameNumCount == 1) {
					DLOGV("frameNumCount is 1 \n");
					pVpu->iRepeatCntForFBNum1++;

					if (pVpu->iRepeatCntForFBNum1 == 1) {
						WAVE5_dec_hold_all_framebuffer(pVpu);
					}
					#ifdef ENABLE_FORCE_ESCAPE
					if (gWave5InitFirst_exit > 0) {
						DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
						return RETCODE_CODEC_EXIT;
					}
					#endif
				} else {
					if (
						((handle->CodecInfo.decInfo.frameNumCount == 2) && (handle->CodecInfo.decInfo.openParam.CQ_Depth == 1)) ||
						((handle->CodecInfo.decInfo.frameNumCount >= 2) && (handle->CodecInfo.decInfo.openParam.CQ_Depth > 1) && (handle->CodecInfo.decInfo.instanceQueueCount > 0))) {
						int iRet = 0;
						int framebufcnt = 0;
						unsigned int setFlag = 0;
						unsigned int clrFlag = 0;

						pVpu->iRepeatCntForFBNum2++;
						if (pVpu->iRepeatCntForFBNum2 == 1) {
							clrFlag = 0;
							for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
								if (handle->CodecInfo.decInfo.previous_frameDisplayFlag & (1 << framebufcnt)) {
									/*dummy*/
								} else {
									clrFlag |= (1 << framebufcnt);
								}
							}

							if (clrFlag > 0) {
								DLOGV("release display framebuffer [frameDisplayFlag:0x%08X,prev:0x%08X][clearDisplayIndexes:0x%08X][skipframeUsed:%d] clear(0x%08X)\n",
									handle->CodecInfo.decInfo.frameDisplayFlag,
									handle->CodecInfo.decInfo.previous_frameDisplayFlag,
									handle->CodecInfo.decInfo.clearDisplayIndexes,
									handle->CodecInfo.decInfo.skipframeUsed, clrFlag);

								iRet = WAVE5_DecClrDispFlagEx(handle, clrFlag);
								if (iRet == RETCODE_CODEC_EXIT) {
									DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecClrDispFlagEx function. \n");
									return RETCODE_CODEC_EXIT;
								}

								DLOGV("release display framebuffer [frameDisplayFlag:0x%08X,prev:0x%08X][clearDisplayIndexes:0x%08X][skipframeUsed:%d] result\n",
									handle->CodecInfo.decInfo.frameDisplayFlag,
									handle->CodecInfo.decInfo.previous_frameDisplayFlag,
									handle->CodecInfo.decInfo.clearDisplayIndexes,
									handle->CodecInfo.decInfo.skipframeUsed);
							}

							// FIXME: Consider to remove SET_DISPLAY_FLAG block. It looks no need.
							setFlag = 0;
							for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
								if (handle->CodecInfo.decInfo.previous_frameDisplayFlag & (1 << framebufcnt)) {
									setFlag |= (1 << framebufcnt);
								}
							}

							if (setFlag > 0) {
								DLOGV("restore display framebuffer [frameDisplayFlag:0x%08X,prev:0x%08X][clearDisplayIndexes:0x%08X][skipframeUsed:%d] set flag(0x%08X)\n",
									handle->CodecInfo.decInfo.frameDisplayFlag,
									handle->CodecInfo.decInfo.previous_frameDisplayFlag,
									handle->CodecInfo.decInfo.clearDisplayIndexes,
									handle->CodecInfo.decInfo.skipframeUsed,
									setFlag);

								WAVE5_DecGiveCommand(handle, DEC_SET_DISPLAY_FLAG_EX, &setFlag);

								DLOGV("restore display framebuffer [frameDisplayFlag:0x%08X,prev:0x%08X][clearDisplayIndexes:0x%08X][skipframeUsed:%d] result\n",
									handle->CodecInfo.decInfo.frameDisplayFlag,
									handle->CodecInfo.decInfo.previous_frameDisplayFlag,
									handle->CodecInfo.decInfo.clearDisplayIndexes,
									handle->CodecInfo.decInfo.skipframeUsed);
							}
						}

						#ifdef ENABLE_FORCE_ESCAPE
						if (gWave5InitFirst_exit > 0) {
							DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
							return RETCODE_CODEC_EXIT;
						}
						#endif
					}
				}
			#else
				int framebufcnt;

				if ((handle->CodecInfo.decInfo.frameNumCount == 1) || (handle->CodecInfo.decInfo.frameNumCount == 2)) {
					DecQueueStatusInfo queueInfo;
					WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &queueInfo);

					if (handle->CodecInfo.decInfo.frameNumCount == 1) {
						if (queueInfo.totalQueueCount == 0) // no command running if (queueInfo.totalQueueCount < (COMMAND_QUEUE_DEPTH-1))  // more command can be queueing
						{
							unsigned int setFlag = 0;
							for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
								setFlag |= (1 << framebufcnt);
							}

							if (setFlag > 0) {
								WAVE5_DecGiveCommand(handle, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
							}
						}
					} else if (handle->CodecInfo.decInfo.frameNumCount == 2) {
						unsigned int clrFlag = 0;
						for (framebufcnt = 0; framebufcnt < pVpu->m_iNeedFrameBufCount; framebufcnt++) {
							if ((handle->CodecInfo.decInfo.dispidx_backup & (1 << framebufcnt)) == 0) {
								clrFlag |= (1 << framebufcnt);
							}
						}
						handle->CodecInfo.decInfo.dispidx_backup = 0;

						if (clrFlag > 0) {
							WAVE5_DecClrDispFlagEx(handle, clrFlag);
						}
					}
				}
			#endif
			}
		}

		// enable superframe internal parsing
		dec_param.vp9SuperFrameParseEnable = FALSE;

		#if TCC_VP9____DEC_SUPPORT == 1
		if (handle->CodecInfo.decInfo.openParam.bitstreamFormat == STD_VP9) {
			if (handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {
				dec_param.vp9SuperFrameParseEnable = TRUE;
			}
		}
		#endif

		if( (handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
			&& (WAVE5_GetPendingInst(coreIdx) == handle )
			&& (handle->CodecInfo.decInfo.m_iPendingReason & (1<<INT_WAVE5_BSBUF_EMPTY))
			)
		{
			Wave5WriteReg(coreIdx, handle->CodecInfo.decInfo.streamWrPtrRegAddr, handle->CodecInfo.decInfo.streamWrPtr);
		}
		else {
			unsigned int timeoutcount = 0;

			DLOGV("|WAVE5_DecStartOneFrame(repeat:%d,%d)|F:%d|Size:%d|frameDisplayFlag:0x%x,prev:0x%x|clearDisplayIndex:0x%x|\n",
				  pVpu->iRepeatCntForFBNum1, pVpu->iRepeatCntForFBNum2, handle->CodecInfo.decInfo.frameNumCount,
				  pInputParam->m_iBitstreamDataSize,
				  handle->CodecInfo.decInfo.frameDisplayFlag,
				  handle->CodecInfo.decInfo.previous_frameDisplayFlag,
				  handle->CodecInfo.decInfo.clearDisplayIndexes);

			do {
				// command queuing(DEC_PIC)
				ret = WAVE5_DecStartOneFrame(handle, &dec_param);
				if ((pVpu->iCqCount > 1) && (LOG_VAR(Trace) == 1)) {
					/* RETCODE_QUEUEING_FAILURE IS RETURNED FROM THE VPU SUPPORTING COMMAND QUEUE FUNCTION. */
					WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
					DLOGT("[QStatus:Dec(%d)] count(%d)/total(%d)/set(%d)", ret,
						qStatus.instanceQueueCount, qStatus.totalQueueCount, pVpu->iCqCount);
				}

			#ifdef CQ_BUFFER_RESILIENCE_HEVC_QUEUEING_FAILURE
				if (pVpu->m_iBitstreamFormat == STD_HEVC) {
					if (ret == RETCODE_QUEUEING_FAILURE) {
						// If the input stream is a B-frame while the B-frame skip option is enabled,
						// the frame cannot be queued.
						if (i_instance_queue_count_before_dec == qStatus.instanceQueueCount) {
							if (dec_param.skipframeMode == 2) {
								dec_param.skipframeMode = 0;
								handle->CodecInfo.decInfo.frameskipmode = dec_param.skipframeMode;
								DLOGW("Dec:ret(RETCODE_QUEUEING_FAILURE) => Disable SkipFrameMode(2=>0)");
							}
						}
					}
				}
			#endif

				if (ret == RETCODE_CODEC_EXIT) {
					DLOGA("exited since WAVE5_DecStartOneFrame");
					return RETCODE_CODEC_EXIT;
				}
				#ifdef ENABLE_FORCE_ESCAPE
				if (gWave5InitFirst_exit > 0) {
					DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}
				#endif

				#ifdef TEST_BUFFERFULL_POLLTIMEOUT
				if (handle->CodecInfo.decInfo.prev_buffer_full == 1) {
					if (ret == RETCODE_QUEUEING_FAILURE) {
						pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
						pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_BUF_FULL;
						pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
						pOutParam->m_DecOutInfo.m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;

						DLOGA("RETCODE_QUEUEING_FAILURE,prev_buffer_full(1),DEC_BUF_FULL,return RETCODE_SUCCESS");
						return RETCODE_SUCCESS;
					}
				}
				#endif

				#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (timeoutcount > VPU_BUSY_CHECK_USLEEP_CNT) {
						DLOGA("RETCODE_CODEC_EXIT: timeoutcount > VPU_BUSY_CHECK_USLEEP_CNT \n");
						WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
						return RETCODE_CODEC_EXIT;
					}
					#ifdef ENABLE_FORCE_ESCAPE
					if (gWave5InitFirst_exit > 0) {
						DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
						WAVE5_dec_reset(NULL, (0 | (1 << 16)));
						return RETCODE_CODEC_EXIT;
					}
					#endif
				}
				#endif

				timeoutcount++;
				if (timeoutcount > VPU_BUSY_CHECK_TIMEOUT) {
					DLOGA("RETCODE_CODEC_EXIT: timeoutcount > VPU_BUSY_CHECK_TIMEOUT \n");
					WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}

			} while (ret == RETCODE_QUEUEING_FAILURE);

			pOutParam->m_DecOutInfo.m_Reserved[19] = ret;
			if (ret == RETCODE_CODEC_EXIT) {
				DLOGA("exited since WAVE5_DecStartOneFrame!");
				#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth, handle->CodecInfo.decInfo.openParam.fw_writing_disable);
				#endif
				WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
				return ret;
			}

			handle->CodecInfo.decInfo.queueing_fail_flag = 0;
			if (ret == RETCODE_QUEUEING_FAILURE) {
				DLOGA("RETCODE_QUEUEING_FAILURE");
				handle->CodecInfo.decInfo.queueing_fail_flag = 1;
				#ifdef TEST_OUTPUTINFO
				pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
				pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_QUEUEING_FAIL;
				pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
				pOutParam->m_DecOutInfo.m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;
				#endif
				ret = RETCODE_SUCCESS;
			}
		#if TCC_VP9____DEC_SUPPORT == 1
			else if ((ret == RETCODE_FIND_SUPERFRAME) && (handle->CodecInfo.decInfo.openParam.bitstreamFormat == STD_VP9)) {
				// The return value RETCODE_FIND_SUPERFRAME indicates that it is a superframe, meaning it entered DecStartOneFrame but
				// this command has not yet been added to the CQ (FAILED for adding a command into VCPU QUEUE).
				// Therefore, the subframes of the analyzed superframe should be sequentially added to the DEC queue again.
				int i;

				pOutParam->m_DecOutInfo.m_iIsSuperFrame = 1;
				pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiNframes = handle->CodecInfo.decInfo.superframe.nframes;
				pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiCurrentIdx = handle->CodecInfo.decInfo.superframe.currentIndex;
				pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiDoneIdx = handle->CodecInfo.decInfo.superframe.doneIndex;
				for (i = 0; i < pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiNframes; i++) {
					pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiFrames[i] = handle->CodecInfo.decInfo.superframe.frames[i];
					pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiFrameSize[i] = handle->CodecInfo.decInfo.superframe.frameSize[i];
				}

				{
					PhysicalAddress superframe_addr;
					unsigned int superframe_size;
					superframe_addr = handle->CodecInfo.decInfo.superframe.frames[handle->CodecInfo.decInfo.superframe.currentIndex];
					superframe_size = handle->CodecInfo.decInfo.superframe.frameSize[handle->CodecInfo.decInfo.superframe.currentIndex];

					ret = WAVE5_DecSetRdPtr(handle, superframe_addr, TRUE);
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecSetRdPtr function. \n");
						return RETCODE_CODEC_EXIT;
					}
					ret = WAVE5_DecUpdateBitstreamBuffer(handle, superframe_size);
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecUpdateBitstreamBuffer function. \n");
						return RETCODE_CODEC_EXIT;
					}

					DLOG_VP9("[VP9SUPERFRAME][INP-%d-%d]====>[VPU:%p][F:%4d][Size:%6d/%6d][Addr:%8x][skipFrameMode:%d][index:%d, nFrames:%d]\n",
							 handle->CodecInfo.decInfo.frameNumCount,
							 handle->CodecInfo.decInfo.superframe.currentIndex,
							 pVpu, handle->CodecInfo.decInfo.frameNumCount,
							 superframe_size, pInputParam->m_iBitstreamDataSize, superframe_addr,
							 pInputParam->m_iSkipFrameMode,
							 handle->CodecInfo.decInfo.superframe.currentIndex,
							 handle->CodecInfo.decInfo.superframe.nframes);

					DLOGV("WAVE5_DecStartOneFrame(repeat:%d,%d), Size(%d), frameDisplayFlag=0x%x,previous_frameDisplayFlag=0x%x,clearDisplayIndex=0x%x\n",
						  pVpu->iRepeatCntForFBNum1, pVpu->iRepeatCntForFBNum2,
						  pInputParam->m_iBitstreamDataSize,
						  handle->CodecInfo.decInfo.frameDisplayFlag,
						  handle->CodecInfo.decInfo.previous_frameDisplayFlag,
						  handle->CodecInfo.decInfo.clearDisplayIndexes);

					ret = WAVE5_DecStartOneFrame(handle, &dec_param);
					pOutParam->m_DecOutInfo.m_Reserved[19] = ret;
					if (ret == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecStartOneFrame function. \n");
						#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
						WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth, handle->CodecInfo.decInfo.openParam.fw_writing_disable);
						#endif
						WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
						return ret;
					}
					handle->CodecInfo.decInfo.queueing_fail_flag = 0;
					if (ret == RETCODE_QUEUEING_FAILURE) {
						handle->CodecInfo.decInfo.queueing_fail_flag = 1;
						#ifdef TEST_OUTPUTINFO
						pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
						pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_QUEUEING_FAIL;
						pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
						pOutParam->m_DecOutInfo.m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;
						#endif
						ret = RETCODE_SUCCESS;
					}

					// Increment after DecStartOneFrame.
					handle->CodecInfo.decInfo.superframe.currentIndex++;
					pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiCurrentIdx = handle->CodecInfo.decInfo.superframe.currentIndex;
					if (handle->CodecInfo.decInfo.superframe.currentIndex >= handle->CodecInfo.decInfo.superframe.nframes) {
						handle->CodecInfo.decInfo.superframe.nframes = 0;
						DLOG_VP9("[VP9SUPERFRAME] Done\n");
						// pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiNframes = handle->CodecInfo.decInfo.superframe.nframes;
					}
				}

				#ifdef TEST_OUTPUTINFO
				pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_NO_FB;
				pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_VP9_SUPER_FRAME;
				pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
				pOutParam->m_DecOutInfo.m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;
				#endif
				ret = RETCODE_SUCCESS;
			}
		#endif
			else {
				#if TCC_VP9____DEC_SUPPORT == 1
				if ((handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) && (pVpu->m_pCodecInst->codecMode == W_VP9_DEC)) {
					if (handle->CodecInfo.decInfo.superframe.nframes > 0) {
						// Increment after DecStartOneFrame.
						handle->CodecInfo.decInfo.superframe.currentIndex++;
						pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiCurrentIdx = handle->CodecInfo.decInfo.superframe.currentIndex;
						#if 0
						DLOG_VP9("[VP9SUPERFRAME][F:%d][index:%d, nFrames:%d]\n",
								 handle->CodecInfo.decInfo.frameNumCount,
								 handle->CodecInfo.decInfo.superframe.currentIndex,
								 handle->CodecInfo.decInfo.superframe.nframes);
						#endif
						if (handle->CodecInfo.decInfo.superframe.currentIndex >= handle->CodecInfo.decInfo.superframe.nframes) {
							handle->CodecInfo.decInfo.superframe.nframes = 0;
							DLOG_VP9("[VP9SUPERFRAME] Done\n");
							// pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiNframes = handle->CodecInfo.decInfo.superframe.nframes;
						}
					}
				}
				#endif

				if (ret != RETCODE_SUCCESS) {
					#ifdef ENABLE_FORCE_ESCAPE
					if (gWave5InitFirst_exit > 0) {
						DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
						WAVE5_dec_reset(NULL, (0 | (1 << 16)));
						return RETCODE_CODEC_EXIT;
					}
					#endif
					return ret;
				}
			}
		}

		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif

		handle->CodecInfo.decInfo.m_iPendingReason = 0;

SEQ_CHANGE_OUTINFO_CQ1:
		// Call interrupt callback
		while (1) {
			int iRet = 0;

			if (Wave5ReadReg(coreIdx, W5_VCPU_CUR_PC) == 0x0) {
				DLOGA("RETCODE_CODEC_EXIT: PC is 0 \n");
				WAVE5_dec_reset(pVpu, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}

			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
			#endif

			if (wave5_interrupt != NULL) {
				DLOGV("[F:%d] Call interrupt callback\n", handle->CodecInfo.decInfo.frameNumCount);
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
			}
			if (iRet == RETCODE_CODEC_EXIT) {
				DLOGA("RETCODE_CODEC_EXIT returned from interrupt callback function. \n");
				#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, handle->CodecInfo.decInfo.openParam.CQ_Depth);
				#endif
				WAVE5_dec_reset(pVpu, (0 | (1 << 16)));

				return RETCODE_CODEC_EXIT;
			}

			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				if (iRet & (1 << INT_WAVE5_DEC_PIC)) {
					DLOGV("\tinterrupt reason:%x, DEC_PIC\n", iRet);
					if ((iRet & (1 << INT_WAVE5_BSBUF_EMPTY)) &&
						(handle->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)) {
						int read_ptr, write_ptr, room;

						DLOGV("\tinterrupt reason:%x, DEC_PIC | BSBUF_EMPTY\n", iRet);

						(void)WAVE5_DecGetBitstreamBuffer(pVpu->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (unsigned int *)&room);
						handle->CodecInfo.decInfo.prev_streamWrPtr = write_ptr;
						WAVE5_SetPendingInst(coreIdx, handle);
						handle->CodecInfo.decInfo.m_iPendingReason = (1 << INT_WAVE5_BSBUF_EMPTY);
						/*
						 * [20191203] Both interrputs for INT_WAVE5_DEC_PIC and INT_WAVE5_BSBUF_EMPTY
						 * happened at the same time. g_KeepPendingInstForSimInts is set to 1.
						 */
						WAVE5_PeekPendingInstForSimInts(1);
					}
					break;
				} else {
					if (iRet & (1 << INT_WAVE5_BSBUF_EMPTY)) {
						DLOGV("\tinterrupt reason:%x, BSBUF_EMPTY\n", iRet);
						if (handle->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
							WAVE5_SetPendingInst(coreIdx, 0);
							return RETCODE_FRAME_NOT_COMPLETE;
						} else {
							int read_ptr, write_ptr, room;
							ret = WAVE5_DecGetBitstreamBuffer(pVpu->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (unsigned int *)&room);
							if (ret != RETCODE_SUCCESS) {
							#ifdef ENABLE_FORCE_ESCAPE_2
								if ((ret == RETCODE_CODEC_EXIT) || (gWave5InitFirst_exit > 0)) {
									DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecGetBitstreamBuffer function. %d \n", ret);
									WAVE5_dec_reset(NULL, (0 | (1 << 16)));
									return RETCODE_CODEC_EXIT;
								}
							#endif
								return ret;
							}

							handle->CodecInfo.decInfo.prev_streamWrPtr = write_ptr;

							WAVE5_SetPendingInst(coreIdx, handle);
							handle->CodecInfo.decInfo.m_iPendingReason = (1 << INT_WAVE5_BSBUF_EMPTY);
							return RETCODE_INSUFFICIENT_BITSTREAM;
						}
					} else if (iRet & (1 << INT_WAVE5_FLUSH_INSTANCE)) {
						DLOGV("\tinterrupt reason:%x, FLUSH_INSTANCE\n", iRet);
						// break;
					} else if (iRet & ((1 << INT_WAVE5_INIT_SEQ) | (1 << INT_WAVE5_SET_FRAMEBUF))) {
						DLOGV("\tinterrupt reason:%x, INIT_SEQ | SET_FRAMEBUF\n", iRet);
						return RETCODE_FAILURE;
					}
				}
			}
			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
			#endif
		}
	}

#ifdef TEST_BUFFERFULL_POLLTIMEOUT
FB_CLEAR_OUTINFO:
#endif

SEQ_CHANGE_OUTINFO_CQ2:
	if ((pVpu->iCqCount > 1) && (LOG_VAR(Trace) == 1)) {
		/* RETCODE_QUEUEING_FAILURE IS RETURNED FROM THE VPU SUPPORTING COMMAND QUEUE FUNCTION. */
		WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
		DLOGT("[QStatus:Dec2] count(%d)/total(%d)/set(%d)",
			  qStatus.instanceQueueCount, qStatus.totalQueueCount, pVpu->iCqCount);
	}
	//! [3] Get Decoder Output
	{
		int disp_idx = 0;
		DecOutputInfo info = {0,};

		DLOGV("[F:%d] Get result\n", handle->CodecInfo.decInfo.frameNumCount);
		do {
			ret = WAVE5_DecGetOutputInfo(handle, &info);
			if ((pVpu->iCqCount > 1) && (LOG_VAR(Trace) == 1)) {
				/* RETCODE_QUEUEING_FAILURE IS RETURNED FROM THE VPU SUPPORTING COMMAND QUEUE FUNCTION. */
				WAVE5_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
				DLOGT("[QStatus:Out(%d)] count(%d)/total(%d)/set(%d)", ret,
					qStatus.instanceQueueCount, qStatus.totalQueueCount, pVpu->iCqCount);
			}
			if (ret == RETCODE_CODEC_EXIT) {
				DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecGetOutputInfo function. \n");
				return RETCODE_CODEC_EXIT;
			}

			#ifdef ENABLE_VDI_STATUS_LOG
			_vpu_4k_d2_dump_status();
			#endif

			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
			#endif

		/* Checking loop condition:
			It checks if the function result is RETCODE_REPORT_NOT_READY.
			If ret equals RETCODE_REPORT_NOT_READY,
			it means there is no result in the result queue or the decoder is still decoding to generate the result queue.
			Since the decoding command is already in progress, in this case, the loop starts again. */
		} while (ret == RETCODE_REPORT_NOT_READY);
		pOutParam->m_DecOutInfo.m_Reserved[20] = ret;

		// CQ2
		// TODO:
		if (handle->CodecInfo.decInfo.openParam.CQ_Depth > 1) {
			if (handle->CodecInfo.decInfo.skipframeUsed) {
				if (handle->CodecInfo.decInfo.frameNumCount == 0) {
					if (info.indexFrameDecoded >= 0) {
						DLOGV("[skipframeUsed:%d][F:%d=>%d][Index(%d/%d)]\n",
							handle->CodecInfo.decInfo.skipframeUsed,
							handle->CodecInfo.decInfo.frameNumCount, handle->CodecInfo.decInfo.frameNumCount + 1,
							info.indexFrameDecoded, info.indexFrameDisplay);
						handle->CodecInfo.decInfo.frameNumCount++;
					}
				} else {
					// Check if decoding succeeds before increasing count
					if (info.indexFrameDecoded != DECODED_IDX_FLAG_SKIP_OR_EOS) {
						DLOGV("[skipframeUsed:%d][F:%d=>%d][Index(%d/%d)]\n",
							handle->CodecInfo.decInfo.skipframeUsed,
							handle->CodecInfo.decInfo.frameNumCount, handle->CodecInfo.decInfo.frameNumCount + 1,
							info.indexFrameDecoded, info.indexFrameDisplay);
						handle->CodecInfo.decInfo.frameNumCount++;
					}
				}

				if ((info.indexFrameDecoded == DECODED_IDX_FLAG_NO_FB) && (handle->CodecInfo.decInfo.frameNumCount == 2)) {
					DLOGV("[skipframeUsed:%d][IndexFrameDecoded(-1=>-2)]\n", handle->CodecInfo.decInfo.skipframeUsed);
					info.indexFrameDecoded = DECODED_IDX_FLAG_SKIP_OR_EOS;
				}
			} else {
				// Check if decoding succeeds before increasing count
				if (info.indexFrameDecoded != DECODED_IDX_FLAG_SKIP_OR_EOS) {
					DLOGV("[skipframeUsed:%d][F:%d=>%d][Index(%d/%d)]\n",
						  handle->CodecInfo.decInfo.skipframeUsed,
						  handle->CodecInfo.decInfo.frameNumCount, handle->CodecInfo.decInfo.frameNumCount + 1,
						  info.indexFrameDecoded, info.indexFrameDisplay);
					handle->CodecInfo.decInfo.frameNumCount++;
				}

				if ((info.indexFrameDecoded == DECODED_IDX_FLAG_NO_FB) && (handle->CodecInfo.decInfo.frameNumCount == 1)) {
					DLOGV("[skipframeUsed:%d][IndexFrameDecoded(-1=>-2)]\n", handle->CodecInfo.decInfo.skipframeUsed);
					info.indexFrameDecoded = DECODED_IDX_FLAG_SKIP_OR_EOS;
				}
			}
		} else {
			if (info.indexFrameDecoded >= 0) {
				handle->CodecInfo.decInfo.frameNumCount++;
			}
		}

		#ifdef ENABLE_VDI_STATUS_LOG
		_vpu_4k_d2_dump_status();
		#endif

		if (ret != RETCODE_SUCCESS) {
			if (handle->CodecInfo.decInfo.superframe.doneIndex > 0) {
				DLOG_VP9("[VP9SUPERFRAME][INP-E-1]====>index=%d\n",
						 handle->CodecInfo.decInfo.superframe.currentIndex);

				handle->CodecInfo.decInfo.superframe.doneIndex--;
				pOutParam->m_DecOutInfo.m_SuperFrameInfo.m_uiDoneIdx = handle->CodecInfo.decInfo.superframe.doneIndex;
				if (handle->CodecInfo.decInfo.superframe.doneIndex == 0) {
					if (wave5_memset) {
						wave5_memset(&handle->CodecInfo.decInfo.superframe, 0x00, sizeof(VP9Superframe), 0);
					}

					DLOG_VP9("[VP9SUPERFRAME][INP-E-2]====>index=%d\n",
							 handle->CodecInfo.decInfo.superframe.currentIndex);
				}

				if (handle->CodecInfo.decInfo.superframe.doneIndex > 0) {
					pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_VP9_SUPER_FRAME;
					if (pOutParam->m_DecOutInfo.m_iDispOutIdx >= 0) {
						pOutParam->m_DecOutInfo.m_iDecodedIdx = pOutParam->m_DecOutInfo.m_iDispOutIdx;
					}
				}
			}
			return ret;
		}

		WAVE5_dec_out_info(pVpu, &pOutParam->m_DecOutInfo, &info, ret);
		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif

		if ((pOutParam->m_DecOutInfo.m_iDecodedIdx >= pVpu->m_iFrameBufferCount) || (pOutParam->m_DecOutInfo.m_iDecodedIdx < -3)) {
			pOutParam->m_DecOutInfo.m_iDecodedIdx = DECODED_IDX_FLAG_SKIP_OR_EOS;
			pOutParam->m_DecOutInfo.m_iDecodingStatus = 0;
		}

		if ((pOutParam->m_DecOutInfo.m_iDispOutIdx >= pVpu->m_iFrameBufferCount) || (pOutParam->m_DecOutInfo.m_iDispOutIdx < -4)) {
			pOutParam->m_DecOutInfo.m_iDispOutIdx = DISPLAY_IDX_FLAG_NO_OUTPUT;
			pOutParam->m_DecOutInfo.m_iOutputStatus = 0;
			DLOGD("dispOutIdx(%d):FBCnt(%d) since WAVE5_dec_out_info\n",
				  pOutParam->m_DecOutInfo.m_iDispOutIdx,
				  pVpu->m_iFrameBufferCount);
		}

		//. User Data Buffer Setting if it's required.
		if (handle->CodecInfo.decInfo.userDataEnable) {
			handle->CodecInfo.decInfo.userDataBufAddr = pInputParam->m_UserDataAddr[PA];
			handle->CodecInfo.decInfo.userDataBufSize = pInputParam->m_iUserDataBufferSize;
			pOutParam->m_DecOutInfo.m_UserDataAddress[0] = pInputParam->m_UserDataAddr[0];
			pOutParam->m_DecOutInfo.m_UserDataAddress[1] = pInputParam->m_UserDataAddr[1];
		}

		if ((info.decOutputExtData.userDataNum > 0) && (handle->CodecInfo.decInfo.userDataEnable != 0)) {
			if (handle->CodecInfo.decInfo.vbUserData.virt_addr != NULL) {
				if ((pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS) && (pOutParam->m_DecOutInfo.m_iDecodedIdx >= 0)) {
					GetDecUserData(pVpu, pOutParam, info);
				} else {
					pOutParam->m_DecOutInfo.m_uiUserData = 0;
				}
			} else {
				pOutParam->m_DecOutInfo.m_uiUserData = 0;
			}
		}

		if (pOutParam->m_DecOutInfo.m_iDispOutIdx > MAX_GDI_IDX) {
			DLOGA("RETCODE_CODEC_EXIT: DispOutIdx (%d) \n", pOutParam->m_DecOutInfo.m_iDispOutIdx);
			return RETCODE_CODEC_EXIT;
		}

		if (pOutParam->m_DecOutInfo.m_iDecodedIdx > MAX_GDI_IDX) {
			DLOGA("RETCODE_CODEC_EXIT: DecodedIdx (%d) \n", pOutParam->m_DecOutInfo.m_iDecodedIdx);
			return RETCODE_CODEC_EXIT;
		}

		DLOGV("<====[OUT][VPU:%8x|cnt:%3d]"
			  "[Dec|%d:%-7s|%s|Idx:%2d|%4dx%-4d][Out|%d:%-7s|Idx:%2d]",
			  pVpu, handle->CodecInfo.decInfo.frameNumCount,
			  pOutParam->m_DecOutInfo.m_iDecodingStatus, (pOutParam->m_DecOutInfo.m_iDecodingStatus == 1) ? "Success" : "Failure",
			  get_pic_type_str(pOutParam->m_DecOutInfo.m_iPicType),
			  pOutParam->m_DecOutInfo.m_iDecodedIdx, info.decPicWidth, info.decPicHeight,
			  pOutParam->m_DecOutInfo.m_iOutputStatus, (pOutParam->m_DecOutInfo.m_iOutputStatus == 1) ? "Success" : "Failure",
			  pOutParam->m_DecOutInfo.m_iDispOutIdx);

		#ifdef PROFILE_DECODING_TIME_INCLUDE_TICK
		if (pVpu->stSetOptions.iMeasureDecPerf == 1) {
			DisplayDecodedInformation(handle, handle->CodecInfo.decInfo.openParam.bitstreamFormat, handle->CodecInfo.decInfo.frameNumCount, &info);
		}
		#endif

// IGNORE VP9 SUPERFRAME ALTREF OUTPUT and KEEP THE PREVIOUS OUTPUT
//
// - Processing vp9 superframe in the vpu library takes a long time to decode.
//   Therefore, vp9 superframe should be splitted and decoded by the kernel vpu driver.
//   However, in this case, altref frames should be handled to be ignored to prevent output overwrite by continuous fast input.
#if 0 // FIXME for AltRef of splitted superframe
		#if TCC_VP9____DEC_SUPPORT == 1
		if ((handle->CodecInfo.decInfo.openParam.bitstreamFormat == STD_VP9) && (pVpu->stSetOptions.iUseBitstreamOffset == 1))
		{
			if (pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS)
			{
				if ((pOutParam->m_DecOutInfo.m_iDecodedIdx >= 0) && (pOutParam->m_DecOutInfo.m_iDispOutIdx == DISPLAY_IDX_FLAG_NO_OUTPUT)) //AltRef
				{
					if ((pVpu->iPrevDecodedIdx >= 0) && (pVpu->iPrevDispOutIdx >= 0))
					{
						DLOGE("IGNORE VP9 SUPERFRAME ALTREF OUTPUT!!!, (Dec:%2d|Out:%d) ==> KEEP THE PREVIOUS OUTPUT!!!, (Dec:%2d|Out:%2d)"
							, pOutParam->m_DecOutInfo.m_iDecodedIdx, pOutParam->m_DecOutInfo.m_iDispOutIdx
							, pVpu->iPrevDecodedIdx, pVpu->iPrevDispOutIdx);

						pOutParam->m_DecOutInfo.m_iDecodedIdx = pVpu->iPrevDecodedIdx;
						pOutParam->m_DecOutInfo.m_iDispOutIdx = pVpu->iPrevDispOutIdx;

						pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_OUTPUT_SUCCESS;
						pOutParam->m_DecOutInfo.m_iOutputStatus   = VPU_DEC_OUTPUT_SUCCESS;
						return RETCODE_SUCCESS;
					}
				}
			}
		}
		#endif
#endif

		if (pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS) {
			int decoded_idx;
			int decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx;

			//! Decoded Buffer
			decoded_idx = pOutParam->m_DecOutInfo.m_iDecodedIdx;

			if (decoded_idx > MAX_GDI_IDX) {
				DLOGA("RETCODE_CODEC_EXIT: DecodedIdx (%d) \n", pOutParam->m_DecOutInfo.m_iDecodedIdx);
				return RETCODE_CODEC_EXIT;
			}

			if (decoded_idx >= 0) {
				if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
					if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
						decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					} else {
						decoded_idx = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
				}

				pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth = info.lumaBitdepth;
				pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].lumaBitDepth = info.lumaBitdepth;
				pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth = info.chromaBitdepth;
				pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].chromaBitDepth = info.chromaBitdepth;
			}
		}

		//! Store Image
		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
			#ifdef PLATFORM_FPGA
			int stride = ((pVpu->m_iPicWidth + 15) & ~15);
			#endif
			int disp_idx;
			int disp_idx_comp = pOutParam->m_DecOutInfo.m_iDispOutIdx;

			//! Display Buffer
			pVpu->m_iDecodedFrameCount++;
			pVpu->m_iDecodedFrameCount = pVpu->m_iDecodedFrameCount % pVpu->m_iFrameBufferCount;
			disp_idx = pOutParam->m_DecOutInfo.m_iDispOutIdx;

			if (disp_idx > MAX_GDI_IDX) {
				DLOGA("RETCODE_CODEC_EXIT: disp_idx (%d) \n", disp_idx);
				return RETCODE_CODEC_EXIT;
			}

			if (pVpu->m_iIndexFrameDisplay >= 0) {
				pVpu->m_iIndexFrameDisplayPrev = pVpu->m_iIndexFrameDisplay;
			}
			pVpu->m_iIndexFrameDisplay = disp_idx;

			if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					disp_idx_comp = pOutParam->m_DecOutInfo.m_iDispOutIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
				} else {
					disp_idx = pOutParam->m_DecOutInfo.m_iDispOutIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
				}
				pVpu->m_iIndexFrameDisplayPrev = pVpu->m_iIndexFrameDisplay;
				pVpu->m_iIndexFrameDisplay = disp_idx;
			}

			if (disp_idx >= 0) {
				// Physical Address
				pOutParam->m_pDispOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pDispOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pDispOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);
				// Virtual Address
				pOutParam->m_pDispOut[VA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
				pOutParam->m_pDispOut[VA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);
				pOutParam->m_pDispOut[VA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCr);

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufCb;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufCb;

				if (handle->CodecInfo.decInfo.enableAfbce) {
					DecOutputInfo dispInfo = {0};
					VpuRect rcDisplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcCompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb;

					rcDisplay.left = 0;
					rcDisplay.top = 0;
					rcDisplay.right = info.dispPicWidth;
					rcDisplay.bottom = info.dispPicHeight;
					dispInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					dispInfo.dispFrame.cbcrInterleave = 0;
					dispInfo.dispFrame.nv21 = 0;
					dispInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					dispInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					// dispInfo.dispFrame.lumaBitDepth =  (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : info.dispFrame.orgLumaBitDepth  ;
					// dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.orgChromaBitDepth;
					dispInfo.dispFrame.lumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispPicWidth = info.dispPicWidth;
					dispInfo.dispPicHeight = info.dispPicHeight;
					dispInfo.dispFrame.endian = info.dispFrame.endian;
					dispInfo.dispFrame.bufY = info.dispFrame.bufY;

					GetAfbceData(handle, &dispInfo, rcDisplay, info.dispFrame.lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					for (compnum = 0; compnum < 3; compnum++) {
						pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];
					}

					{
						int i, j;

						for (i = 0; i < 20; i++) {
							for (j = 0; j < 3; j++)
								pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCY;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCY;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCC;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCC;
				} else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
					} else {
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDisplayWidth, pOutParam->m_DecOutInfo.m_iDisplayHeight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDisplayWidth, pOutParam->m_DecOutInfo.m_iDisplayHeight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayHeight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayHeight));
						}
					}
					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize + 4095) & ~4095) + 4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize + 4095) & ~4095) + 4096;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].virt_addr;

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth;

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableLumaSize = handle->CodecInfo.decInfo.fbcYTblSize;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableChromaSize = handle->CodecInfo.decInfo.fbcCTblSize;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

				pOutParam->m_DecOutInfo.m_Reserved[4] = pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth;
				if (handle->CodecInfo.decInfo.openParam.TenBitDisable) {
					pOutParam->m_DecOutInfo.m_Reserved[4] = 8;
				}

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDisplayWidth) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDisplayWidth / 2) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					} else {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDisplayWidth) / 2 * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}

					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiLumaStride = lumaStride;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiChromaStride = chromaStride;
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian) {
					case VDI_LITTLE_ENDIAN:
						endian = 0x00;
						break;
					case VDI_BIG_ENDIAN:
						endian = 0x0f;
						break;
					case VDI_32BIT_LITTLE_ENDIAN:
						endian = 0x04;
						break;
					case VDI_32BIT_BIG_ENDIAN:
						endian = 0x03;
						break;
					default:
						endian = pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian;
						break;
					}
					endian = endian & 0xf;
					pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiFrameEndian = endian;
				}
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;
			} else {
				pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
				pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
				pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[PA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[VA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[PA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[VA] = NULL;

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableLumaSize = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableChromaSize = NULL;
				pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

				pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop = NULL;
				pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom = NULL;
				pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft = NULL;
				pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight = NULL;

				pOutParam->m_DecOutInfo.m_Reserved[4] = NULL;
			}
		} else {
			pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
			pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
			pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_CompressedCb[VA] = NULL;

			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableLumaSize = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_uiCompressionTableChromaSize = NULL;
			pOutParam->m_DecOutInfo.m_DispMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft = NULL;
			pOutParam->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight = NULL;

			pOutParam->m_DecOutInfo.m_Reserved[4] = NULL;
		}

		if (pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS) {
			int decoded_idx;
			int decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx;

			//! Decoded Buffer
			decoded_idx = pOutParam->m_DecOutInfo.m_iDecodedIdx;

			if (decoded_idx > MAX_GDI_IDX) {
				DLOGA("RETCODE_CODEC_EXIT: decoded_idx (%d) \n", decoded_idx);
				return RETCODE_CODEC_EXIT;
			}

			if (decoded_idx >= 0) {
				if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
					if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
						decoded_idx_compressed = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					} else {
						decoded_idx = pOutParam->m_DecOutInfo.m_iDecodedIdx + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
				}

				// Physical Address
				pOutParam->m_pCurrOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufY);
				pOutParam->m_pCurrOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCb);
				pOutParam->m_pCurrOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCr);
				// Virtual Address
				pOutParam->m_pCurrOut[VA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufY);
				pOutParam->m_pCurrOut[VA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCb);
				pOutParam->m_pCurrOut[VA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCr);

				if (handle->CodecInfo.decInfo.enableAfbce) {
					DecOutputInfo decpInfo = {0};
					VpuRect rcDecplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufY;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCb;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufY;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcCompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCb;

					rcDecplay.left = 0;
					rcDecplay.top = 0;
					rcDecplay.right = info.decPicWidth;
					rcDecplay.bottom = info.decPicHeight;
					decpInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					decpInfo.dispFrame.cbcrInterleave = 0;
					decpInfo.dispFrame.nv21 = 0;
					decpInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					decpInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					// decpInfo.dispFrame.lumaBitDepth =  (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : info.dispFrame.orgLumaBitDepth  ;
					// decpInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.orgChromaBitDepth;
					decpInfo.dispFrame.lumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					decpInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					decpInfo.dispPicWidth = info.decPicWidth;
					decpInfo.dispPicHeight = info.decPicHeight;
					decpInfo.dispFrame.endian = pVpu->m_stFrameBuffer[PA][decoded_idx].endian; // handle->CodecInfo.decInfo.frameBufPool[decoded_idx].endian;
					decpInfo.dispFrame.bufY = pVpu->m_stFrameBuffer[PA][decoded_idx].bufY;

					GetAfbceData(handle, &decpInfo, rcDecplay, handle->CodecInfo.decInfo.decOutInfo[decoded_idx].lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					for (compnum = 0; compnum < 3; compnum++)
						pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];

					{
						int i, j;
						for (i = 0; i < 20; i++) {
							for (j = 0; j < 3; j++)
								pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							pOutParam->m_DecOutInfo.m_CurrAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufY;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufCb;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufY;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufCb;

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufFBCY;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufFBCY;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].bufFBCC;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx_compressed].bufFBCC;
				} else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
					} else {
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
							fbcYTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
							fbcYTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
						}
					}
					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize + 4095) & ~4095) + 4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize + 4095) & ~4095) + 4096;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[decoded_idx_compressed].phys_addr;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[decoded_idx_compressed].virt_addr;

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[decoded_idx_compressed].phys_addr;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[decoded_idx_compressed].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth;

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableLumaSize = handle->CodecInfo.decInfo.fbcYTblSize;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableChromaSize = handle->CodecInfo.decInfo.fbcCTblSize;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

				pOutParam->m_DecOutInfo.m_Reserved[3] = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth;
				if (handle->CodecInfo.decInfo.openParam.TenBitDisable)
					pOutParam->m_DecOutInfo.m_Reserved[3] = 8;

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth) * (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth / 2) * (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					} else {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth) * (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth) / 2 * (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}

					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiLumaStride = lumaStride;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiChromaStride = chromaStride;
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].endian) {
					case VDI_LITTLE_ENDIAN:
						endian = 0x00;
						break;
					case VDI_BIG_ENDIAN:
						endian = 0x0f;
						break;
					case VDI_32BIT_LITTLE_ENDIAN:
						endian = 0x04;
						break;
					case VDI_32BIT_BIG_ENDIAN:
						endian = 0x03;
						break;
					default:
						endian = pVpu->m_stFrameBuffer[PA][decoded_idx_compressed].endian;
						break;
					}
					endian = endian & 0xf;
					pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiFrameEndian = endian;
				}
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;
			} else {
				pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[PA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[VA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[PA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[VA] = NULL;

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableLumaSize = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableChromaSize = NULL;
				pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

				pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropTop = NULL;
				pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropBottom = NULL;
				pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropLeft = NULL;
				pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropRight = NULL;

				pOutParam->m_DecOutInfo.m_Reserved[3] = NULL;
			}
		} else {
			pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
			pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
			pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_CompressedCb[VA] = NULL;

			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableLumaSize = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_uiCompressionTableChromaSize = NULL;
			pOutParam->m_DecOutInfo.m_CurrMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropTop = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropBottom = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropLeft = NULL;
			pOutParam->m_DecOutInfo.m_DecodedCropInfo.m_iCropRight = NULL;

			pOutParam->m_DecOutInfo.m_Reserved[3] = NULL;
		}

		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
			int disp_idx_comp = pVpu->m_iIndexFrameDisplayPrev;

			//! Previously Displayed Buffer
			disp_idx = pVpu->m_iIndexFrameDisplayPrev;
			if (disp_idx >= 0) {
				if ((handle->CodecInfo.decInfo.wtlEnable) || (handle->CodecInfo.decInfo.enableAfbce)) {
					if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
						disp_idx_comp = pVpu->m_iIndexFrameDisplayPrev + handle->CodecInfo.decInfo.numFbsForDecoding;
					} else {
						disp_idx = pVpu->m_iIndexFrameDisplayPrev + handle->CodecInfo.decInfo.numFbsForDecoding;
					}
				}

				// Physical Address
				pOutParam->m_pPrevOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pPrevOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);

				// Virtual Address
				pOutParam->m_pPrevOut[VA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
				pOutParam->m_pPrevOut[VA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[VA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCr);

				if (handle->CodecInfo.decInfo.enableAfbce) {
					DecOutputInfo dispInfo = {0};
					VpuRect rcDisplay;
					AfbcFrameInfo afbcframeinfo;
					int compnum;

					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY;
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb;
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY;
					pOutParam->m_DecOutInfo.m_PrevAfbcDecInfo.m_AfbcCompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb;

					rcDisplay.left = 0;
					rcDisplay.top = 0;
					rcDisplay.right = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicWidth;	// pVpu->m_stFrameBuffer[PA][disp_idx].width;//info.dispPicWidth;
					rcDisplay.bottom = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicHeight; // pVpu->m_stFrameBuffer[PA][disp_idx].height;//info.dispPicHeight;
					dispInfo.dispFrame.format = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? FORMAT_420_ARM : FORMAT_420_P10_16BIT_LSB_ARM;
					dispInfo.dispFrame.cbcrInterleave = 0;
					dispInfo.dispFrame.nv21 = 0;
					dispInfo.dispFrame.orgLumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.lumaBitDepth;
					dispInfo.dispFrame.orgChromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.chromaBitDepth;
					// dispInfo.dispFrame.lumaBitDepth =  (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8  : info.dispFrame.orgLumaBitDepth  ;
					// dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : info.dispFrame.orgChromaBitDepth;
					dispInfo.dispFrame.lumaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispFrame.chromaBitDepth = (handle->CodecInfo.decInfo.openParam.afbceFormat == AFBCE_FORMAT_420) ? 8 : 10;
					dispInfo.dispPicWidth = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicWidth;	  // info.dispPicWidth;
					dispInfo.dispPicHeight = handle->CodecInfo.decInfo.decOutInfo[disp_idx].decPicHeight; // info.dispPicHeight;
					dispInfo.dispFrame.endian = pVpu->m_stFrameBuffer[PA][disp_idx].endian;							  // info.dispFrame.endian;
					dispInfo.dispFrame.bufY = pVpu->m_stFrameBuffer[PA][disp_idx].bufY;								  // info.dispFrame.bufY;

					GetAfbceData(handle, &dispInfo, rcDisplay, handle->CodecInfo.decInfo.decOutInfo[disp_idx].lfEnable && handle->codecMode == HEVC_DEC, NULL, &afbcframeinfo, NULL);

					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iVersion = afbcframeinfo.version;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iWidth = afbcframeinfo.width;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iHeight = afbcframeinfo.height;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iLeft_crop = afbcframeinfo.left_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iTop_crop = afbcframeinfo.top_crop;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbw = afbcframeinfo.mbw;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iMbh = afbcframeinfo.mbh;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubsampling = afbcframeinfo.subsampling;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iYuv_transform = afbcframeinfo.yuv_transform;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[0] = afbcframeinfo.inputbits[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iInputbits[1] = afbcframeinfo.inputbits[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNsubblocks = afbcframeinfo.nsubblocks;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNplanes = afbcframeinfo.nplanes;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[0] = afbcframeinfo.ncomponents[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iNcomponents[1] = afbcframeinfo.ncomponents[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[0] = afbcframeinfo.first_component[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iFirst_component[1] = afbcframeinfo.first_component[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iBody_base_ptr_bits = afbcframeinfo.body_base_ptr_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSubblock_size_bits = afbcframeinfo.subblock_size_bits;
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[0] = afbcframeinfo.uncompressed_size[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iUncompressed_size[1] = afbcframeinfo.uncompressed_size[1];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[0] = afbcframeinfo.sbs_multiplier[0];
					pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iSbs_multiplier[1] = afbcframeinfo.sbs_multiplier[1];

					for (compnum = 0; compnum < 3; compnum++) {
						pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_iCompbits[compnum] = afbcframeinfo.compbits[compnum];
					}

					{
						int i, j;
						for (i = 0; i < 20; i++) {
							for (j = 0; j < 3; j++) {
								pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = afbc_blkorder[i][j];
							}
							pOutParam->m_DecOutInfo.m_DispAfbcDecInfo.m_AfbcFrameInfo.m_Subblock_order[i][j] = 0;
						}
					}
				}

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufCb;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufY;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufCb;

				if (handle->CodecInfo.decInfo.openParam.buffer2Enable) {
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCY;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCY;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = (codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx_comp].bufFBCC;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = (codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx_comp].bufFBCC;
				} else {
					unsigned int fbcYTblSize = 0;
					unsigned int fbcCTblSize = 0;

					if (handle->CodecInfo.decInfo.openParam.FullSizeFB) {

						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(handle->CodecInfo.decInfo.openParam.maxwidth, handle->CodecInfo.decInfo.openParam.maxheight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxwidth), WAVE5_ALIGN64(handle->CodecInfo.decInfo.openParam.maxheight));
						}
					} else {
						if (pVpu->m_iBitstreamFormat == STD_HEVC) {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pOutParam->m_DecOutInfo.m_iDecodedWidth, pOutParam->m_DecOutInfo.m_iDecodedHeight);
						} else {
							fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
							fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth), WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedHeight));
						}
					}
					fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
					fbcYTblSize = ((fbcYTblSize + 4095) & ~4095) + 4096;
					fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
					fbcCTblSize = ((fbcCTblSize + 4095) & ~4095) + 4096;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcYTbl[disp_idx_comp].virt_addr;

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].phys_addr;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = handle->CodecInfo.decInfo.vbFbcCTbl[disp_idx_comp].virt_addr;
				}

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiLumaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiChromaBitDepth = pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth;

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableLumaSize = handle->CodecInfo.decInfo.fbcYTblSize;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableChromaSize = handle->CodecInfo.decInfo.fbcCTblSize;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;

				{
					unsigned int lumaStride = 0;
					unsigned int chromaStride = 0;

					if (pVpu->m_iBitstreamFormat == STD_HEVC) {
						lumaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN16(pOutParam->m_DecOutInfo.m_iDecodedWidth / 2) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					} else {
						lumaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth) * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].lumaBitDepth > 8 ? 5 : 4);
						lumaStride = WAVE5_ALIGN32(lumaStride);
						chromaStride = WAVE5_ALIGN64(pOutParam->m_DecOutInfo.m_iDecodedWidth) / 2 * (pVpu->m_stFrameBuffer[PA][disp_idx_comp].chromaBitDepth > 8 ? 5 : 4);
						chromaStride = WAVE5_ALIGN32(chromaStride);
					}

					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiLumaStride = lumaStride;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiChromaStride = chromaStride;
				}

				{
					unsigned int endian;
					switch (pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian) {
					case VDI_LITTLE_ENDIAN:
						endian = 0x00;
						break;
					case VDI_BIG_ENDIAN:
						endian = 0x0f;
						break;
					case VDI_32BIT_LITTLE_ENDIAN:
						endian = 0x04;
						break;
					case VDI_32BIT_BIG_ENDIAN:
						endian = 0x03;
						break;
					default:
						endian = pVpu->m_stFrameBuffer[PA][disp_idx_comp].endian;
						break;
					}
					endian = endian & 0xf;
					pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiFrameEndian = endian;
				}
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;
			} else {
				pOutParam->m_pPrevOut[PA][COMP_Y] = pOutParam->m_pPrevOut[VA][COMP_Y] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_U] = pOutParam->m_pPrevOut[VA][COMP_U] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_V] = pOutParam->m_pPrevOut[VA][COMP_V] = NULL;

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[PA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[VA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[PA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[VA] = NULL;

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableLumaSize = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableChromaSize = NULL;
				pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;
			}
		} else {
			pOutParam->m_pPrevOut[PA][COMP_Y] = pOutParam->m_pPrevOut[VA][COMP_Y] = NULL;
			pOutParam->m_pPrevOut[PA][COMP_U] = pOutParam->m_pPrevOut[VA][COMP_U] = NULL;
			pOutParam->m_pPrevOut[PA][COMP_V] = pOutParam->m_pPrevOut[VA][COMP_V] = NULL;

			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedY[VA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_CompressedCb[VA] = NULL;

			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcYOffsetAddr[VA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[PA] = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_FbcCOffsetAddr[VA] = NULL;

			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableLumaSize = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_uiCompressionTableChromaSize = NULL;
			pOutParam->m_DecOutInfo.m_PrevMapConvInfo.m_Reserved[0] = pVpu->m_iBitstreamFormat;
		}

	#ifdef VP9_ERROR_RESILIENCE_FOR_WATCHDOG_TIMEOUT
		#if TCC_VP9____DEC_SUPPORT == 1
		if (pVpu->m_iBitstreamFormat == STD_VP9) {
			if (info.errorReason == WAVE5_SYSERR_WATCHDOG_TIMEOUT) {
				if ((info.decodingSuccess == 0) /*decoding failure*/ && (info.indexFrameDecoded >= 0)) {
					Int32 interruptFlag = 0;
					Uint32 loopCount, interruptTimeout = VPU_DEC_TIMEOUT;
					Uint32 interruptWaitTime = 1;
					int timeoutCount = 0;
					int idx;
					int res;
					int dispFlag, clrFlag;
					char *psz_err_reason = (info.errorReason & WAVE5_SYSERR_WATCHDOG_TIMEOUT) ? "WATCHDOG_TIMEOUT" : "DEC_VLC_BUF_FULL";

					DLOGA("decodingFail: '%s' happened framdIdx %d error(0x%08x) reason(0x%08x), reasonExt(0x%08x)\n",
							psz_err_reason,
							info.indexFrameDecoded, info.decodingSuccess, info.errorReason, info.errorReasonExt);

					handle->CodecInfo.decInfo.watchdog_timeout = 1;

					#if 1
					pOutParam->m_DecOutInfo.m_iDecodingStatus = pOutParam->m_DecOutInfo.m_iOutputStatus;
					pOutParam->m_DecOutInfo.m_iDecodedIdx = pOutParam->m_DecOutInfo.m_iDispOutIdx;
					#else
					// return Buffer full for retry decoding
					pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_BUF_FULL;
					pOutParam->m_DecOutInfo.m_iDecodedIdx = -1;
					pVpu->m_pCodecInst->CodecInfo.decInfo.prev_buffer_full = 1;

					dispFlag = handle->CodecInfo.decInfo.frameDisplayFlag;
					clrFlag = handle->CodecInfo.decInfo.clearDisplayIndexes;
					DLOGA("(B)dispFlag(0x%x), clrFlag(0x%x) \n", dispFlag, clrFlag);
					for (idx = 0; idx < MAX_GDI_IDX; idx++) {
						if ((dispFlag >> idx) & 0x01) {
							DLOGA("(B)dispFlag(%x): FrameNum.'%d' displayed \n", dispFlag, idx);
						}
						if ((clrFlag >> idx) & 0x01) {
							DLOGA("(B)clrFlag(%x): FrameNum.'%d' cleared \n", clrFlag, idx);
						}
					}
					pVpu->m_pCodecInst->CodecInfo.decInfo.clearDisplayIndexes |= (1 << info.indexFrameDecoded);
					DLOGA("(X)clear buffer index (%d) \n", info.indexFrameDecoded);
					#endif
				}
				return ret;
			}
		}
		#endif
	#endif

		if (info.decodingSuccess == 0) {
			/*
			 * [20191203] when decodingSuccess == 0 && info.errorReason & 0x10000,
			 * Make-up to RETCODE_CODEC_EXIT
			 */
			if ((info.errorReason & WAVE5_SYSERR_DEC_VLC_BUF_FULL) || (info.errorReason & WAVE5_ERR_TIMEOUT_VCPU)) {
				DLOGA("RETCODE_CODEC_EXIT: errorReason (0x%x) \n", info.errorReason);
				return RETCODE_CODEC_EXIT;
			} else {
				return ret;
			}
		}

		if (info.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END) {
			return RETCODE_CODEC_FINISH;
		} else if (info.indexFrameDisplay > pVpu->m_iNeedFrameBufCount) {
			return RETCODE_INVALID_FRAME_BUFFER;
		}

	#if TCC_VP9____DEC_SUPPORT == 1
		// VP9 Superframe && Seq.Chaned && FullFB
		if ((pVpu->m_iBitstreamFormat == STD_VP9) && (handle->CodecInfo.decInfo.sequencechange_detected) && (handle->CodecInfo.decInfo.openParam.FullSizeFB)) {
			RetCode ret_sc = 0;
			int index, dispFlag;
			pOutParam->m_DecOutInfo.m_iDecodingStatus = 1;

			{
				int iRet = 0;
				unsigned int setFlag = 0, clrFlag = 0;

				clrFlag = 0;
				for (index = 0; index < MAX_GDI_IDX; index++) {
					clrFlag |= (1 << index);
				}

				if (clrFlag > 0) {
					iRet = WAVE5_DecClrDispFlagEx(pVpu->m_pCodecInst, clrFlag);
					if (iRet == RETCODE_CODEC_EXIT) {
						DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_DecClrDispFlagEx function. \n");
						return RETCODE_CODEC_EXIT;
					}
					DLOGW("[Seq.Changed][frameDisplayFlag:0x%x,prev:0x%x][clearDisplayIndex:0x%x] Clear DisplayFlags %x \n",
						  handle->CodecInfo.decInfo.frameDisplayFlag,
						  handle->CodecInfo.decInfo.previous_frameDisplayFlag,
						  handle->CodecInfo.decInfo.clearDisplayIndexes,
						  clrFlag);
				}
				#ifdef ENABLE_FORCE_ESCAPE
				if (gWave5InitFirst_exit > 0) {
					DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}
				#endif

				dispFlag = info.frameDisplayFlag;
				setFlag = 0;
				for (index = 0; index < MAX_GDI_IDX; index++) {
					if ((dispFlag >> index) & 0x01) {
						setFlag |= (1 << index);
					}
				}
				if (setFlag > 0) {
					WAVE5_DecGiveCommand(handle, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
					DLOGW("[Seq.Changed][frameDisplayFlag:0x%x,prev:0x%x][clearDisplayIndex:0x%x] Set DisplayFlags %x \n",
						  handle->CodecInfo.decInfo.frameDisplayFlag,
						  handle->CodecInfo.decInfo.previous_frameDisplayFlag,
						  handle->CodecInfo.decInfo.clearDisplayIndexes,
						  setFlag);
				}
				#ifdef ENABLE_FORCE_ESCAPE
				if (gWave5InitFirst_exit > 0) {
					DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
					return RETCODE_CODEC_EXIT;
				}
				#endif
			}

			ret_sc = WAVE5_dec_register_frame_buffer_FullFB_SC(pVpu);
			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
			#endif

			if (ret_sc != RETCODE_SUCCESS) {
				return ret_sc;
			}

			seqchange_reinterrupt = 1;

			DLOGE("[Seq.Changed][CQ:%d/%d/%d] Retry for Output Info, %d, displayFlag(%x, %x), ApplyPrevOutInfo(%d)\n",
				handle->CodecInfo.decInfo.instanceQueueCount, handle->CodecInfo.decInfo.totalQueueCount,
				handle->CodecInfo.decInfo.openParam.CQ_Depth,
				handle->CodecInfo.decInfo.frameNumCount,
				handle->CodecInfo.decInfo.frameDisplayFlag, info.frameDisplayFlag,
				pVpu->iApplyPrevOutInfo);

			if (handle->CodecInfo.decInfo.openParam.CQ_Depth > 1) {
				if (handle->CodecInfo.decInfo.instanceQueueCount == handle->CodecInfo.decInfo.openParam.CQ_Depth) {
					// Since the command queue is full and cannot accept any more commands,
					// the existing commands in the queue must be completed before accepting new commands.
					// To ensure that the VPU enters a waiting state until the completion interrupt occurs,
					// call the interrupt handler.
					// This allows the VPU to handle the full command queue state appropriately.
					goto SEQ_CHANGE_OUTINFO_CQ1;
				}
			}

			if (handle->CodecInfo.decInfo.openParam.CQ_Depth == 2) {
				goto SEQ_CHANGE_OUTINFO_CQ2;
			}
			goto SEQ_CHANGE_OUTINFO_CQ1;
		}
	#endif
	}

#ifdef CQ_BUFFER_RESILIENCE
	if (pVpu->iApplyPrevOutInfo == 10) { // cq2 error special code
		if (pOutParam->m_DecOutInfo.m_iDecodingStatus != VPU_DEC_DETECT_RESOLUTION_CHANGE) {
			DLOGW("Decoding Status: %s", getDecodingStatusString(pOutParam->m_DecOutInfo.m_iDecodingStatus));
			ret = RETCODE_QUEUEING_FAILURE;
		}
		pVpu->iApplyPrevOutInfo = 0;
	}
#endif

	return ret;
}

static codec_result_t
WAVE5_dec_ring_buffer_automatic_control_new(wave5_t *pVpu, vpu_4K_D2_dec_init_t* pDec_init, vpu_4K_D2_dec_ring_buffer_setting_in_t* pDec_ring_setting)
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	unsigned int ring_buffer_read_pointer = 0;
	unsigned int ring_buffer_write_pointer = 0;
	int available_space_in_buffer = 0; // Input provided for VPU API call. I don't think it will be used in practice. Because the packet size is fixed...
	int copied_size = 0;
	int fill_size = 0; // Must be smaller than the available buffer space.
	int available_room_to_the_end_point;
	int bitstream_buffer_physical_virtual_offset = 0;

	bitstream_buffer_physical_virtual_offset = (unsigned long long)pDec_init->m_BitstreamBufAddr[0] - (unsigned long long)pDec_init->m_BitstreamBufAddr[1];

	// Receive read point, write point and available size information
	ret = WAVE5_DecGetBitstreamBuffer(handle, (PhysicalAddress *)&ring_buffer_read_pointer, (PhysicalAddress *)&ring_buffer_write_pointer, (unsigned int *)&available_space_in_buffer);
	if (ret == RETCODE_CODEC_EXIT) {
		return RETCODE_CODEC_EXIT;
	}

	// ring buffer implementation here
	// Fill Buffer

	fill_size = pDec_ring_setting->m_iOnePacketBufferSize; // size of packet

	if ((ring_buffer_write_pointer + fill_size) > (pDec_init->m_BitstreamBufAddr[0] + pDec_init->m_iBitstreamBufSize)) { // If the capacity to be written to the buffer exceeds the end point of the buffer, write to the end of the buffer and then write again from the beginning of the buffer.
		available_room_to_the_end_point = (pDec_init->m_BitstreamBufAddr[0] + pDec_init->m_iBitstreamBufSize) - ring_buffer_write_pointer ;

		// Write to the end of the buffer
		if (available_room_to_the_end_point != 0) {
			codec_addr_t wptr_addr = ring_buffer_write_pointer - bitstream_buffer_physical_virtual_offset;
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(wptr_addr),
						CONVERT_CA_PVOID(buff_addr),
						(unsigned int)available_room_to_the_end_point,
						1);
		}

		// Remaining packets are written again from the beginning of the buffer
		if ((fill_size - available_room_to_the_end_point) != 0) {
			codec_addr_t bitstream_addr = pDec_init->m_BitstreamBufAddr[1];
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr + available_room_to_the_end_point;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(bitstream_addr),
						CONVERT_CA_PVOID(buff_addr),
						(unsigned int)(fill_size - available_room_to_the_end_point),
						1);
		}

		copied_size = fill_size;
	}
	else { // If it is possible to just write serially to the buffer

		if (fill_size != 0) {
			codec_addr_t wptr_addr = ring_buffer_write_pointer - bitstream_buffer_physical_virtual_offset;
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(wptr_addr),
						CONVERT_CA_PVOID(buff_addr),
						(unsigned int)fill_size,
						1);
		}

		copied_size = fill_size;
	}

	ret = WAVE5_DecUpdateBitstreamBuffer(handle, copied_size);

	return ret;
}

static codec_result_t
WAVE5_dec_flush_ring_buffer(wave5_t *pVpu)
{
	codec_result_t ret = 0;
	CodecInst * pCodecInst;
	DecInfo * pDecInfo;
	int i, coreIdx = 0;

	pCodecInst = pVpu->m_pCodecInst;
	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (WAVE5_GetPendingInst(coreIdx)) {
		#ifndef TEST_INTERRUPT_REASON_REGISTER
		int reason = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		int reason = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif

		if ((reason & (1<<INT_WAVE5_BSBUF_EMPTY))  || ( pDecInfo->m_iPendingReason & (1<<INT_WAVE5_BSBUF_EMPTY))) {
			unsigned val = Wave5ReadReg(coreIdx, pDecInfo->streamRdPtrRegAddr);

			Wave5WriteReg(coreIdx, pDecInfo->streamWrPtrRegAddr, val);

		#ifndef TEST_BITBUFF_FLUSH_INTERRUPT
			while ( 1 ) {
				int iRet = 0;

				if (Wave5ReadReg(coreIdx, W5_VCPU_CUR_PC) == 0x0) {
					return RETCODE_CODEC_EXIT;
				}

				if (wave5_interrupt != NULL) {
					iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
				}

				if (iRet == RETCODE_CODEC_EXIT) {
					#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
					WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, pCodecInst, handle->CodecInfo.decInfo.openParam.CQ_Depth);
					#endif
					WAVE5_dec_reset( pVpu, ( 0 | (1<<16) ) );
					return RETCODE_CODEC_EXIT;
				}
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
				#else
				iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
				#endif
				if (iRet) {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					#ifndef TEST_INTERRUPT_REASON_REGISTER
					WAVE5_ClearInterrupt(coreIdx);
					#endif

					if (iRet & ((1<<INT_WAVE5_DEC_PIC) | (1<<INT_WAVE5_FLUSH_INSTANCE) | (1<<INT_WAVE5_INIT_SEQ))) {
						WAVE5_ClearPendingInst(coreIdx);
						break;
					}
					else //if(( iRet & (1<<INT_BIT_BIT_BUF_EMPTY) ) || ( iRet & (1<<INT_BIT_DEC_FIELD) )
					{
						return RETCODE_CODEC_EXIT;
					}
				}
			}
		#endif
		}
		WAVE5_ClearPendingInst((Uint32)coreIdx);
		pDecInfo->m_iPendingReason = 0;
	}

	//Clear frame buffers
	{
		int iRet = 0;
		unsigned int clrFlag = 0;

		for (i=0; i<31; i++) {
			// [20191203] - not consider clearDisplayIndexes
			//if (pDecInfo->clearDisplayIndexes & (1<<i))
			{
				clrFlag |= (1 << i);
			}
		}

		if (clrFlag > 0) {
		#ifdef TEST_BUFFERFULL_POLLTIMEOUT
			if (pDecInfo->prev_buffer_full == 1)
				pDecInfo->fb_cleared = 1;
			else
				pDecInfo->fb_cleared = 0;
		#endif

 			iRet = WAVE5_DecClrDispFlagEx((DecHandle)pCodecInst, clrFlag);
 			if (iRet == RETCODE_CODEC_EXIT)
				return RETCODE_CODEC_EXIT;

			pDecInfo->clearDisplayIndexes = 0;
		}
	}

	// Pop all VPU outputs till VPU gets to idle
	{
		#define VPU_GET_OUTPUT_USLEEP_DELAY	1000 //1 msec

		DecOutputInfo outputInfo = { '\0', };

		while (RETCODE_SUCCESS == WAVE5_DecGetOutputInfo(pCodecInst, &outputInfo))
		{
			if (0 <= outputInfo.indexFrameDisplay)
				WAVE5_dec_buffer_flag_clear_ex(pVpu, outputInfo.indexFrameDisplay);

			if (wave5_usleep != NULL)
				wave5_usleep(VPU_GET_OUTPUT_USLEEP_DELAY, VPU_GET_OUTPUT_USLEEP_DELAY);
		}

		while (RETCODE_VPU_STILL_RUNNING == WAVE5_DecFrameBufferFlush(pCodecInst, NULL, NULL))
		{
			int intReason = 0;

			wave5_usleep(VPU_GET_OUTPUT_USLEEP_DELAY, VPU_GET_OUTPUT_USLEEP_DELAY);

			intReason = Wave5ReadReg(pCodecInst->coreIdx, W5_VPU_VINT_REASON_USR);
			if (intReason > 0) {
				Wave5WriteReg(pCodecInst->coreIdx, W5_VPU_VINT_REASON_CLR, intReason);
				Wave5WriteReg(pCodecInst->coreIdx, W5_VPU_VINT_CLEAR, 1);

				if (intReason & (1<<W5_INT_DEC_PIC)) {
					if (RETCODE_SUCCESS == WAVE5_DecGetOutputInfo(pCodecInst, &outputInfo)) {
						if (0 <= outputInfo.indexFrameDisplay)
							WAVE5_dec_buffer_flag_clear_ex(pVpu, outputInfo.indexFrameDisplay);
					}
				}

				if (intReason & (1<<INT_WAVE5_BSBUF_EMPTY))
					(void)WAVE5_DecUpdateBitstreamBuffer(pVpu->m_pCodecInst, -1);

				//if (intReason & (1<<W5_INT_FLUSH_INSTANCE)) {
				//}
			}
		}
	}

	// Unset all interrups
	Wave5WriteReg(pCodecInst->coreIdx, W5_VPU_VINT_ENABLE, 0x00);

	// Reset flags regarding buffer full status
	pCodecInst->CodecInfo.decInfo.prev_buffer_full = 0;
	pCodecInst->CodecInfo.decInfo.fb_cleared = 0;

	pCodecInst->CodecInfo.decInfo.frameNumCount = 0;
	pCodecInst->CodecInfo.decInfo.superframe.nframes = 0;
	pCodecInst->CodecInfo.decInfo.superframe.currentIndex = 0;
	pCodecInst->CodecInfo.decInfo.skipframeUsed = 0;
	pCodecInst->CodecInfo.decInfo.isFirstFrame = 0;
	pCodecInst->CodecInfo.decInfo.prev_sequencechanged = 0;
	pCodecInst->CodecInfo.decInfo.sequencechange_detected = 0;

	ret = WAVE5_DecSetRdPtr(pCodecInst, pDecInfo->openParam.bitstreamBuffer, 1);
	if (ret != RETCODE_SUCCESS)
		return ret;

	ret = WAVE5_DecUpdateBitstreamBuffer(pCodecInst, STREAM_END_SIZE);
	if (ret != RETCODE_SUCCESS)
		return ret;

	return RETCODE_SUCCESS;
}

int WAVE5_dec_disable_axi_transaction(int coreIdx, int loop_cnt)
{
	int ret = 0;	//RETCODE_SUCCESS
#ifdef ENABLE_DISABLE_AXI_TRANSACTION_TELPIC_147

	// VPU doesn't send response. Force to set BUSY flag to 0.
	Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 0);

	// Waiting for completion of bus transaction
	// Step1 : disable request
	//vdi_fio_write_register(coreIdx, W5_GDI_BUS_CTRL, 0x100);
	{
		unsigned int ctrl;
		unsigned int data = 0x100;
		unsigned int addr = W5_GDI_BUS_CTRL;

		Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
		ctrl  = (addr&0xffff);
		ctrl |= (1<<16);    /* write operation */
		Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
	}

	//MACRO_WAVE5_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);
	{
		int loop = loop_cnt;
		unsigned int val=0;

		#ifdef ENABLE_WAIT_POLLING_USLEEP
		if(wave5_usleep != NULL) {
			loop = VPU_BUSY_CHECK_USLEEP_CNT_1MSEC;
			for(;loop>0;loop--) {
				val = Wave5ReadReg(coreIdx, W5_GDI_BUS_STATUS);
				if(val == 0x738)
					break;

				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
			}
		}
		else
		#endif
		{
			for(;loop>0;loop--) {
				val = Wave5ReadReg(coreIdx, W5_GDI_BUS_STATUS);
				if(val == 0x738)
					break;
			}
		}
		if( loop <= 0 ) {
			ret = 2;
			// W5_GDI_BUS_STATUS  TELPIC-147
			// BIT	Name	Type	Reset Value
			//[31:11]	reserved	RW	0
			//[ 10]	sec_wresp_empty	RW	1
			//[ 9]	sec_wd_empty	RW	1
			//[ 8]	sec_rq_empty	RW	1
			//[7:6]	reserved	RW	0
			//[ 5]	pri_wresp_empty	RW	1
			//[ 4]	pri_wd_empty	RW	1
			//[ 3]	pri_rq_empty	RW	1
			//[ 2: 0]	reserved	RW	0
		}
	}
#endif
	return ret;
}


static codec_result_t set_vpu_log_status(vpu_4K_D2_dec_ctrl_log_status_t* pstLogStatus) {
	if (pstLogStatus == NULL) {
		return RETCODE_INVALID_PARAM;
	} else {
		// Assign the log print callback function
		LOG_CALLBACK(Print) = (void (*)(const char *, ...))pstLogStatus->pfLogPrintCb;

		// Assign log levels using LOG_VAR macro
		LOG_VAR(Verbose) = pstLogStatus->stLogLevel.bVerbose;
		LOG_VAR(Debug)   = pstLogStatus->stLogLevel.bDebug;
		LOG_VAR(Info)    = pstLogStatus->stLogLevel.bInfo;
		LOG_VAR(Warn)    = pstLogStatus->stLogLevel.bWarn;
		LOG_VAR(Error)   = pstLogStatus->stLogLevel.bError;
		LOG_VAR(Assert)  = pstLogStatus->stLogLevel.bAssert;
		LOG_VAR(Func)    = pstLogStatus->stLogLevel.bFunc;
		LOG_VAR(Trace)   = pstLogStatus->stLogLevel.bTrace;

		// Assign decode success condition
		LOG_CONDITION(Decode) = pstLogStatus->stLogCondition.bDecodeSuccess;

		// Print the current log status
		V_PRINTF(VPU_KERN_ERR PRT_PREFIX
				"VPU_4KD2_CTRL_LOG_STATUS: V/D/I/W/E/A/F/T/=%d/%d/%d/%d/%d/%d/%d/%d\n",
				LOG_VAR(Verbose), LOG_VAR(Debug), LOG_VAR(Info), LOG_VAR(Warn),
				LOG_VAR(Error), LOG_VAR(Assert), LOG_VAR(Func), LOG_VAR(Trace));
	}

	return RETCODE_SUCCESS;
}

static codec_result_t set_vpu4k_d2_fw_addr(vpu_4K_D2_dec_set_fw_addr_t *pstFWAddr) {
	codec_result_t ret = RETCODE_SUCCESS;
	if (pstFWAddr == NULL) {
		return RETCODE_INVALID_PARAM;
	} else {
		gWave5FWAddr = pstFWAddr->m_FWBaseAddr;
		DLOGI("gWave5Addr 0x%x", gWave5FWAddr);
	}

	return RETCODE_SUCCESS;
}

int dec_check_handle_instance(wave5_t *pVpu, CodecInst *pCodecInst)
{
	//check global variables
	int ret = 0;
	int bitstreamformat;
	unsigned int uret = 0;

	if ((pVpu == NULL) || (pCodecInst == NULL))
		return RETCODE_INVALID_HANDLE;

	ret = check_wave5_handle_addr(pVpu);
	if (ret == 0)
		return RETCODE_INVALID_HANDLE;

	ret = check_wave5_handle_use(pVpu);
	if (ret == 0)
		return RETCODE_INVALID_HANDLE;

	bitstreamformat = pVpu->m_iBitstreamFormat;

	ret = checkWave5CodecInstanceAddr((void *)pCodecInst);
	if (ret == 0)
		return RETCODE_INVALID_HANDLE;

	ret = checkWave5CodecInstanceUse((void *)pCodecInst);
	if (ret == 0)
		return RETCODE_INVALID_HANDLE;

	ret = checkWave5CodecInstancePended((void *)pCodecInst);
	if (ret == 0)
		return RETCODE_INSTANCE_MISMATCH_ON_PENDING_STATUS;

	return 0;
}

codec_result_t
TCC_VPU_4K_D2_DEC_EXT(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if (Op == 1)
		gWave5InitFirst_exit  = 2;

	return 0;
}

codec_result_t
TCC_VPU_4K_D2_DEC_ESC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if (Op == 1)
		gWave5InitFirst_exit  = 1;

	return 0;
}

codec_result_t
TCC_VPU_4K_D2_DEC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	codec_result_t ret = RETCODE_SUCCESS;
	wave5_t *p_vpu_dec = NULL;
	int coreIdx = 0;

	if (pHandle != NULL) {
		p_vpu_dec = (wave5_t *)(*pHandle);
		DLOGV("OPCODE:0x%x, handle:0x%x, pParam1:0x%08x, pParam2:0x%08x\n", Op, *pHandle, pParam1, pParam2);
	}

	if (Op == VPU_4KD2_CTRL_LOG_STATUS) {
		if (pParam1 == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			vpu_4K_D2_dec_ctrl_log_status_t *pst_ctrl_log = (vpu_4K_D2_dec_ctrl_log_status_t *)pParam1;
			ret = set_vpu_log_status(pst_ctrl_log);
		}
	} else if (Op == VPU_4KD2_SET_FW_ADDRESS) {
		if (pParam1 == NULL) {
			return RETCODE_INVALID_PARAM;
		} else {
			vpu_4K_D2_dec_set_fw_addr_t *pst_fw_addr = (vpu_4K_D2_dec_set_fw_addr_t *)pParam1;
			ret = set_vpu4k_d2_fw_addr(pst_fw_addr);
		}
	} else if (Op == VPU_DEC_INIT) {
		vpu_4K_D2_dec_init_t *p_init_param = (vpu_4K_D2_dec_init_t *)pParam1;
		vpu_4K_D2_dec_initial_info_t *p_initial_info_param = (vpu_4K_D2_dec_initial_info_t *)pParam2;

		if ((p_init_param == NULL) || (p_init_param == NULL))
			return RETCODE_INVALID_HANDLE;

		ret = WAVE5_dec_init(&p_vpu_dec, p_init_param, p_initial_info_param);

		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif

		if ((ret != RETCODE_SUCCESS) && (ret != RETCODE_CALLED_BEFORE)) {
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return ret;
		}

		V_PRINTF(VPU_KERN_ERR PRT_PREFIX "handle:%p(%x), RevisionFW: %d, RTL(Date: %d, revision: %d)\n",
				 p_vpu_dec, (codec_handle_t)p_vpu_dec,
				 p_vpu_dec->m_iRevisionFW, p_vpu_dec->m_iDateRTL, p_vpu_dec->m_iRevisionRTL);
		p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlag = 0;
		*pHandle = (codec_handle_t)p_vpu_dec;
	} else if (Op == VPU_DEC_SEQ_HEADER) {
		vpu_4K_D2_dec_initial_info_t *p_initial_info_param = (vpu_4K_D2_dec_initial_info_t *)pParam2;
		vpu_4K_D2_dec_input_t *p_input_param;
		int seq_stream_size = 0;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);

		// Print Decoder Options or Flags From VPU_DEC_INIT
		{
			CodecInst *pInst = p_vpu_dec->m_pCodecInst;

			DLOGI("============== Decoder Options or Flags ==============\n");
			//Codec
			if (pInst->CodecInfo.decInfo.openParam.bitstreamFormat == STD_VP9) {
				DLOGI("Codec: VPU4K_VP9\n");
			} else if (pInst->CodecInfo.decInfo.openParam.bitstreamFormat == STD_HEVC) {
				DLOGI("Codec: VPU4K_HEVC\n");
			} else {
				DLOGI("Codec: Not supported %d\n", pInst->CodecInfo.decInfo.openParam.bitstreamFormat);
			}
			//CQ Count
			if (p_vpu_dec->iCqCount > 0) { //pInst->CodecInfo.decInfo.openParam.CQ_Depth
				DLOGI("CQ Count: %d\n", p_vpu_dec->iCqCount);
			}
			//Bistream Parsing Mode
			if (p_vpu_dec->m_bFilePlayEnable > 0) { //pInst->CodecInfo.decInfo.openParam.bitstreamMode
				DLOGI("Bitstream Parsing Mode: FilePlay\n");
			} else {
				DLOGI("Bitstream Parsing Mode: Ringbuffer\n");
			}
			//WTL
			if (pInst->CodecInfo.decInfo.openParam.wtlEnable == 1) {
				DLOGI("WTL(Write Compressed to Linear): ON\n");
			} else {
				DLOGI("WTL(Write Compressed to Linear): OFF\n");
			}
			//Max. Frame Buffer Mode
			if (pInst->CodecInfo.decInfo.openParam.FullSizeFB == 1) {
				DLOGI("Max. Frame Buffer Mode: ON\n");
			} else {
				DLOGI("Max. Frame Buffer Mode: OFF\n");
			}
			//Bit Depth
			if (pInst->CodecInfo.decInfo.openParam.TenBitDisable == 1) {
				DLOGI("Output Bit Depth: 8 bits, FORMAT_YUV420\n");
			} else {
				DLOGI("Output Bit Depth: 10 bits, FORMAT_420_P10_16BIT_LSB\n");
			}
			DLOGI("======================================================\n");
		}

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.seq_init_bsbuff_change) {
			p_input_param = (vpu_4K_D2_dec_input_t *)pParam1;
			seq_stream_size = p_input_param->m_iBitstreamDataSize;
		} else {
			seq_stream_size = (int)((codec_addr_t)pParam1);
		}

		DLOGV("[VPU_DEC_SEQ_HEADER] seq_stream_size = %d\n", seq_stream_size);
		if (wave5_memset)
			wave5_memset(&gsHevcDecInitialInfo, 0, sizeof(gsHevcDecInitialInfo), 0);

		if (seq_stream_size <= 0)
			seq_stream_size = p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamBufSize - 1;

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {
			DecInfo *pDecInfo;

			pDecInfo = &p_vpu_dec->m_pCodecInst->CodecInfo.decInfo;
			ret = WAVE5_DecSetRdPtr(p_vpu_dec->m_pCodecInst, pDecInfo->streamBufStartAddr, 0);
			if (ret != RETCODE_SUCCESS) {
				if (ret == RETCODE_CODEC_EXIT) {
					WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				}
				return ret;
			}
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.seq_init_bsbuff_change) {
			if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {
				DecInfo *pDecInfo;
				pDecInfo = &p_vpu_dec->m_pCodecInst->CodecInfo.decInfo;
				pDecInfo->streamRdPtr = p_input_param->m_BitstreamDataAddr[PA];
				ret = WAVE5_DecSetRdPtr(p_vpu_dec->m_pCodecInst, pDecInfo->streamRdPtr, 1);
				if (ret != RETCODE_SUCCESS) {
					if (ret == RETCODE_CODEC_EXIT) {
						WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
						WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					}
					return ret;
				}
			}
		}

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
			ret = WAVE5_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, seq_stream_size);
			if (ret != RETCODE_SUCCESS) {
				if (ret == RETCODE_CODEC_EXIT) {
					WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				}
				return ret;
			}
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		ret = WAVE5_dec_seq_header(p_vpu_dec, &gsHevcDecInitialInfo);

		if (p_initial_info_param != NULL)
			wave5_memcpy(p_initial_info_param, &gsHevcDecInitialInfo, sizeof(gsHevcDecInitialInfo), 0);

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return ret;
		}

		p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.seqinitdone = 1;
	} else if (Op == VPU_DEC_GET_INFO) {
		vpu_4K_D2_dec_initial_info_t *p_initial_info_param = (vpu_4K_D2_dec_initial_info_t *)pParam2;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);
		if (p_initial_info_param != NULL)
			wave5_memcpy(p_initial_info_param, &gsHevcDecInitialInfo, sizeof(gsHevcDecInitialInfo), 0);
	} else if (Op == VPU_DEC_REG_FRAME_BUFFER) {
		vpu_4K_D2_dec_buffer_t *p_buffer_param = (vpu_4K_D2_dec_buffer_t *)pParam1;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);
		ret = WAVE5_dec_register_frame_buffer(p_vpu_dec, p_buffer_param);

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return ret;
		}
	}
#ifdef ADD_FBREG_2
	else if (Op == VPU_DEC_REG_FRAME_BUFFER2) {
		vpu_4K_D2_dec_buffer2_t *pst_buffer = (vpu_4K_D2_dec_buffer2_t *)pParam1;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);
		ret = WAVE5_dec_register_frame_buffer2(p_vpu_dec, pst_buffer);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}
	}
#endif

#ifdef ADD_FBREG_3
	else if (Op == VPU_DEC_REG_FRAME_BUFFER3) {
		vpu_4K_D2_dec_buffer3_t *pst_buffer = (vpu_4K_D2_dec_buffer3_t *)pParam1;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);
		ret = WAVE5_dec_register_frame_buffer3(p_vpu_dec, pst_buffer);
		if (ret != RETCODE_SUCCESS) {
			return ret;
		}
	}
#endif
	else if (Op == VPU_DEC_DECODE) {
		vpu_4K_D2_dec_input_t *p_input_param = (vpu_4K_D2_dec_input_t *)pParam1;
		vpu_4K_D2_dec_output_t *p_output_param = (vpu_4K_D2_dec_output_t *)pParam2;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);

		WAVE5_dec_clear_framebuffer(p_vpu_dec);

		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif

		ret = WAVE5_dec_decode(p_vpu_dec, p_input_param, p_output_param);

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			DLOGA("RETCODE_CODEC_EXIT: gWave5InitFirst_exit > 0 \n");
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (ret == RETCODE_CODEC_EXIT) {
			DLOGA("RETCODE_CODEC_EXIT returned from WAVE5_dec_decode function. \n");
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return ret;
		}

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) {
			if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.isFirstFrame == 0)
				p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.seqinitdone = 0;
			if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.isFirstFrame == 1)
				p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.isFirstFrame = 0;
		}

		if (ret != RETCODE_SUCCESS) {
		#ifdef FIX_BUG__IGNORE_DECODE_CODEC_FINISH
			if (ret == RETCODE_CODEC_FINISH) {
				if (p_output_param->m_DecOutInfo.m_iDecodingStatus == 1)
					ret = RETCODE_SUCCESS;
			}
		#endif
			return ret;
		}
	}
#ifdef TEST_OUTPUTINFO
	else if (Op == VPU_DEC_GET_OUTPUT_INFO) {
		// vpu_4K_D2_dec_input_t* p_input_param	= (vpu_4K_D2_dec_input_t*)pParam1;
		vpu_4K_D2_dec_output_t *p_output_param = (vpu_4K_D2_dec_output_t *)pParam2;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);

		ret = WAVE5_dec_outputinfo(p_vpu_dec, p_output_param);
		if (ret != RETCODE_SUCCESS)
			return ret;
	}
#endif
	else if (Op == VPU_DEC_BUF_FLAG_CLEAR) {
		int *p_idx_display = (int *)pParam1;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);

		if (*p_idx_display > 31) // if the frameBufferCount exceeds the maximum buffer count number 31..
			return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

		if (*p_idx_display < 0)
			return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

		#ifndef TEST_CQ2_CLEAR_INTO_DECODE
		WAVE5_dec_buffer_flag_clear(p_vpu_dec, *p_idx_display);
		#else
		p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.clearDisplayIndexes |= (1 << (*p_idx_display));
		#endif
		DLOGV("VPU_DEC_BUF_FLAG_CLEAR index=%d : clearDisplayIndexes 0x%X, frameDisplayFlag 0x%X\n",
			  *p_idx_display,
			  p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.clearDisplayIndexes,
			  p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlag);

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif
	} else if (Op == VPU_DEC_FLUSH_OUTPUT) {
		vpu_4K_D2_dec_input_t *p_input_param = (vpu_4K_D2_dec_input_t *)pParam1;
		vpu_4K_D2_dec_output_t *p_output_param = (vpu_4K_D2_dec_output_t *)pParam2;
		// int i;
	#ifdef TEST_FLUSH_TELPIC_110
		int displayflag, framebufcnt;
	#endif

		coreIdx = p_vpu_dec->m_pCodecInst->coreIdx;
		p_input_param->m_iBitstreamDataSize = 0;

		CHECK_HANDLE_AND_SEQINIT(p_vpu_dec, p_vpu_dec->m_pCodecInst, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.initialInfoObtained);

		WAVE5_dec_clear_framebuffer(p_vpu_dec);	// [V1.01.0.28 (000)] 2019.08.15

	#ifdef TEST_FLUSH_TELPIC_110 // TEST_FLUSH_TELPIC_110 1) Back up display flags that are not cleared before calling FrameBufferFlush.
		displayflag = Wave5ReadReg(0, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlagRegAddr);
	#endif

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		ret = WAVE5_dec_decode(p_vpu_dec, p_input_param, p_output_param);

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (ret != RETCODE_SUCCESS) {
			if (ret == RETCODE_CODEC_FINISH) {
				int iRet = 0;

				iRet = WAVE5_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, -1);
				if (iRet == RETCODE_CODEC_EXIT) {
					WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return RETCODE_CODEC_EXIT;
				}

				{
					int i;
					unsigned int clrFlag = 0;
					for (i = 0; i < p_vpu_dec->m_iNeedFrameBufCount; i++) // TEST_FLUSH_TELPIC_110 2) Clear all display flags
					{
						clrFlag |= (1 << i);
					}
					if (clrFlag > 0) {
						WAVE5_dec_buffer_flag_clear_ex(p_vpu_dec, clrFlag);
					}

					{
						unsigned int timeoutcount = 0;
						int iRet = 0;
						while (1) {
							iRet = WAVE5_DecFrameBufferFlush(p_vpu_dec->m_pCodecInst, NULL, NULL);
							if (iRet != RETCODE_VPU_STILL_RUNNING) {
								if (iRet == RETCODE_CODEC_EXIT) {
									ret = RETCODE_CODEC_EXIT;
								}
								break;
							}
							timeoutcount++;
						#ifdef ENABLE_WAIT_POLLING_USLEEP
							if (wave5_usleep != NULL) {
								wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
								if (timeoutcount > VPU_BUSY_CHECK_USLEEP_CNT) {
									ret = RETCODE_CODEC_EXIT;
									break;
								}
							}
						#endif
							if (timeoutcount > VPU_BUSY_CHECK_TIMEOUT) {
								ret = RETCODE_CODEC_EXIT;
								break;
							}
						}
					}
				}
			#ifdef FLUSH_CODEC_FINISH_RET_STATUS_FAIL
				if (p_output_param->m_DecOutInfo.m_iDecodingStatus == RETCODE_SUCCESS)
					p_output_param->m_DecOutInfo.m_iDecodingStatus = RETCODE_FAILURE;

				if (p_output_param->m_DecOutInfo.m_iOutputStatus == RETCODE_SUCCESS)
					p_output_param->m_DecOutInfo.m_iOutputStatus = RETCODE_FAILURE;
			#endif
			}

			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return ret;
			}

		#ifdef TEST_FLUSH_TELPIC_110 // TEST_FLUSH_TELPIC_110 4) Restore the backed up display flag
			{
				unsigned int setFlag = 0;
				for (framebufcnt = 0; framebufcnt < p_vpu_dec->m_iNeedFrameBufCount; framebufcnt++) {
					if (displayflag & (1 << framebufcnt)) {
						setFlag |= (1 << framebufcnt);
					}
				}
				if (setFlag > 0) {
					WAVE5_DecGiveCommand(p_vpu_dec->m_pCodecInst, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
				}
			}
		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				return RETCODE_CODEC_EXIT;
			}
		#endif
		#endif

			return ret;
		}

	#ifdef TEST_FLUSH_TELPIC_110 // TEST_FLUSH_TELPIC_110 4) Restore the backed up display flag
		{
			unsigned int setFlag = 0;
			for (framebufcnt = 0; framebufcnt < p_vpu_dec->m_iNeedFrameBufCount; framebufcnt++) {
				if (displayflag & (1 << framebufcnt)) {
					setFlag |= (1 << framebufcnt);
				}
			}
			if (setFlag > 0) {
				WAVE5_DecGiveCommand(p_vpu_dec->m_pCodecInst, DEC_SET_DISPLAY_FLAG_EX, &setFlag);
			}
		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				return RETCODE_CODEC_EXIT;
			}
		#endif
		}
	#endif

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif
	} else if (Op == GET_RING_BUFFER_STATUS) // . for ring buffer.
	{
		vpu_4K_D2_dec_ring_buffer_status_out_t *p_buf_status = (vpu_4K_D2_dec_ring_buffer_status_out_t *)pParam2;
		int read_ptr, write_ptr, room;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);
		ret = WAVE5_DecGetBitstreamBuffer(p_vpu_dec->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (unsigned int *)&room);
		if (ret != RETCODE_SUCCESS) {
			p_buf_status->m_ulAvailableSpaceInRingBuffer = 0;

		#ifdef ENABLE_FORCE_ESCAPE_2
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				return RETCODE_CODEC_EXIT;
			}
		#endif

			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			}
			return ret;
		}
		p_buf_status->m_ulAvailableSpaceInRingBuffer = (unsigned int)room;
		p_buf_status->m_ptrReadAddr_PA = read_ptr;
		p_buf_status->m_ptrWriteAddr_PA = write_ptr;

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.seqinitdone) {
			p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.seqinitdone = 0;
			p_buf_status->m_ptrReadAddr_PA = read_ptr;
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif
	} else if (Op == VPU_UPDATE_WRITE_BUFFER_PTR) // . for ring buffer.
	{
		int copied_size = (int)((codec_addr_t)pParam1);
		int flush_flag = (int)((codec_addr_t)pParam2);
		int insufficient_status = 0;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);
		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
			return RETCODE_INVALID_PARAM;
		}
	#ifndef TEST_BITBUFF_FLUSH_INTERRUPT
		if ((p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) && (flush_flag)) {
			unsigned int pendingInst, vpu_pendingInst; //, updateFlag;
			pendingInst = (unsigned int)((codec_addr_t)WAVE5_GetPendingInst(coreIdx));
			vpu_pendingInst = (unsigned int)(*((unsigned int *)p_vpu_dec->m_pPendingHandle));

			if ((pendingInst) && (pendingInst == vpu_pendingInst) && (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason == (1 << INT_WAVE5_BSBUF_EMPTY))) {
				if (WAVE5_IsBusy(coreIdx)) {
					insufficient_status = 1;

					{
						unsigned int val;
						val = Wave5ReadReg(coreIdx, W5_CMD_BS_PARAM);
						val &= ~(3 << 3);
						val |= (2 << 3);
						Wave5WriteReg(coreIdx, W5_CMD_BS_PARAM, val);
						Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_USR, 0);
					#ifdef TEST_INTERRUPT_REASON_REGISTER
						Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON, 0);
					#endif
						p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = 0;
					}
				}
			}
		}
	#endif
		if (flush_flag) {
			ret = WAVE5_dec_flush_ring_buffer(p_vpu_dec);
			if (insufficient_status) {
				unsigned int val;
				val = Wave5ReadReg(coreIdx, W5_CMD_BS_PARAM);
				val &= ~(3 << 3);
				Wave5WriteReg(coreIdx, W5_CMD_BS_PARAM, val);
			}
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return ret;
			}
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (copied_size <= 0) {
			if (flush_flag == 1)
				return ret;

			return RETCODE_INVALID_PARAM;
		}

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) {
			unsigned int pendingInst, vpu_pendingInst, updateFlag;
			pendingInst = (unsigned int)((codec_addr_t)WAVE5_GetPendingInst(coreIdx));
			vpu_pendingInst = (unsigned int)(*((unsigned int *)p_vpu_dec->m_pPendingHandle));

			if ((pendingInst) && (pendingInst == vpu_pendingInst) && (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason == (1 << INT_WAVE5_BSBUF_EMPTY))) {
				updateFlag = 0;
			} else {
				updateFlag = 1;
			}
			ret = WAVE5_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, copied_size);
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return ret;
		} else {
			ret = WAVE5_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, copied_size);
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return ret;
		}
	} else if (Op == FILL_RING_BUFFER_AUTO) // . for ring buffer.
	{
		vpu_4K_D2_dec_init_t *p_init_param = (vpu_4K_D2_dec_init_t *)pParam1;
		vpu_4K_D2_dec_ring_buffer_setting_in_t *p_ring_input_setting = (vpu_4K_D2_dec_ring_buffer_setting_in_t *)pParam2;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);

		ret = WAVE5_dec_ring_buffer_automatic_control_new(p_vpu_dec, p_init_param, p_ring_input_setting);
		if (ret == RETCODE_CODEC_EXIT) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
		}
	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif
		return ret;
	} else if (Op == GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY) // . for ring buffer.
	{
		vpu_4K_D2_dec_initial_info_t *p_initial_info_param = (vpu_4K_D2_dec_initial_info_t *)pParam1;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);

		ret = WAVE5_dec_seq_header(p_vpu_dec, p_initial_info_param);
		if (ret == RETCODE_CODEC_EXIT) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		return ret;
	} else if (Op == VPU_CODEC_GET_VERSION) {
		char *ppsz_version = (char *)pParam1;
		char *ppsz_build_date = (char *)pParam2;

		if ((ppsz_version == NULL) || (ppsz_build_date == NULL))
			return RETCODE_INVALID_PARAM;

		wave5_memcpy(ppsz_version, VPU4K_D2_NAME_STR, VPU4K_D2_NAME_LEN, 2);
		wave5_memcpy(ppsz_build_date, VPU4K_D2_BUILD_DATE_STR, VPU4K_D2_BUILD_DATE_LEN - 1, 2);

		if (p_vpu_dec == NULL)
			return RETCODE_INVALID_HANDLE;
	} else if (Op == VPU_4KD2_GET_VERSION) {
		vpu_4K_D2_dec_get_version_t *pst_getversion = (vpu_4K_D2_dec_get_version_t *)pParam1;

		if (pst_getversion == NULL)
			return RETCODE_INVALID_PARAM;

		// 1. check version of api header
		if (pst_getversion->pszHeaderApiVersion != NULL) {
			const char *wave5_api_version = VPU4K_D2_API_VERSION;
			DLOGV("[VPU_4KD2_GET_VERSION] API Version = %s \n", wave5_api_version);
			if (wave5_strcmp(pst_getversion->pszHeaderApiVersion, wave5_api_version) != 0) {
				DLOGE("[VPU_4KD2_GET_VERSION] API Version must be %s \n", wave5_api_version);
				return RETCODE_API_VERSION_MISMATCH;
			}
		}

		// 2. get version
		return get_version_4kd2(pst_getversion->szGetVersion, pst_getversion->szGetBuildDate);
	} else if (Op == VPU_4KD2_SET_OPTIONS) {
		vpu_4K_D2_dec_set_options_t *pst_options = (vpu_4K_D2_dec_set_options_t *)pParam1;

		if (pst_options == NULL)
			return RETCODE_INVALID_PARAM;

		ret = set_options_4kd2(p_vpu_dec, pst_options);
	} else if (Op == VPU_DEC_CLOSE) {
		DecHandle handle;
		int timeoutCount = 0;
	#ifdef ENABLE_LIBSWRESET_CLOSE
		int nOption = 0;

		if (pParam2 != NULL)
			nOption = (int)((codec_addr_t)pParam2);
	#endif
		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);

		handle = p_vpu_dec->m_pCodecInst;

	#ifdef ENABLE_LIBSWRESET_CLOSE
		if (nOption) {
			if (nOption & 0x10) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}
			return RETCODE_SUCCESS;
		}
	#endif

		if (gWave5InitFirst == 0)
			return RETCODE_NOT_INITIALIZED;

		if (p_vpu_dec->m_pCodecInst->inUse == 0) // To prevent the close function from being called multiple times
			return RETCODE_NOT_INITIALIZED;

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) {
			if (p_vpu_dec->m_pCodecInst) {
				if (WAVE5_IsBusy(coreIdx) || (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason & (1 << INT_WAVE5_BSBUF_EMPTY))) {
					ret = WAVE5_DecUpdateBitstreamBuffer(handle, 0);
					if (WAVE5_GetPendingInst(coreIdx) == handle) {
						WAVE5_ClearPendingInst(coreIdx);
					}
					if (ret == RETCODE_CODEC_EXIT) {
						WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
						WAVE5_dec_reset(NULL, (0 | (1 << 16)));
						return ret;
					}
				}
				p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = 0;
			}
			if (p_vpu_dec->m_pPendingHandle) {
				if (*(DecHandle *)p_vpu_dec->m_pPendingHandle == handle)
					*(DecHandle *)p_vpu_dec->m_pPendingHandle = (DecHandle)0;
			}
		} else {
			if (WAVE5_IsBusy(coreIdx)) {
				ret = WAVE5_DecUpdateBitstreamBuffer(handle, 0);
				if (WAVE5_GetPendingInst(coreIdx) == handle) {
					WAVE5_ClearPendingInst(coreIdx);
				}
				if (ret == RETCODE_CODEC_EXIT) {
					WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
					WAVE5_dec_reset(NULL, (0 | (1 << 16)));
					return ret;
				}
			}
		}

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		while ((ret = WAVE5_DecClose(handle)) == RETCODE_VPU_STILL_RUNNING) {
		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (timeoutCount > VPU_BUSY_CHECK_USLEEP_CNT) {
					ret = RETCODE_CODEC_EXIT;
					break;
				}
			}
		#endif
			if (timeoutCount >= VPU_BUSY_CHECK_COUNT) {
				ret = RETCODE_CODEC_EXIT;
				break;
			}
			timeoutCount++;
		}
		// ret = WAVE5_DecClose( handle );

	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif

		if (ret == RETCODE_CODEC_EXIT) {
		#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
			WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.CQ_Depth);
		#endif
			WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			return ret;
		}

		if (ret == RETCODE_FRAME_NOT_COMPLETE) {
			ret = WAVE5_DecUpdateBitstreamBuffer(handle, 0);
			if (ret == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			}

			if (WAVE5_GetPendingInst(coreIdx) == handle) {
				WAVE5_ClearPendingInst(coreIdx);
			}
			ret = WAVE5_DecClose(handle);

		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif

			if (ret == RETCODE_CODEC_EXIT) {
			#ifndef DISABLE_WAVE5_SWReset_AT_CODECEXIT
				WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.CQ_Depth);
			#endif
				WAVE5_dec_disable_axi_transaction(0, 0x7FFF);
				WAVE5_dec_reset(NULL, (0 | (1 << 16)));
				return ret;
			}
		}
		wave5_free_vpu_handle(p_vpu_dec);

		// Reset log status
		{
			vpu_4K_D2_dec_ctrl_log_status_t st_ctrl_log = {0};
			DLOGV("Reset log status \n");
			ret = set_vpu_log_status(&st_ctrl_log);
		}

	} else if (Op == VPU_DEC_SWRESET) {
		DecHandle handle;

		CHECK_HANDLE(p_vpu_dec, p_vpu_dec->m_pCodecInst);
		handle = p_vpu_dec->m_pCodecInst;

		if (WAVE5_GetPendingInst(coreIdx)) {
			WAVE5_ClearPendingInst(coreIdx);
		}

		WAVE5_SWReset(coreIdx, SW_RESET_SAFETY, handle, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.CQ_Depth);
		WAVE5_dec_reset(NULL, (0 | (1 << 16)));

		return RETCODE_SUCCESS;
	} else if (Op == VPU_RESET_SW) {
		if (p_vpu_dec != NULL) {
			wave5_free_vpu_handle(p_vpu_dec);
		} else {
			WAVE5_dec_reset(NULL, (0 | (1 << 16)));
			ret = RETCODE_INVALID_HANDLE;
		}
	} else {
		ret = RETCODE_INVALID_COMMAND;
	}

	return ret;
}
