/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU4K_D2_VERSION__H
#define VPU4K_D2_VERSION__H

//------------------------------------------------------
// *: VPU4K_D2 Version Information
//------------------------------------------------------
// 1. lot of changes (structures, schemes)
//
// 2. adding a API
//    adding a member variable or change name of variable
//    adding a major op. code
//
// 3. adding a minor function & CNM FW
//
// 4. fixing a bug or adding an internal function
//
#define VPU4K_D2_BUILD_DATE_STR "2025-08-19/11:10"
#define VPU4K_D2_BUILD_DATE_LEN 16

#define VPU4K_D2_VERSION_MAJOR_CODE	"3"
#define MAJOR_CODE_LEN 1
#define VPU4K_D2_VERSION_API_CODE	"0"
#define API_CODE_LEN 1
#define VPU4K_D2_VERSION_MINOR_CODE	"0"
#define MINOR_CODE_LEN 1
#define VPU4K_D2_VERSION_FIX_CODE 	"1"
#define FIX_CODE_LEN 1
#define VPU4K_D2_CODEC_API_VERSION VPU4K_D2_VERSION_MAJOR_CODE "." VPU4K_D2_VERSION_API_CODE

#define VPU4K_D2_VERSION_STR VPU4K_D2_VERSION_MAJOR_CODE "." VPU4K_D2_VERSION_API_CODE "." VPU4K_D2_VERSION_MINOR_CODE "." VPU4K_D2_VERSION_FIX_CODE
#define VPU4K_D2_VERSION_LEN (MAJOR_CODE_LEN + API_CODE_LEN + MINOR_CODE_LEN + FIX_CODE_LEN + 3) // dot=3

#define VPU4K_D2_CHIP_STR "TCC_VPU4K_D2"
#define VPU4K_D2_CHIP_LEN 12

#define VPU4K_D2_NAME_STR (VPU4K_D2_CHIP_STR "_V" VPU4K_D2_VERSION_STR)
#define VPU4K_D2_NAME_LEN (VPU4K_D2_CHIP_LEN + 2 + VPU4K_D2_VERSION_LEN)

static const char *vpu4k_d2_get_version(void) {
	return VPU4K_D2_VERSION_STR;
}
static const char *vpu4k_d2_get_build_date(void) {
	return VPU4K_D2_BUILD_DATE_STR;
}

#endif // VPU4K_D2_VERSION__H
