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
#include "vpu_core.h"

#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
#	include "TCC_VPU_D6.h"
#else
#	include "TCC_VPU_C7.h"
#endif

#include "vpuapifunc.h"
#include "vpu_version.h"
#include "vpu_err.h"

#if defined(ARM_LINUX) || defined(ARM_WINCE)
extern codec_addr_t gVirtualBitWorkAddr;
extern codec_addr_t gVirtualRegBaseAddr;
extern codec_addr_t gFWAddr;
#endif

extern int gMaxInstanceNum;
extern unsigned int g_nFrameBufferAlignSize;

static dec_initial_info_t gsDecInitialInfo;

static FRAME_BUF FrameBufDec[MAX_FRAME];


static int get_vpu_status_info(CodecInst *pCodecInstCurr, unsigned int *pinstcount, unsigned int *phandle)
{
	int ret = RETCODE_FAILURE;
	int instance_count = 0;
	CodecInst *pCodecInst = NULL;

	*pinstcount = 0;
	*phandle = 0;

	if (pCodecInstCurr != 0) {
		instance_count = ((get_vpu_instance_number()       & 0xFF) << 24) |		//VPU status info : [31:24] used instance number
						 (((pCodecInstCurr->instIndex + 1) & 0xFF) << 16);	    //                  [23:16] current instance number

		pCodecInst = GetPendingInst();	//get the pointer of pending codec instance
		if (pCodecInst != NULL) {
			instance_count |= (((pCodecInst->instIndex + 1) & 0xFF) << 8); //: [15: 8] //Pending instance number + 1
			*phandle = (unsigned int)((codec_addr_t)pCodecInst);
		}

		*pinstcount = instance_count;
		ret = RETCODE_SUCCESS;
	}
	return ret;
}

static void frame_buffer_init_dec(
				PhysicalAddress Addr, int stdMode,
				int interleave, int format, int width, int height,
				int frameBufNum,
				unsigned int bufAlignSize, unsigned int bufStartAddrAlign)
{
	int chr_hscale, chr_vscale;
	int size_dpb_lum, size_dpb_chr;
	int size_mvcolbuf;
	int chr_size_y, chr_size_x;
	int  i;

	//width(pVpu->m_iFrameBufWidth) is a multiple of 16
	//height(pVpu->m_iFrameBufHeight) is a multiple of 32

	switch (format) {
	case FORMAT_400:
	case FORMAT_420:
		height = VALIGN02(height);
		width  = VALIGN02(width);
		break;
	case FORMAT_224:
		height = VALIGN02(height);
		break;
	case FORMAT_422:
		width  = VALIGN02(width);
		break;
	case FORMAT_444:
		/* no alignment needed */
		break;
	default:
		/* Unknown format: optionally handle or assert */
		break;
	}

	chr_hscale = ((format == FORMAT_420) || (format == FORMAT_422)) ? 2 : 1;
	chr_vscale = ((format == FORMAT_420) || (format == FORMAT_224)) ? 2 : 1;

	chr_size_y = (height / chr_hscale);
	chr_size_x = (width  / chr_vscale);

	size_dpb_lum   = (width * height);
	size_dpb_chr   = (chr_size_y * chr_size_x);

	size_mvcolbuf = ((size_dpb_lum + 2 * size_dpb_chr) + 4) / 5; // round up by 5
	size_mvcolbuf = VALIGN16(size_mvcolbuf) + 1024; // round up by 8

	for (i = 0; i < frameBufNum; i++) {
		FrameBufDec[i].Format = format;
		FrameBufDec[i].mapType = LINEAR_FRAME_MAP;
		FrameBufDec[i].Index  = i;
		FrameBufDec[i].stride_lum = width;
		FrameBufDec[i].height  = height;
		FrameBufDec[i].stride_chr = width/chr_hscale;

		Addr = VALIGN(Addr, bufStartAddrAlign);
		FrameBufDec[i].vbY.phys_addr = Addr;
		FrameBufDec[i].vbY.size = size_dpb_lum;

		Addr += FrameBufDec[i].vbY.size;
		FrameBufDec[i].vbCb.phys_addr = Addr;
		FrameBufDec[i].vbCb.size = size_dpb_chr;

		Addr += FrameBufDec[i].vbCb.size;
		if (!interleave) {
			FrameBufDec[i].vbCr.phys_addr = Addr;
			FrameBufDec[i].vbCr.size = size_dpb_chr;
		}

		Addr += size_dpb_chr;
		Addr = VALIGN08(Addr);

		FrameBufDec[i].vbMvCol.phys_addr = Addr;
		FrameBufDec[i].vbMvCol.size = size_mvcolbuf;

		Addr += FrameBufDec[i].vbMvCol.size;
		Addr = VALIGN08(Addr);

		DLOGD("[%d] bufY:0x%x, bufCb:0x%x, bufCr:0x%x, bufMvCol:0x%x",
				i,
				FrameBufDec[i].vbY.phys_addr,
				FrameBufDec[i].vbCb.phys_addr,
				FrameBufDec[i].vbCr.phys_addr,
				FrameBufDec[i].vbMvCol.phys_addr);
	}
}

static FRAME_BUF *get_frame_buffer_dec(int index)
{
	return &FrameBufDec[index];
}

static void
frame_buffer_init_dec2(vpu_t *pstVpu, int format, dec_buffer2_t *pstBuffer)
{
	int divX, divY;
	int i;
	int picX = pstVpu->m_iFrameBufWidth;
	int picY = pstVpu->m_iFrameBufHeight;
	FrameBuffer *m_stFrameBufferPA = NULL;
	FrameBuffer *m_stFrameBufferVA = NULL;

	divX = ((format == MODE420) || (format == MODE422)) ? 2 : 1;
	divY = ((format == MODE420) || (format == MODE224)) ? 2 : 1;

	for (i = 0; i < pstVpu->m_iNeedFrameBufCount; i++) {
		m_stFrameBufferPA = &pstVpu->m_stFrameBuffer[PA][i];
		m_stFrameBufferVA = &pstVpu->m_stFrameBuffer[VA][i];

		// FRAME BUFFER POINTER
		m_stFrameBufferPA->bufY = pstBuffer->m_addrFrameBuffer[PA][i];
		m_stFrameBufferVA->bufY = pstBuffer->m_addrFrameBuffer[VA][i];

		m_stFrameBufferPA->bufCb = m_stFrameBufferPA->bufY  + picX * picY;
		m_stFrameBufferVA->bufCb = m_stFrameBufferVA->bufY  + picX * picY;

		m_stFrameBufferPA->bufCr = m_stFrameBufferPA->bufCb + picX/divX * picY/divY;
		m_stFrameBufferVA->bufCr = m_stFrameBufferVA->bufCb + picX/divX * picY/divY;

		m_stFrameBufferPA->bufMvCol = m_stFrameBufferPA->bufCr + picX/divX * picY/divY;
		m_stFrameBufferVA->bufMvCol = m_stFrameBufferVA->bufCr + picX/divX * picY/divY;

		m_stFrameBufferPA->bufY  = VALIGN08(m_stFrameBufferPA->bufY);
		m_stFrameBufferPA->bufCb = VALIGN08(m_stFrameBufferPA->bufCb);
		m_stFrameBufferPA->bufCr = VALIGN08(m_stFrameBufferPA->bufCr);

		m_stFrameBufferVA->bufY  = VALIGN08(m_stFrameBufferVA->bufY);
		m_stFrameBufferVA->bufCb = VALIGN08(m_stFrameBufferVA->bufCb);
		m_stFrameBufferVA->bufCr = VALIGN08(m_stFrameBufferVA->bufCr);
	}
}


static void frame_buffer_init_dec3(
				dec_buffer3_t* pstBuffer, int stdMode,
				int interleave, int format, int width, int height,
				int frameBufNum,
				unsigned int bufAlignSize, unsigned int bufStartAddrAlign)
{
	int chr_hscale, chr_vscale;
	int size_dpb_lum, size_dpb_chr;
	int size_mvcolbuf;
	int chr_size_y, chr_size_x;
	int  i;

	//width(pVpu->m_iFrameBufWidth) is a multiple of 16
	//height(pVpu->m_iFrameBufHeight) is a multiple of 32

	switch (format)
	{
	case FORMAT_400:
	case FORMAT_420:
		height = VALIGN02(height);
		width  = VALIGN02(width);
		break;
	case FORMAT_224:
		height = VALIGN02(height);
		break;
	case FORMAT_422:
		width  = VALIGN02(width);
		break;
	case FORMAT_444:
		/* no alignment needed */
		break;
	default:
		/* Unknown format: optionally handle or assert */
		break;
	}

	chr_hscale = ((format == FORMAT_420) || (format == FORMAT_422)) ? 2 : 1;
	chr_vscale = ((format == FORMAT_420) || (format == FORMAT_224)) ? 2 : 1;

	chr_size_y = (height / chr_hscale);
	chr_size_x = (width  / chr_vscale);

	size_dpb_lum   = (width * height);
	size_dpb_chr   = (chr_size_y * chr_size_x);

	size_mvcolbuf = ((size_dpb_lum + 2 * size_dpb_chr) + 4) / 5; // round up by 5
	size_mvcolbuf = VALIGN16(size_mvcolbuf) + 1024; // round up by 8

	for (i = 0; i < frameBufNum; i++) {
		FrameBufDec[i].Format = format;
		FrameBufDec[i].mapType = LINEAR_FRAME_MAP;
		FrameBufDec[i].Index  = i;
		FrameBufDec[i].stride_lum = width;
		FrameBufDec[i].height  = height;
		FrameBufDec[i].stride_chr = (width / chr_hscale);

		FrameBufDec[i].vbY.phys_addr = pstBuffer->m_addrFrameBuffer[PA][i][COMP_Y];
		FrameBufDec[i].vbY.size = size_dpb_lum;

		FrameBufDec[i].vbCb.phys_addr = pstBuffer->m_addrFrameBuffer[PA][i][COMP_U];
		FrameBufDec[i].vbCb.size = size_dpb_chr;

		FrameBufDec[i].vbCr.phys_addr = pstBuffer->m_addrFrameBuffer[PA][i][COMP_V];
		FrameBufDec[i].vbCr.size = size_dpb_chr;


		FrameBufDec[i].vbMvCol.phys_addr = pstBuffer->m_addrFrameBuffer[PA][i][3];
		FrameBufDec[i].vbMvCol.size = size_mvcolbuf;

		DLOGD("[%d] bufY:0x%x, bufCb:0x%x, bufCr:0x%x, bufMvCol:0x%x",
				i,
				FrameBufDec[i].vbY.phys_addr,
				FrameBufDec[i].vbCb.phys_addr,
				FrameBufDec[i].vbCr.phys_addr,
				FrameBufDec[i].vbMvCol.phys_addr);
	}

	return;
}

static RetCode dec_reset(vpu_t *pVpu, int iOption)
{
#ifdef USE_CODEC_PIC_RUN_INTERRUPT
	if( iOption & (1   ) )
	{
		VpuWriteReg(BIT_INT_REASON, 0);
		VpuWriteReg(BIT_INT_CLEAR, 1);
	}
#endif

	SetPendingInst(0);

	//reset global var.
	reset_vpu_global_var((iOption >> 16) & 0x0FF);

	return RETCODE_SUCCESS;
}

int IsSupportInterlaceMode(CodStd bitstreamFormat, DecInitialInfo *pSeqInfo)
{
	int bSupport = 0;
	int profile = pSeqInfo->profile;

	switch (bitstreamFormat)
	{
	case STD_AVC:
		profile = (pSeqInfo->profile == 66) ? 0 :
				 ((pSeqInfo->profile == 77) ? 1 :
				 ((pSeqInfo->profile == 88) ? 2 :
				 ((pSeqInfo->profile == 100) ? 3 : 4)));
		if (profile == 0) {	// BP
			bSupport = 0;
		} else {
			bSupport = 1;
		}
		break;
	default:
		bSupport = 0;
		break;
	}

	return bSupport;
}

static void
dec_buffer_flag_clear(vpu_t *pstVpu, int iIndexDisplay)
{
	if (iIndexDisplay >= 0) {
		VPU_DecClrDispFlag(pstVpu->m_pCodecInst, iIndexDisplay);
	}
}

#ifndef USERDATA_SIZE_CHECK
static void swap_user_data(unsigned char *pData, unsigned int UserBufSize)
{
	unsigned int i;
	unsigned int * pBuf;
	unsigned int dword1, dword2, dword3;
	unsigned int nWordCount;

	if (pData == 0) { //error
		return;
	}

	//Check if user data is exist
	nWordCount = ((pData[4] | (pData[5] << 8)) + 7) / 8;
	if (nWordCount != 0) {
		if (((nWordCount + 17) * 8) >  UserBufSize) {
			*(unsigned int*)pData = (unsigned int)0;
			*(unsigned int*)(pData+4) = (unsigned int)0;
			*(unsigned int*)(pData+8) = (unsigned int)0;
			*(unsigned int*)(pData+12) = (unsigned int)0;
			return;
		}
	}

	if (nWordCount != 0) {
		pBuf = (unsigned int *)pData;
		nWordCount = nWordCount + 17;
		for(i = 0;i < nWordCount; i++) {
			dword1 = pBuf[i * 2];
			dword2  = ( dword1 >> 24) & 0xFF;
			dword2 |= ((dword1 >> 16) & 0xFF) <<  8;
			dword2 |= ((dword1 >>  8) & 0xFF) << 16;
			dword2 |= ((dword1 >>  0) & 0xFF) << 24;
			dword3 = dword2;
			dword1  = pBuf[i * 2 + 1];
			dword2  = ( dword1 >> 24) & 0xFF;
			dword2 |= ((dword1 >> 16) & 0xFF) <<  8;
			dword2 |= ((dword1 >>  8) & 0xFF) << 16;
			dword2 |= ((dword1 >>  0) & 0xFF) << 24;
			pBuf[i * 2]   = dword2;
			pBuf[i * 2 + 1] = dword3;
		}
	}
}
#endif

#ifdef USERDATA_SIZE_CHECK
// If the user data size exceeds the buffer, the user data size is 0
// If the user data size exceeds the buffer, the total size excludes the user data size.
// user data num remains as before
static void check_user_data_size(unsigned char * pData, unsigned int UserBufSize, DecOutputInfo* pInpInfo)
{
	unsigned int i, j;
	unsigned int nTotalCount = 0;
	unsigned int nTotalSize = 0;
	unsigned int nUserSize = 0;
	unsigned int nAccUserSize = 0;
	unsigned int nOutUserCount = 0;
	unsigned int nOutUserTotalSize = 0;
	unsigned int UserOffset = 8*17;
	unsigned int val = 0;

	if (pData == 0) { //error
		return;
	}

	//Check if user data is exist
	nTotalCount = nOutUserCount = pData[1] | (pData[0] << 8);
	nTotalSize = nOutUserTotalSize = pData[3] | (pData[2] << 8);

	pInpInfo->decOutputExtData.userDataNum = nTotalCount;
	pInpInfo->decOutputExtData.userDataSize = nTotalSize;

	val = ((pData[4]<<24) & 0xFF000000) |
		((pData[5]<<16) & 0x00FF0000) |
		((pData[6]<< 8) & 0x0000FF00) |
		((pData[7]<< 0) & 0x000000FF);

	if (nTotalCount == 0) {
		pInpInfo->decOutputExtData.userDataBufFull = 0;
	} else {
		pInpInfo->decOutputExtData.userDataBufFull = (val >> 16) & 0xFFFF;
	}

	if ((nTotalSize + UserOffset) > UserBufSize) {
		for (i = 0;i < nTotalCount; i++) {
			nUserSize = pData[8*(i+1) + 3] | (pData[8*(i+1) + 2] << 8);
			nUserSize = (nUserSize+7)/8*8;
			nAccUserSize += nUserSize;
			if (nAccUserSize + UserOffset > UserBufSize) {
				*(unsigned int*)(pData+(8*(i+1))) = (unsigned int)0;
				*(unsigned int*)(pData+(8*(i+1)+4)) = (unsigned int)0;
				nOutUserCount--;
				nOutUserTotalSize -= nUserSize;
				if (i == 0) {
					*(unsigned short*)(pData+2) = (unsigned short)0;
					for (j = 0 ; j < nTotalCount ; j++) {
						*(unsigned int*)(pData+(8*(j+1)  )) = (unsigned int)0;
						*(unsigned int*)(pData+(8*(j+1)+4)) = (unsigned int)0;
					}
					return;
				}
			}
		}

		if (nTotalCount != nOutUserCount) {
			*(unsigned short*)(pData+2) = (unsigned short)(((nOutUserTotalSize>>8)&0xff) | ((nOutUserTotalSize&0xff)<<8));
		}
	}
	return;
}
#endif	//USERDATA_SIZE_CHECK

