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

#ifndef WAVE510_FUNCTION_H
#define WAVE510_FUNCTION_H

#include "wave5api.h"
#include "product.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BSOPTION_ENABLE_EXPLICIT_END (1<<0)

#define WTL_RIGHT_JUSTIFIED          0
#define WTL_LEFT_JUSTIFIED           1
#define WTL_PIXEL_8BIT               0
#define WTL_PIXEL_16BIT              1
#define WTL_PIXEL_32BIT              2

extern Uint32 Wave5VpuIsInit(Uint32 coreIdx);
extern Int32 Wave5VpuIsBusy(Uint32 coreIdx);
extern Int32 WaveVpuGetProductId(Uint32 coreIdx);
extern void Wave5BitIssueCommand(CodecInst *instance, Uint32 cmd);
extern RetCode Wave5VpuGetVersion(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision);
extern RetCode Wave5VpuInit(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void *firmware,
	Uint32 size,
	Int32 cq_count
	);
extern RetCode Wave5VpuSleepWake(
	Uint32 coreIdx,
	int iSleepWake,
	const Uint16* code,
	Uint32 size,
	BOOL reset,
	Int32 cq_count
	);
extern RetCode Wave5VpuReset(Uint32 coreIdx, SWResetMode resetMode, Int32 cq_count);
extern RetCode Wave5VpuBuildUpDecParam(CodecInst *instance, DecOpenParam *param);
extern RetCode Wave5VpuDecSetBitstreamFlag(CodecInst *instance, BOOL running, BOOL eos, BOOL explictEnd);
extern RetCode Wave5VpuDecRegisterFramebuffer(CodecInst *inst, FrameBuffer *fbArr, TiledMapType mapType, Uint32 count);
extern RetCode Wave5VpuDecRegisterFramebuffer2(CodecInst *inst, FrameBuffer *fbArr, TiledMapType mapType, Uint32 count);
extern RetCode Wave5VpuDecFlush(CodecInst *instance, FramebufferIndex *framebufferIndexes, Uint32 size);
extern RetCode Wave5VpuReInit_tc(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void *firmware,
	Uint32 size,
	Int32 cq_count
	);
extern RetCode Wave5VpuReInit(
	Uint32 coreIdx,
	PhysicalAddress workBuf,
	PhysicalAddress codeBuf,
	void *firmware,
	Uint32 size,
	Int32 cq_count
	);
extern RetCode Wave5VpuDecInitSeq(CodecInst *instance);
extern RetCode Wave5VpuDecGetSeqInfo(CodecInst *instance, DecInitialInfo *info);
extern RetCode Wave5VpuDecode(CodecInst *instance, DecParam *option);
extern RetCode Wave5VpuDecGetResult(CodecInst *instance, DecOutputInfo *result);
extern RetCode Wave5VpuDecFiniSeq(CodecInst *instance);
extern RetCode Wave5DecWriteProtect(CodecInst *instance);
extern RetCode Wave5DecClrDispFlag(CodecInst *instance, Uint32 index);
extern RetCode Wave5DecSetDispFlag(CodecInst *instance, Uint32 index);
extern RetCode Wave5DecClrDispFlagEx(CodecInst *instance, Uint32 dispFlag);
extern RetCode Wave5DecSetDispFlagEx(CodecInst *instance, Uint32 dispFlag);
extern Int32 Wave5VpuWaitInterrupt(CodecInst *instance, Int32 timeout, BOOL pending);
extern RetCode Wave5VpuClearInterrupt(Uint32 coreIdx, Uint32 flags);
extern RetCode Wave5VpuDecGetRdPtr(CodecInst *instance, PhysicalAddress *rdPtr);
extern RetCode Wave5VpuDecSetRdPtr(CodecInst *instance, PhysicalAddress rdPtr);
extern void WAVE5_SetPendingInst(Uint32 coreIdx, CodecInst *inst);
extern void WAVE5_ClearPendingInst(Uint32 coreIdx);
extern void *WAVE5_GetPendingInstPointer( void );
extern CodecInst *WAVE5_GetPendingInst(Uint32 coreIdx);
extern void WAVE5_PeekPendingInstForSimInts(int keep);
extern int WAVE5_PokePendingInstForSimInts();

extern int WAVE5_ResetRegisters(Uint32 coreIdx, int option);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif