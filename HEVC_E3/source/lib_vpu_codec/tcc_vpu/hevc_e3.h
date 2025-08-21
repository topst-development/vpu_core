/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/**
 * @file hevc_e3.h
 * @brief HEVC_E3 specific conversion functions and macros.
 */

#ifndef HEVC_E3__H
#define HEVC_E3__H

#include "vpu_common.h"

/**
 * @brief Declare conversion functions for HEVC_E3.
 */
DECLARE_CONVERT_FUNCS(hevc_e3)

/**
 * @brief Convert an unsigned long address for HEVC_E3.
 * @param address The unsigned long address to convert.
 */
#define CONVERT_UL_PVOID(address) CONV_TO_PVOID(hevc_e3, ulong, address)

/**
 * @brief Convert a codec-specific address for HEVC_E3.
 * @param address The codec-specific address to convert.
 */
#define CONVERT_CA_PVOID(address) CONV_TO_PVOID(hevc_e3, caddr, address)

#endif // HEVC_E3__H