static void
dec_out_info( vpu_t* pstVpu, dec_output_info_t* pOutInfo, DecOutputInfo* pInpInfo )
{
	DecHandle handle = pstVpu->m_pCodecInst;

	pOutInfo->m_iTopFieldFirst = pInpInfo->topFieldFirst;
	pOutInfo->m_iRepeatFirstField = pInpInfo->repeatFirstField;
	pOutInfo->m_iPictureStructure = pInpInfo->pictureStructure;


	pOutInfo->m_iPicType = pInpInfo->picType;
	pOutInfo->m_iInterlacedFrame = pInpInfo->interlacedFrame;
	pOutInfo->m_iNumOfErrMBs = pInpInfo->numOfErrMBs;
	pOutInfo->m_iDecodingStatus = pInpInfo->decodingSuccess;

#ifdef USERDATA_SIZE_CHECK
	if (pstVpu->m_pCodecInst->CodecInfo.decInfo.userDataEnable) {
		pOutInfo->m_Reserved[4] = pInpInfo->decOutputExtData.userDataBufFull;
	} else {
		pOutInfo->m_Reserved[4] = 0;
	}
#endif

	if ((pstVpu->m_pCodecInst->codecMode == AVC_DEC) &&
		(pstVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 1)) {
		pOutInfo->m_iPicType |= (pInpInfo->decFrameInfo << 15);
	}

	if (pstVpu->m_pCodecInst->codecMode == AVC_DEC) {
		pOutInfo->m_AvcVuiInfo.m_iAvcVuiColourPrimaries = pInpInfo->avcVuiInfo.colorPrimaries;
		pOutInfo->m_AvcVuiInfo.m_iAvcVuiMatrixCoefficients = pInpInfo->avcVuiInfo.vuiMatrixCoefficients;
		pOutInfo->m_AvcVuiInfo.m_iAvcVuiTransferCharacteristics = pInpInfo->avcVuiInfo.vuiTransferCharacteristics;
		pOutInfo->m_AvcVuiInfo.m_iAvcVuiVideoFullRangeFlag = pInpInfo->avcVuiInfo.vidFullRange;
		pOutInfo->m_Reserved[5] = pInpInfo->avcCtType;
		pOutInfo->m_iExtAvcCtType = pInpInfo->avcCtType;
		pOutInfo->m_iExtAvcPocPic = pInpInfo->avcPocPic;
	}

	if (pInpInfo->notSufficientPsBuffer != 0) {
		pOutInfo->m_iDecodingStatus = VPU_DEC_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF;
	}

	if (pInpInfo->notSufficientSliceBuffer != 0) {
		pOutInfo->m_iDecodingStatus = VPU_DEC_INFO_NOT_SUFFICIENT_SLICE_BUFF;
	}

	//DecodedPicIdx RET_DEC_PIC_DECODED_IDX (0x1DC)
	// BIT returns -1(0xFFFF), if BIT does not decode a picture at this picture run command because there is not enough frame buffer to continue decoding process.
	//                         In case of VC-1, 2 frame buffers are necessary to decode one frame because of post-processing.
	// BIT returns -2(0xFFFE), if BIT does not decode a picture at this picture run command.
	//                         It might result from one of the reasons - there was some error in picture header,
	//                         the picture has not decoded data, or this picture is skipped as a P_SKIP picture of VC-1 stream or skip option is on.
	// When RET_DEC_PIC_TYPE(0X1CC) returns 4 and then this register returns -2, host can find out that this picture is VC-1 stream whose picture type is P_SKIP.

	if (pInpInfo->indexFrameDecoded == -1) {
		pOutInfo->m_iDecodingStatus = VPU_DEC_BUF_FULL;
	}

	if (!(pstVpu->m_uiDecOptFlags & DISABLE_DETECT_RESOLUTION_CHANGE)) {
		int resol_change_flag = 0;
		pOutInfo->m_iSeqChangeReasons = 0;

		if ((pInpInfo->decodingSuccess != 0) &&
			(pInpInfo->sequenceChanged != 0) &&
			((pInpInfo->indexFrameDecoded == -1) ||
				(pInpInfo->indexFrameDecoded == -2))) {
			resol_change_flag = 1;
			pInpInfo->indexFrameDecoded = -1;
			pInpInfo->chunkReuseRequired = 0;
			pOutInfo->m_iDecodingStatus = VPU_DEC_DETECT_SEQUENCE_CHANGE;
			pOutInfo->m_iSeqChangeReasons |= CHANGE_SEQUENCE;
		} else if (!pInpInfo->decodingSuccess &&
				(pInpInfo->indexFrameDecoded == -2)) {
			if (pstVpu->m_pCodecInst->codecMode == AVC_DEC) {
				resol_change_flag = 1;
			}
		}

		if (resol_change_flag == 1) {
			if ((pInpInfo->decPicHeight > 0) && (pInpInfo->decPicWidth > 0)) {
				if ((pstVpu->m_iFrameBufHeight != pInpInfo->decPicHeight) ||
					(pstVpu->m_iFrameBufWidth != pInpInfo->decPicWidth  )) {
					pOutInfo->m_iDecodingStatus = VPU_DEC_DETECT_SEQUENCE_CHANGE;
					pOutInfo->m_iSeqChangeReasons |= CHANGE_RESOLUTION;
					DLOGI("DecodingSuccess %d sequenceChange %d DecIdx %d, (%dx%d => %dx%d)",
							pInpInfo->decodingSuccess,
							pInpInfo->sequenceChanged,
							pInpInfo->indexFrameDecoded,
							pstVpu->m_iFrameBufWidth, pstVpu->m_iFrameBufHeight,
							pInpInfo->decPicHeight, pInpInfo->decPicWidth);
				}
			}
		}
	}

#ifdef USE_AVC_SEI_PICTIMING_INFO
	if( pstVpu->m_pCodecInst->codecMode == AVC_DEC) {
		//progressive is signalled by setting frame_mbs_only_flag: 1 in the SPS
		//interlaced(or progressive) is signalled by setting frame_mbs_only_flag: 0 in the SPS
		int frame_mbs_only_flag = pstVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.interlace;
		if ((frame_mbs_only_flag != 1) && (pOutInfo->m_iInterlacedFrame == 0)) {
			if (pInpInfo->pic_struct_PicTimingSei) {
				switch (pInpInfo->pic_struct_PicTimingSei - 1) {
				case 1: //top field
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 1;
					break;
				case 2: //bottom field
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 0;
					break;
				case 3: //top field, bottom field, in that order
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 1;
					break;
				case 4: //bottom field, top field, in that order
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 0;
					break;
				case 5: //top field, bottom field, top field repeated, in that order
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 1;
					pOutInfo->m_iRepeatFirstField = 1;
					break;
				case 6: //bottom filed, top field, bottom field repeated, in that order
					pOutInfo->m_iInterlacedFrame = 1; //interlaced
					pOutInfo->m_iTopFieldFirst = 0;
					pOutInfo->m_iRepeatFirstField = 1;
					break;
				}
			}
		}
	}
#endif

	//DecPicIdx RET_DEC_PIC_DISPLAY_IDX (0x1C4)
	//	-1 (0xFFFF) : VPU does not have more pictures to decode and display.
	//	-2 (0xFFFE) : It was frame skip so VPU cannot display or VPU does not have more picture to display after execution of current PIC_RUN.
	//	-3 (0xFFFD) : Display delay happened because of picture ordering

	if(    (pInpInfo->indexFrameDisplay != -3)
	    && (pInpInfo->indexFrameDisplay != -2)
	    && (pInpInfo->indexFrameDisplay != -4)) {
		pOutInfo->m_iOutputStatus = VPU_DEC_OUTPUT_SUCCESS;
	} else {
		pOutInfo->m_iOutputStatus = VPU_DEC_OUTPUT_FAIL;
	}

	pOutInfo->m_iDispOutIdx = pInpInfo->indexFrameDisplay;
	pOutInfo->m_iDecodedIdx = pInpInfo->indexFrameDecoded;

	if (handle->codecMode == AVC_DEC) {
		pOutInfo->m_MvcPicInfo.m_iViewIdxDisplay = pInpInfo->mvcPicInfo.viewIdxDisplay;
		pOutInfo->m_MvcPicInfo.m_iViewIdxDecoded = pInpInfo->mvcPicInfo.viewIdxDecoded;

		pOutInfo->m_MvcAvcFpaSei.m_iContent_Interpretation_Type = pInpInfo->avcFpaSei.contentInterpretationType;
		pOutInfo->m_MvcAvcFpaSei.m_iCurrent_Frame_Is_Frame0_Flag = pInpInfo->avcFpaSei.currentFrameIsFrame0Flag;
		pOutInfo->m_MvcAvcFpaSei.m_iExist = pInpInfo->avcFpaSei.exist;
		pOutInfo->m_MvcAvcFpaSei.m_iField_Views_Flag = pInpInfo->avcFpaSei.fieldViewsFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame0_Flipped_Flag = pInpInfo->avcFpaSei.frame0FlippedFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame0_Grid_Position_X = pInpInfo->avcFpaSei.frame0GridPositionX;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame0_Grid_Position_Y = pInpInfo->avcFpaSei.frame0GridPositionY;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame0_Self_Contained_Flag = pInpInfo->avcFpaSei.frame0SelfContainedFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame1_Grid_Position_X = pInpInfo->avcFpaSei.frame1GridPositionX;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame1_Grid_Position_Y = pInpInfo->avcFpaSei.frame1GridPositionY;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame1_Self_Contained_Flag = pInpInfo->avcFpaSei.frame1SelfContainedFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame_Packing_Arrangement_Cancel_Flag = pInpInfo->avcFpaSei.framePackingArrangementCancelFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame_Packing_Arrangement_Extension_Flag = pInpInfo->avcFpaSei.framePackingArrangementExtensionFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame_Packing_Arrangement_Id = pInpInfo->avcFpaSei.framePackingArrangementId;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame_Packing_Arrangement_Repetition_Period = pInpInfo->avcFpaSei.framePackingArrangementRepetitionPeriod;
		pOutInfo->m_MvcAvcFpaSei.m_iFrame_Packing_Arrangement_Type = pInpInfo->avcFpaSei.framePackingArrangementType;
		pOutInfo->m_MvcAvcFpaSei.m_iQuincunx_Sampling_Flag = pInpInfo->avcFpaSei.quincunxSamplingFlag;
		pOutInfo->m_MvcAvcFpaSei.m_iSpatial_Flipping_Flag = pInpInfo->avcFpaSei.spatialFlippingFlag;
	}

	pOutInfo->m_iHeight = pInpInfo->decPicHeight;
	pOutInfo->m_iWidth = pInpInfo->decPicWidth;

	pOutInfo->m_CropInfo.m_iCropTop = pInpInfo->decPicCrop.top;
	pOutInfo->m_CropInfo.m_iCropLeft = pInpInfo->decPicCrop.left;
	pOutInfo->m_CropInfo.m_iCropRight = pInpInfo->decPicCrop.right;
	pOutInfo->m_CropInfo.m_iCropBottom = pInpInfo->decPicCrop.bottom;

	if (pInpInfo->decPicCrop.right != 0) {
		pOutInfo->m_CropInfo.m_iCropRight = (pInpInfo->decPicWidth - pInpInfo->decPicCrop.right);
	}

	if (pInpInfo->decPicCrop.bottom != 0) {
		pOutInfo->m_CropInfo.m_iCropBottom = (pInpInfo->decPicHeight - pInpInfo->decPicCrop.bottom);
	}

	pOutInfo->m_Reserved[11] = pInpInfo->avcPocPic;

	pOutInfo->m_iConsumedBytes = (int)pInpInfo->consumedByte;

}

