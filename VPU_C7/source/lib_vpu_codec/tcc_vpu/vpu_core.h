/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU_CORE_H_INCLUDED
#define VPU_CORE_H_INCLUDED

#include "vpu_def.h"
#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
#include "TCC_VPU_D6.h"
#else
#include "TCC_VPU_C7.h"
#endif
#include "vpu_log.h"
#include "vpu_c7.h"


//! Global Callback Funcs
typedef void *(vpu_memcpy_func) (void *, const void *, unsigned int, unsigned int);	//!< memcpy
typedef void (vpu_memset_func) (void *, int, unsigned int, unsigned int);		//!< memset
typedef int  (vpu_interrupt_func) (void);						//!< Interrupt
typedef void *(vpu_ioremap_func) (unsigned int, unsigned int);
typedef void (vpu_iounmap_func) (void *);
typedef unsigned int (vpu_reg_read_func) (void *, unsigned int);
typedef void (vpu_reg_write_func) (void *, unsigned int, unsigned int);
typedef void (vpu_usleep_func) (unsigned int, unsigned int);				//!< usleep_range(min, max)

extern vpu_memcpy_func                  *vpu_memcpy;
extern vpu_memset_func                  *vpu_memset;
extern vpu_interrupt_func               *vpu_interrupt;
extern vpu_ioremap_func                 *vpu_ioremap;
extern vpu_iounmap_func                 *vpu_iounmap;
extern vpu_reg_read_func                *vpu_read_reg;
extern vpu_reg_write_func               *vpu_write_reg;
extern vpu_usleep_func                  *vpu_usleep;

extern int gInitFirst;
extern int gInitFirst_exit;

int check_vpu_handle_addr(vpu_t *pVpu);
int check_vpu_handle_use(vpu_t *pVpu);

int get_vpu_instance_number(void);
int get_vpu_handle(vpu_t **ppVpu);

void free_vpu_handle(vpu_t *pVpu);
void reset_vpu_global_var(int);

void set_register_bit(unsigned int uiBit, unsigned int uiRegisterAddr);

void vpu_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int type);
void *vpu_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int type);

#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
int check_aarch_vpu_d6(void);
#else
int check_aarch_vpu_c7(void);
#endif

#endif	//VPU_CORE_H_INCLUDED