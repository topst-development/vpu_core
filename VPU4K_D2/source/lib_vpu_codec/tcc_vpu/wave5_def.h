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

#ifndef VPU_4KD2_DEF_H
#define VPU_4KD2_DEF_H

#include "wave5_pre_define.h"
#include "wave5apifunc.h"

struct FrameBuffer;
#ifndef MAX_FRAME
#define MAX_FRAME               (32)	// AVC REF 16, REC 1 , FULL DUP , Additional 11. (in total of 32)
#endif
#define NUM_FRAME_BUF           MAX_FRAME

#ifndef NULL
	#define NULL 0
#endif

#define MIXER_ENDIAN            0
#define FRAME_STRIDE            1920
#define FRAME_ENDIAN            1	// 0 (little endian), 1 (big endian)
#define FRAME_MAX_X             1920
#define FRAME_MAX_Y             1088
#define MIXER_LAYER_DISP        1

// changed for coda960
typedef struct {
	int  Format;
	int  Index;
	int mapType;
	vpu_buffer_t vbY;
	vpu_buffer_t vbCb;
	vpu_buffer_t vbCr;
	int  stride_lum;
	int  stride_chr;
	int  height;
	vpu_buffer_t vbMvCol;
} FRAME_BUF;

typedef struct wave5_t
{
	// Codec Instance
	int m_iVpuInstIndex;
	int m_bInUse;
	CodecInst* m_pCodecInst;

	// Codec Info
	int m_iBitstreamFormat;	//!< STD_HEVC, ...
	int m_iPicWidth;
	int m_iPicHeight;
	int m_iFrameFormat;

	int m_iFrameBufWidth;
	int m_iFrameBufHeight;
	int m_uiMaxResolution;	//!< Reserved
	int m_iNeedFrameBufCount;
	int m_iMinFrameBufferSize;

	PhysicalAddress m_FrameBufferStartPAddr;
	FrameBuffer m_stFrameBuffer[2][62];

	int m_iPrevDispIdx;
	int m_iIndexFrameDisplay;
	int m_iIndexFrameDisplayPrev;	//!< decoder only for previous m_iIndexFrameDisplay

	PhysicalAddress m_StreamRdPtrRegAddr;
	PhysicalAddress m_StreamWrPtrRegAddr;
	int m_iBitstreamOutBufferPhyVirAddrOffset;

	unsigned long m_BitWorkAddr[2];	//!< physical[0] and virtual[1] address of a working space of the decoder. This working buffer space consists of work buffer, code buffer, and parameter buffer.

	int m_iFrameBufferCount;	//!< allocated number of frame buffers
	int m_iDecodedFrameCount;	//!< The number of decoded frame

	unsigned int m_bFilePlayEnable;
	unsigned int m_uiFrameBufDelay;

	//! Decoding Options
	//	WAVE5_WTL_DISABLE          0		// (default)
	//	WAVE5_WTL_ENABLE           1		// WTL Enable
	//	SEC_AXI_BUS_DISABLE        (1<<1)	// SecAxi Disable
	//	WAVE5_HEVC_REORDER_DISABLE (1<<2)	// reorder disable only for HEVC(not yet), (default) reorder enable
	//	WAVE5_10BITS_DISABLE       (1<<3)	// 10 to 8 bits Output Enable
	//	WAVE5_nv21_ENABLE          (1<<4)	// NV21 Enable
	//	WAVE5_RESOLUTION_CHANGE    (1<<16)	// alloc. max. resolution for changing resolution
	unsigned int m_uiDecOptFlags;

	unsigned int m_iVersionFW;
	unsigned int m_iRevisionFW;

	unsigned int m_iDateRTL;
	unsigned int m_iRevisionRTL;

	struct
	{
		int iUseBitstreamOffset;		/**< [inp] Indicates the offset from the start pointer of the bitstream. This only works if the variable 'vpu_4K_D2_dec_init_t::m_iFilePlayEnable' is zero. */
		int iMeasureDecPerf;		  	/**< [inp] Measure decoder performance and show profiling data through the pfPrintCb callback. */
		void (*pfPrintCb)(const char *, ...);	/**< [inp] A pointer to the printk callback function used to display decoded information. */

#define VPU4K_D2_OPT_SET_CQ_COUNT 101
		int iReserved[16];
	} stSetOptions; // VPU_4KD2_SET_OPTIONS

	void *m_pPendingHandle;
	int iApplyPrevOutInfo;
	DecOutputInfo stPrevDecOutputInfo;

	int iCqCount;

	int iRepeatCntForFBNum1;
	int iRepeatCntForFBNum2;
} wave5_t;

#endif