static RetCode
dec_seq_header(vpu_t *pVpu, dec_initial_info_t *pInitialInfo)
{
	RetCode ret = 0;
	DecHandle handle = pVpu->m_pCodecInst;

	//! [5] Seq Init & Get Initial Info
	{
		DecInitialInfo initial_info = {0,};

		VpuWriteReg(RET_DEC_SEQ_SEQ_ERR_REASON, 0);

		if (pVpu->m_pCodecInst->codecMode != AVC_DEC)
		{
			pInitialInfo->m_iReportErrorReason = RETCODE_CODEC_SPECOUT;
			ret = RETCODE_CODEC_SPECOUT;
		}

		if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
		{
			ret = VPU_DecSetEscSeqInit(handle, 1);
			if (ret != RETCODE_SUCCESS)
			{
				return ret;
			}
		}

		ret = VPU_DecGetInitialInfo( handle, &initial_info );
		if (ret == RETCODE_CODEC_EXIT)
		{
		#ifdef ADD_SWRESET_VPU_HANGUP
			MACRO_VPU_SWRESET
			dec_reset(pVpu, (0 | (1<<16)));
		#endif
			pInitialInfo->m_iReportErrorReason = RETCODE_ERR_SEQ_INIT_HANGUP;
			return ret;
		}

		if (ret == RETCODE_FRAME_NOT_COMPLETE)
		{
			return ret;
		}

		if( ret != RETCODE_SUCCESS )
		{
			#define VPUMACRO_BITMASK(VAL, MASKBIT) ( ((unsigned int)VAL) & ( 0x1<<MASKBIT ) )

			volatile unsigned int inputStdMode, vpuStdMode;
			volatile unsigned int seqErrReason;

			vpuStdMode = pVpu->m_pCodecInst->codecMode;
			inputStdMode = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamFormat;

			seqErrReason = VpuReadReg(RET_DEC_SEQ_SEQ_ERR_REASON);
			switch( vpuStdMode )
			{
			case AVC_DEC:
				if (seqErrReason) {
					int spec_out_profile = 0;

					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_BIT_DEPTH_LUMA_MINUS8) )	//SPS		bit_depth_luma_minus8 shall be 0.
						spec_out_profile = 1;
					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_BIT_DEPTH_CHROMA_MINUS8) )//SPS		bit_depth_chroma_minus8 shall be 0.
						spec_out_profile = 1;
					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_NOT_SUPPORT_PROFILEIDC) ) //SPS		66(base), 77(main), 100(high), 118(mvc), and 128 (mvc) Supported.
						spec_out_profile = 1;
					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_NOT_SUPPORT_LEVELIDC) ) //SPS		Supported up to 51.
						spec_out_profile = 1;
					if( spec_out_profile )
					{
						pInitialInfo->m_iReportErrorReason = RETCODE_H264ERR_PROFILE;
						ret = RETCODE_CODEC_SPECOUT;
					}

					spec_out_profile = 0;

					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_PIC_WIDTH_IN_MBS_MINUS1) )  //SPS		pic_width_in_mbs_minus1 shall be less than or equal to MAX_MB_NUM_X -1.
						spec_out_profile = 1;
					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_PIC_HEIGHT_IN_MAP_MINUS1) ) //SPS		pic_height_in_map_minus1 shall be less than or equal to MAX_MB_NUM_Y -1.
						spec_out_profile = 1;
					if( VPUMACRO_BITMASK(seqErrReason, VPU_AVCERR_OVER_MAX_MB_SIZE) ) //SPS		pic_width_in_mbs and pic_height_in_map shall be less than 2048.
						spec_out_profile = 1;
					if( spec_out_profile )
					{
						pInitialInfo->m_iReportErrorReason = RETCODE_ERR_MAX_RESOLUTION;
						ret = RETCODE_CODEC_SPECOUT;
					}
				}
				break;
			default:
				ret = RETCODE_CODEC_SPECOUT;
				break;
			}

			if (seqErrReason & 0x16000)
				seqErrReason = RETCODE_ERR_MAX_RESOLUTION;

			if (ret == RETCODE_INVALID_STRIDE)
			{
				pInitialInfo->m_iReportErrorReason = RETCODE_ERR_STRIDE_ZERO_OR_ALIGN8;
			}
			else if (ret != RETCODE_CODEC_SPECOUT)
			{
				pInitialInfo->m_iReportErrorReason = seqErrReason;
			}

			if (VPUMACRO_BITMASK(seqErrReason, VPU_ERR_SEQ_HEADER_NOT_FOUND))
				pInitialInfo->m_iReportErrorReason = RETCODE_ERR_SEQ_HEADER_NOT_FOUND;

			return ret;
		}

		if (initial_info.chromaFormat != 1)
		{
			pInitialInfo->m_iReportErrorReason = RETCODE_ERR_CHROMA_FORMAT;
			return RETCODE_CODEC_SPECOUT;
		}

		if (initial_info.interlace == 0)
		{
			if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 1)
				initial_info.minFrameBufferCount += 4;
			else
				initial_info.minFrameBufferCount += 1;
		}

		if (initial_info.minFrameBufferCount < 16)
			initial_info.minFrameBufferCount += 1;

		// output
		pInitialInfo->m_iAspectRateInfo = initial_info.aspectRateInfo;
		pInitialInfo->m_iAvcConstraintSetFlag[0] = initial_info.constraint_set_flag[0];
		pInitialInfo->m_iAvcConstraintSetFlag[1] = initial_info.constraint_set_flag[1];
		pInitialInfo->m_iAvcConstraintSetFlag[2] = initial_info.constraint_set_flag[2];
		pInitialInfo->m_iAvcConstraintSetFlag[3] = initial_info.constraint_set_flag[3];
		pInitialInfo->m_iAvcParamerSetFlag = initial_info.direct8x8Flag;
		pInitialInfo->m_iProfile = initial_info.profile;
		pInitialInfo->m_iInterlace = initial_info.interlace;
		pInitialInfo->m_iLevel = initial_info.level;
		pInitialInfo->m_Reserved[5] = initial_info.avcCtType;

		pInitialInfo->m_iFrameBufDelay = initial_info.frameBufDelay;
		pInitialInfo->m_uiFrameRateRes = initial_info.frameRateRes;
		pInitialInfo->m_uiFrameRateDiv = initial_info.frameRateDiv;
		pInitialInfo->m_iMinFrameBufferCount = initial_info.minFrameBufferCount;
		pInitialInfo->m_iNormalSliceSize = initial_info.normalSliceSize;
		pInitialInfo->m_iWorstSliceSize = initial_info.worstSliceSize;

		pInitialInfo->m_iPicWidth  = initial_info.picWidth;
		pInitialInfo->m_iPicHeight = initial_info.picHeight;

		if( initial_info.picCropRect.left || initial_info.picCropRect.right ||
			initial_info.picCropRect.top  || initial_info.picCropRect.bottom )
		{
			pInitialInfo->m_iAvcPicCrop.m_iCropLeft = initial_info.picCropRect.left;
			pInitialInfo->m_iAvcPicCrop.m_iCropTop = initial_info.picCropRect.top;
			pInitialInfo->m_iAvcPicCrop.m_iCropRight  = ( initial_info.picWidth - initial_info.picCropRect.right );
			pInitialInfo->m_iAvcPicCrop.m_iCropBottom = ( initial_info.picHeight - initial_info.picCropRect.bottom );
		}

		if (pVpu->m_pCodecInst->codecMode == AVC_DEC)
		{
			pInitialInfo->m_AvcVuiInfo.m_iAvcVuiColourPrimaries = initial_info.avcVuiInfo.colorPrimaries;
			pInitialInfo->m_AvcVuiInfo.m_iAvcVuiMatrixCoefficients = initial_info.avcVuiInfo.vuiMatrixCoefficients;
			pInitialInfo->m_AvcVuiInfo.m_iAvcVuiTransferCharacteristics = initial_info.avcVuiInfo.vuiTransferCharacteristics;
			pInitialInfo->m_AvcVuiInfo.m_iAvcVuiVideoFullRangeFlag = initial_info.avcVuiInfo.vidFullRange;
			pInitialInfo->m_AvcVuiInfo.m_iAvcVuiVideoFormat = initial_info.avcVuiInfo.vidFormat;
		}

		pVpu->m_iFrameFormat = 0; // 4:2:0

		pVpu->m_iPicWidth = pInitialInfo->m_iPicWidth;
		pVpu->m_iPicHeight = pInitialInfo->m_iPicHeight;
		pVpu->m_uiFrameBufDelay = pInitialInfo->m_iFrameBufDelay;
		pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.profile = pInitialInfo->m_iProfile;

		// check min. resolution
		if ((pVpu->m_iPicWidth < 48) || (pVpu->m_iPicHeight < 32))
		{
			pInitialInfo->m_iReportErrorReason = RETCODE_ERR_MIN_RESOLUTION;
			return RETCODE_INVALID_STRIDE;
		}

		// check max. resolution
		{
			int i_width_max  = 2032;	//RESOLUTION_1080_HD
			int i_height_max = 2032;	//max 4096 15bits

			if( pVpu->m_uiMaxResolution == RESOLUTION_720P_HD )
			{
				i_width_max = 1280;
			}
			else if( pVpu->m_uiMaxResolution == RESOLUTION_625_SD )
			{
				i_width_max = 720;
			}

			if( pVpu->m_iPicWidth > i_width_max || pVpu->m_iPicHeight > i_height_max )
			{
				pInitialInfo->m_iReportErrorReason = RETCODE_ERR_MAX_RESOLUTION;
				return RETCODE_INVALID_STRIDE;
			}
		}

		pVpu->m_iNeedFrameBufCount = pInitialInfo->m_iMinFrameBufferCount;

		{
			int frame_buf_size, mv_col_buf_size;
			int frame_buf_width,frame_buf_height;
			unsigned int bufAlignSize = 0, bufStartAddrAlign = 0;

			bufAlignSize = pVpu->m_pCodecInst->bufAlignSize;
			bufStartAddrAlign = pVpu->m_pCodecInst->bufStartAddrAlign;

			frame_buf_width  = ( ( pInitialInfo->m_iPicWidth  + 15 ) & ~15 );
			frame_buf_height = ( ( pInitialInfo->m_iPicHeight + 31 ) & ~31 );

		#ifdef USE_MAX_RESOLUTION
			if (pVpu->m_uiDecOptFlags & RESOLUTION_CHANGE_SUPPORT) {
				int i_real_width  = pInitialInfo->m_iPicWidth -
					pInitialInfo->m_iAvcPicCrop.m_iCropLeft -
					pInitialInfo->m_iAvcPicCrop.m_iCropRight;
				int i_real_height = pInitialInfo->m_iPicHeight -
					pInitialInfo->m_iAvcPicCrop.m_iCropBottom -
					pInitialInfo->m_iAvcPicCrop.m_iCropTop;

				if (i_real_width >= i_real_height)
				{
					unsigned int sizeY = frame_buf_width * frame_buf_height;

					frame_buf_width = 1920;		//RESOLUTION_1080_HD
					frame_buf_height = 1088;

					if (pVpu->m_uiMaxResolution == RESOLUTION_720P_HD)
					{
						frame_buf_width = 1280;
						frame_buf_height = 736;
					}
					else if (pVpu->m_uiMaxResolution == RESOLUTION_625_SD)
					{
						frame_buf_width = 720;
						frame_buf_height = 576;
					}

					if (sizeY > (unsigned int)(frame_buf_width * frame_buf_height))
					{
						pInitialInfo->m_iReportErrorReason = RETCODE_ERR_MAX_RESOLUTION;
						return RETCODE_INVALID_STRIDE;
					}
				}
				else //vertical video
				{
					unsigned int sizeY = frame_buf_width * frame_buf_height;

					frame_buf_width = 1088;		//RESOLUTION_1080_HD
					frame_buf_height = 1920;

					if (pVpu->m_uiMaxResolution == RESOLUTION_720P_HD)
					{
						frame_buf_width = 736;
						frame_buf_height = 1280;
					}
					else if (pVpu->m_uiMaxResolution == RESOLUTION_625_SD)
					{
						frame_buf_width = 576;
						frame_buf_height = 720;
					}

					if (sizeY > (unsigned int)(frame_buf_width * frame_buf_height))
					{
						pInitialInfo->m_iReportErrorReason = RETCODE_ERR_MAX_RESOLUTION;
						return RETCODE_INVALID_STRIDE;
					}
				}
			}
		#endif

			frame_buf_size = (frame_buf_width * frame_buf_height) + (frame_buf_width * frame_buf_height / 2);	//frame_buf_width is a multiple of 16, frame_buf_height is a multiple of 32

			mv_col_buf_size = (frame_buf_size + 4) / 5; // round up by 5
			mv_col_buf_size = VALIGN16(mv_col_buf_size) + 1024; // round up by 8

			pInitialInfo->m_iMinFrameBufferSize = frame_buf_size + mv_col_buf_size + bufStartAddrAlign;
			pInitialInfo->m_iMinFrameBufferSize = VALIGN(pInitialInfo->m_iMinFrameBufferSize, bufAlignSize);
			pVpu->m_iMinFrameBufferSize = pInitialInfo->m_iMinFrameBufferSize;

			pVpu->m_iFrameBufWidth  = frame_buf_width;
			pVpu->m_iFrameBufHeight = frame_buf_height;
		}
	}

	return ret;
}

static RetCode check_bitstreamformat(int m_iBitstreamFormat)
{
	RetCode ret = RETCODE_SUCCESS;

	if (m_iBitstreamFormat != STD_AVC) {
		ret = RETCODE_CODEC_SPECOUT;
	}

	return ret;
}

static RetCode
dec_init(vpu_t **ppVpu, dec_init_t *pInitParam, dec_initial_info_t *pInitialInfoParam)
{
	RetCode ret;
#ifdef USE_EXTERNAL_FW_WRITER
	int fw_writing_disable = 0;
	PhysicalAddress codeBase;
#endif
	DecHandle handle = { 0 };
	DecOpenParam op = {0,};

	ret = check_bitstreamformat(pInitParam->m_iBitstreamFormat);

	if (ret != RETCODE_SUCCESS){
		return RETCODE_CODEC_SPECOUT;
	}

	op.seq_init_bsbuff_change = (pInitParam->m_uiDecOptFlags & SEQ_INIT_BS_BUFFER_CHANGE_ENABLE)>>SEQ_INIT_BS_BUFFER_CHANGE_ENABLE_SHIFT;
	if (pInitParam->m_iFilePlayEnable == 0)
		op.seq_init_bsbuff_change = 0;

#ifdef USE_EXTERNAL_FW_WRITER
	fw_writing_disable = (pInitParam->m_uiDecOptFlags >> FW_WRITING_DISABLE_SHIFT) & 0x1;
	codeBase = (PhysicalAddress)gFWAddr;

	DLOGE("fw_writing_disable %d codeBase 0x%x", fw_writing_disable, codeBase);
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
		vpu_usleep = pInitParam->m_Usleep;
	#ifdef MEM_FN_REPLACE_INTERNAL_FN
		if (vpu_memcpy == NULL)
			vpu_memcpy = (vpu_memcpy_func*)vpu_local_mem_cpy;

		if (vpu_memset == NULL)
			vpu_memset = (vpu_memset_func*)vpu_local_mem_set;
	#endif
	}

	//! [1] VPU Init
	if ((gInitFirst == 0) || (VpuReadReg(BIT_CUR_PC)==0))
	{
		gInitFirst_exit = 0;
		//gInitFirst = 1;
	#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
		gInitFirst = check_aarch_vpu_d6();	//4 for ARM32, 8 for ARM64
	#else
		gInitFirst = check_aarch_vpu_c7();	//4 for ARM32, 8 for ARM64
	#endif

	#if defined(ARM_LINUX) || defined(ARM_WINCE)
		gVirtualBitWorkAddr = pInitParam->m_BitWorkAddr[VA];
		gVirtualRegBaseAddr = pInitParam->m_RegBaseVirtualAddr;
	#endif

		gMaxInstanceNum = MAX_NUM_INSTANCE;

	#ifdef USE_MAX_INSTANCE_NUM_CTRL
		if ( (pInitParam->m_uiDecOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF )
			gMaxInstanceNum = (pInitParam->m_uiDecOptFlags >> USE_MAX_INSTANCE_NUM_CTRL_SHIFT) & 0xF;
		if (gMaxInstanceNum > 8)
			return RETCODE_INVALID_PARAM;
	#endif

		//reset the parameter buffer
		{
			char * pInitParaBuf;
		#if defined(ARM_LINUX) || defined(ARM_WINCE)
			pInitParaBuf = (char*)gVirtualBitWorkAddr;
		#else
			pInitParaBuf = (char*)pInitParam->m_BitWorkAddr[PA];
		#endif

		#ifndef USE_EXTERNAL_FW_WRITER
			if( vpu_memset )
			{
				vpu_memset(pInitParaBuf, 0, PARA_BUF_SIZE, ((gInitFirst == 4)?(0):(1)));
				//used mem. : <= 2Kbytes,
			}
		#else
			//if ( fw_writing_disable == 0 )
			{
				if( vpu_memset )
				{
					vpu_memset(pInitParaBuf, 0, PARA_BUF_SIZE, ((gInitFirst == 4)?(0):(1)));
					//used mem. : <= 2Kbytes,
				}
			}
		#endif
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
				dec_reset( NULL, ( 0 | (1<<16) ) );
			#endif
				return RETCODE_CODEC_EXIT;
			}
			return ret;
		}
	}
