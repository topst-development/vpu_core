/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/**
 * @file vpu_common.h
 * @brief VPU common utility functions and macros.
 * @version 1.0.0
 * @date 2024-09-05
 *
 * This header defines utility macros and functions for VPU codecs.
 * It aims to prevent symbol conflicts by providing codec-specific function prefixes.
 */

#ifndef VPU_COMMON__H
#define VPU_COMMON__H

#include "TCCxxxx_VPU_CODEC_COMMON.h"

/**
 * @brief A union for different address types used in codec operations.
 */
typedef union covert_addr_t {
	codec_addr_t caAddr;
	unsigned long ulAddr;
	void *pvPtr;
	unsigned int *puiAddr;
	int *piAddr;
	unsigned char *pucAddr;
	char *pcAddr;
} covert_addr_t;

/**
 * @brief Macro to declare conversion functions with a specific prefix.
 *
 * @param prefix The codec-specific prefix to avoid function name conflicts.
 */
#define DECLARE_CONVERT_FUNCS(prefix) \
void* prefix##_convert_ulong_to_pvoid(unsigned long addr); \
void* prefix##_convert_caddr_to_pvoid(codec_addr_t addr);

/**
 * @brief Macro to convert an address to a pointer using a codec-specific function.
 *
 * @param prefix The codec-specific prefix to avoid function name conflicts.
 * @param type The type of address to convert (e.g., ulong, caddr).
 * @param addr The address to convert.
 */
#define CONV_TO_PVOID(prefix, type, addr) prefix##_convert_##type##_to_pvoid(addr)

/**
 * @brief Macro to mark a variable as used but do nothing with it.
 *
 * @param var The variable to mark as used.
 */
#define USE_TMP_VAR(var) (void)(var)

#endif // VPU_COMMON__H
