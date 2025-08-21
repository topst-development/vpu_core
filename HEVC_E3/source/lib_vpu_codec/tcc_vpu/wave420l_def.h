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

#ifndef VPU_HEVC_ENC_DEF_H
#define VPU_HEVC_ENC_DEF_H

#include "wave420l_pre_define.h"
#include "vpuapifunc.h"
#include "wave420l_log.h"
#include "wave420l_version.h"

#ifndef MAX_FRAME_HEVC_ENC
#define MAX_FRAME_HEVC_ENC      (8)
#endif
#define NUM_FRAME_BUF_HEVC_ENC  MAX_FRAME_HEVC_ENC

#ifndef NULL
	#define NULL            0
#endif

#define FRAME_ENDIAN_HEVC_ENC   1	// 0 (little endian), 1 (big endian)
#define FRAME_MAX_X_HEVC_ENC    3840
#define FRAME_MAX_Y_HEVC_ENC    2160
#define FRAME_MIN_X_HEVC_ENC    256
#define FRAME_MIN_Y_HEVC_ENC    128

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

enum {
	MODE420 = 0,
	MODE422 = 1,
	MODE224 = 2,
	MODE444 = 3,
	MODE400 = 4,
	MODE400_NULL = 0x7FFFFFFF
};

typedef struct wave420l_t
{
	// Codec Instance
	int m_iVpuInstIndex;
	int m_bInUse;
	CodecInst *m_pCodecInst;

	// Codec Info
	int m_iBitstreamFormat;	// STD_HEVC_ENC(17)  => in CNM code WAVE420L_STD_HEVC(12)
	int m_iFrameFormat;

	int m_iPicWidth;
	int m_iFrameBufWidth;   // 8 bytes aligned
	int m_iFrameBufStride;   //32 bytes aligned
	int m_iPicHeight;
	int m_iFrameBufHeight;  // 8 bytes aligned

	int m_iNeedFrameBufCount;
	int m_iMinFrameBufferSize;

	unsigned int bufAlignSize;	// Aligned size of buffer (>= VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN 4Kbytes)
	unsigned int bufStartAddrAlign;	// Aligned start address of buffer (>= VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN 4Kbytes)

	PhysicalAddress m_FrameBufferStartPAddr;
	FrameBuffer m_stFrameBuffer[2][8];

	int m_iBitstreamOutBufferPhyVirAddrOffset;

	unsigned long m_BitWorkAddr[2];	//!< physical[0] and virtual[1] address of a working space of the decoder. This working buffer space consists of work buffer, code buffer, and parameter buffer.

	//! Encoding Options
	unsigned int m_uiEncOptFlags;

	unsigned int m_iVersionFW;
	unsigned int m_iRevisionFW;

	unsigned int m_iDateRTL;
	unsigned int m_iRevisionRTL;
	unsigned int m_iVersionRTL;

	void *m_pPendingHandle;

} wave420l_t;

#endif//VPU_HEVC_ENC_DEF_H