#ifdef ADD_VPU_INIT_SWRESET
	else
	{
	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		ret = VPU_SWReset2();	//VPU_SWReset( SW_RESET_SAFETY );
		if( ret == RETCODE_CODEC_EXIT )
		{
			dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
	#else
		VPU_SWReset2();	//VPU_SWReset(SW_RESET_SAFETY);
	#endif
	}
#endif

	//! [2] Get Dec Handle
	ret = get_vpu_handle(ppVpu);
	if (ret == RETCODE_FAILURE)
	{
		*ppVpu = 0;
		return RETCODE_HANDLE_FULL;
	}

	(*ppVpu)->m_BitWorkAddr[PA] = pInitParam->m_BitWorkAddr[PA]; // Added to check frame buffer status information
	(*ppVpu)->m_BitWorkAddr[VA] = pInitParam->m_BitWorkAddr[VA]; // Added to check frame buffer status information

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
				dec_reset(NULL, (0 | (1<<16)));
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

		DLOGI("Topst Codeversion %d", revision);
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
	#endif	//USE_HW_AND_FW_VERSION

	//! [3] Open Decoder
	{
		op.bitstreamFormat = pInitParam->m_iBitstreamFormat;

		op.bitstreamBuffer = pInitParam->m_BitstreamBufAddr[PA];		//!< physical address
		op.bitstreamBufferSize = pInitParam->m_iBitstreamBufSize;

		op.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;	// CBCR_INTERLEAVE;

		op.mapType = 0;
		op.tiled2LinearEnable = 0;
		op.bwbEnable      = 1;	// VPU_ENABLE_BWB;

		op.frameEndian    = 0;	// VDI_LITTLE_ENDIAN;
		op.streamEndian   = 0;	// VDI_LITTLE_ENDIAN;
		op.bitstreamMode  = 0;

		op.pBitStream = (BYTE*)pInitParam->m_BitstreamBufAddr[VA];

		op.picWidth = pInitParam->m_iPicWidth;
		op.picHeight = pInitParam->m_iPicHeight;

		op.avcnpfenable = 0;
		if (pInitParam->m_iBitstreamFormat == STD_AVC)
		{
			if (pInitParam->m_uiDecOptFlags & AVC_FIELD_DISPLAY)
				op.avcnpfenable = 1;
		}

		op.ReorderEnable = 1;
		if (pInitParam->m_uiDecOptFlags & AVC_VC1_REORDER_DISABLE)
			op.ReorderEnable = 0;

		op.SWRESETEnable = 0;
		if (pInitParam->m_uiDecOptFlags& (1<<4))
			op.SWRESETEnable = 1;

		op.workBuffer = (PhysicalAddress)((codec_addr_t)pInitParam->m_pSpsPpsSaveBuffer);
		op.workBufferSize = pInitParam->m_iSpsPpsSaveBufferSize;
		if (op.workBufferSize < PS_SAVE_SIZE)
			return RETCODE_INSUFFICIENT_PS_BUF;

		ret = VPU_DecOpen( &handle, &op );
		if( ret != RETCODE_SUCCESS )
		{
			*ppVpu = 0;
			return ret;
		}

		(*ppVpu)->m_pCodecInst = handle;
		(*ppVpu)->m_iBitstreamFormat = op.bitstreamFormat;

		(*ppVpu)->m_StreamRdPtrRegAddr = handle->CodecInfo.decInfo.streamRdPtrRegAddr;
		(*ppVpu)->m_StreamWrPtrRegAddr = handle->CodecInfo.decInfo.streamWrPtrRegAddr;

		(*ppVpu)->m_uiMaxResolution = pInitParam->m_iMaxResolution;
		(*ppVpu)->m_uiDecOptFlags = pInitParam->m_uiDecOptFlags;

		(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave = pInitParam->m_bCbCrInterleaveMode;

		handle->CodecInfo.decInfo.userDataEnable = pInitParam->m_bEnableUserData;

		if ((op.bitstreamFormat == STD_AVC) && (pInitParam->m_uiDecOptFlags & (1 << USE_AVC_ErrorConcealment_CTRL_SHIFT)))
		{	// On iPod, mode must be set to 6.
			AVCErrorConcealMode mode;

			mode = (AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_MISSING_REFERENCE | AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_WRONG_FRAME_NUM);
			VPU_DecGiveCommand(handle, DEC_SET_AVC_ERROR_CONCEAL_MODE, &mode);
		}
	}

	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useBitEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBitUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useIpEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufIpAcDcUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkYEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkYUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkCEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkCUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useOvlEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufOvlUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useBtpEnable = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBtpUse = 0;
	(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufSize = 0;
	(*ppVpu)->m_pCodecInst->m_iVersionRTL = (*ppVpu)->m_iVersionRTL;

#ifdef USE_SEC_AXI_SRAM
	{
		PhysicalAddress secAxiFlag, secAXIaddr;

		secAxiFlag = (*ppVpu)->m_uiDecOptFlags & SEC_AXI_ENABLE_CTRL;

		if( 1 )
		{
			PhysicalAddress secAXIaddr_SRAM;
			secAxiFlag >>= SEC_AXI_ENABLE_CTRL_SHIFT;
			secAXIaddr_SRAM = USE_SEC_AXI_SRAM_BASE_ADDR;

			secAXIaddr = (*ppVpu)->m_BitWorkAddr[PA] + TEMP_BUF_SIZE + PARA_BUF_SIZE + CODE_BUF_SIZE;// + ((WORK_BUF_SIZE)*MAX_NUM_INSTANCE);

			if( secAXIaddr&0x0FF ) //256 byte aligned
			{
				secAXIaddr = (secAXIaddr+0x0FF)& ~(0x0FF);
			}

		#ifdef USE_SEC_AXI_BIT
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useBitEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBitUse = secAXIaddr;
			secAXIaddr += BIT_INTERNAL_USE_BUF_SIZE_FULLHD;
			if( (secAxiFlag == 1) || (secAxiFlag == 2) )
			{
				(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufBitUse = secAXIaddr_SRAM;
				secAXIaddr_SRAM += BIT_INTERNAL_USE_BUF_SIZE_FULLHD;
			}
		#endif
		#ifdef USE_SEC_AXI_IPACDC
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useIpEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufIpAcDcUse = secAXIaddr;
			secAXIaddr += IPACDC_INTERNAL_USE_BUF_SIZE_FULLHD;
			if( (secAxiFlag == 1) || (secAxiFlag == 2) )
			{
				(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufIpAcDcUse = secAXIaddr_SRAM;
				secAXIaddr_SRAM += IPACDC_INTERNAL_USE_BUF_SIZE_FULLHD;
			}
		#endif
		#ifdef USE_SEC_AXI_DBKY
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkYEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkYUse = secAXIaddr;
			secAXIaddr += DBKY_INTERNAL_USE_BUF_SIZE_FULLHD;
			if( secAxiFlag == 2 )
			{
				(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkYUse = secAXIaddr_SRAM;
				secAXIaddr_SRAM += DBKY_INTERNAL_USE_BUF_SIZE_FULLHD;
			}
		#endif

		#ifdef USE_SEC_AXI_DBKC
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.useDbkCEnable = 1;
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufDbkCUse = secAXIaddr;
			secAXIaddr += DBKC_INTERNAL_USE_BUF_SIZE_FULLHD;
		#endif
			(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.secAxiUse.bufSize = USE_SEC_AXI_SIZE;
		}

		(*ppVpu)->m_iBitstreamOutBufferPhyVirAddrOffset = pInitParam->m_BitstreamBufAddr[PA] - pInitParam->m_BitstreamBufAddr[VA];

	}
#endif //USE_SEC_AXI_SRAM

	//! [4] Get & Update Stream Buffer
	{
		int bitstreamMode = pInitParam->m_iFilePlayEnable;

		switch( bitstreamMode )
		{
		case 0: //Ring buffer mode : return BS_MODE_INTERRUPT
			bitstreamMode = BS_MODE_INTERRUPT; break;
		case 2: //rollback mode : return BS_MODE_ROLLBACK
			bitstreamMode = BS_MODE_ROLLBACK; break;
		case 1: //File play mode : return BS_MODE_PIC_END
		default:
			bitstreamMode = BS_MODE_PIC_END;  break;
		}

		(*ppVpu)->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode =
		(*ppVpu)->m_bFilePlayEnable = bitstreamMode;
	}

	// [5] Update Buffer aligned info. - check aligned size & address of buffers (frame buffer....)
	{
		unsigned int bufAlignSize, bufStartAddrAlign;
		unsigned int minFrameSizeAlign, minBufStartAddrAlign;

	#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
		minFrameSizeAlign = VPU_C7_MIN_BUF_SIZE_ALIGN;
		minBufStartAddrAlign = VPU_C7_MIN_BUF_START_ADDR_ALIGN;
	#else
		minFrameSizeAlign = VPU_D6_MIN_BUF_SIZE_ALIGN;
		minBufStartAddrAlign = VPU_D6_MIN_BUF_START_ADDR_ALIGN;
	#endif

		bufAlignSize = g_nFrameBufferAlignSize;
		bufStartAddrAlign = g_nFrameBufferAlignSize;

		if (bufAlignSize < minFrameSizeAlign) {
			bufAlignSize = minFrameSizeAlign;
		}

		if (bufStartAddrAlign < minBufStartAddrAlign) {
			bufStartAddrAlign = minBufStartAddrAlign;
		}

		(*ppVpu)->m_pCodecInst->bufAlignSize = bufAlignSize;
		(*ppVpu)->m_pCodecInst->bufStartAddrAlign = bufStartAddrAlign;
	}

	return ret;
}

static RetCode
dec_register_frame_buffer(vpu_t *pVpu, dec_buffer_t *pBufferParam)
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	DecBufInfo buf_info = {0,};
	long pv_offset = 0;

	int needFrameBufCount;
	int stride;
	int i;
	int framebufWidth;
	int framebufHeight;

	framebufWidth = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picWidth+15)&~15);
	if (IsSupportInterlaceMode(pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamFormat, &pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo))
		framebufHeight = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picHeight+31)&~31); // framebufheight must be aligned by 31 because of the number of MB height would be odd in each filed picture.
	else
		framebufHeight = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picHeight+15)&~15);

	if (pBufferParam->m_iFrameBufferCount > 32) // if the frameBufferCount exceeds the maximum buffer count number 32..
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

	if( pBufferParam->m_iFrameBufferCount > pVpu->m_iNeedFrameBufCount )
		pVpu->m_iNeedFrameBufCount = pBufferParam->m_iFrameBufferCount;

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_FrameBufferStartAddr[PA];
	pVpu->m_FrameBufferStartVAddr = pBufferParam->m_FrameBufferStartAddr[VA];
	needFrameBufCount = pVpu->m_iNeedFrameBufCount;
	pv_offset = pBufferParam->m_FrameBufferStartAddr[PA] - pBufferParam->m_FrameBufferStartAddr[VA];


	stride = pVpu->m_iFrameBufWidth;

	frame_buffer_init_dec(
			pBufferParam->m_FrameBufferStartAddr[PA], pVpu->m_iBitstreamFormat,
			pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave,
			pVpu->m_iFrameFormat, pVpu->m_iFrameBufWidth, pVpu->m_iFrameBufHeight, needFrameBufCount,
			pVpu->m_pCodecInst->bufAlignSize, pVpu->m_pCodecInst->bufStartAddrAlign);

	if (pVpu->m_iVersionRTL != 0)	// RTL 2.1.3 or above
	{
		MaverickCacheConfig decCacheConfig;
		unsigned int cacheConfig = 0;
		int mapType = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.mapType;
		int bypass = 0;
		int burst = 0;
		int merge = 3;
		int wayshape = 15;
		int interleave = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave;

		if (mapType == 0)// LINEAR_FRAME_MAP
		{
			if(!interleave)
				burst = 0;
			wayshape = 15;
			if (merge == 1)
				merge = 3;
			if ((merge== 1) && (burst)) //GDI constraint. Width should not be over 64
				burst = 0;
		} else { //horizontal merge constraint in tiled map
			if (merge == 1)	merge = 3;
		}

		cacheConfig = (merge & 0x3) << 9;
		cacheConfig = cacheConfig | ((wayshape & 0xf) << 5);
		cacheConfig = cacheConfig | ((burst & 0x1) << 3);
		cacheConfig = cacheConfig | (bypass & 0x3);

		if(mapType != 0)//LINEAR_FRAME_MAP
			cacheConfig = cacheConfig | 0x00000004;

		///{16'b0, 5'b0, merge[1:0], wayshape[3:0], 1'b0, burst[0], map[0], bypass[1:0]};
		decCacheConfig.CacheMode = cacheConfig;

		VPU_DecGiveCommand(handle, SET_CACHE_CONFIG, &decCacheConfig);
	}

	for( i = 0; i < pVpu->m_iNeedFrameBufCount; ++i )
	{
		FRAME_BUF* pFrame = get_frame_buffer_dec(i);

		pVpu->m_stFrameBuffer[PA][i].bufY     = pFrame->vbY.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufCb    = pFrame->vbCb.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufCr    = pFrame->vbCr.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufMvCol = pFrame->vbMvCol.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].myIndex  = i;

		// for output addr
		//save offset from m_FrameBufferStartPAddr
		pVpu->m_stFrameBuffer[VA][i].bufY     = pVpu->m_stFrameBuffer[PA][i].bufY - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufCb    = pVpu->m_stFrameBuffer[PA][i].bufCb - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufCr    = pVpu->m_stFrameBuffer[PA][i].bufCr - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufMvCol = pVpu->m_stFrameBuffer[PA][i].bufMvCol - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].myIndex = i;

	#ifdef DBG_PRINTF_RUN_STATUS
		if( i > 0 )
		{
			int min_frame_buffer_size = pVpu->m_stFrameBuffer[PA][i].bufY - pVpu->m_stFrameBuffer[PA][i-1].bufY;
			if( pVpu->m_iMinFrameBufferSize != min_frame_buffer_size )
			{
				DSTATUS( "[VPU_DEC] NO!!! pVpu->m_iMinFrameBufferSize = alloc(%d), min_frame_buffer_size = addr(%d) \n", pVpu->m_iMinFrameBufferSize, min_frame_buffer_size );
			}
			else
			{
				DSTATUS( "[VPU_DEC] OK!!! pVpu->m_iMinFrameBufferSize = %d \n", pVpu->m_iMinFrameBufferSize );
			}
		}
	#endif
	}

	buf_info.avcSliceBufInfo.bufferBase = pBufferParam->m_AvcSliceSaveBufferAddr;
	buf_info.avcSliceBufInfo.bufferSize = pBufferParam->m_iAvcSliceSaveBufferSize;

#ifdef USE_SEC_AXI_SRAM
	VPU_DecGiveCommand(handle, SET_SEC_AXI, &handle->CodecInfo.decInfo.secAxiUse);
#endif

	{
		unsigned int maxDecMbNumXY = 0; //[31:16] MaxMbNum, [15: 8] MaxMbX, [ 7: 0] MaxMbY
		if(!(pVpu->m_uiDecOptFlags & DISABLE_DETECT_RESOLUTION_CHANGE))
		{
			unsigned int maxDecMbNum, maxDecMbX, maxDecMbY;

			maxDecMbX = ((pVpu->m_iFrameBufWidth+15)&(~15));	//MaxDecMbX: Maximum decodable frame width in MB. If the value of MaxMbX is 0, Frame Height checking will be ignored
			maxDecMbY = ((pVpu->m_iFrameBufHeight+31)&(~31));	//MaxDecMbY: Maximum decodable frame height in MB. If the value of MaxMbY is 0, Frame Height checking will be ignored
			maxDecMbX >>= 4;
			maxDecMbY >>= 4;
			maxDecMbNum = maxDecMbX * maxDecMbY;			//maxDecMbNum: Maximum decodable frame MB number. If the value of MaxMbNum is 0, Frame MB number checking will be ignored.
			maxDecMbNumXY = (maxDecMbNum <<16) | (maxDecMbX<<8) | (maxDecMbY);
		}
		VpuWriteReg( CMD_SET_FRAME_MAX_DEC_SIZE, maxDecMbNumXY);
	}

#ifdef TEST_SET_FRAME_DELAY
#	if 0
	#define NUM_FRAME_DELAY	0
	decConfig.frameDelay = NUM_FRAME_DELAY;
	VPU_DecGiveCommand(handle, DEC_SET_FRAME_DELAY, &decConfig.frameDelay);
#	else
	pVpu->m_pCodecInst->CodecInfo.decInfo.frameDelay = 0;
#	endif
#endif

	ret = VPU_DecRegisterFrameBuffer(handle, pVpu->m_stFrameBuffer[PA], needFrameBufCount, stride, pVpu->m_iFrameBufHeight, &buf_info);
	if (ret != RETCODE_SUCCESS)
	{
	#ifdef ADD_SWRESET_VPU_HANGUP
		if (ret == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_SWRESET
			dec_reset( NULL, (0 | (1<<16)));
		}
	#endif

		return ret;
	}

	pVpu->m_iFrameBufferCount = pBufferParam->m_iFrameBufferCount;
	pVpu->m_iDecodedFrameCount = 0;

	return ret;
}

