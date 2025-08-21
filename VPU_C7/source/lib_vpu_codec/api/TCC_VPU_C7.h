/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef TCC_VPU_C7_H_INCLUDED
#define TCC_VPU_C7_H_INCLUDED

#include "TCC_VPU_C7_CODEC.h"

#undef MAX_NUM_INSTANCE
#define MAX_NUM_INSTANCE                8 // internal count
#define STRINGIFY(x) #x
#define MAX_NUM_INSTANCE_STR            STRINGIFY(MAX_NUM_INSTANCE)

// FIXME: These define below are the same as the error value in the kernel header (TCCxxxx_VPU_CODEC_COMMON.h)
#define RETCODE_FAIL_INIT_RUN           23
#define RETCODE_FAIL_GET_INSTANCE       24
#define RETCODE_HANDLE_FULL             25


#endif //TCC_VPU_C7_H_INCLUDED