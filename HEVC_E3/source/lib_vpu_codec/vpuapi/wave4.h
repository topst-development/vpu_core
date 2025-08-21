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

#ifndef __WAVE410_FUNCTION_H__
#define __WAVE410_FUNCTION_H__

#include "vpuapi.h"
#include "product.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern RetCode wave420l_Wave4VpuEncRegisterFramebuffer(
			CodecInst*      instance,
			FrameBuffer*    fb,
			TiledMapType    mapType,
			Uint32          count
			);

extern RetCode wave420l_Wave4VpuEncSetup(
			CodecInst*      instance
			);

extern RetCode wave420l_Wave4VpuEncode(
			CodecInst* instance,
			EncParam* option
			);

extern RetCode wave420l_Wave4VpuEncGetResult(
			CodecInst* instance,
			EncOutputInfo* result
			);

extern RetCode Wave4VpuEncGiveCommand(
			CodecInst*      pCodecInst,
			CodecCommand    cmd,
			void*           param
			);

extern RetCode wave420l_CheckEncCommonParamValid(
			EncOpenParam* pop
			);

extern RetCode wave420l_CheckEncRcParamValid(
			EncOpenParam* pop
			);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WAVE410_FUNCTION_H__ */