static int
dec_register_frame_buffer2( vpu_t* pstVpu, dec_buffer2_t* pstBuffer )
{
	int ret;
	DecHandle handle = pstVpu->m_pCodecInst;
	DecBufInfo buf_info;

	int needFrameBufCount;
	int stride = pstVpu->m_iFrameBufWidth;

	if (pstBuffer->m_ulFrameBufferCount > 32) // if the frameBufferCount exceeds the maximum buffer count number 32..
	{
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
	}

	if ((int)pstBuffer->m_ulFrameBufferCount > pstVpu->m_iNeedFrameBufCount)
		pstVpu->m_iNeedFrameBufCount = pstBuffer->m_ulFrameBufferCount;

	needFrameBufCount = pstVpu->m_iNeedFrameBufCount;

	frame_buffer_init_dec2(pstVpu, pstVpu->m_iFrameFormat, pstBuffer);

	if (pstVpu->m_iVersionRTL != 0)	// RTL 2.1.3 or above
	{
		MaverickCacheConfig decCacheConfig;
		unsigned int cacheConfig = 0;
		int mapType = pstVpu->m_pCodecInst->CodecInfo.decInfo.openParam.mapType;
		int bypass = 0;
		int burst = 0;
		int merge = 3;
		int wayshape = 15;
		int interleave = pstVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave;

		if (mapType == 0)// LINEAR_FRAME_MAP
		{
			if (!interleave)
				burst = 0;
			wayshape = 15;
			if (merge == 1)
				merge = 3;
			if (( merge== 1) && (burst)) //GDI constraint. Width should not be over 64
				burst = 0;
		} else { //horizontal merge constraint in tiled map
			if (merge == 1)	merge = 3;
		}

		cacheConfig = (merge & 0x3) << 9;
		cacheConfig = cacheConfig | ((wayshape & 0xf) << 5);
		cacheConfig = cacheConfig | ((burst & 0x1) << 3);
		cacheConfig = cacheConfig | (bypass & 0x3);

		if(mapType != 0)//LINEAR_FRAME_MAP
			cacheConfig = cacheConfig | 0x00000004;

		///{16'b0, 5'b0, merge[1:0], wayshape[3:0], 1'b0, burst[0], map[0], bypass[1:0]};
		decCacheConfig.CacheMode = cacheConfig;

		VPU_DecGiveCommand(handle, SET_CACHE_CONFIG, &decCacheConfig);
	}

	buf_info.avcSliceBufInfo.bufferBase = pstBuffer->m_addrAvcSliceSaveBuffer;
	buf_info.avcSliceBufInfo.bufferSize = pstBuffer->m_ulAvcSliceSaveBufferSize;
	#ifdef USE_SEC_AXI_SRAM
	VPU_DecGiveCommand(handle, SET_SEC_AXI, &handle->CodecInfo.decInfo.secAxiUse);
	#endif

	{
		unsigned int maxDecMbNumXY = 0; //[31:16] MaxMbNum, [15: 8] MaxMbX, [ 7: 0] MaxMbY
		if( !(pstVpu->m_uiDecOptFlags & DISABLE_DETECT_RESOLUTION_CHANGE) )
		{
			unsigned int maxDecMbNum, maxDecMbX, maxDecMbY;
			maxDecMbX = ((pstVpu->m_iFrameBufWidth+15)&(~15));	//MaxDecMbX: Maximum decodable frame width in MB. If the value of MaxMbX is 0, Frame Height checking will be ignored
			maxDecMbY = ((pstVpu->m_iFrameBufHeight+31)&(~31));	//MaxDecMbY: Maximum decodable frame height in MB. If the value of MaxMbY is 0, Frame Height checking will be ignored
			maxDecMbX >>= 4;
			maxDecMbY >>= 4;
			maxDecMbNum = maxDecMbX * maxDecMbY;			//maxDecMbNum: Maximum decodable frame MB number. If the value of MaxMbNum is 0, Frame MB number checking will be ignored.
			maxDecMbNumXY = (maxDecMbNum <<16) | (maxDecMbX<<8) | (maxDecMbY);
		}
		VpuWriteReg( CMD_SET_FRAME_MAX_DEC_SIZE, maxDecMbNumXY);
	}

	ret = VPU_DecRegisterFrameBuffer( handle, pstVpu->m_stFrameBuffer[PA], needFrameBufCount, stride, pstVpu->m_iFrameBufHeight, &buf_info );
	if( ret != RETCODE_SUCCESS )
		return ret;

	VPU_DecGiveCommand( handle, DISABLE_REP_USERDATA, 0 );
	pstVpu->m_iFrameBufferCount = pstBuffer->m_ulFrameBufferCount;

	return ret;
}

static RetCode
dec_register_frame_buffer3(vpu_t *pVpu, dec_buffer3_t *pBufferParam)
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	DecBufInfo buf_info = {0,};

	int needFrameBufCount;
	int stride;
	int i;
	int framebufWidth;
	int framebufHeight;

	framebufWidth = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picWidth+15)&~15);
	if (IsSupportInterlaceMode(pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamFormat, &pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo))
		framebufHeight = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picHeight+31)&~31); // framebufheight must be aligned by 31 because of the number of MB height would be odd in each filed picture.
	else
		framebufHeight = ((pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.picHeight+15)&~15);

	if (pBufferParam->m_ulFrameBufferCount > 32) // if the frameBufferCount exceeds the maximum buffer count number 32..
		return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

	if( pBufferParam->m_ulFrameBufferCount > pVpu->m_iNeedFrameBufCount )
		pVpu->m_iNeedFrameBufCount = pBufferParam->m_ulFrameBufferCount;

	pVpu->m_FrameBufferStartPAddr = pBufferParam->m_addrFrameBuffer[PA][0][0]; //framebuffer index 0, Y addr
	pVpu->m_FrameBufferStartVAddr = pBufferParam->m_addrFrameBuffer[VA][0][0];
	needFrameBufCount = pVpu->m_iNeedFrameBufCount;

	stride = pVpu->m_iFrameBufWidth;

	frame_buffer_init_dec3(
			pBufferParam, pVpu->m_iBitstreamFormat,
			pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave,
			pVpu->m_iFrameFormat, pVpu->m_iFrameBufWidth, pVpu->m_iFrameBufHeight, needFrameBufCount,
			pVpu->m_pCodecInst->bufAlignSize, pVpu->m_pCodecInst->bufStartAddrAlign);

	if (pVpu->m_iVersionRTL != 0)	// RTL 2.1.3 or above
	{
		MaverickCacheConfig decCacheConfig;
		unsigned int cacheConfig = 0;
		int mapType = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.mapType;
		int bypass = 0;
		int burst = 0;
		int merge = 3;
		int wayshape = 15;
		int interleave = pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.cbcrInterleave;

		if (mapType == 0)// LINEAR_FRAME_MAP
		{
			if(!interleave)
				burst = 0;
			wayshape = 15;
			if (merge == 1)
				merge = 3;
			if ((merge== 1) && (burst)) //GDI constraint. Width should not be over 64
				burst = 0;
		} else { //horizontal merge constraint in tiled map
			if (merge == 1)	merge = 3;
		}

		cacheConfig = (merge & 0x3) << 9;
		cacheConfig = cacheConfig | ((wayshape & 0xf) << 5);
		cacheConfig = cacheConfig | ((burst & 0x1) << 3);
		cacheConfig = cacheConfig | (bypass & 0x3);

		if(mapType != 0)//LINEAR_FRAME_MAP
			cacheConfig = cacheConfig | 0x00000004;

		///{16'b0, 5'b0, merge[1:0], wayshape[3:0], 1'b0, burst[0], map[0], bypass[1:0]};
		decCacheConfig.CacheMode = cacheConfig;

		VPU_DecGiveCommand(handle, SET_CACHE_CONFIG, &decCacheConfig);
	}

	for( i = 0; i < pVpu->m_iNeedFrameBufCount; ++i )
	{
		FRAME_BUF* pFrame = get_frame_buffer_dec(i);

		pVpu->m_stFrameBuffer[PA][i].bufY     = pFrame->vbY.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufCb    = pFrame->vbCb.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufCr    = pFrame->vbCr.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].bufMvCol = pFrame->vbMvCol.phys_addr;
		pVpu->m_stFrameBuffer[PA][i].myIndex  = i;

		// for output addr
		//save offset from m_FrameBufferStartPAddr
		pVpu->m_stFrameBuffer[VA][i].bufY     = pVpu->m_stFrameBuffer[PA][i].bufY - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufCb    = pVpu->m_stFrameBuffer[PA][i].bufCb - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufCr    = pVpu->m_stFrameBuffer[PA][i].bufCr - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].bufMvCol = pVpu->m_stFrameBuffer[PA][i].bufMvCol - pVpu->m_FrameBufferStartPAddr;
		pVpu->m_stFrameBuffer[VA][i].myIndex = i;

	#ifdef DBG_PRINTF_RUN_STATUS
		if( i > 0 )
		{
			int min_frame_buffer_size = pVpu->m_stFrameBuffer[PA][i].bufY - pVpu->m_stFrameBuffer[PA][i-1].bufY;
			if( pVpu->m_iMinFrameBufferSize != min_frame_buffer_size )
			{
				DSTATUS( "[VPU_DEC] NO!!! pVpu->m_iMinFrameBufferSize = alloc(%d), min_frame_buffer_size = addr(%d) \n", pVpu->m_iMinFrameBufferSize, min_frame_buffer_size );
			}
			else
			{
				DSTATUS( "[VPU_DEC] OK!!! pVpu->m_iMinFrameBufferSize = %d \n", pVpu->m_iMinFrameBufferSize );
			}
		}
	#endif
	}

	buf_info.avcSliceBufInfo.bufferBase = pBufferParam->m_AvcSliceSaveBufferAddr;
	buf_info.avcSliceBufInfo.bufferSize = pBufferParam->m_iAvcSliceSaveBufferSize;

#ifdef USE_SEC_AXI_SRAM
	VPU_DecGiveCommand(handle, SET_SEC_AXI, &handle->CodecInfo.decInfo.secAxiUse);
#endif

	{
		unsigned int maxDecMbNumXY = 0; //[31:16] MaxMbNum, [15: 8] MaxMbX, [ 7: 0] MaxMbY
		if(!(pVpu->m_uiDecOptFlags & DISABLE_DETECT_RESOLUTION_CHANGE))
		{
			unsigned int maxDecMbNum, maxDecMbX, maxDecMbY;

			maxDecMbX = ((pVpu->m_iFrameBufWidth+15)&(~15));	//MaxDecMbX: Maximum decodable frame width in MB. If the value of MaxMbX is 0, Frame Height checking will be ignored
			maxDecMbY = ((pVpu->m_iFrameBufHeight+31)&(~31));	//MaxDecMbY: Maximum decodable frame height in MB. If the value of MaxMbY is 0, Frame Height checking will be ignored
			maxDecMbX >>= 4;
			maxDecMbY >>= 4;
			maxDecMbNum = maxDecMbX * maxDecMbY;			//maxDecMbNum: Maximum decodable frame MB number. If the value of MaxMbNum is 0, Frame MB number checking will be ignored.
			maxDecMbNumXY = (maxDecMbNum <<16) | (maxDecMbX<<8) | (maxDecMbY);
		}
		VpuWriteReg( CMD_SET_FRAME_MAX_DEC_SIZE, maxDecMbNumXY);
	}

	ret = VPU_DecRegisterFrameBuffer(handle, pVpu->m_stFrameBuffer[PA], needFrameBufCount, stride, pVpu->m_iFrameBufHeight, &buf_info);
	if (ret != RETCODE_SUCCESS)
	{
	#ifdef ADD_SWRESET_VPU_HANGUP
		if (ret == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_SWRESET
			dec_reset( NULL, (0 | (1<<16)));
		}
	#endif

		return ret;
	}

	pVpu->m_iFrameBufferCount = pBufferParam->m_ulFrameBufferCount;
	pVpu->m_iDecodedFrameCount = 0;

	return ret;
}

