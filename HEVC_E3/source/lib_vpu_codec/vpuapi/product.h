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

#ifndef VPUAPI_PRODUCT_ABSTRACT_H
#define VPUAPI_PRODUCT_ABSTRACT_H

#include "vpuapi.h"
#include "vpuapifunc.h"

enum {
	FramebufCacheNone,
	FramebufCacheMaverickI,
	FramebufCacheMaverickII,
};

typedef enum {
	AUX_BUF_TYPE_MVCOL,
	AUX_BUF_TYPE_FBC_Y_OFFSET,
	AUX_BUF_TYPE_FBC_C_OFFSET
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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern RetCode wave420l_ProductVpuGetVersion(
		Uint32  coreIdx,
		Uint32* versionInfo,
		Uint32* revision
		);

extern RetCode wave420l_ProductVpuInit(
		Uint32 coreIdx,
		PhysicalAddress workBuf,
		PhysicalAddress codeBuf,
		void*  firmware,
		Uint32 size
		);

extern RetCode wave420l_ProductVpuReInit (
		Uint32 coreIdx,
		PhysicalAddress workBuf,
		PhysicalAddress codeBuf,
		void* firmware, Uint32 size
		);

extern Uint32 wave420l_ProductVpuIsInit(
		Uint32 coreIdx
		);

extern Int32 wave420l_ProductVpuIsBusy(
		Uint32 coreIdx
		);

extern Int32 wave420l_ProductVpuWaitInterrupt(
		CodecInst*  instance,
		Int32       timeout
		);

extern RetCode wave420l_ProductVpuReset(
		Uint32 coreIdx,
		SWResetMode resetMode
		);

extern RetCode wave420l_ProductVpuClearInterrupt(
		Uint32      coreIdx,
		Uint32      flags
		);


/**
 *  \brief      Allocate framebuffers with given parameters
 */
extern RetCode wave420l_ProductVpuAllocateFramebuffer(
		CodecInst*          instance,
		FrameBuffer*        fbArr,
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
extern RetCode wave420l_ProductVpuRegisterFramebuffer(
		CodecInst*      instance
		);

extern Int32 wave420l_ProductCalculateFrameBufSize(
		Int32               productId,
		Int32               stride,
		Int32               height,
		TiledMapType        mapType,
		FrameBufferFormat   format,
		BOOL                interleave,
		DRAMConfig*         pDramCfg
		);

extern Int32 wave420l_ProductCalculateAuxBufferSize(
		AUX_BUF_TYPE    type,
		CodStd          codStd,
		Int32           width,
		Int32           height
		);


/************************************************************************/
/* ENCODER                                                              */
/************************************************************************/
extern RetCode wave420l_ProductVpuEncBuildUpOpenParam(
		CodecInst*      pCodec,
		EncOpenParam*   param
		);

extern RetCode wave420l_ProductVpuEncFiniSeq(
		CodecInst*      instance
		);

extern RetCode wave420l_ProductCheckEncOpenParam(
		EncOpenParam*   param
		);

extern RetCode wave420l_ProductVpuEncSetup(
		CodecInst*      instance
		);

extern RetCode wave420l_ProductVpuEncode(
		CodecInst*      instance,
		EncParam*       param
		);

extern RetCode wave420l_ProductVpuEncGetResult(
		CodecInst*      instance,
		EncOutputInfo*  result
		);

extern RetCode wave420l_Wave4VpuEncGetHeader(
		EncHandle instance,
		EncHeaderParam * encHeaderParam
		);

extern RetCode wave420l_Wave4VpuEncParaChange(
		EncHandle instance,
		EncChangeParam* param
		);

extern RetCode Coda7qVpuEncGetHeader(
		EncHandle instance,
		EncHeaderParam * encHeaderParam
		);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* VPUAPI_PRODUCT_ABSTRACT_H */

