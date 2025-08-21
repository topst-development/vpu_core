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
// File         :
// Description  :
//-----------------------------------------------------------------------------

#ifndef VPU_WAVE5_TYPES_H
#define VPU_WAVE5_TYPES_H

typedef unsigned char           Uint8;
typedef unsigned int            Uint32;
typedef unsigned short          Uint16;
typedef char                    Int8;
typedef int                     Int32;
typedef short                   Int16;

#if defined(_MSC_VER)
typedef unsigned _int64         Uint64;
typedef _int64                  Int64;
#else
typedef unsigned long long      Uint64;
typedef long long               Int64;
#endif

#ifndef PhysicalAddress
typedef Uint32                  PhysicalAddress;
#endif

#ifndef BYTE
typedef unsigned char           BYTE;
#endif

#ifndef BOOL
typedef int                     BOOL;
#endif

#ifndef TRUE
#define TRUE                    1
#endif /* TRUE */

#ifndef FALSE
#define FALSE                   0
#endif /* FALSE */

#ifndef NULL
#define NULL                    0
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) { (P) = (P); }
#endif

#ifndef size_t
#define size_t                 unsigned long
#endif


#ifndef VPU_CODEC_ADDR_T_DEF
#	define VPU_CODEC_ADDR_T_DEF
#	if defined(CONFIG_ARM64)
typedef unsigned long long codec_addr_t;	// address - 64 bit
#	else
typedef unsigned long codec_addr_t;	// address - 32 bit
#	endif
#endif

#endif