static RetCode
dec_decode( vpu_t* pVpu, dec_input_t* pInputParam, dec_output_t* pOutParam )
{
	RetCode ret = RETCODE_SUCCESS;
	DecHandle handle = pVpu->m_pCodecInst;
	DecParam dec_param = {0,};
	int iPackedPBframe_flag = 0; //MPEG-4 Packed PB-frame
	int startOneFrameOn = 0;

	vpu_memset(&pOutParam->m_DecOutInfo, 0, sizeof(pOutParam->m_DecOutInfo), 0);

	//check overflow of chunk size
	if (dec_param.jpgChunkSize > pVpu->m_pCodecInst->CodecInfo.decInfo.streamBufSize)
		return RETCODE_INVALID_PARAM;

	//! [2] Run Decoder
	{
		dec_param.avcDpbResetOnIframeSearch = 0;
		if (pInputParam->m_Reserved[0] > 0)
		{
			unsigned int uflag = (pInputParam->m_Reserved[0] & 0x01000001);	//[24]:control flag(1), [0]:disable(0)/enable(1)

			if (uflag == 0x01000001)
			{
				dec_param.avcDpbResetOnIframeSearch = 1;
			}
		}

		dec_param.jpgChunkSize = pInputParam->m_iBitstreamDataSize;
		dec_param.pJpgChunkBase = (BYTE*)pInputParam->m_BitstreamDataAddr[VA];
		dec_param.skipframeMode = pInputParam->m_iSkipFrameMode;
//		dec_param.skipframeNum = pInputParam->m_iSkipFrameNum;
		dec_param.iframeSearchEnable = pInputParam->m_iFrameSearchEnable;

		if (dec_param.iframeSearchEnable)
		{
			//dec_param.iframeSearchEnable = 1;
			//dec_param.skipframeNum = 0; //only processing to find the first I(IDR)-frame
			dec_param.skipframeMode = 0;

			pVpu->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlag = 0;
		}

		if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END)
		{
			if ((dec_param.iframeSearchEnable == 0x201) || (dec_param.iframeSearchEnable == 1) || (dec_param.skipframeMode ==1))
			{
				pVpu->m_pCodecInst->CodecInfo.decInfo.isMp4PackedPBstream = 0;
			}
		}

		if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
		{
			int bsBufSize;
			unsigned int rdptr, wrptr;

			if (pInputParam->m_iDecOptions == DEC_IN_OPTS_SET_EOS) {
				VpuWriteReg(BIT_BIT_STREAM_PARAM, (1<<2));
			}

			ret = VPU_DecGetBitstreamBuffer(handle, &rdptr, &wrptr, &bsBufSize);

			if (ret != RETCODE_SUCCESS)
				return ret;

			{
				long long size;
				if( rdptr < wrptr )
				{
					size = (long long)(wrptr - rdptr);
				}
				else
				{
					long long start_addr, end_addr;

					start_addr = (long long)(pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamBuffer&0x0FFFFFFFF);
					end_addr = start_addr + pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamBufferSize;
					size = (end_addr - rdptr);
					size += wrptr;
					size -= start_addr;
				}
				size -= 1;

				if ((pInputParam->m_iDecOptions != DEC_IN_OPTS_SET_EOS) && (size < 1024)) {
					return RETCODE_INSUFFICIENT_BITSTREAM;
				}
			}
		}
		else //BS_MODE_PIC_END
		{
			unsigned char seqlength = 0;

			if ( (pVpu->m_pCodecInst->CodecInfo.decInfo.isFirstFrame == 0) ||
				 (pVpu->m_pCodecInst->codecMode == AVC_DEC)
				 )
			{
				VPU_DecSetRdPtr(handle, pInputParam->m_BitstreamDataAddr[PA]+seqlength, 1);
				ret = VPU_DecUpdateBitstreamBuffer(pVpu->m_pCodecInst, pInputParam->m_iBitstreamDataSize);
			}

			if ( pVpu->m_pCodecInst->CodecInfo.decInfo.isFirstFrame != 0 )
				pVpu->m_pCodecInst->CodecInfo.decInfo.isFirstFrame = 0;
		}

DEC_REPEAT_FRAME:

		if(pInputParam->m_iBitstreamDataSize == 0)
		{
			VPU_DecUpdateBitstreamBuffer(handle, 0);

			if( pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT )
			{
				PhysicalAddress wrptr;

				wrptr = pVpu->m_pCodecInst->CodecInfo.decInfo.streamWrPtr;
				pVpu->m_pCodecInst->CodecInfo.decInfo.streamRdPtr = wrptr;
				pVpu->m_pCodecInst->CodecInfo.decInfo.streamWrPtr = wrptr;
			}
		}


		//VPU_DecGiveCommand( handle, DISABLE_REP_USERDATA, 0 );

		#if 0
		// frame buffer status
		{
			unsigned char tempBuf[8];					// 64bit bus & endian
			unsigned int val;
			unsigned char* picParaBaseAddr_rk = 0;

			val = handle->CodecInfo.decInfo.frameBufStateAddr;

			tempBuf[0] = (val >> 0) & 0xff;
			tempBuf[1] = (val >> 8) & 0xff;
			tempBuf[2] = (val >> 16) & 0xff;
			tempBuf[3] = (val >> 24) & 0xff;
			tempBuf[4] = 0;
			tempBuf[5] = 0;
			tempBuf[6] = 0;
			tempBuf[7] = 0;

			picParaBaseAddr_rk = pVpu->m_BitWorkAddr[1] + 914*1024 + 1024;
			vpu_memcpy( picParaBaseAddr_rk, (unsigned char *)tempBuf, 8);
		} // frame buffer status
		#endif

		//. User Data Buffer Setting if it's required.
		if(handle->CodecInfo.decInfo.userDataEnable)
		{
			handle->CodecInfo.decInfo.userDataBufAddr = pInputParam->m_UserDataAddr[PA];
			handle->CodecInfo.decInfo.userDataBufSize = pInputParam->m_iUserDataBufferSize;
			pOutParam->m_DecOutInfo.m_UserDataAddress[0] = pInputParam->m_UserDataAddr[0];
			pOutParam->m_DecOutInfo.m_UserDataAddress[1] = pInputParam->m_UserDataAddr[1];
		}

RINGMODE_BOTTOM_FIELD_DEC:

		if( (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
			&& (GetPendingInst() == handle )
			&& (pVpu->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason & (1<<INT_BIT_BIT_BUF_EMPTY))
			)
		{
			VpuWriteReg(handle->CodecInfo.decInfo.streamWrPtrRegAddr, handle->CodecInfo.decInfo.streamWrPtr);
		}
		else
		{
			int reason;
			reason = VpuReadReg(BIT_INT_REASON);
			if(reason & (1<<INT_BIT_DEC_FIELD))
			{
				if ( pVpu->m_pCodecInst != GetPendingInst())
				{
					pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_INVALID_INSTANCE;
					return RETCODE_SUCCESS;
				}

				VpuWriteReg(BIT_INT_REASON, 0);
				VpuWriteReg(BIT_INT_CLEAR, 1);
			}
			else
			{
				startOneFrameOn = 1;
				ret = VPU_DecStartOneFrame( handle, &dec_param );
				if( ret != RETCODE_SUCCESS )
				{
					return ret;
				}
			}
		}

		pVpu->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = 0;


		while (1) //After Pic run, wait until VPU operation is completed.
		{
			int iRet = 0;
		#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
			if(VpuReadReg(BIT_CUR_PC) == 0x0)
			{
				dec_reset(pVpu, (0 | (1 << 16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif

			if (vpu_interrupt != NULL)
				iRet = vpu_interrupt();

			if (iRet == RETCODE_CODEC_EXIT)
			{
			#ifdef ADD_SWRESET_VPU_HANGUP
				MACRO_VPU_SWRESET
				dec_reset(pVpu, (0 | (1 << 16)));
			#endif
				return RETCODE_CODEC_EXIT;
			}

			iRet = VpuReadReg(BIT_INT_REASON);
			if (iRet)
			{
				if (iRet & ((1<<INT_BIT_PIC_RUN)|(1<<INT_BIT_DEC_BUF_FLUSH)))
				{
					break;
				}
				else
				{
					if (iRet & (1<<INT_BIT_BIT_BUF_EMPTY))
					{
						VpuWriteReg(BIT_INT_REASON, 0);
						VpuWriteReg(BIT_INT_CLEAR, 1);
						if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT)
						{
							SetPendingInst(0);
							return RETCODE_FRAME_NOT_COMPLETE;
						}
						else
						{
							SetPendingInst(handle);
							pVpu->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = (1<<INT_BIT_BIT_BUF_EMPTY);
							return RETCODE_INSUFFICIENT_BITSTREAM;
						}
					}
					else if (iRet & (1<<INT_BIT_BIT_BUF_FULL))
					{
						VpuWriteReg(BIT_INT_REASON, 0);
						VpuWriteReg(BIT_INT_CLEAR, 1);
						return RETCODE_INSUFFICIENT_FRAME_BUFFERS;
					}
					else if( iRet & (	 (1<<INT_BIT_SEQ_END)
										|(1<<INT_BIT_SEQ_INIT)
										|(1<<INT_BIT_FRAMEBUF_SET)
										|(1<<INT_BIT_ENC_HEADER)
										|(1<<INT_BIT_DEC_PARA_SET)
										|(1<<INT_BIT_USERDATA)		 ) )
					{
						VpuWriteReg(BIT_INT_REASON, 0);
						VpuWriteReg(BIT_INT_CLEAR, 1);
						return RETCODE_FAILURE;
					}
					else if (iRet & (1<<INT_BIT_DEC_FIELD))
					{
						if ( pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END )
						{
							SetPendingInst(pVpu->m_pCodecInst);
							pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_SUCCESS_FIELD_PICTURE;	//5
							{
								int val, picType, picTypeFirst, indexFrameDecoded;
								int decFrameInfo;	// AvcPicTypeExt

								val = VpuReadReg(RET_DEC_PIC_TYPE);
								picType = val & 7;	//picType = val & 0x1F;	//for V2.1.8

								pOutParam->m_DecOutInfo.m_iPicType = picType;
								picTypeFirst = (val & 0x38) >> 3;
								pOutParam->m_DecOutInfo.m_iTopFieldFirst = (val >> 21) & 0x0001;	// TopFieldFirst[18]
								indexFrameDecoded = (int)VpuReadReg(RET_DEC_PIC_DECODED_IDX);

								pOutParam->m_DecOutInfo.m_iDispOutIdx = -3;
								pOutParam->m_DecOutInfo.m_iDecodedIdx = indexFrameDecoded;
								//pOutParam->m_DecOutInfo.m_iPicType = picTypeFirst;	// V2.1.8
								if( (pVpu->m_pCodecInst->codecMode == AVC_DEC) && (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 1) )
								{
									decFrameInfo = (val >> 15) & 0x1;
									pOutParam->m_DecOutInfo.m_iPicType |= (decFrameInfo << 15);
								}
							}

							pOutParam->m_DecOutInfo.m_iInterlacedFrame = 1;
							//pOutParam->m_DecOutInfo.m_iTopFieldFirst = 1;
							pOutParam->m_DecOutInfo.m_iNumOfErrMBs = -1;
							pOutParam->m_DecOutInfo.m_iOutputStatus = 0;

							{	// consumedbytes
								int rdptr;
								rdptr = VpuReadReg(pVpu->m_pCodecInst->CodecInfo.decInfo.streamRdPtrRegAddr);
								pVpu->m_pCodecInst->CodecInfo.decInfo.frameEndPos = rdptr;
								if (pVpu->m_pCodecInst->CodecInfo.decInfo.frameEndPos > (int)pVpu->m_pCodecInst->CodecInfo.decInfo.streamBufEndAddr)
								{
									pOutParam->m_DecOutInfo.m_iConsumedBytes = pVpu->m_pCodecInst->CodecInfo.decInfo.streamBufEndAddr - pVpu->m_pCodecInst->CodecInfo.decInfo.frameStartPos;
									pOutParam->m_DecOutInfo.m_iConsumedBytes += pVpu->m_pCodecInst->CodecInfo.decInfo.frameEndPos - pVpu->m_pCodecInst->CodecInfo.decInfo.streamBufStartAddr;
								}
								else
								{
									pOutParam->m_DecOutInfo.m_iConsumedBytes = pVpu->m_pCodecInst->CodecInfo.decInfo.frameEndPos - pVpu->m_pCodecInst->CodecInfo.decInfo.frameStartPos;
								}
							}
						#ifdef TEST_INTERLACE_BOTTOM_TEST
							if (handle->CodecInfo.decInfo.framenum==0)	// 20120214
							{
								VPU_SWReset(0);
								VpuWriteReg(BIT_INT_REASON, 0);
								VpuWriteReg(BIT_INT_CLEAR, 1);
							}
							handle->CodecInfo.decInfo.framenum++;	// 20120214
						#endif

							return RETCODE_SUCCESS;
						}
						else if ( pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT )
						{
							goto RINGMODE_BOTTOM_FIELD_DEC;
						}
					}
				}
			}
		}

		VpuWriteReg(BIT_INT_REASON, 0);
		VpuWriteReg(BIT_INT_CLEAR, 1);
	}

	//! [3] Get Decoder Output
	{
		int disp_idx = 0;
		DecOutputInfo info = {0,};

		ret = VPU_DecGetOutputInfo(handle, &info);
		if( ret != RETCODE_SUCCESS )
		{
			return ret;
		}

		if (info.avcNpfFieldInfo)
		{
			if (pVpu->m_iVersionRTL == 3)
				pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx |= (1 << info.indexFrameDecoded);
			else
				pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx |= (1 << info.indexFrameDisplay);
		}

		if(handle->CodecInfo.decInfo.userDataEnable)
		{
		#ifndef USERDATA_SIZE_CHECK
			swap_user_data((unsigned char *)pOutParam->m_DecOutInfo.m_UserDataAddress[VA], handle->CodecInfo.decInfo.userDataBufSize);
		#else
			check_user_data_size((unsigned char *)pOutParam->m_DecOutInfo.m_UserDataAddress[VA], handle->CodecInfo.decInfo.userDataBufSize, &info);
		#endif
		}

		dec_out_info( pVpu, &pOutParam->m_DecOutInfo, &info );

		pOutParam->m_DecOutInfo.m_Reserved[12] = info.frameCycle; //DBG_DEC_OUTINFO_FRAME


	#ifdef DBG_COUNT_PIC_INDEX
		printf( "[VPU_DEC] #%d, indexFrameDisplay %d || picType %d || indexFrameDecoded %d|| MSG_0 %x|| MSG_1 %x|| MSG_2 %x|| MSG_3 %x|| rd Ptr %x|| wr ptr %x\n",
				pVpu->m_iDbgPicIndex, info.indexFrameDisplay, info.picType,
				info.indexFrameDecoded, VpuReadReg(BIT_MSG_0), VpuReadReg(BIT_MSG_1), VpuReadReg(BIT_MSG_2), VpuReadReg(BIT_MSG_3),
				VpuReadReg(BIT_RD_PTR_0),VpuReadReg(BIT_WR_PTR_0) );
	#endif

		//! Store Image
		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS)
		{
			int disp_idx;

			//! Display Buffer
			pVpu->m_iDecodedFrameCount++;
			pVpu->m_iDecodedFrameCount = pVpu->m_iDecodedFrameCount % pVpu->m_iFrameBufferCount;
			disp_idx= pOutParam->m_DecOutInfo.m_iDispOutIdx;

			if (pVpu->m_iIndexFrameDisplay >= 0)
				pVpu->m_iIndexFrameDisplayPrev = pVpu->m_iIndexFrameDisplay;

			pVpu->m_iIndexFrameDisplay = disp_idx;
			if (disp_idx >= 0)
			{
				//Physical Address
				pOutParam->m_pDispOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pDispOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pDispOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);
				//Virtual Address
				pOutParam->m_pDispOut[VA][COMP_Y] = (unsigned char *)(pVpu->m_FrameBufferStartVAddr + (codec_addr_t)(pVpu->m_stFrameBuffer[VA][disp_idx].bufY));
				pOutParam->m_pDispOut[VA][COMP_U] = (unsigned char *)(pVpu->m_FrameBufferStartVAddr + (codec_addr_t)(pVpu->m_stFrameBuffer[VA][disp_idx].bufCb));
				pOutParam->m_pDispOut[VA][COMP_V] = (unsigned char *)(pVpu->m_FrameBufferStartVAddr + (codec_addr_t)(pVpu->m_stFrameBuffer[VA][disp_idx].bufCr));
			}
			else
			{
				pOutParam->m_pDispOut[PA][COMP_Y] = pOutParam->m_pDispOut[VA][COMP_Y] = NULL;
				pOutParam->m_pDispOut[PA][COMP_U] = pOutParam->m_pDispOut[VA][COMP_U] = NULL;
				pOutParam->m_pDispOut[PA][COMP_V] = pOutParam->m_pDispOut[VA][COMP_V] = NULL;
			}

		}

		if (pOutParam->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_OUTPUT_SUCCESS)
		{
			int decoded_idx;

			//! Decoded Buffer
			decoded_idx= pOutParam->m_DecOutInfo.m_iDecodedIdx;
			if (decoded_idx >= 0)
			{
			#ifdef DBG_COUNT_PIC_INDEX
				pVpu->m_iDbgPicIndex++;
			#endif

				//Physical Address
				pOutParam->m_pCurrOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufY);
				pOutParam->m_pCurrOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCb);
				pOutParam->m_pCurrOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][decoded_idx].bufCr);
				//Virtual Address
				pOutParam->m_pCurrOut[VA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufY);
				pOutParam->m_pCurrOut[VA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCb);
				pOutParam->m_pCurrOut[VA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][decoded_idx].bufCr);
			}
			else
			{
				pOutParam->m_pCurrOut[PA][COMP_Y] = pOutParam->m_pCurrOut[VA][COMP_Y] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_U] = pOutParam->m_pCurrOut[VA][COMP_U] = NULL;
				pOutParam->m_pCurrOut[PA][COMP_V] = pOutParam->m_pCurrOut[VA][COMP_V] = NULL;
			}
		}

		if (pOutParam->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS)
		{
			//! Previously Displayed Buffer
			//disp_idx = pVpu->m_iPrevDispIdx;
			disp_idx = pVpu->m_iIndexFrameDisplayPrev;
			if (disp_idx >= 0)
			{
				//Physical Address
				pOutParam->m_pPrevOut[PA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufY);
				pOutParam->m_pPrevOut[PA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[PA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[PA][disp_idx].bufCr);
				//Virtual Address
				pOutParam->m_pPrevOut[VA][COMP_Y] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufY);
				pOutParam->m_pPrevOut[VA][COMP_U] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCb);
				pOutParam->m_pPrevOut[VA][COMP_V] = (unsigned char *)((codec_addr_t)pVpu->m_stFrameBuffer[VA][disp_idx].bufCr);
			}
			else
			{
				pOutParam->m_pPrevOut[PA][COMP_Y] = pOutParam->m_pPrevOut[VA][COMP_Y] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_U] = pOutParam->m_pPrevOut[VA][COMP_U] = NULL;
				pOutParam->m_pPrevOut[PA][COMP_V] = pOutParam->m_pPrevOut[VA][COMP_V] = NULL;
			}
		}

		if (info.decodingSuccess == 0)
			return ret;

		if (info.indexFrameDisplay == -1)
		{
			return RETCODE_CODEC_FINISH; //DecPicIdx RET_DEC_PIC_DISPLAY_IDX (0x1C4)  -1(0xFFFF) : VPU does not have more pictures to decode and display.
		}
		else if ((info.indexFrameDisplay > pVpu->m_iNeedFrameBufCount))
		{
			return RETCODE_CODEC_FINISH;
		}

		if (info.chunkReuseRequired)
		{
			if (pVpu->m_pCodecInst->codecMode == MP4_DEC)
				return RETCODE_CODEC_SPECOUT;

			if (pVpu->m_pCodecInst->codecMode == AVC_DEC) {
				pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_BUF_FULL;
				// AVC && PIC_END => ((RET_DEC_PIC_SUCCESS >> 16) & 0x01); //1 - Input stream for decoding should be reused by Non-Paired Field, 0 - Decoded normally
				// [20190602] Only in pic end mode, here pic end mode is not checked so confirmation is required (???)
				//  H.264/AVC: This flag is valid when AVC decoding and StreamMode in BIT_BIT_STREAM_PARAM[4:3] register is set as picend mode.
				// [20190602] npf situation: In a state where only one field was previously decoded, the current input is determined to be a field in another frame, so NPF is returned and has not yet been decoded, and chunkReuseRequired is generated.
				// Therefore, in Frame output mode, it goes back to decode the input again.
				if (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 0) {
					pOutParam->m_DecOutInfo.m_iDecodedIdx = -2;
					goto DEC_REPEAT_FRAME;
				}
			}
		}

		if ((pVpu->m_pCodecInst->codecMode == AVC_DEC)
			&& (pVpu->m_pCodecInst->CodecInfo.decInfo.initialInfo.interlace == 0))
		{
			//Since it is an AVC NPF situation (one-field missing state), the frame cannot be output if it is not in one-field output mode. (If necessary later, it seems necessary to take it out and process it with an Error MB.)
			if (pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx && (info.indexFrameDisplay >= 0))
			{
				if ((((pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx >> info.indexFrameDisplay) & 0x1) == 1)
					&& (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 0))
				{   //If the avc one-field output option is disabled: The display index is cleared internally and the index is not exported externally.
					dec_buffer_flag_clear( pVpu, info.indexFrameDisplay );
					pOutParam->m_DecOutInfo.m_iDispOutIdx = -3;
					pOutParam->m_DecOutInfo.m_iOutputStatus = 0;
					pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx &= ~(1 << info.indexFrameDisplay);
				}
			}

			if (startOneFrameOn && (info.indexFrameDecoded == -4))	//-4 : Occurs in AVC NPF.
			{
				pOutParam->m_DecOutInfo.m_iDecodingStatus = -3;
				if (pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx && (info.indexFrameDisplay >= 0))
				{
					if ((((pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx >> info.indexFrameDisplay) & 0x1) == 1)
						&& (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 0))
					{
						dec_buffer_flag_clear( pVpu, info.indexFrameDisplay );
						pOutParam->m_DecOutInfo.m_iDispOutIdx = -3;
						pOutParam->m_DecOutInfo.m_iOutputStatus = 0;
						pVpu->m_pCodecInst->CodecInfo.decInfo.avcNpfFieldIdx &= ~(1 << info.indexFrameDisplay);
					}
				}
				return RETCODE_SUCCESS;
			}

			if (info.indexFrameDecoded == -4)
				pOutParam->m_DecOutInfo.m_iDecodingStatus = VPU_DEC_BUF_FULL;

		#ifdef TEST_AVC_NPF_TCC_VPU_D6_ARM32_V1_03_4_02_004
			//In order to avoid a situation where the buffer displayed as a decoded index is later reused as a decoded index without being displayed as a display index.
			// TMMT-1840
			if ((info.avcNpfFieldInfo)
				&& (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.avcnpfenable == 0)
				&& (pVpu->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT))
			{
				if ((pOutParam->m_DecOutInfo.m_iDecodingStatus == 1) && (pOutParam->m_DecOutInfo.m_iDecodedIdx >= 0))
					pOutParam->m_DecOutInfo.m_iDecodingStatus = -3;
			}
		#endif
		}
	}

	if (iPackedPBframe_flag)
		pOutParam->m_DecOutInfo.m_iPicType |= (0x100); //B-frame of Packed PB-frame

	return ret;
}

