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

#ifndef VPU_DEF_HEADER
#define VPU_DEF_HEADER

#include "vpu_pre_define.h"
#include "vpuapifunc.h"

struct FrameBuffer;
#ifndef MAX_FRAME
#define MAX_FRAME                       (16+1+4+11)	// AVC REF 16, REC 1 , FULL DUP , Additional 11. (in total of 32)
#endif
#define NUM_FRAME_BUF                   MAX_FRAME

#ifndef NULL
	#define NULL 0
#endif

#define VPU_PRODUCT_NAME_REGISTER       0x1040	// DBG_CONFIG_REPORT_0
#define VPU_PRODUCT_CODE_REGISTER       0x1044	// DBG_CONFIG_REPORT_1

#define MIXER_ENDIAN                    0
#define FRAME_STRIDE                    1920
#define FRAME_ENDIAN                    1	// 0 (little endian), 1 (big endian)
#define FRAME_MAX_X                     1920
#define FRAME_MAX_Y                     1088
#define MIXER_LAYER_DISP                1

typedef enum {
	VDI_LITTLE_ENDIAN               = 0,
	VDI_BIG_ENDIAN,
	VDI_32BIT_LITTLE_ENDIAN,
	VDI_32BIT_BIG_ENDIAN,
	VDI_NULL                        = 0x7FFFFFFF
} EndianMode;

typedef struct vpu_buffer_t {
	int size;
	unsigned long phys_addr;
	unsigned long base;
	unsigned long virt_addr;
} vpu_buffer_t;

typedef struct {
	int  Format;
	int  Index;
	int mapType;
	vpu_buffer_t vbY;
	vpu_buffer_t vbCb;
	vpu_buffer_t vbCr;
	int stride_lum;
	int stride_chr;
	int height;
	vpu_buffer_t vbMvCol;
} FRAME_BUF;

enum {
	MODE420                         = 0,
	MODE422                         = 1,
	MODE224                         = 2,
	MODE444                         = 3,
	MODE400                         = 4,
	MODE400_NULL                    = 0x7FFFFFFF
};

typedef struct vpu_field_info_t
{
	int m_lPicType;
	int m_lDispIndex;
	int m_lDecIndex;
	int m_lOutputStatus;
	int m_lConsumedBytes;
	int m_Reserved;
} vpu_field_info_t;

typedef struct vpu_t
{
	// Codec Instance
	int m_iVpuInstIndex;
	int m_bInUse;
	CodecInst *m_pCodecInst;

	// Codec Info
	int m_iBitstreamFormat;         // STD_AVC, STD_VC1, ...
	int m_iPicWidth;
	int m_iPicHeight;
	int m_iFrameFormat;

	int m_iFrameBufWidth;
	int m_iFrameBufHeight;
	int m_uiMaxResolution;          // #define RESOLUTION_1080_HD 0 //1920x1088
	                                // #define RESOLUTION_720P_HD 1 //1280x720
	                                // #define RESOLUTION_625_SD  2 //720x576
	int m_iNeedFrameBufCount;
	int m_iMinFrameBufferSize;

	PhysicalAddress m_FrameBufferStartPAddr;
	codec_addr_t    m_FrameBufferStartVAddr;
	FrameBuffer m_stFrameBuffer[2][NUM_FRAME_BUF];

	int m_iPrevDispIdx;             // decoder only
	int m_iIndexFrameDisplay;       // decoder only
	int m_iIndexFrameDisplayPrev;   // decoder only for previous m_iIndexFrameDisplay

	PhysicalAddress m_StreamRdPtrRegAddr;
	PhysicalAddress m_StreamWrPtrRegAddr;
	int m_iBitstreamOutBufferPhyVirAddrOffset;

	codec_addr_t m_BitWorkAddr[2];  // physical[0] and virtual[1] address of a working space of the decoder. This working buffer space consists of work buffer, code buffer, and parameter buffer.

	int m_iFrameBufferCount;        // allocated number of frame buffers
	int m_iDecodedFrameCount;       // The number of decoded frame

	unsigned int m_bFilePlayEnable;
	unsigned int m_uiFrameBufDelay;

	//! Decoding Options
	//#define M4V_DEBLK_DISABLE     0	// (default)
	//#define M4V_DEBLK_ENABLE      1	// mpeg-4 deblocking
	//#define M4V_GMC_FILE_SKIP     (0<<1)	// (default) seq.init failure
	//#define M4V_GMC_FRAME_SKIP    (1<<1)	// frame skip without decoding
	//#define RESOLUTION_CHANGE_SUPPORT (1<<2)	// alloc. max. resolution for changing resolution
	unsigned int m_uiDecOptFlags;
	unsigned int m_uiEncOptFlags;

	vpu_field_info_t m_stFieldInfo;

#if defined(USE_VC1_SKIPFRAME_KEEPING)
	int m_iVc1SkipFrameKeepEnable;
	int m_iVc1SkipFrameKeepIdx;
	int m_iVc1SkipFrameClearIdx;
#endif

#if defined(DBG_COUNT_PIC_INDEX)
	int m_iDbgPicIndex;
#endif

#ifdef USE_HW_AND_FW_VERSION
	unsigned int m_iVersionFW;
	unsigned int m_iCodeRevision;
	unsigned int m_iVersionRTL;
	unsigned int m_iHwRevision;
	unsigned int m_iHwDate;
#endif

	void *m_pPendingHandle;

	//for 8-bytes align.
#if !defined(USE_VC1_SKIPFRAME_KEEPING) && defined(DBG_COUNT_PIC_INDEX)
	int m_Reserved;
#elif defined(USE_VC1_SKIPFRAME_KEEPING) && !defined(DBG_COUNT_PIC_INDEX)
	int m_Reserved;
#endif
	EncOpenParam m_openParam;

} vpu_t;

#endif	//VPU_DEF_HEADER
