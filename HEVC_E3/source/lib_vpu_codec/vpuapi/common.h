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

#ifndef __COMMON_FUNCTION_H__
#define __COMMON_FUNCTION_H__

#include "vpuapi.h"
#include "product.h"

#define VPU_HANDLE_TO_ENCINFO(_handle)          (&(((CodecInst*)_handle)->CodecInfo->encInfo))

#define WAVE4_SUBSAMPLED_ONE_SIZE(_w, _h)   ((((_w/4)+15)&~15)*(((_h/4)+7)&~7))

#define BSOPTION_ENABLE_EXPLICIT_END        (1<<0)

#define W4_WTL_RIGHT_JUSTIFIED          0
#define W4_WTL_LEFT_JUSTIFIED           1
#define W4_WTL_PIXEL_8BIT               0
#define W4_WTL_PIXEL_16BIT              1
#define W4_WTL_PIXEL_32BIT              2

typedef struct CodecInstHeader {
	Int32   inUse;
	Int32   coreIdx;
	Int32   productId;
	Int32   instIndex;
	Int32   codecMode;
	Int32   codecModeAux;
	Int32   loggingEnable;
    Uint32  isDecoder;
} CodecInstHeader;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern Int32 WaveVpuGetProductId(
	Uint32      coreIdx
    );

extern RetCode wave420l_Wave4VpuGetVersion(
	Uint32  coreIdx,
	Uint32* versionInfo,
	Uint32* revision
    );

extern RetCode wave420l_Wave4VpuInit(
	Uint32      coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void*       firmware,
	Uint32      size
    );

extern RetCode wave420l_Wave4VpuReInit(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void* firmware,
	Uint32 size
    );

extern RetCode Wave4VpuDeinit(
	Uint32      coreIdx
    );

extern Uint32 wave420l_Wave4VpuIsInit(
	Uint32      coreIdx
    );

extern Int32 wave420l_Wave4VpuIsBusy(
	Uint32      coreIdx
    );

extern Int32 wave420l_Wave4VpuWaitInterrupt(
	CodecInst*  handle,
	Int32       timeout
    );

extern RetCode wave420l_Wave4VpuClearInterrupt(
	Uint32      coreIdx,
	Uint32      flags
    );

extern RetCode wave420l_Wave4VpuReset(
	Uint32 coreIdx,
	SWResetMode resetMode
    );

extern RetCode wave420l_Wave4VpuSleepWake(
	Uint32      coreIdx,
	int         iSleepWake,
	const Uint16* code,
	Uint32 size
    );

extern RetCode Wave4DecWriteProtect(
	CodecInst* instance
    );

extern RetCode Wave4EncWriteProtect(
	CodecInst* instance
    );

extern void wave420l_Wave4BitIssueCommand(
    CodecInst* instance,
    Uint32 cmd
    );

extern RetCode Wave4VpuEncFiniSeq(
    CodecInst*  instance
    );

extern RetCode wave420l_Wave4VpuBuildUpEncParam(
    CodecInst*      pCodec,
    EncOpenParam*   param
    );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __COMMON_FUNCTION_H__ */

