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

#ifndef WAVE420L_CORE_H
#define WAVE420L_CORE_H

#include "TCC_VPU_HEVC_ENC.h"
#include "wave420l_def.h"
#include "hevc_e3.h"

int wave420l_get_vpu_handle(wave420l_t **ppVpu);
void wave420l_free_vpu_handle(wave420l_t *pVpu);
void wave420l_reset_global_var(int);

void wave420l_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int type);
void *wave420l_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int type);

unsigned int wave420l_vdi_fio_read_register(unsigned long core_idx, unsigned int addr);
void wave420l_vdi_fio_write_register(unsigned long core_idx, unsigned int addr, unsigned int data);
int wave420l_vdi_wait_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag);
int wave420l_vdi_wait_vpu_busy(unsigned long core_idx, int timeout, unsigned int addr_bit_busy_flag);
int wave420l_vdi_wait_interrupt(unsigned long coreIdx, unsigned int loop_count,  unsigned int addr_bit_int_reason, unsigned addr_status_reg,  unsigned int use_interrupt_flag);

void wave420l_print_vpu_status(unsigned long coreIdx, unsigned long productId);
void wave420l_vdi_print_vpu_status(unsigned long coreIdx);
void wave420l_vdi_log(unsigned long core_idx, int cmd, int step);

void wave420l_byte_swap(unsigned char *data, int len);
void wave420l_word_swap(unsigned char *data, int len);
void wave420l_dword_swap(unsigned char *data, int len);
void wave420l_lword_swap(unsigned char *data, int len);
int wave420l_swap_endian(unsigned int coreIdx, unsigned char *data, int len, int endian);
int wave420l_vdi_convert_endian(unsigned int coreIdx, unsigned int endian);

unsigned int cal_vpu_hevc_enc_framebuf_size (unsigned int width, unsigned int height, unsigned int frame_count, unsigned int bufAlignSize, unsigned int bufStartAddrAlign);
unsigned int wave420l_CalcMinFrameBufferSize (unsigned int width, unsigned int height, unsigned int frame_count);
int wave420l_HevcLevelCalculation(unsigned int picWidth, unsigned int picHeight, unsigned int rcEnable, unsigned int initialDelay, unsigned int targetRate, unsigned int frameRate);


//----------------------------------------------------------------------
// Global Callback functions and Variables (declared in vpuapi.c)
//----------------------------------------------------------------------

typedef void *(wave420l_memcpy_func) (void *, const void *, unsigned int, unsigned int);	//!< memcpy
typedef void (wave420l_memset_func) (void *, int, unsigned int, unsigned int);			//!< memset
typedef int  (wave420l_interrupt_func) (void);							//!< Interrupt
typedef void *(wave420l_ioremap_func) (unsigned int, unsigned int);
typedef void (wave420l_iounmap_func) (void *);
typedef unsigned int (wave420l_reg_read_func) (void *, unsigned int);
typedef void (wave420l_reg_write_func) (void *, unsigned int, unsigned int);
typedef void (wave420l_usleep_func) (unsigned int, unsigned int);

extern wave420l_memcpy_func *wave420l_memcpy;
extern wave420l_memset_func *wave420l_memset;
extern wave420l_interrupt_func *wave420l_interrupt;
extern wave420l_ioremap_func *wave420l_ioremap;
extern wave420l_iounmap_func *wave420l_iounmap;
extern wave420l_reg_read_func *wave420l_read_reg;
extern wave420l_reg_write_func *wave420l_write_reg;
extern wave420l_usleep_func *wave420l_usleep;

extern int gHevcEncInitFirst;
extern int gHevcEncInitFirst_exit;
extern int gHevcEncMaxInstanceNum;

extern codec_addr_t gHevcEncVirtualBitWorkAddr;
extern PhysicalAddress gHevcEncPhysicalBitWorkAddr;
extern codec_addr_t gHevcEncVirtualRegBaseAddr;

extern CodecInst *gpPendingInsthevcEnc;
extern CodecInst *gpCodecInstPoolWave420l;
extern CodecInst gsCodecInstPoolWave420l[VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE];

//----------------------------------------------------------------------
// Global Callback functions and Variables (declared in wave420l_core.c)
//----------------------------------------------------------------------
//extern unsigned int hevc_bit_code_size;
extern codec_addr_t gHevcEncFWAddr;
extern unsigned int gsHevcEncBitCodeSize;
extern wave420l_t *gpHevcEncHandlePool;
extern wave420l_t gsHevcEncHandlePool[VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE];

#endif	// WAVE420L_CORE_H
