/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/**
 * @file vpu_c7.h
 * @brief VPU_C7 specific conversion functions and macros.
 */

#ifndef VPU_C7__H
#define VPU_C7__H

#include "vpu_common.h"

/**
 * @brief Declare conversion functions for VPU_C7.
 */
DECLARE_CONVERT_FUNCS(vpu_c7)

/**
 * @brief Convert an unsigned long address for VPU_C7.
 * @param address The unsigned long address to convert.
 */
#define CONVERT_UL_PVOID(address) CONV_TO_PVOID(vpu_c7, ulong, address)

/**
 * @brief Convert a codec-specific address for VPU_C7.
 * @param address The codec-specific address to convert.
 */
#define CONVERT_CA_PVOID(address) CONV_TO_PVOID(vpu_c7, caddr, address)

#endif // VPU_C7__H
