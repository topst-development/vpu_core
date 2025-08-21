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

#ifndef _VPU_TYPES_H_
#define _VPU_TYPES_H_

#if !defined(Uint8)
typedef unsigned char   Uint8;
typedef unsigned int    Uint32;
typedef unsigned short  Uint16;
typedef signed char     Int8;
typedef int             Int32;
typedef short           Int16;
typedef unsigned long   Uint64;
typedef long            Int64;
#endif

#ifndef PhysicalAddress
/**
* @brief    This is a type for representing physical addresses which are recognizable by VPU. 
In general, VPU hardware does not know about virtual address space 
which is set and handled by host processor. All these virtual addresses are 
translated into physical addresses by Memory Management Unit. 
All data buffer addresses such as stream buffer and frame buffer should be given to
VPU as an address on physical address space.
*/
typedef Uint32 PhysicalAddress;
#endif
#ifndef BYTE
/**
* @brief This type is an 8-bit unsigned integral type.
*/
typedef unsigned char   BYTE;
#endif
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef TRUE
#define TRUE            1
#endif /* TRUE */
#ifndef FALSE
#define FALSE           0
#endif /* FALSE */
#ifndef NULL
#define NULL	        0
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P)          \
    /*lint -save -e527 -e530 */ \
{ \
    (P) = (P); \
} \
    /*lint -restore */
#endif

#ifdef __GNUC__
#define UNREFERENCED_FUNCTION __attribute__ ((unused))
#else
#define UNREFERENCED_FUNCTION
#endif

#endif	/* _VPU_TYPES_H_ */
 
