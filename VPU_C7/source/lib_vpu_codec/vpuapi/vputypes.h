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
// File         : vputypes.h
// Description  :
//-----------------------------------------------------------------------------

#ifndef VPU_TYPES_H_INCLUDED
#define VPU_TYPES_H_INCLUDED

typedef unsigned char  Uint8;
typedef unsigned int   Uint32;
typedef unsigned short Uint16;
typedef short Int16;
#if defined(_MSC_VER)
typedef unsigned _int64 Uint64;
typedef _int64 Int64;
#else
typedef unsigned long long Uint64;
typedef long long Int64;
#endif
typedef Uint32 PhysicalAddress;
typedef unsigned char   BYTE;
typedef int BOOL;

#endif	/* VPU_TYPES_H_INCLUDED */
