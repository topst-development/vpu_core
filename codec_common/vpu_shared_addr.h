/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU_SHARED_MEM_ADDR
#define VPU_SHARED_MEM_ADDR

#if defined(CONFIG_TCC805X_CA53Q) || defined(CONFIG_TCC807X_CA55_SUB)
#	define SHARE_POINT_ADDR  0x50000000
#else
#	define SHARE_POINT_ADDR 0x30000000 //base address
#endif

#endif
