/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef WAVE420L_VERSION__H
#define WAVE420L_VERSION__H

//------------------------------------------------------
// *: VPU Version Information
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
#define WAVE420L_BUILD_DATE_STR	"2025-07-17/15:00"
#define WAVE420L_BUILD_DATE_LEN	16

#define WAVE420L_VERSION_MAJOR_CODE   "2"
	#define MAJOR_CODE_LEN    1
#define WAVE420L_VERSION_API_CODE     "0"
	#define API_CODE_LEN      1
#define WAVE420L_VERSION_MINOR_CODE   "0"
	#define MINOR_CODE_LEN    1
#define WAVE420L_VERSION_FIX_CODE     "0"
	#define FIX_CODE_LEN      1
#define WAVE420L_API_VERSION      WAVE420L_VERSION_MAJOR_CODE"."WAVE420L_VERSION_API_CODE

#define WAVE420L_VERSION_STR      WAVE420L_VERSION_MAJOR_CODE"."\
                                  WAVE420L_VERSION_API_CODE"."\
                                  WAVE420L_VERSION_MINOR_CODE"."\
                                  WAVE420L_VERSION_FIX_CODE

#define WAVE420L_VERSION_LEN      (MAJOR_CODE_LEN+API_CODE_LEN+MINOR_CODE_LEN+FIX_CODE_LEN+3) //dot=3

static const char* vpu_get_version(void)
{
	return WAVE420L_VERSION_STR;
}
static const char* vpu_get_build_date(void)
{
	return WAVE420L_BUILD_DATE_STR;
}

//##############################################################
// For legacy compatibility
#if defined(WAVE420L_ARM_ARCH)
    #if WAVE420L_ARM_ARCH == 32
        #define WAVE420L_CODEC_CHIP_STR	"TCC_HEVC_E3_ARM32"
        #define WAVE420L_CODEC_CHIP_LEN	17
    #elif WAVE420L_ARM_ARCH == 64
        #define WAVE420L_CODEC_CHIP_STR	"TCC_HEVC_E3_ARM64"
        #define WAVE420L_CODEC_CHIP_LEN	17
    #else
        #define WAVE420L_CODEC_CHIP_STR	"TCCxxxx"
        #define WAVE420L_CODEC_CHIP_LEN	7
    #endif
#endif
#ifndef WAVE420L_CODEC_CHIP_STR
        #define WAVE420L_CODEC_CHIP_STR	"TCCxxxx"
        #define WAVE420L_CODEC_CHIP_LEN	7
#endif
#define WAVE420L_CODEC_NAME_STR	( WAVE420L_CODEC_CHIP_STR"_"WAVE420L_VERSION_STR )
#define WAVE420L_CODEC_NAME_LEN	( WAVE420L_VERSION_LEN + 1 + WAVE420L_CODEC_CHIP_LEN )

#define WAVE420L_CODEC_BUILD_DATE_STR	WAVE420L_BUILD_DATE_STR
#define WAVE420L_CODEC_BUILD_DATE_LEN	WAVE420L_BUILD_DATE_LEN

#endif//WAVE420L_VERSION__H
