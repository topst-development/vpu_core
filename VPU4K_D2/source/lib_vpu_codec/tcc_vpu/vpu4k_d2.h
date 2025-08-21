/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/**
 * @file vpu4k_d2.h
 * @brief VPU4K_D2 specific conversion functions and macros.
 */

#ifndef VPU4K_D2__H
#define VPU4K_D2__H

#include "vpu_common.h"

/**
 * @brief Declare conversion functions for VPU4K_D2.
 */
DECLARE_CONVERT_FUNCS(vpu4k_d2)

/**
 * @brief Convert an unsigned long address for VPU4K_D2.
 * @param address The unsigned long address to convert.
 */
#define CONVERT_UL_PVOID(address) CONV_TO_PVOID(vpu4k_d2, ulong, address)

/**
 * @brief Convert a codec-specific address for VPU4K_D2.
 * @param address The codec-specific address to convert.
 */
#define CONVERT_CA_PVOID(address) CONV_TO_PVOID(vpu4k_d2, caddr, address)

#endif // VPU4K_D2__H
