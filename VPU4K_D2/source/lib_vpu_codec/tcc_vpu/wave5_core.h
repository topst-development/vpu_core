/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef _WAVE5_CORE_H_
#define _WAVE5_CORE_H_

#include "wave5_def.h"
#include "TCC_VPU4K_D2.h"
#include "vpu4k_d2.h"

//! Global Callback Funcs
typedef void *(wave5_memcpy_func) (void *, const void *, unsigned int, unsigned int);	//!< memcpy
typedef void  (wave5_memset_func) (void *, int, unsigned int, unsigned int);		//!< memset
typedef int  (wave5_interrupt_func) (unsigned int);					//!< Interrupt
typedef void *(wave5_ioremap_func) (unsigned int, unsigned int);
typedef void (wave5_iounmap_func) (void *);
typedef unsigned int (wave5_reg_read_func) (void *, unsigned int);
typedef void (wave5_reg_write_func) (void *, unsigned int, unsigned int);
typedef void (wave5_usleep_func) (unsigned int, unsigned int);				//!< usleep_range(min, max)

extern wave5_memcpy_func    *wave5_memcpy;
extern wave5_memset_func    *wave5_memset;
extern wave5_interrupt_func *wave5_interrupt;
extern wave5_ioremap_func   *wave5_ioremap;
extern wave5_iounmap_func   *wave5_iounmap;
extern wave5_reg_read_func  *wave5_read_reg;
extern wave5_reg_write_func *wave5_write_reg;
extern wave5_usleep_func    *wave5_usleep;

extern int gWave5InitFirst;
extern int gWave5InitFirst_exit;
extern codec_addr_t gWave5FWAddr;

int wave5_get_vpu_handle(wave5_t **ppVpu);
void wave5_free_vpu_handle(wave5_t *pVpu);
void wave5_reset_global_var(int iOption);

//void *wave5_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int isIO);
void wave5_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int isIO);
void *wave5_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int isIO);

int check_wave5_handle_addr(wave5_t *pVpu);
int check_wave5_handle_use(wave5_t *pVpu);

#endif	// _WAVE5_CORE_H_