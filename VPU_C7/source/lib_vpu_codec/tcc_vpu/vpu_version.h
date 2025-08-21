/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU_VERSION__H
#define VPU_VERSION__H

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
#define VPU_CODEC_BUILD_DATE_STR        "2025-08-08/20:00"
#define VPU_CODEC_BUILD_DATE_LEN        16

#define VPU_CODEC_VERSION_MAJOR_CODE    "6"
	#define MAJOR_CODE_LEN          1
#define VPU_CODEC_VERSION_API_CODE      "0"
	#define API_CODE_LEN            1
#define VPU_CODEC_VERSION_MINOR_CODE    "1"
	#define MINOR_CODE_LEN          1
#define VPU_CODEC_VERSION_FIX_CODE      "1"
	#define FIX_CODE_LEN            1
#define VPU_CODEC_API_VERSION           VPU_CODEC_VERSION_MAJOR_CODE"."VPU_CODEC_VERSION_API_CODE

#define VPU_CODEC_VERSION_STR           VPU_CODEC_VERSION_MAJOR_CODE"."\
                                        VPU_CODEC_VERSION_API_CODE"."\
                                        VPU_CODEC_VERSION_MINOR_CODE"."\
                                        VPU_CODEC_VERSION_FIX_CODE
#define VPU_CODEC_VERSION_LEN           (MAJOR_CODE_LEN+API_CODE_LEN+MINOR_CODE_LEN+FIX_CODE_LEN+3) //dot=3

#define VPU_CODEC_CHIP_STR              "TCC_VPU_C7_ARM64"
#define VPU_CODEC_CHIP_LEN              16


//------------------------------------------------------
// *: VPU Maximum number Information of instances supported by lib.
//------------------------------------------------------
// Used only when changing the value defined as MAX_NUM_INSTANCE in
//   the TCC_VPU_C7.h file to a small number.
//   (It must be greater than or equal to the value of
//    MAX_NUM_INSTANCE defined in the TCC_VPU_C7_CODEC.h file.)
#if defined(USE_VPU_MAX_NUM_INSTANCE)
#	define VPU_MAX_NUM_INSTANCE_STR "_CH"USE_VPU_MAX_NUM_INSTANCE
#else
#	define VPU_MAX_NUM_INSTANCE_STR ""
#endif


#define VPU_CODEC_NAME_STR              ( VPU_CODEC_CHIP_STR"_V"VPU_CODEC_VERSION_STR""VPU_MAX_NUM_INSTANCE_STR)
#define VPU_CODEC_NAME_LEN              ( VPU_CODEC_CHIP_LEN + 2 + VPU_CODEC_VERSION_LEN )


static const char* vpu_get_version(void)
{
	return VPU_CODEC_VERSION_STR;
}
static const char* vpu_get_build_date(void)
{
	return VPU_CODEC_BUILD_DATE_STR;
}

static char *vpu_strcpy(char *dest, const char *source) {
	char *p;
	const char *psz_string = source;
	char *psz_dest = dest;

	if (psz_dest == NULL) {
		return NULL;
	}
	if (source == NULL) {
		return NULL;
	}

	p = dest;
	while (*psz_string != '\0') {
		*psz_dest = *psz_string;
		psz_dest++;
		psz_string++;
	}
	*psz_dest = '\0';
	return p;
}

static int vpu_strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}

	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int get_version_vpu(char *pszVersion, char *pszBuildDate) {
	int ret = 0;

	if (pszVersion == NULL) {
		return 2;//RETCODE_INVALID_HANDLE
	}
	if (pszBuildDate == NULL) {
		return 2;//RETCODE_INVALID_HANDLE
	}

	vpu_strcpy(pszVersion, vpu_get_version());
	pszVersion[VPU_CODEC_VERSION_LEN] = '\0';
	vpu_strcpy(pszBuildDate, vpu_get_build_date());
	pszBuildDate[VPU_CODEC_BUILD_DATE_LEN] = '\0';
	return ret;
}


#endif//VPU_VERSION__H
