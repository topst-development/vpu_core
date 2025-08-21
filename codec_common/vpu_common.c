/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#include "vpu_common.h"

/**
 * @defgroup AddressConversion Address Conversion Functions
 * @brief Functions related to converting addresses to pointers for codec operations.
 *
 * This group includes functions that convert various address types, such as
 * unsigned long or codec-specific addresses, to generic void pointers.
 * @{
 */

/** @brief Converts an unsigned long address to a pointer. */
static void* convert_ulong_to_pvoid(unsigned long addr) {
	covert_addr_t un_covert_addr;
	un_covert_addr.ulAddr = addr;
	return un_covert_addr.pvPtr;
}

/** @brief Converts a codec address (codec_addr_t) to a pointer. */
static void* convert_caddr_to_pvoid(codec_addr_t addr) {
	covert_addr_t un_covert_addr;
	un_covert_addr.caAddr = addr;
	return un_covert_addr.pvPtr;
}

/** @} */ // End of AddressConversion group

/**
 * @brief Defines conversion functions for a specific codec prefix.
 *
 * This macro creates two functions for each codec, prefixed with the codec's
 * name. These functions convert unsigned long addresses and codec addresses
 * (codec_addr_t) to pointers. The use of this macro helps avoid function name
 * conflicts by ensuring each codec has its own unique conversion functions.
 *
 * @param prefix The codec-specific prefix to avoid function name conflicts.
 */
#define DEFINE_CONVERT_FUNCS(prefix) \
void* prefix##_convert_ulong_to_pvoid(unsigned long addr) { \
	return convert_ulong_to_pvoid(addr); \
} \
void* prefix##_convert_caddr_to_pvoid(codec_addr_t addr) { \
	return convert_caddr_to_pvoid(addr); \
}

/**
 * @defgroup CodecSpecificFunctions Codec-Specific Conversion
 * @brief Defines conversion functions for each codec based on configuration.
 *
 * This section defines codec-specific conversion functions using macros.
 * The conversion functions are conditionally compiled based on the codec configuration.
 * @{
 */

#if defined(CONFIG_HEVC_D1)
	DEFINE_CONVERT_FUNCS(hevc_d1)
#endif

#if defined(CONFIG_HEVC_E3)
	DEFINE_CONVERT_FUNCS(hevc_e3)
#endif

#if defined(CONFIG_HEVC_E3_2)
	DEFINE_CONVERT_FUNCS(hevc_e3_2)
#endif

#if defined(CONFIG_JPU_C6)
	DEFINE_CONVERT_FUNCS(jpu_c6)
#endif

#if defined(CONFIG_VPU_D6)
	DEFINE_CONVERT_FUNCS(vpu_d6)
#endif

#if defined(CONFIG_VPU_C7)
	DEFINE_CONVERT_FUNCS(vpu_c7)
#endif

#if defined(CONFIG_VPU4K_D2)
	DEFINE_CONVERT_FUNCS(vpu4k_d2)
#endif

/** @} */ // End of CodecSpecificFunctions group
