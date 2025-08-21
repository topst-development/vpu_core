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
// File         : config.h
// Description  :
//-----------------------------------------------------------------------------

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include "vpu_pre_define.h"

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__MINGW32__)
#	if defined(_WIN32_WCE) || defined(UNDER_CE)
#		define PLATFORM_WINCE
#		define PLATFORM_SOC
#	else
#		define PLATFORM_WIN32
#	endif
#elif defined(linux) || defined(__linux) || defined(ANDROID)
#	if defined(ARM) || defined(_ARM_) ||  defined(ARMV4) || defined(__arm__)
#		define PLATFORM_LINUX
#		define PLATFORM_SOC
#	else
#		define PLATFORM_LINUX
#	endif
#elif defined(ARM) || defined(_ARM_) ||  defined(ARMV4) || defined(__arm__) || defined(__ARMCC__)
#		define PLATFORM_NON_OS
#		define PLATFORM_SOC
#		msg "can not supprot platform definition"
#else
#		error "can not supprot platform definition"
#endif

#if defined(_WIN32_WCE) || defined(UNDER_CE)
#if defined(_MSC_VER)
#	include <windows.h>
//#	include <conio.h>
#ifndef inline
#	define inline _inline
#endif
#	define VPU_DELAY_MS(X)		Sleep(X)
#	define VPU_DELAY_US(X)		Sleep(X)	// should change to delay function which can be delay a microsecond unut.
#	define kbhit _kbhit
#	define getch _getch
#elif defined(__GNUC__)
#	define VPU_DELAY_MS(X)		usleep(X*1000)		//udelay(X*1000)
#	define VPU_DELAY_US(X)		usleep(X)		//udelay(X)
#elif defined(__ARMCC__)
#else
#  error "Unknown compiler."
#endif
#endif

#define API_VERSION 510


// VPU_C2 0x0200 CodaDx8
// VPU_C3 0x0300 CodaHx14
// VPU_C5 0x0500 Coda8550
// VPU_C7 0x0700 Coda960
//        0x0710    RTL V1.x
//        0x0720    RTL V2.x
//        0x0730    RTL V3.x
// VPU_D6 0x0600 Boda950
//        0x0630    RTL 3.x
#define TCC_VPU_2K_IP  0x0700
#if TCC_VPU_2K_IP == 0x0700
#	define TCC_AVC____ENC_SUPPORT 1	// [ENC] AVC    0: Not support, 1: support
#endif
#if TCC_VPU_2K_IP == 0x0630
#	define TCC_AVC____ENC_SUPPORT 0	// [ENC] AVC    0: Not support, 1: support
#endif

#endif	/* CONFIG_H_INCLUDED */