static codec_result_t
dec_ring_buffer_automatic_control_new(vpu_t *pVpu, dec_init_t *pDec_init, dec_ring_buffer_setting_in_t *pDec_ring_setting)
{
	RetCode ret;
	DecHandle handle = pVpu->m_pCodecInst;
	unsigned int ring_buffer_read_pointer = 0;
	unsigned int ring_buffer_write_pointer = 0;
	int available_space_in_buffer = 0; // Input provided for VPU API call. I don't think it will be used in practice. This is because the packet size is fixed.
	int copied_size = 0;
	int fill_size = 0; // It must be smaller than the available buffer space.
	int available_room_to_the_end_point;
	int bitstream_buffer_physical_virtual_offset = 0;

	bitstream_buffer_physical_virtual_offset = pDec_init->m_BitstreamBufAddr[0] - pDec_init->m_BitstreamBufAddr[1];

	// Receives read point, write point and available size information
	VPU_DecGetBitstreamBuffer(handle, (PhysicalAddress *)&ring_buffer_read_pointer, (PhysicalAddress *)&ring_buffer_write_pointer, (int *)&available_space_in_buffer);

	// ring buffer implementation here

	// Fill Buffer
	fill_size = pDec_ring_setting->m_iOnePacketBufferSize; // the size of packet

	// If the capacity to write to the buffer exceeds the end of the buffer, write to the end of the buffer and then write again from the beginning of the buffer.
	if ((ring_buffer_write_pointer + fill_size) > (pDec_init->m_BitstreamBufAddr[0] + pDec_init->m_iBitstreamBufSize))
	{
		available_room_to_the_end_point = (pDec_init->m_BitstreamBufAddr[0] + pDec_init->m_iBitstreamBufSize) - ring_buffer_write_pointer ;

		// Write to the end of the buffer
		if(available_room_to_the_end_point != 0) {
			codec_addr_t wptr_addr = ring_buffer_write_pointer - bitstream_buffer_physical_virtual_offset;
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(wptr_addr),
				CONVERT_CA_PVOID(buff_addr),
				(unsigned int)available_room_to_the_end_point,
				((gInitFirst == 4)?(0):(1)));
		}

		// The remaining packets are written again from the beginning of the buffer.
		if ((fill_size-available_room_to_the_end_point) != 0) {
			codec_addr_t bitstream_addr = pDec_init->m_BitstreamBufAddr[1];
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr + available_room_to_the_end_point;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(bitstream_addr),
				CONVERT_CA_PVOID(buff_addr),
				(unsigned int)(fill_size - available_room_to_the_end_point),
				((gInitFirst == 4) ? (0) : (1)));
		}

		copied_size = fill_size;
	}
	else // When it is possible to write sequentially to the buffer
	{
		if (fill_size != 0) {
			codec_addr_t wptr_addr = ring_buffer_write_pointer - bitstream_buffer_physical_virtual_offset;
			codec_addr_t buff_addr = pDec_ring_setting->m_OnePacketBufferAddr;
			pDec_init->m_Memcpy(CONVERT_CA_PVOID(wptr_addr),
				CONVERT_CA_PVOID(buff_addr),
				(unsigned int)fill_size,
				((gInitFirst == 4)?(0):(1)));
		}

		copied_size = fill_size;
	}

	ret = VPU_DecUpdateBitstreamBuffer(handle, copied_size);

	return ret;
}


static codec_result_t
dec_flush_ring_buffer(vpu_t *pVpu)
{
	codec_result_t ret = 0;
	CodecInst *pCodecInst;
	DecInfo *pDecInfo;
	int i;

	pCodecInst = pVpu->m_pCodecInst;

	pDecInfo = &pCodecInst->CodecInfo.decInfo;

	if (GetPendingInst())
	{
		int reason = VpuReadReg(BIT_INT_REASON);

		if ((reason & ((1<<INT_BIT_DEC_FIELD) | (1<<INT_BIT_BIT_BUF_EMPTY))) || (pDecInfo->m_iPendingReason & (1<<INT_BIT_BIT_BUF_EMPTY)))
		{
			unsigned val = VpuReadReg(pDecInfo->streamRdPtrRegAddr);

			if(reason & (1<<INT_BIT_DEC_FIELD))
			{
				VpuWriteReg(pDecInfo->streamWrPtrRegAddr, val );
				VpuWriteReg(BIT_INT_REASON, 0);
				VpuWriteReg(BIT_INT_CLEAR, 1);
			}
			else if ((reason & (1<<INT_BIT_BIT_BUF_EMPTY)) || (pDecInfo->m_iPendingReason & (1<<INT_BIT_BIT_BUF_EMPTY)))
			{
				VpuWriteReg(pDecInfo->streamWrPtrRegAddr, val);
				while (1)
				{
					int iRet = 0;

					if (VpuReadReg(BIT_CUR_PC) == 0x0)
					{
					#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
						dec_reset(pVpu, (0 | (1<<16)));
					#endif
						return RETCODE_CODEC_EXIT;
					}

					if (vpu_interrupt != NULL)
						iRet = vpu_interrupt();

					if (iRet == RETCODE_CODEC_EXIT)
					{
					#ifdef ADD_SWRESET_VPU_HANGUP
						MACRO_VPU_SWRESET
						dec_reset(pVpu, (0 | (1<<16)));
					#endif
						return RETCODE_CODEC_EXIT;
					}

					iRet = VpuReadReg(BIT_INT_REASON);
					if (iRet)
					{
						VpuWriteReg(BIT_INT_REASON, 0);
						VpuWriteReg(BIT_INT_CLEAR, 1);
						break;
					}
				}
			}

			while( 1 )
			{
				int iRet = 0;

				if (VpuReadReg(BIT_CUR_PC) == 0x0)
				{
				#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
					dec_reset(pVpu, (0 | (1<<16)));
				#endif
					return RETCODE_CODEC_EXIT;
				}

				if (vpu_interrupt != NULL)
					iRet = vpu_interrupt();

				if (iRet == RETCODE_CODEC_EXIT)
				{
				#ifdef ADD_SWRESET_VPU_HANGUP
					MACRO_VPU_SWRESET
					dec_reset(pVpu, (0 | (1<<16)));
				#endif
					return RETCODE_CODEC_EXIT;
				}

				iRet = VpuReadReg(BIT_INT_REASON);
				if (iRet)
				{
					if( iRet & (   (1<<INT_BIT_PIC_RUN) | (1<<INT_BIT_DEC_BUF_FLUSH) | (1<<INT_BIT_SEQ_END) ) )
					{
						ClearPendingInst();
						VpuWriteReg(BIT_INT_REASON, 0);
						VpuWriteReg(BIT_INT_CLEAR, 1);
						break;
					}
					else //if(( iRet & (1<<INT_BIT_BIT_BUF_EMPTY) ) || ( iRet & (1<<INT_BIT_DEC_FIELD) )
					{
						return RETCODE_CODEC_EXIT;
					}
				}
			}
		}
		ClearPendingInst();
		pDecInfo->m_iPendingReason = 0;
	}

	for(i=0; i<32;i++)
	{
		if( pDecInfo->clearDisplayIndexes & (1<<i) )
			VPU_DecClrDispFlag((DecHandle)pCodecInst, i);
	}

	ret = VPU_DecBitBufferFlush( (DecHandle)pCodecInst );
	if (ret != RETCODE_SUCCESS)
	{
		if (ret == RETCODE_CODEC_EXIT)
		{
		#ifdef ADD_SWRESET_VPU_HANGUP
			MACRO_VPU_SWRESET
			dec_reset(NULL, (0 | (1<<16)));
		#endif
			return RETCODE_CODEC_EXIT;
		}
		return RETCODE_FAILURE;
	}
	return RETCODE_SUCCESS;
}

static codec_result_t set_vpuc7_fw_addr(vpu_c7_set_fw_addr_t *pstFWAddr) {
	codec_result_t ret = RETCODE_SUCCESS;
	if (pstFWAddr == NULL) {
		return RETCODE_INVALID_PARAM;
	} else {
		gFWAddr = pstFWAddr->m_FWBaseAddr;
		DLOGI("gFWAddr 0x%x", gFWAddr);
	}
	return RETCODE_SUCCESS;
}

int dec_check_handle_instance(vpu_t *pVpu, CodecInst *pCodecInst)
{
	//check global variables
	int ret = RETCODE_SUCCESS;
	int bitstreamformat;
	unsigned int codecType = 0;

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

	ret = checkCodecInstancePended((void *)pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INSTANCE_MISMATCH_ON_PENDING_STATUS;
	}

	codecType = checkCodecType(bitstreamformat, (void *)pCodecInst);
	if (codecType == 0U) {
		ret = RETCODE_NOT_INITIALIZED;
	} else {
		unsigned int isEncoder = (codecType >> 8U) & 0x00FFU;	//0: Decoder(default), 1: Encoder
		unsigned int isCheckCodec = codecType & 0x00FFU;	//0: not yet, 1: confirmed, 2: codec mis-match

		if ((isEncoder == 1U) || (isCheckCodec == 0U) || (isCheckCodec == 2U)) {
			ret = RETCODE_NOT_INITIALIZED;
		}
	}
	return ret;
}

codec_result_t
TCC_VPU_DEC_EXT(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if (Op == 1)
	{
		gInitFirst_exit = 2;
	}
	return 0;
}

codec_result_t
TCC_VPU_DEC_ESC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	if (Op == 1)
	{
		gInitFirst_exit = 1;
	}
	return 0;
}

