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


#ifndef __VPUAPI_PRODUCT_ABSTRACT_H__
#define __VPUAPI_PRODUCT_ABSTRACT_H__

#include "wave5apifunc.h"

#define IS_DECODER_HANDLE(_instance)      (_instance->codecMode < AVC_ENC)
#define IS_7Q_DECODER_HANDLE(_instance)   (_instance->codecMode < W_AVC_ENC)

enum {
	FramebufCacheNone,
	FramebufCacheMaverickI,
	FramebufCacheMaverickII,
	FramebufCache_NULL = 0x7FFFFFFF
};

typedef enum {
	AUX_BUF_TYPE_MVCOL,
	AUX_BUF_TYPE_FBC_Y_OFFSET,
	AUX_BUF_TYPE_FBC_C_OFFSET,
	AUX_BUF_TYPE_NULL = 0x7FFFFFFF
} AUX_BUF_TYPE;

typedef struct _tag_FramebufferIndex {
	Int16 tiledIndex;
	Int16 linearIndex;
} FramebufferIndex;

typedef struct _tag_VpuAttrStruct {
	Uint32  productId;
	Uint32  productNumber;
	char    productName[8];
	Uint32  supportDecoders;            /* bitmask: See CodStd in vpuapi.h */
	Uint32  supportEncoders;            /* bitmask: See CodStd in vpuapi.h */
	Uint32  numberOfMemProtectRgns;     /* always 10 */
	Uint32  numberOfVCores;
	BOOL    supportWTL;
	BOOL    supportTiled2Linear;
	BOOL    supportTiledMap;
	BOOL    supportMapTypes;            /* Framebuffer map type */
	BOOL    supportLinear2Tiled;        /* Encoder */
	BOOL    support128bitBus;
	BOOL    supportThumbnailMode;
	BOOL    supportBitstreamMode;
	BOOL    supportFBCBWOptimization;   /* WAVE4xx decoder feature */
	BOOL    supportGDIHW;
	BOOL    supportCommandQueue;
	Uint32  supportEndianMask;
	Uint32  framebufferCacheType;       /* 0: for None, 1 - Maverick-I, 2 - Maverick-II */
	Uint32  bitstreamBufferMargin;
	Uint32  interruptEnableFlag;
	Uint32  hwConfigDef0;
	Uint32  hwConfigDef1;
	Uint32  hwConfigFeature;            /**<< supported codec standards */
	Uint32  hwConfigDate;               /**<< Configuation Date Decimal, ex)20151130 */
	Uint32  hwConfigRev;                /**<< SVN revision, ex) 83521 */
	Uint32  hwConfigType;               /**<< Not defined yet */
} VpuAttr;

extern VpuAttr g_VpuCoreAttributes[MAX_NUM_VPU_CORE];

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Returns 0 - not found product id
 *         1 - found product id
 */

extern RetCode ProductVpuGetVersion(
	Uint32  coreIdx,
	Uint32* versionInfo,
	Uint32* revision
	);

extern RetCode ProductVpuInit(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void*  firmware,
	Uint32 size,
	Int32 cq_count
	);

extern Uint32 ProductVpuIsInit(
	Uint32 coreIdx
	);

extern RetCode ProductVpuReInit(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void*  firmware,
	Uint32 size,
	Int32 cq_count
	);

extern Int32 ProductVpuIsBusy(
	Uint32 coreIdx
	);

extern RetCode ProductVpuReset(
	Uint32      coreIdx,
	SWResetMode resetMode,
	Int32		cq_count
	);

extern RetCode ProductVpuClearInterrupt(
	Uint32      coreIdx,
	Uint32      flags
	);

extern RetCode ProductVpuDecBuildUpOpenParam(
	CodecInst*    instance,
	DecOpenParam* param
	);

extern RetCode ProductCheckDecOpenParam(
	DecOpenParam* param
	);

extern RetCode ProductVpuDecInitSeq(
	CodecInst*  instance
	);

extern RetCode ProductVpuDecFiniSeq(
	CodecInst*  instance
	);

extern RetCode ProductVpuDecSetBitstreamFlag(
	CodecInst*  instance,
	BOOL        running,
	Int32       size
	);

/*
 * FINI_SEQ
 */
extern RetCode ProductVpuDecEndSequence(
	CodecInst*  instance
	);

/**
 *  @brief      Abstract function for SEQ_INIT.
 */
extern RetCode ProductVpuDecGetSeqInfo(
	CodecInst*      instance,
	DecInitialInfo* info
	);

/**
 *  \brief      Check parameters for product specific decoder.
 */
extern RetCode ProductVpuDecCheckCapability(
	CodecInst*  instance
	);

/**
 * \brief       Decode a coded picture.
 */
extern RetCode ProductVpuDecode(
	CodecInst*  instance,
	DecParam*   option
	);

/**
 *
 */
extern RetCode ProductVpuDecGetResult(
	CodecInst*      instance,
	DecOutputInfo*  result
	);

/**
 * \brief                   Flush framebuffers to prepare decoding new sequence
 * \param   instance        decoder handle
 * \param   retIndexes      Storing framebuffer indexes in display order.
 *                          If retIndexes[i] is -1 then there is no display index from i-th.
 */
extern RetCode ProductVpuDecFlush(
	CodecInst*          instance,
	FramebufferIndex*   retIndexes,
	Uint32              size
	);

/**
 *  \brief      Allocate framebuffers with given parameters
 */
extern RetCode ProductVpuAllocateFramebuffer(
	CodecInst*          instance,
	FrameBuffer*        fbArr,
	PhysicalAddress     addr,
	TiledMapType        mapType,
	Int32               num,
	Int32               stride,
	Int32               height,
	FrameBufferFormat   format,
	BOOL                cbcrInterleave,
	BOOL                nv21,
	Int32               endian,
	vpu_buffer_t*       vb,
	Int32               gdiIndex,
	FramebufferAllocType fbType
	);

/**
 *  \brief      Register framebuffers to VPU
 */
extern RetCode ProductVpuRegisterFramebuffer(
	CodecInst*      instance
	);

extern RetCode ProductVpuRegisterFramebuffer2(
	CodecInst*      instance
	);

extern RetCode ProductVpuRegisterFramebuffer3(
	CodecInst*      instance
	);

extern Int32 ProductCalculateFrameBufSize(
	Int32               productId,
	Int32               stride,
	Int32               height,
	TiledMapType        mapType,
	FrameBufferFormat   format,
	BOOL                interleave,
	DRAMConfig*         pDramCfg
	);

extern Int32 ProductCalculateAuxBufferSize(
	AUX_BUF_TYPE    type,
	Uint32          codStd,
	Int32           width,
	Int32           height
	);

extern RetCode ProductClrDispFlag(
	CodecInst* instance,
	Uint32 index
	);

extern RetCode ProductSetDispFlag(
	CodecInst* instance,
	Uint32 index
	);
extern RetCode ProductClrDispFlagEx(
	CodecInst* instance,
	Uint32 dispFlag
	);

extern RetCode ProductSetDispFlagEx(
	CodecInst* instance,
	Uint32 dispFlag
	);
extern PhysicalAddress ProductVpuDecGetRdPtr(
	CodecInst* instance
	);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* __VPUAPI_PRODUCT_ABSTRACT_H__ */