codec_result_t
TCC_VPU_DEC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	codec_result_t ret = RETCODE_SUCCESS;
	vpu_t *p_vpu_dec = NULL;

	if (pHandle != NULL) {
		p_vpu_dec = (vpu_t *)(*pHandle);
	}

	if (Op == VPU_CTRL_LOG_STATUS) {
		vpu_dec_ctrl_log_status_t *pst_ctrl_log = (vpu_dec_ctrl_log_status_t *)pParam1;
		if (pst_ctrl_log == NULL) {
			ret = RETCODE_INVALID_PARAM;
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
			ret = RETCODE_INVALID_PARAM;
		} else {
			vpu_c7_set_fw_addr_t *pst_fw_addr = (vpu_c7_set_fw_addr_t *)pParam1;
			ret = set_vpuc7_fw_addr(pst_fw_addr);
		}
	}
	else if (Op == VPU_DEC_INIT)
	{
		dec_init_t *p_init_param = (dec_init_t *)pParam1;
		dec_initial_info_t *p_initial_info_param = (dec_initial_info_t *)pParam2;

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

		ret = dec_init(&p_vpu_dec, p_init_param, p_initial_info_param);
		if ((ret != RETCODE_SUCCESS) && (ret != RETCODE_CALLED_BEFORE))
		{
			return ret;
		}

		V_PRINTF(VPU_KERN_ERR PRT_PREFIX "handle:%p(%x), RevisionFW: %d, RTL(Date: %d, revision: %d)\n",
				 p_vpu_dec, (codec_handle_t)p_vpu_dec,
				 p_vpu_dec->m_iCodeRevision, p_vpu_dec->m_iHwDate, p_vpu_dec->m_iVersionRTL);

		*pHandle = (codec_handle_t)p_vpu_dec;
	}
	else if( Op == VPU_DEC_SEQ_HEADER )
	{
		dec_initial_info_t *p_initial_info_param = (dec_initial_info_t *)pParam2;
		dec_input_t *p_input_param = NULL;
		DecOpenParam *openParam = NULL;
		int seq_stream_size = 0;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		openParam = &p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam;

		if (openParam->seq_init_bsbuff_change != NULL) {
			p_input_param	= (dec_input_t*)pParam1;
			seq_stream_size = p_input_param->m_iBitstreamDataSize;
		} else {
			seq_stream_size = (int)((codec_addr_t)pParam1);
		}

		vpu_memset(&gsDecInitialInfo, 0, sizeof(gsDecInitialInfo), 0);

		if (seq_stream_size <= 0) {
			seq_stream_size = p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamBufSize - 1;
		}

		if (openParam->bitstreamMode == BS_MODE_PIC_END) {
			DecInfo *pDecInfo;

			pDecInfo = &p_vpu_dec->m_pCodecInst->CodecInfo.decInfo;
			VPU_DecSetRdPtr(p_vpu_dec->m_pCodecInst, pDecInfo->streamBufStartAddr, 0);
		}

		if (openParam->seq_init_bsbuff_change != 0) {
			if (openParam->bitstreamMode == BS_MODE_PIC_END) {
				DecInfo *pDecInfo;

				pDecInfo = &p_vpu_dec->m_pCodecInst->CodecInfo.decInfo;
				if (p_input_param != NULL) {
					pDecInfo->streamRdPtr = p_input_param->m_BitstreamDataAddr[PA];
				} else {
					pDecInfo->streamRdPtr = NULL;
				}

				ret = VPU_DecSetRdPtr(p_vpu_dec->m_pCodecInst, pDecInfo->streamRdPtr, 1);
				if (ret != RETCODE_SUCCESS) {
					return ret;
				}
			}
		}

		if (openParam->bitstreamMode != BS_MODE_INTERRUPT) {
			ret = VPU_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, seq_stream_size);
			if (ret != RETCODE_SUCCESS) {
				return ret;
			}
		}

		ret = dec_seq_header(p_vpu_dec, &gsDecInitialInfo);
		if (p_initial_info_param != NULL) {
			vpu_memcpy(p_initial_info_param, &gsDecInitialInfo, sizeof(gsDecInitialInfo), 0);
		}
	}
	else if (Op == VPU_DEC_GET_INFO)
	{
		dec_initial_info_t *p_initial_info_param = (dec_initial_info_t *)pParam2;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		if( p_initial_info_param != NULL )
			vpu_memcpy( p_initial_info_param, &gsDecInitialInfo, sizeof(gsDecInitialInfo), 0 );
	}
	else if (Op == VPU_DEC_REG_FRAME_BUFFER)
	{
		dec_buffer_t *p_buffer_param = (dec_buffer_t *)pParam1;

		if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = dec_register_frame_buffer(p_vpu_dec, p_buffer_param);
		if( ret != RETCODE_SUCCESS )
			return ret;
	}
	else if (Op == VPU_DEC_REG_FRAME_BUFFER2)
	{
		dec_buffer2_t *pst_buffer = (dec_buffer2_t *)pParam1;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = dec_register_frame_buffer2(p_vpu_dec, pst_buffer);
		if( ret != RETCODE_SUCCESS )
			return ret;
	}
	else if( Op == VPU_DEC_REG_FRAME_BUFFER3 )
	{
		dec_buffer3_t* pst_buffer = (dec_buffer3_t*)pParam1;

		if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
		{
			return RETCODE_INVALID_HANDLE;
		}

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = dec_register_frame_buffer3(p_vpu_dec, pst_buffer);
		if( ret != RETCODE_SUCCESS )
		{
			return ret;
		}
	}
	else if (Op == VPU_DEC_DECODE)
	{
		dec_input_t *p_input_param   = (dec_input_t  *)pParam1;
		dec_output_t *p_output_param = (dec_output_t *)pParam2;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		ret = dec_decode(p_vpu_dec, p_input_param, p_output_param);

	#if USE_VPU_STATUS_RESERVED_VAR >= 1
		get_vpu_status_info(p_vpu_dec->m_pCodecInst, &p_output_param->m_DecOutInfo.m_Reserved[2], &p_output_param->m_DecOutInfo.m_Reserved[3]);
	#endif
		if (ret == RETCODE_CODEC_EXIT)
			return ret;

		if ((p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.SWRESETEnable) &&
		    (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT))
		{
			{
				int busy_check = 0;

				busy_check = VpuReadReg(BIT_BUSY_FLAG);

				if (GetPendingInst() || busy_check)
				{
					if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_PIC_END)
					{
						ClearPendingInst();
						MACRO_VPU_SWRESET
					}
					else if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
					{
						p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamRdPtr = p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamRdPtrBackUp;
						ClearPendingInst();
						MACRO_VPU_SWRESET
						VpuWriteReg(p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamRdPtrRegAddr, p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.streamRdPtr);
						{
							int decidx, val;
							decidx = VpuReadReg(RET_DEC_PIC_DECODED_IDX);
							dec_buffer_flag_clear(p_vpu_dec, decidx);
							p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlag = VpuReadReg(p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlagRegAddr);
							val = p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlag;
							val &= ~p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.clearDisplayIndexes;
							VpuWriteReg(p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.frameDisplayFlagRegAddr, val);
							p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.clearDisplayIndexes = 0;
						}
					}
				}
			}
		}

		if( ret != RETCODE_SUCCESS )
		{
			return ret;
		}
	}
	else if (Op == VPU_DEC_BUF_FLAG_CLEAR)
	{
		int *p_idx_display = (int *)pParam1;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		if(*p_idx_display > 32) // if the frameBufferCount exceeds the maximum buffer count number 32..
			return RETCODE_INSUFFICIENT_FRAME_BUFFERS; // FIXME error code

		dec_buffer_flag_clear( p_vpu_dec, *p_idx_display );
	}
	else if (Op == VPU_DEC_FLUSH_OUTPUT)
	{
		dec_input_t *p_input_param   = (dec_input_t  *)pParam1;
		dec_output_t *p_output_param = (dec_output_t *)pParam2;

		int inputSize = p_input_param->m_iBitstreamDataSize;

		p_input_param->m_iBitstreamDataSize = 0;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0) {
				return res;
			}
		}

		if (p_vpu_dec->m_pCodecInst->inUse == 0)
			return RETCODE_INVALID_HANDLE;

		if ((p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
				&& (p_input_param->m_iDecOptions == DEC_IN_OPTS_SET_EOS)) {
			p_input_param->m_iBitstreamDataSize = inputSize;
		}

		ret = dec_decode(p_vpu_dec, p_input_param, p_output_param);
		if (ret != RETCODE_SUCCESS)
		{
			if (ret == RETCODE_CODEC_FINISH)
			{
				int i;

				VPU_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, -1); //STREAM_END_SET_FLAG(-1) set to stream end condition to pump out a delayed framebuffer.

				//[20190602] There are two cases of return from dec_decode(), both of which are forced to buf. It seems necessary to check whether clear is correct.
				//					1) if( info.indexFrameDisplay == -1 )
				//					2) if( info.indexFrameDisplay > pVpu->m_iNeedFrameBufCount)
				for (i = 0 ; i < p_vpu_dec->m_iNeedFrameBufCount ; i++)
					dec_buffer_flag_clear(p_vpu_dec, i);

				ret = VPU_DecFrameBufferFlush(p_vpu_dec->m_pCodecInst);
				if (ret == RETCODE_CODEC_EXIT)
				{
				#ifdef ADD_SWRESET_VPU_HANGUP
					MACRO_VPU_SWRESET
					dec_reset(NULL, (0 | (1 << 16)));
				#endif
					return RETCODE_CODEC_EXIT;
				}
				ret = RETCODE_CODEC_FINISH;
			}
			return ret;
		}
	}
	else if (Op == GET_RING_BUFFER_STATUS) // . for ring buffer.
	{
		dec_ring_buffer_status_out_t *p_buf_status = (dec_ring_buffer_status_out_t *)pParam2;
		PhysicalAddress read_ptr, write_ptr;
		int room;

		if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0)
				return res;
		}

		ret = VPU_DecGetBitstreamBuffer(p_vpu_dec->m_pCodecInst, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (int *)&room);
		if (ret != RETCODE_SUCCESS)
		{
			p_buf_status->m_ulAvailableSpaceInRingBuffer = 0;
			return ret;
		}
		p_buf_status->m_ulAvailableSpaceInRingBuffer = (unsigned int)room;
		p_buf_status->m_ptrReadAddr_PA = read_ptr;
		p_buf_status->m_ptrWriteAddr_PA = write_ptr;
	}
	else if (Op == VPU_UPDATE_WRITE_BUFFER_PTR) // . for ring buffer.
	{
		int copied_size = (int)((codec_addr_t)pParam1);
		int flush_flag = (int)((codec_addr_t)pParam2);
		int insufficient_status = 0;

		if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0)
				return res;
		}

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT)
		{
			return RETCODE_INVALID_PARAM;
		}

		if (flush_flag)
		{
			unsigned int pendingInst, vpu_pendingInst;	//FIXME : CodecInst *

			pendingInst = (unsigned int)((codec_addr_t)GetPendingInst());
			vpu_pendingInst = (unsigned int)(*((unsigned int *)p_vpu_dec->m_pPendingHandle));
			if ((pendingInst)
					&& (pendingInst == vpu_pendingInst)
					&& (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason == (1<<INT_BIT_BIT_BUF_EMPTY)))
			{
				if (VPU_IsBusy())
				{
					insufficient_status = 1;

					{
						unsigned int val;
						val = VpuReadReg(BIT_BIT_STREAM_PARAM);
						val &= ~(3<<3);
						val |= (2<<3);
						VpuWriteReg(BIT_BIT_STREAM_PARAM, val);
						VpuWriteReg(BIT_INT_REASON, 0);
						p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = 0;
					}
				}
			}
		}

		if (flush_flag)
		{
			ret = dec_flush_ring_buffer(p_vpu_dec);
			if (insufficient_status)
			{
				unsigned int val;
				val = VpuReadReg(BIT_BIT_STREAM_PARAM);
				val &= ~(3<<3);
				VpuWriteReg(BIT_BIT_STREAM_PARAM, val);
			}
			if (ret == RETCODE_CODEC_EXIT)
				return ret;
		}

		if ((flush_flag == 1) && (copied_size <= 0))
			return ret;
		if ((flush_flag != 1) && (copied_size <= 0))
			return RETCODE_INVALID_PARAM;

		if (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT)
		{
			unsigned int updateFlag;
			unsigned int pendingInst, vpu_pendingInst;	//FIXME : CodecInst *

			pendingInst = (unsigned int)((codec_addr_t)GetPendingInst());
			vpu_pendingInst = (unsigned int)(*((unsigned int *)p_vpu_dec->m_pPendingHandle));
			if (    (pendingInst)
			     && (pendingInst == vpu_pendingInst)
			     && (p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason == (1<<INT_BIT_BIT_BUF_EMPTY)))
			{
				updateFlag = 0;
			}
			else
			{
				updateFlag = 1;
				if (VpuReadReg(BIT_INT_REASON))
				{
					if (!(VpuReadReg(BIT_INT_REASON)&(1<<INT_BIT_DEC_FIELD)))
					{
						return RETCODE_FAILURE; //FIXME : VPU_SWReset(0) ???
					}
				}
			}
			ret = VPU_DecUpdateBitstreamBuffer2(p_vpu_dec->m_pCodecInst, copied_size, updateFlag);
		}
		else
		{
			ret = VPU_DecUpdateBitstreamBuffer(p_vpu_dec->m_pCodecInst, copied_size);
		}
	}
	else if (Op == FILL_RING_BUFFER_AUTO) // . for ring buffer.
	{
		dec_init_t* p_init_param = (dec_init_t*)pParam1;
		dec_ring_buffer_setting_in_t* p_ring_input_setting = (dec_ring_buffer_setting_in_t *) pParam2;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0)
				return ret;
		}

		ret = dec_ring_buffer_automatic_control_new ( p_vpu_dec, p_init_param, p_ring_input_setting );
	}
	else if (Op == GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY) // . for ring buffer.
	{
		dec_initial_info_t* p_initial_info_param = (dec_initial_info_t*)pParam1;

		if( (p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL) )
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0)
				return res;
		}

		ret = dec_seq_header( p_vpu_dec, p_initial_info_param );
	}
	else if (Op == VPU_CODEC_GET_VERSION)
	{
		char* ppsz_version    = (char*)pParam1;
		char* ppsz_build_date = (char*)pParam2;

		if( (ppsz_version == NULL) ||  (ppsz_build_date==NULL) )
		{
			return RETCODE_INVALID_PARAM;
		}

		vpu_memcpy(ppsz_version, VPU_CODEC_NAME_STR, VPU_CODEC_NAME_LEN, ((gInitFirst == 4)?(0):(2)));
		vpu_memcpy(ppsz_build_date, VPU_CODEC_BUILD_DATE_STR, VPU_CODEC_BUILD_DATE_LEN, ((gInitFirst == 4)?(0):(2)));

		if( p_vpu_dec == NULL )
			return RETCODE_INVALID_HANDLE;
	}
	else if (Op == VPU_GET_VERSION)
	{
		vpu_get_version_t *pst_getversion = (vpu_get_version_t *)pParam1;

		if (pst_getversion == NULL)
			return RETCODE_INVALID_PARAM;

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
	else if (Op == VPU_DEC_CLOSE)
	{
		DecHandle handle;
		dec_output_t *p_output_param  = (dec_output_t *)pParam2;

		if ((p_vpu_dec == NULL) || (p_vpu_dec->m_pCodecInst == NULL))
			return RETCODE_INVALID_HANDLE;

		//check handle and instance information
		{
			int res;
			res = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (res != 0)
				return res;
		}

		handle = p_vpu_dec->m_pCodecInst;

		if (gInitFirst == 0)
			return RETCODE_NOT_INITIALIZED;

		if( p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT )
		{
			if( p_vpu_dec->m_pCodecInst )
			{
				if( VPU_IsBusy() ||
					(p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason & (1<<INT_BIT_BIT_BUF_EMPTY) )
					)
				{
					VPU_DecUpdateBitstreamBuffer(handle, 0);
					if (GetPendingInst() == handle)
					{
						ClearPendingInst();
					}
				}
				p_vpu_dec->m_pCodecInst->CodecInfo.decInfo.m_iPendingReason = 0;
			}
			if( p_vpu_dec->m_pPendingHandle )
			{
				if( *(DecHandle*)p_vpu_dec->m_pPendingHandle == handle )
					*(DecHandle*)p_vpu_dec->m_pPendingHandle = (DecHandle)0;
			}
		}
		else
		{
			if (VPU_IsBusy())
			{
				VPU_DecUpdateBitstreamBuffer(handle, 0);
				if (GetPendingInst() == handle)
				{
					ClearPendingInst();
				}
			}
		}

		ret = VPU_DecClose(handle);

	#ifdef ADD_VPU_CLOSE_SWRESET
		if (ret == RETCODE_CODEC_EXIT)
		{
			MACRO_VPU_SWRESET
			dec_reset( NULL, (0 | (1 << 16)));
			return ret;
		}
	#endif

		if (ret == RETCODE_FRAME_NOT_COMPLETE)
		{
			VPU_DecUpdateBitstreamBuffer(handle, 0);
			if (GetPendingInst() == handle)
			{
				ClearPendingInst();
			}
			ret  = VPU_DecClose(handle);
		#ifdef ADD_VPU_CLOSE_SWRESET
			if (ret == RETCODE_CODEC_EXIT)
			{
				MACRO_VPU_SWRESET
				dec_reset(NULL, (0 | (1 << 16)));
				return ret;
			}
		#endif
		}

		free_vpu_handle( p_vpu_dec );
	}
	else if (Op == VPU_DEC_SWRESET)
	{
		if (GetPendingInst())
		{
			ClearPendingInst();
		}
		MACRO_VPU_SWRESET
		return RETCODE_SUCCESS;
	}
	else if (Op == VPU_RESET_SW)
	{
		if (p_vpu_dec == NULL)
			return RETCODE_INVALID_HANDLE;

		dec_reset(p_vpu_dec, (1 | (1 << 16)));
		free_vpu_handle(p_vpu_dec);
	}
	else if (Op == VPU_DEC_GET_INSTANCE_STATUS)
	{
		//Add command to check VPU instance information
		//    TCC_VPU_DEC( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
		//        OP : #define VPU_DEC_GET_INSTANCE_STATUS	0x100  (in TCC_VPU_D6.h)
		//        pHandle : VPU handle
		//        pParam1 : unsigned int option[16]
		//        pParam2 : unsigned int results[16]
		//                  results[0]: VPU status info
		//                            : [31:24] used instance number
		//                            : [23:16] current instance index + 1
		//                            : [15: 8] Pending instance index + 1
		//                            : [ 7: 0]
		//                  results[1]: VPU Pending handle : [0:31] handle
		DecHandle handle;
		unsigned int *pOption = (unsigned int *)pParam1;	//unsigned int option[16]
		unsigned int *pResults = (unsigned int *)pParam2;	//unsigned int results[16]

		if ((pOption == NULL) || (pResults == NULL)) {
			ret = RETCODE_INVALID_HANDLE;
		}

		//check handle and instance information
		if (ret == RETCODE_SUCCESS){
			ret = dec_check_handle_instance(p_vpu_dec, p_vpu_dec->m_pCodecInst);
			if (ret == RETCODE_SUCCESS) {
				handle = p_vpu_dec->m_pCodecInst;
				ret = get_vpu_status_info(p_vpu_dec->m_pCodecInst, (unsigned int *)pResults, (unsigned int *)(pResults+1));
			}
		}
	} else {
		ret = RETCODE_INVALID_COMMAND;
	}

	return ret;
}
