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

#include "wave420l_pre_define.h"

#if defined(CONFIG_HEVC_E3_2)
#include "wave420l_core_2.h"
#else
#include "wave420l_core.h"
#endif
#include "common_regdefine.h"
#include "wave4_regdefine.h"

#define VCORE_DBG_ADDR(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x300
#define VCORE_DBG_DATA(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x304
#define VCORE_DBG_READY(__vCoreIdx)     0x8000+(0x1000*__vCoreIdx) + 0x308


void (*g_pfPrintCb)(const char *, ...) = NULL; /**< printk callback */
unsigned short *g_pusBitCode;
unsigned int g_uiBitCodeSize;
// Global variables for controlling log levels and conditions in the VPU 4K D2 decoder.
int g_bLogVerbose = 0;
int g_bLogDebug = 0;
int g_bLogInfo = 0;
int g_bLogWarn = 0;
int g_bLogError = 0;
int g_bLogAssert = 1;
int g_bLogFunc = 0;
int g_bLogTrace = 0;
int g_bLogEncodeSuccess = 0;

//unsigned int hevc_bit_code_size = 0;

unsigned int gsHevcEncBitCodeSize = 0;
codec_addr_t gHevcEncFWAddr = 0;
wave420l_t *gpHevcEncHandlePool = (wave420l_t*)0;
wave420l_t gsHevcEncHandlePool[VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE];

wave420l_memcpy_func    *wave420l_memcpy = NULL;
wave420l_memset_func    *wave420l_memset = NULL;
wave420l_interrupt_func *wave420l_interrupt = NULL;
wave420l_ioremap_func   *wave420l_ioremap = NULL;
wave420l_iounmap_func   *wave420l_iounmap = NULL;
wave420l_reg_read_func  *wave420l_read_reg = NULL;
wave420l_reg_write_func *wave420l_write_reg = NULL;
wave420l_usleep_func    *wave420l_usleep = NULL;

int gHevcEncInitFirst = 0;
int gHevcEncInitFirst_exit = 0;
int gHevcEncMaxInstanceNum = VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE;
//codec_addr_t gHevcEncInitFw = 0; //address

codec_addr_t gHevcEncVirtualBitWorkAddr;
PhysicalAddress gHevcEncPhysicalBitWorkAddr;
codec_addr_t gHevcEncVirtualRegBaseAddr;

CodecInst *gpPendingInsthevcEnc = (CodecInst *)0;  //global
CodecInst *gpCodecInstPoolWave420l = (CodecInst *)0;
CodecInst gsCodecInstPoolWave420l[VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE];

typedef struct {
	unsigned int level;
	unsigned int max_luma_ps;
	unsigned int max_sqrt_luma_ps_8;
	unsigned int max_luma_sr;
	unsigned int max_cpb;   // main tier
	unsigned int max_br;    // main tier
	unsigned int max_tile_rows;
	unsigned int max_tile_cols;
} hevc_level_t;

static const hevc_level_t wave420l_HEVC_LEVEL_TABLE[8] = {
	// level, MaxLumaPs, Sqrt(MaxLumaPs*8),  MaxLumaSr,  MaxCPB_1100, MaxBr_1100, MaxTileRows, MaxTileCols
	{30,        36864,               543,      552960U,      385000,     140800,           1,          1}, //1.0 [0]
	{60,       122880,               991,     3686400U,     1650000,    1650000,           1,          1}, //2.0 [1]
	{63,       245760,              1402,     7372800U,     3300000,    3300000,           1,          1}, //2.1 [2]
	{90,       552960,              2103,    16588800U,     6600000,    6600000,           2,          2}, //3.0 [3]
	{93,       983040,              2804,    33177600U,    11000000,   11000000,           3,          3}, //3.1 [4]
	{120,     2228224,              4222,    66846720U,    13200000,   13200000,           5,          5}, //4.0 [5]
	{123,     2228224,              4222,    133693440U,   22000000,   22000000,           5,          5}, //4.1 [6]
	{150,     8912896,              8444,    267386880U,   27500000,   27500000,          11,          10} //5.0 [7]
	//  ,
	//  {153,     8912896,              8444,    534773760U,   44000000,   44000000,          11,          10}, //5.1 [8]
	//  {156,     8912896,              8444,   1069547520U,   66000000,   66000000,          11,          10}, //5.2 [9]
	//  {180,     35651584,            16888,   1069547520U,   66000000,   66000000,          22,          20}, //6.0 [10]
	//  {183,     35651584,            16888,   2139095040U,  132000000,  132000000,          22,          20}, //6.1 [11]
	//  {186,     35651584,            16888,   4278190080U,  264000000,  264000000,          22,          20}  //6.2 [12]
};

void *wave420l_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int type)
{
	unsigned int i;
	unsigned char *pTmpAddr = dst_addr;
	const unsigned char *pSrcAddr = src_addr;

	if(pSrcAddr == NULL)
		return (void*)0;

	if( pTmpAddr == NULL)
		return (void*)0;

	for(i=0;i<count;i++)
		*pTmpAddr++ = *pSrcAddr++;

	return dst_addr;
}

//void *wave420l_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int type)
void wave420l_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int type)
{
	unsigned int i;
	unsigned char *pSrcAddr = src_addr;

	if (pSrcAddr == NULL)
		return;

	for(i = 0; i < count; i++)
		*pSrcAddr++ = val;

	return; //return src_addr;
}

int wave420l_get_vpu_handle(wave420l_t **ppVpu)
{
	int i;
	wave420l_t *p_vpu;

	//p_vpu = gpHevcEncHandlePool;
	p_vpu = &gsHevcEncHandlePool[0];
	for (i = 0; i < gHevcEncMaxInstanceNum; ++i, ++p_vpu) {
		if (!p_vpu->m_bInUse)
			break;
	}

	if (i == gHevcEncMaxInstanceNum) {
		*ppVpu = 0;
		return RETCODE_FAILURE;
	}

	p_vpu->m_bInUse = 1;
	p_vpu->m_iVpuInstIndex = i;
	//p_vpu->m_iPrevDispIdx = -1;
	//p_vpu->m_iIndexFrameDisplayPrev = -1;
	p_vpu->m_pPendingHandle = wave420l_GetPendingInstPointer(); //wave420l_GetPendingInst( );
	*ppVpu = p_vpu;

	return RETCODE_SUCCESS;
}

void wave420l_reset_global_var(int iOption)
{
	gHevcEncInitFirst = 0;
	gHevcEncInitFirst_exit = 0;
	if (iOption & 0x1) {
		if (wave420l_memset)
			wave420l_memset(gsHevcEncHandlePool, 0, sizeof(wave420l_t)*gHevcEncMaxInstanceNum, 0);
	}

	//gHevcEncMaxInstanceNum = 0;

	if (iOption & 0x2) {
		gpPendingInsthevcEnc = (CodecInst *)0;
		gpCodecInstPoolWave420l = (CodecInst *)0;
		//gsCodecInstPoolWave420l[]
		gpHevcEncHandlePool = (wave420l_t *)0;
		//gsHevcEncHandlePool[]
	}

	if ( iOption & 0x4) {
		gHevcEncVirtualBitWorkAddr = (codec_addr_t)0;
		gHevcEncPhysicalBitWorkAddr = (PhysicalAddress)0;
		gHevcEncVirtualRegBaseAddr = (codec_addr_t)0;
	}

	if (iOption & 0x8) {
		wave420l_memcpy = (wave420l_memcpy_func *)NULL;
		wave420l_memset = (wave420l_memset_func *)NULL;
		wave420l_interrupt = (wave420l_interrupt_func *)NULL;
		wave420l_ioremap = (wave420l_ioremap_func *)NULL;
		wave420l_iounmap = (wave420l_iounmap_func *)NULL;
		wave420l_read_reg = (wave420l_reg_read_func *)NULL;
		wave420l_write_reg = (wave420l_reg_write_func *)NULL;
		wave420l_usleep = (wave420l_usleep_func *)NULL;
	}
}

void wave420l_free_vpu_handle(wave420l_t *pVpu)
{
	int i, inuse = 0;
	wave420l_t *p_vpu;

	if (pVpu != NULL) {
		pVpu->m_bInUse = 0;
		if (wave420l_memset)
			wave420l_memset(pVpu, 0, sizeof(*pVpu), 0);
	}

	p_vpu = &gsHevcEncHandlePool[0];
	for (i = 0; i < gHevcEncMaxInstanceNum; ++i, ++p_vpu) {
		if (p_vpu->m_bInUse) {
			inuse = 1;
			break;
		}
	}
	if (inuse == 0) {
		gHevcEncInitFirst = 0;
		gHevcEncInitFirst_exit = 0;
	}
}

#define WAVE420L_FIO_TIMEOUT         100

unsigned int wave420l_vdi_fio_read_register(unsigned long core_idx, unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data  = 0xffffffff;

	ctrl  = (addr&0xffff);
	ctrl |= (0<<16);    /* read operation */
	VpuWriteReg(core_idx, W4_VPU_FIO_CTRL_ADDR, ctrl);
	count = WAVE420L_FIO_TIMEOUT;
	while (count--) {
		ctrl = VpuReadReg(core_idx, W4_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			data = VpuReadReg(core_idx, W4_VPU_FIO_DATA);
			break;
		}
	}
	return data;
}

void wave420l_vdi_fio_write_register(unsigned long core_idx, unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	VpuWriteReg(core_idx, W4_VPU_FIO_DATA, data);
	ctrl  = (addr&0xffff);
	ctrl |= (1<<16);    /* write operation */
	VpuWriteReg(core_idx, W4_VPU_FIO_CTRL_ADDR, ctrl);
	return;
}


int wave420l_vdi_wait_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag)
{
	unsigned int loop_count = 0x7FFF;

	if (timeout > 0) {
		loop_count = timeout;
	}

	while (loop_count > 0) {
		if (wave420l_vdi_fio_read_register(core_idx, gdi_busy_flag) == 0) {
			break;
		}
#ifdef ENABLE_WAIT_POLLING_USLEEP
		if (wave420l_usleep != NULL) {
			wave420l_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
		}
#endif
		loop_count--;
	}

	if (loop_count == 0) { // exit or timeout
		return -1;
		//intr_reason = RETCODE_CODEC_EXIT;
		//return RETCODE_CODEC_EXIT;  //RETCODE_VPU_RESPONSE_TIMEOUT
	}

	return 0;
}

int wave420l_vdi_wait_vpu_busy(unsigned long core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
	Uint32 normalReg = TRUE;
	unsigned int loop_count = 0x7FFF;

	if (timeout > 0)
		loop_count = timeout;

	if (addr_bit_busy_flag&0x8000)
		normalReg = FALSE;

	while (loop_count > 0)
	{
		if (normalReg == TRUE) {
			if ( VpuReadReg(core_idx, addr_bit_busy_flag) == 0) break;
		}
		else {
			if (wave420l_vdi_fio_read_register(core_idx, addr_bit_busy_flag) == 0) break;
		}
		loop_count--;
	}

	if( loop_count == 0 ) // exit or timeout
	{
		return -1;
		//intr_reason = RETCODE_CODEC_EXIT;
		//return RETCODE_CODEC_EXIT;  //RETCODE_VPU_RESPONSE_TIMEOUT
	}

	return 0;
}

// coreIdx : [input] TBD (0)
// loop_count : [input]  0 -    default setting( interrupt mode : VPU_BUSY_CHECK_USLEEP_CNT, polling mode : 0xFFFF)
// addr_bit_int_reason : [input] - 0 : W4_VPU_VINT_REASON ,  the others : input register offset from base reg.
// addr_status_reg : [input] - 0 : W4_VPU_VPU_INT_STS ,  the others : input register offset from base reg.
// use_interrupt_flag :  [input] 0 - polling, 1 - interrupt
//vdi_wait_interrupt() in vdi.c
int wave420l_vdi_wait_interrupt(unsigned long coreIdx, unsigned int loop_count,  unsigned int addr_bit_int_reason, unsigned addr_status_reg,  unsigned int use_interrupt_flag)
{
	int intr_reason = 0;
	//int ret;
	Uint32          intrStatusReg = W4_VPU_VPU_INT_STS;

#if !defined(USE_HEVC_INTERRUPT)
	use_interrupt_flag = 0;
#endif

	if (loop_count == 0) {
		loop_count = 0xFFFF;

#ifdef ENABLE_WAIT_POLLING_USLEEP
		if (use_interrupt_flag > 0)
			loop_count = VPU_BUSY_CHECK_USLEEP_CNT;
#endif
	}

	if (addr_status_reg != 0)
		intrStatusReg = addr_status_reg;

	if (addr_bit_int_reason == 0)
		addr_bit_int_reason = W4_VPU_VINT_REASON;

	//check interrupt callback function
	if (use_interrupt_flag > 0) {  //interrupt
		if (wave420l_interrupt == NULL)
			use_interrupt_flag = 0;
	}

	//interrupt
	if (use_interrupt_flag > 0) {
		intr_reason = wave420l_interrupt();
		if (intr_reason == RETCODE_INTR_DETECTION_NOT_ENABLED)
			use_interrupt_flag = 0;

		if (intr_reason == RETCODE_CODEC_EXIT)
			return -1; //RETCODE_CODEC_EXIT;
	}

	if (use_interrupt_flag > 0) {
		if (VpuReadReg(coreIdx, intrStatusReg)) {
			intr_reason=VpuReadReg(coreIdx, addr_bit_int_reason);
			if (intr_reason == 0)
				return -1; // time out

			return intr_reason;
		}
		else {
			return -1; // time out
		}
	}

	//Polling
	if (use_interrupt_flag == 0) {
		while (loop_count > 0)
		{
			if (VpuReadReg(coreIdx, intrStatusReg)) {
				if ((intr_reason=VpuReadReg(coreIdx, addr_bit_int_reason)))
					break;
			}
#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave420l_usleep != NULL) {
				wave420l_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				//if( cnt > VPU_BUSY_CHECK_USLEEP_CNT )    return RETCODE_CODEC_EXIT;
			}
#endif

			loop_count--;
		}
	}

	if (loop_count == 0) // exit or timeout
		intr_reason = -1; //RETCODE_CODEC_EXIT;

	return intr_reason;
}

#define WAVE420L_VCORE_DBG_ADDR(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x300
#define WAVE420L_VCORE_DBG_DATA(__vCoreIdx)      0x8000+(0x1000*__vCoreIdx) + 0x304
#define WAVE420L_VCORE_DBG_READY(__vCoreIdx)     0x8000+(0x1000*__vCoreIdx) + 0x308

static unsigned int wave420l_read_vce_register(
		unsigned int core_idx,
		unsigned int vce_core_idx,
		unsigned int vce_addr
		)
{
	int     vcpu_reg_addr;
	int     udata;
	int     vce_core_base = 0x8000 + 0x1000 * vce_core_idx;

	wave420l_vdi_fio_write_register(core_idx, WAVE420L_VCORE_DBG_READY(vce_core_idx), 0);

	vcpu_reg_addr = vce_addr >> 2;

	wave420l_vdi_fio_write_register(core_idx, WAVE420L_VCORE_DBG_ADDR(vce_core_idx), vcpu_reg_addr + vce_core_base);

	while (TRUE) {
		if (wave420l_vdi_fio_read_register(0, WAVE420L_VCORE_DBG_READY(vce_core_idx)) == 1) {
			udata= wave420l_vdi_fio_read_register(0, WAVE420L_VCORE_DBG_DATA(vce_core_idx));
			break;
		}
	}
	return udata;
}

void wave420l_vdi_print_vpu_status(unsigned long coreIdx)
{
#if defined(TCC_SW_SIM)
#else
	{
		int      rd, wr;
		unsigned int    tq, ip, mc, lf;
		unsigned int    avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
		unsigned int	 ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy, lf_fsm, bs_data, bbusy, fv;
		unsigned int    reg_val;
		unsigned int    index;
		unsigned int    vcpu_reg[31]= {0,};

		DLOGV("-------------------------------------------------------------------------------\n");
		DLOGV("------                            VCPU STATUS                             -----\n");
		DLOGV("-------------------------------------------------------------------------------\n");
		rd = VpuReadReg(coreIdx, W4_BS_RD_PTR);
		wr = VpuReadReg(coreIdx, W4_BS_WR_PTR);
		DLOGV("RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x\n",
				rd, wr, VpuReadReg(coreIdx, W4_BS_OPTION), VpuReadReg(coreIdx, W4_BS_PARAM));

		// --------- VCPU register Dump
		DLOGV("[+] VCPU REG Dump\n");
		for (index = 0; index < 25; index++)
		{
			VpuWriteReg (coreIdx, 0x14, (1<<9) | (index & 0xff));
			vcpu_reg[index] = VpuReadReg (coreIdx, 0x1c);


			if (index < 16)
			{
				DLOGV("0x%08x\t",  vcpu_reg[index]);
				if ((index % 4) == 3) DLOGV("\n");
			}
			else
			{
				switch (index)
				{
					case 16: DLOGV("CR0: 0x%08x\t", vcpu_reg[index]); break;
					case 17: DLOGV("CR1: 0x%08x\n", vcpu_reg[index]); break;
					case 18: DLOGV("ML:  0x%08x\t", vcpu_reg[index]); break;
					case 19: DLOGV("MH:  0x%08x\n", vcpu_reg[index]); break;
					case 21: DLOGV("LR:  0x%08x\n", vcpu_reg[index]); break;
					case 22: DLOGV("PC:  0x%08x\n", vcpu_reg[index]);break;
					case 23: DLOGV("SR:  0x%08x\n", vcpu_reg[index]);break;
					case 24: DLOGV("SSP: 0x%08x\n", vcpu_reg[index]);break;
				}
			}
		}
		DLOGV("[-] VCPU REG Dump\n");
		// --------- BIT register Dump
		DLOGV("[+] BPU REG Dump\n");
		DLOGV("BITPC = 0x%08x\n", wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x18)));
		DLOGV("BIT START=0x%08x, BIT END=0x%08x\n", wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x11c)),
				wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x120)) );
		DLOGV("CODE_BASE			%x \n", wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x7000 + 0x18)));
		DLOGV("VCORE_REINIT_FLAG	%x \n", wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x7000 + 0x0C)));

		// --------- BIT HEVC Status Dump
		ctu_fsm		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x48));
		nb_fsm		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x4c));
		cabac_fsm	= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x50));
		cu_info		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x54));
		mvp_fsm		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x58));
		tc_busy		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x5c));
		lf_fsm		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x60));
		bs_data		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x64));
		bbusy		= wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x68));
		fv		    = wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x6C));

		DLOGV("[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d\n",  wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x40)), wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x44)));
		DLOGV("[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x\n", ((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff), ((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
		DLOGV("[DEBUG-BPUHEVC] NB_FSM:	0x%02x\n", nb_fsm & 0xff);
		DLOGV("[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x\n", ((cabac_fsm>>25) & 0x3f), ((cabac_fsm>>19) & 0x3f), ((cabac_fsm>>13) & 0x3f), ((cabac_fsm>>6) & 0x7f), (cabac_fsm & 0x3f));
		DLOGV("[DEBUG-BPUHEVC] CU_INFO value = 0x%04x \n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)\n", cu_info,
				((cu_info>> 16) & 0x3), ((cu_info>> 13) & 0x7), ((cu_info>> 10) & 0x7), ((cu_info>> 9) & 0x3), ((cu_info>> 8) & 0x1), ((cu_info>> 6) & 0x3), ((cu_info>> 4) & 0x3), ((cu_info>> 2) & 0x3), (cu_info & 0x3));
		DLOGV("[DEBUG-BPUHEVC] MVP_FSM> 0x%02x\n", mvp_fsm & 0xf);
		DLOGV("[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x\n", ((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
		DLOGV("[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x\n", ((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
		DLOGV("[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x\n", ((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff), (bs_data & 0xfff));
		DLOGV("[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d\n", ((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1) , (bbusy & 0x1));
		DLOGV("[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d\n\n", ((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1), ((fv >> 1) & 0x1), (fv & 0x1));
		DLOGV("[-] BPU REG Dump\n");

		// --------- VCE register Dump
		DLOGV("[+] VCE REG Dump\n");
		tq = wave420l_read_vce_register(0, 0, 0xd0);
		ip = wave420l_read_vce_register(0, 0, 0xd4);
		mc = wave420l_read_vce_register(0, 0, 0xd8);
		lf = wave420l_read_vce_register(0, 0, 0xdc);
		avail_cu = (wave420l_read_vce_register(0, 0, 0x11C)>>16) - (wave420l_read_vce_register(0, 0, 0x110)>>16);
		avail_tu = (wave420l_read_vce_register(0, 0, 0x11C)&0xFFFF) - (wave420l_read_vce_register(0, 0, 0x110)&0xFFFF);
		avail_tc = (wave420l_read_vce_register(0, 0, 0x120)>>16) - (wave420l_read_vce_register(0, 0, 0x114)>>16);
		avail_lf = (wave420l_read_vce_register(0, 0, 0x120)&0xFFFF) - (wave420l_read_vce_register(0, 0, 0x114)&0xFFFF);
		avail_ip = (wave420l_read_vce_register(0, 0, 0x124)>>16) - (wave420l_read_vce_register(0, 0, 0x118)>>16);
		DLOGV("       TQ            IP              MC             LF      GDI_EMPTY          ROOM \n");
		DLOGV("------------------------------------------------------------------------------------------------------------\n");
		DLOGV("| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)\n",
				(tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
				(ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
				(mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
				(lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
				wave420l_vdi_fio_read_register(0, 0x88f4),                      /* GDI empty */
				avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);
		/* CU/TU Queue count */
		reg_val = wave420l_read_vce_register(0, 0, 0x12C);
		DLOGV("[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
		reg_val = wave420l_read_vce_register(0, 0, 0x1A0);
		DLOGV("TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
		reg_val = wave420l_read_vce_register(0, 0, 0x1A4);
		DLOGV("LF(%5d)\n", (reg_val>>16)&0xffff);
		DLOGV("VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
				"               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d) \n",
				(reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1, (reg_val>>3)&1,
				(reg_val>>2)&1, (reg_val>>1)&1, (reg_val>>0)&1,
				(reg_val>>10)&1, (reg_val>>9)&1, (reg_val>>8)&1, (reg_val>>7)&1);

		DLOGV("State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x\n",
				wave420l_read_vce_register(0, 0, 0xd0), wave420l_read_vce_register(0, 0, 0xd4), wave420l_read_vce_register(0, 0, 0xd8), wave420l_read_vce_register(0, 0, 0xdc));
		DLOGV("BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", wave420l_read_vce_register(0, 0, 0x194), wave420l_read_vce_register(0, 0, 0x198));
		DLOGV("BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", wave420l_read_vce_register(0, 0, 0x194), wave420l_read_vce_register(0, 0, 0x198));
		DLOGV("DCI INFO\n");
		DLOGV("READ_CNT_0 : 0x%08x\n", wave420l_read_vce_register(0, 0, 0x110));
		DLOGV("READ_CNT_1 : 0x%08x\n", wave420l_read_vce_register(0, 0, 0x114));
		DLOGV("READ_CNT_2 : 0x%08x\n", wave420l_read_vce_register(0, 0, 0x118));
		DLOGV("WRITE_CNT_0: 0x%08x\n", wave420l_read_vce_register(0, 0, 0x11c));
		DLOGV("WRITE_CNT_1: 0x%08x\n", wave420l_read_vce_register(0, 0, 0x120));
		DLOGV("WRITE_CNT_2: 0x%08x\n", wave420l_read_vce_register(0, 0, 0x124));
		reg_val = wave420l_read_vce_register(0, 0, 0x128);
		DLOGV("LF_DEBUG_PT: 0x%08x\n", reg_val & 0xffffffff);
		DLOGV("cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d\n",
				(reg_val >> 16) & 0x1f,
				(reg_val >> 15) & 0x1,
				(reg_val >> 14) & 0x1);
		DLOGV("para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d\n",
				(reg_val >> 13) & 0x1,
				(reg_val >> 12) & 0x1,
				(reg_val >> 11) & 0x1,
				(reg_val >> 10) & 0x1);
		DLOGV("lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d\n",
				(reg_val >> 9) & 0x1,
				(reg_val >> 8) & 0x1,
				(reg_val >> 6) & 0x1,
				(reg_val >> 4) & 0x1,
				reg_val & 0xf);
		DLOGV("[-] VCE REG Dump\n");
		DLOGV("[-] VCE REG Dump\n");

		DLOGV("-------------------------------------------------------------------------------\n");
	}
#endif
}

Uint32 ReadRegVCE(
		unsigned int core_idx,
		unsigned int vce_core_idx,
		unsigned int vce_addr
		)
{
	int     vcpu_reg_addr;
	int     udata;
	int     vce_core_base = 0x8000 + 0x1000*vce_core_idx;

	wave420l_vdi_fio_write_register(core_idx, VCORE_DBG_READY(vce_core_idx), 0);

	vcpu_reg_addr = vce_addr >> 2;

	wave420l_vdi_fio_write_register(core_idx, VCORE_DBG_ADDR(vce_core_idx), vcpu_reg_addr + vce_core_base);

	if (wave420l_vdi_fio_read_register(0, VCORE_DBG_READY(vce_core_idx)) == 1)
		udata= wave420l_vdi_fio_read_register(0, VCORE_DBG_DATA(vce_core_idx));
	else {
		DLOGV("failed to read VCE register: %d, 0x%04x\n", vce_core_idx, vce_addr);
		udata = -2;//-1 can be a valid value
	}

	return udata;
}

void WriteRegVCE(
		unsigned int core_idx,
		unsigned int vce_core_idx,
		unsigned int vce_addr,
		unsigned int udata
		)
{
	int vcpu_reg_addr;

	wave420l_vdi_fio_write_register(core_idx, VCORE_DBG_READY(vce_core_idx),0);

	vcpu_reg_addr = vce_addr >> 2;

	wave420l_vdi_fio_write_register(core_idx, VCORE_DBG_DATA(vce_core_idx), udata);
	wave420l_vdi_fio_write_register(core_idx, VCORE_DBG_ADDR(vce_core_idx), (vcpu_reg_addr) & 0x00007FFF);

	if (wave420l_vdi_fio_read_register(0, VCORE_DBG_READY(vce_core_idx)) < 0)
		DLOGV("failed to write VCE register: 0x%04x\n", vce_addr);
}

static void DisplayVceEncDebugCommon(int core_idx, int vcore_idx, int set_mode, int debug0, int debug1, int debug2)
{
	int reg_val;
	DLOGV("---------------Common Debug INFO-----------------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,0 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug0);
	DLOGV("\t- pipe_on       :  0x%x\n", ((reg_val >> 8) & 0xf));
	DLOGV("\t- cur_pipe      :  0x%x\n", ((reg_val >> 12) & 0xf));
	DLOGV("\t- cur_s2ime     :  0x%x\n", ((reg_val >> 16) & 0xf));
	DLOGV("\t- cur_s2cache   :  0x%x\n", ((reg_val >> 20) & 0x7));
	DLOGV("\t- subblok_done  :  0x%x\n", ((reg_val >> 24) & 0x7f));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug1);
	DLOGV("\t- subblok_done  :  SFU 0x%x LF 0x%x RDO 0x%x IMD 0x%x FME 0x%x IME 0x%x\n", ((reg_val >> 5) & 0x1), ((reg_val >> 4) & 0x1), ((reg_val >> 3) & 0x1), ((reg_val >> 2) & 0x1), ((reg_val >> 1) & 0x1), ((reg_val >> 0) & 0x1));
	DLOGV("\t- pipe_on       :  0x%x\n", ((reg_val >> 8) & 0xf));
	DLOGV("\t- cur_pipe      :  0x%x\n", ((reg_val >> 12) & 0xf));
	DLOGV("\t- cur_s2cache   :  0x%x\n", ((reg_val >> 16) & 0x7));
	DLOGV("\t- cur_s2ime     :  0x%x\n", ((reg_val >> 24) & 0xf));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug2);
	DLOGV("\t- main_ctu_ypos :  0x%x\n", ((reg_val >> 0) & 0xff));
	DLOGV("\t- main_ctu_xpos :  0x%x\n", ((reg_val >> 8) & 0xff));
	DLOGV("\t- o_prp_ctu_ypos:  0x%x\n", ((reg_val >> 16) & 0xff));
	DLOGV("\t- o_prp_ctu_xpos:  0x%x\n", ((reg_val >> 24) & 0xff));

	reg_val = wave420l_vdi_fio_read_register(core_idx, W4_GDI_VCORE0_BUS_STATUS);
	DLOGV("\t- =========== GDI_BUS_STATUS ==========  \n");
	DLOGV("\t- pri_bus_busy:  0x%x\n", ((reg_val >> 0) & 0x1));
	DLOGV("\t- sec_bus_busy:  0x%x\n", ((reg_val >> 16) & 0x1));
	reg_val= VpuReadReg(core_idx, W4_RET_ENC_PIC_TYPE);
	DLOGV("[DEBUG] ret_core1_init : %d\n", (reg_val >> 16) & 0x3ff);
	reg_val = VpuReadReg(core_idx, W4_RET_ENC_PIC_FLAG);
	DLOGV("[DEBUG] ret_core0_init : %d\n", (reg_val >> 5) & 0x3ff);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugDCI(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 0 : DCI DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,0 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- i_cnt_dci_rd_tuh       :  0x%x\n", ((reg_val >> 16) & 0xffff));
	DLOGV("\t- i_cnt_dci_wd_tuh       :  0x%x\n", ((reg_val >> 0) & 0xffff));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_cnt_dci_rd_cu        :  0x%x\n", ((reg_val >> 16) & 0xffff));
	DLOGV("\t- i_cnt_dci_wd_cu        :  0x%x\n", ((reg_val >> 0) & 0xffff));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cnt_dci_rd_ctu       :  0x%x\n", ((reg_val >> 16) & 0xffff));
	DLOGV("\t- i_cnt_dci_2d_ctu       :  0x%x\n", ((reg_val >> 0) & 0xffff));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- i_cnt_dci_rd_coef      :  0x%x\n", ((reg_val >> 16) & 0xffff));
	DLOGV("\t- i_cnt_dci_wd_coef      :  0x%x\n", ((reg_val >> 0) & 0xffff));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- i_dci_full_empty_flag  :  0x%x\n", reg_val);

	DLOGV("----------- MODE 0 : VCE_CTRL DEBUG INFO----------\n");
	// HW_PARAM
	reg_val = ReadRegVCE(core_idx, vcore_idx, 0x0b08);
	DLOGV("\t- r_cnt_enable           :  0x%x\n", (reg_val >> 8) & 0x1);
	DLOGV("\t- r_pic_done_sel         :  0x%x\n", (reg_val >> 9) & 0x1);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- vce_cnt                :  0x%x\n", reg_val);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- prp_cnt                :  0x%x\n", reg_val);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugRDO(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	int reg_val_sub;

	DLOGV("----------- MODE 1 : RDO DEBUG INFO ----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,1 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- o_rdo_cu_root_cb                    :  0x%x\n", ((reg_val >> 0) & 0x1));
	DLOGV("\t- o_rdo_tu_cbf_y                      :  0x%x\n", ((reg_val >> 1) & 0x1));
	DLOGV("\t- o_rdo_tu_cbf_u                      :  0x%x\n", ((reg_val >> 2) & 0x1));
	DLOGV("\t- o_rdo_tu_cbf_v                      :  0x%x\n", ((reg_val >> 3) & 0x1));
	DLOGV("\t- w_rdo_wdma_wait                     :  0x%x\n", ((reg_val >> 4) & 0x1));
	DLOGV("\t- |o_rdo_tu_sb_luma_csbf[63: 0]       :  0x%x\n", ((reg_val >> 5) & 0x1));
	DLOGV("\t- |o_rdo_tu_sb_chro_csbf[31:16]       :  0x%x\n", ((reg_val >> 6) & 0x1));
	DLOGV("\t- |o_rdo_tu_sb_chro_csbf[15: 0]       :  0x%x\n", ((reg_val >> 7) & 0x1));
	DLOGV("\t- o_sub_ctu_coe_ready                 :  0x%x\n", ((reg_val >> 8) & 0x1));
	DLOGV("\t- o_sub_ctu_rec_ready                 :  0x%x\n", ((reg_val >> 9) & 0x1));
	DLOGV("\t- o_rdo_wdma_busy                     :  0x%x\n", ((reg_val >> 10) & 0x1));
	DLOGV("\t- w_rdo_rdma_wait                     :  0x%x\n", ((reg_val >> 11) & 0x1));
	DLOGV("\t- o_log2_cu_size[07:06]               :  0x%x\n", ((reg_val >> 12) & 0x3));
	DLOGV("\t- o_log2_cu_size[15:14]               :  0x%x\n", ((reg_val >> 14) & 0x3));
	DLOGV("\t- o_log2_cu_size[23:22]               :  0x%x\n", ((reg_val >> 16) & 0x3));
	DLOGV("\t- o_log2_cu_size[31:30]               :  0x%x\n", ((reg_val >> 18) & 0x3));
	DLOGV("\t- o_rdo_dbk_valid                     :  0x%x\n", ((reg_val >> 20) & 0x1));

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- debug_status_ctrl                   :  0x%x\n", ((reg_val >> 0) & 0xff));
	reg_val_sub = (reg_val >>  0) & 0xff;
	DLOGV("\t- debug_status_ctrl.fsm_main_cur      :  0x%x\n", ((reg_val_sub >> 0) & 0x7));
	DLOGV("\t- debug_status_ctrl.i_rdo_wdma_wait   :  0x%x\n", ((reg_val_sub >> 3) & 0x1));
	DLOGV("\t- debug_status_ctrl.fsm_cu08_cur      :  0x%x\n", ((reg_val_sub >> 4) & 0x7));
	DLOGV("\t- debug_status_ctrl.init_hold         :  0x%x\n", ((reg_val_sub >> 7) & 0x1));

	DLOGV("\t- debug_status_nb                     :  0x%x\n", ((reg_val >> 8) & 0xff));
	reg_val_sub =(reg_val >> 8) & 0xff;
	DLOGV("\t- debug_status_nb.fsm_save_cur        :  0x%x\n", ((reg_val_sub >> 0) & 0x7));
	DLOGV("\t- debug_status_nb.fsm_load_cur        :  0x%x\n", ((reg_val_sub >> 4) & 0x7));

	DLOGV("\t- debug_status_rec                    :  0x%x\n", ((reg_val >> 16) & 0xf));
	reg_val_sub = (reg_val >> 16) & 0xf;
	DLOGV("\t- debug_status_rec.fsm_obuf_cur       :  0x%x\n", ((reg_val_sub >> 0) & 0x7));

	DLOGV("\t- debug_status_coe                    :  0x%x\n", ((reg_val >> 20) & 0xf));
	reg_val_sub = (reg_val >> 20) & 0xf;
	DLOGV("\t- debug_status_coe.fsm_obuf_cur       :  0x%x\n", ((reg_val_sub >> 0) & 0x7));

	DLOGV("\t- debug_status_para                   :  0x%x\n", ((reg_val >> 24) & 0xff));
	reg_val_sub = (reg_val >> 24) & 0xff;
	DLOGV("\t- debug_status_para.cur_sfu_rd_state  :  0x%x\n", ((reg_val_sub >> 0) & 0xf));
	DLOGV("\t- debug_status_para.cur_para_state    :  0x%x\n", ((reg_val_sub >> 4) & 0xf));

	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugLF(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 2 : LF DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,2 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);

	DLOGV("\t- cur_enc_main_state   : 0x%x \n", (reg_val>>27)&0x1F);
	DLOGV("\t- i_sao_para_valie     : 0x%x \n", (reg_val>>26)&0x1);
	DLOGV("\t- i_sao_fetch_done     : 0x%x \n", (reg_val>>25)&0x1);
	DLOGV("\t- i_global_encode_en   : 0x%x \n", (reg_val>>24)&0x1);
	DLOGV("\t- i_bs_valid           : 0x%x \n", (reg_val>>23)&0x1);
	DLOGV("\t- i_rec_buf_rdo_ready  : 0x%x \n", (reg_val>>22)&0x1);
	DLOGV("\t- o_rec_buf_dbk_hold   : 0x%x \n", (reg_val>>21)&0x1);
	DLOGV("\t- cur_main_state       : 0x%x \n", (reg_val>>16)&0x1F);
	DLOGV("\t- r_lf_pic_dbk_disable : 0x%x \n", (reg_val>>15)&0x1);
	DLOGV("\t- r_lf_pic_sao_disable : 0x%x \n", (reg_val>>14)&0x1);
	DLOGV("\t- para_load_done       : 0x%x \n", (reg_val>>13)&0x1);
	DLOGV("\t- i_rdma_ack_wait      : 0x%x \n", (reg_val>>12)&0x1);
	DLOGV("\t- i_sao_intl_col_done  : 0x%x \n", (reg_val>>11)&0x1);
	DLOGV("\t- i_sao_outbuf_full    : 0x%x \n", (reg_val>>10)&0x1);
	DLOGV("\t- lf_sub_done          : 0x%x \n", (reg_val>>9)&0x1);
	DLOGV("\t- i_wdma_ack_wait      : 0x%x \n", (reg_val>>8)&0x1);
	DLOGV("\t- lf_all_sub_done      : 0x%x \n", (reg_val>>6)&0x1);
	DLOGV("\t- cur_ycbcr            : 0x%x \n", (reg_val>>5)&0x3);
	DLOGV("\t- sub8x8_done          : 0x%x \n", (reg_val>>0)&0xF);

	DLOGV("----------- MODE 2 : SYNC_Y_POS DEBUG INFO----------\n");
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);

	DLOGV("\t- fbc_y_pos            : 0x%x \n", (reg_val>>0)&0xff);
	DLOGV("\t- bwb_y_pos            : 0x%x \n", (reg_val>>16)&0xff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- trace_frame        :  0x%x\n", ((reg_val >> 0) & 0xffff));

	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugSFU(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 3 : SFU DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,3 );

	reg_val = ReadRegVCE(0, vcore_idx, debug[3]);
	DLOGV("\t- i_sub_ctu_pos_y         : 0x%x \n", (reg_val>>0)&0xff);
	DLOGV("\t- i_sub_ctu_pos_x         : 0x%x \n", (reg_val>>8)&0xff);
	DLOGV("\t- i_cu_fifo_wvalid        : 0x%x \n", (reg_val>>16)&0x1);
	DLOGV("\t- i_ctu_busy              : 0x%x \n", (reg_val>>20)&0x1);
	DLOGV("\t- i_cs_sctu               : 0x%x \n", (reg_val>>24)&0x7);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_ctu_pos_y             : 0x%x \n", (reg_val>>0)&0xff);
	DLOGV("\t- i_ctu_pos_x             : 0x%x \n", (reg_val>>8)&0xff);
	DLOGV("\t- i_sao_rdo_valid         : 0x%x \n", (reg_val>>16)&0x1);
	DLOGV("\t- i_sao_en_r              : 0x%x \n", (reg_val>>20)&0x1);
	DLOGV("\t- i_ctu_fifo_wvalid       : 0x%x \n", (reg_val>>24)&0x1);
	DLOGV("\t- cs_sfu_ctu              : 0x%x \n", (reg_val>>28)&0x3);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cu_fifo_wvalid        : 0x%x \n", (reg_val>>0)&0x1);
	DLOGV("\t- i_rdo_cu_rd_valid       : 0x%x \n", (reg_val>>4)&0x1);
	DLOGV("\t- i_cu_size_r             : 0x%x \n", (reg_val>>8)&0x3);
	DLOGV("\t- i_cu_idx_r              : 0x%x \n", (reg_val>>12)&0xf);
	DLOGV("\t- cs_cu                   : 0x%x \n", (reg_val>>16)&0x7);
	DLOGV("\t- cs_fifo                 : 0x%x \n", (reg_val>>20)&0x7);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- w_dbg_tu_fifo_fsm       : 0x%x \n", (reg_val>>0)&0xff);
	DLOGV("\t- i_coff_fifo_wvalid      : 0x%x \n", (reg_val>>8)&0x1);
	DLOGV("\t- i_tuh_fifo_wvalid       : 0x%x \n", (reg_val>>12)&0x1);
	DLOGV("\t- w_dbg_tu_ctrl_fsm       : 0x%x \n", (reg_val>>16)&0xf);
	DLOGV("\t- i_rdo_tc_ready          : 0x%x \n", (reg_val>>20)&0x1);
	DLOGV("\t- w_dbg_coef_st_in_pic    : 0x%x \n", (reg_val>>24)&0x7);
	DLOGV("\t- i_rdo_tu_rd-valid       : 0x%x \n", (reg_val>>28)&0x1);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugDCI2(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 4 : DCI2 DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,4 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- i_cnt_dci_rd_tuh2       : 0x%x \n", (reg_val>>16)&0xffff);
	DLOGV("\t- i_cnt_dci_wd_tuh2       : 0x%x \n", (reg_val>> 0)&0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_cnt_dci_rd_cu2        : 0x%x \n", (reg_val>>16)&0xffff);
	DLOGV("\t- i_cnt_dci_wd_cu2        : 0x%x \n", (reg_val>> 0)&0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cnt_dci_rd_ctu2       : 0x%x \n", (reg_val>>16)&0xffff);
	DLOGV("\t- i_cnt_dci_wd_ctu2       : 0x%x \n", (reg_val>> 0)&0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- i_cnt_dci_rd_coef2      : 0x%x \n", (reg_val>>16)&0xffff);
	DLOGV("\t- i_cnt_dci_wd_coef2      : 0x%x \n", (reg_val>> 0)&0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- i_dci_full_empty_flag   : 0x%x \n", reg_val);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugDCILast(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 5 : DCI LAST DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,5 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- i_cnt_dci_last_rdata[143:112]    : 0x%x \n", reg_val);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_cnt_dci_last_rdata[111: 96]    : 0x%x \n", reg_val & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cnt_dci_last_rdata[ 95: 64]    : 0x%x \n", reg_val);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- i_cnt_dci_last_rdata[ 63: 32]    : 0x%x \n", reg_val);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- i_cnt_dci_last_rdata[ 31:  0]    : 0x%x \n", reg_val);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- i_wr_read_point                  : 0x%x \n", (reg_val >> 16) & 0x7ff );
	DLOGV("\t- i_wr_read_point_limit            : 0x%x \n", (reg_val >> 0) & 0x7ff );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[9]);
	DLOGV("\t- i_sbuf_raddr_store               : 0x%x \n", (reg_val >> 0) & 0x3f );
	DLOGV("\t- i_read_point                     : 0x%x \n", (reg_val >> 8) & 0x1f );
	DLOGV("\t- i_dci_write_addr_b               : 0x%x \n", (reg_val >> 16) & 0x3f );
	DLOGV("\t- i_dci_write_addr_c               : 0x%x \n", (reg_val >> 24) & 0x1f );
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugBigBufferCnt(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 6 : BIG BUFFER CNT DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,6 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- i_cnt_bbuf_read_tuh    : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_bbuf_write_tuh   : 0x%x \n", (reg_val >>  0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_cnt_bbuf_read_cu     : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_bbuf_write_cu    : 0x%x \n", (reg_val >> 0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cnt_bbuf_read_coef   : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_bbuf_write_coef  : 0x%x \n", (reg_val >>  0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- i_cnt_sbuf_read_tuh    : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_sbuf_write_tuh   : 0x%x \n", (reg_val >>  0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- i_cnt_sbuf_read_cu     : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_sbuf_write_cu    : 0x%x \n", (reg_val >> 0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- i_cnt_sbuf_read_ctu    : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_sbuf_write_tcu   : 0x%x \n", (reg_val >>  0) & 0xffff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[9]);
	DLOGV("\t- i_cnt_sbuf_read_coef   : 0x%x \n", (reg_val >> 16) & 0xffff);
	DLOGV("\t- i_cnt_sbuf_write_coef  : 0x%x \n", (reg_val >>  0) & 0xffff);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugBigBufferAddr(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 6 : BIG BUFFER ADDR DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,7 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- i_cnt_bbuf_raddr_read_tuh    : 0x%x \n", (reg_val >> 16) & 0x7ff);
	DLOGV("\t- i_cnt_bbuf_raddr_write_tuh   : 0x%x \n", (reg_val >>  0) & 0x7ff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- i_cnt_bbuf_raddr_read_cu     : 0x%x \n", (reg_val >> 16) & 0x1ff);
	DLOGV("\t- i_cnt_bbuf_raddr_write_cu    : 0x%x \n", (reg_val >>  0) & 0x1ff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- i_cnt_bbuf_raddr_read_coef   : 0x%x \n", (reg_val >> 16) & 0xfff);
	DLOGV("\t- i_cnt_bbuf_raddr_write_coef  : 0x%x \n", (reg_val >>  0) & 0xfff);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- i_cnt_sbuf_raddr_read_tuh    : 0x%x \n", (reg_val >> 16) & 0x1f);
	DLOGV("\t- i_cnt_sbuf_raddr_write_tuh   : 0x%x \n", (reg_val >>  0) & 0x1f);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- i_cnt_sbuf_raddr_read_cu     : 0x%x \n", (reg_val >> 16) & 0x1f);
	DLOGV("\t- i_cnt_sbuf_raddr_write_cu    : 0x%x \n", (reg_val >> 0) & 0x1f);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- i_cnt_sbuf_raddr_read_ctu    : 0x%x \n", (reg_val >> 16) & 0x1f);
	DLOGV("\t- i_cnt_sbuf_raddr_write_tcu   : 0x%x \n", (reg_val >>  0) & 0x1f);

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[9]);
	DLOGV("\t- i_cnt_sbuf_raddr_read_coef   : 0x%x \n", (reg_val >> 16) & 0x1f);
	DLOGV("\t- i_cnt_sbuf_raddr_write_coef  : 0x%x \n", (reg_val >>  0) & 0x1f);
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugSubWb(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 7 : SUB_WB DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,8 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- subwb_debug_0              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- subwb_debug_1              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- subwb_debug_2              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- subwb_debug_3              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- subwb_debug_4              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- int_sync_ypos              : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[9]);
	DLOGV("\t- pic_run_cnt                : 0x%x \n", ((reg_val) >> 0) & 0xffff);
	DLOGV("\t- pic_init_ct                : 0x%x \n", ((reg_val) >> 16) & 0xffff);

	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

static void DisplayVceEncDebugFBC(int core_idx, int vcore_idx, int set_mode, int *debug)
{
	int reg_val;
	DLOGV("----------- MODE 8 : FBC DEBUG INFO----------\n");

	WriteRegVCE(core_idx,vcore_idx, set_mode,9 );

	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[3]);
	DLOGV("\t- ofs_request_count            : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[4]);
	DLOGV("\t- ofs_bvalid_count             : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[5]);
	DLOGV("\t- dat_request_count            : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[6]);
	DLOGV("\t- dat_bvalid_count             : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[7]);
	DLOGV("\t- fbc_debug                    : 0x%x \n", ((reg_val) >> 0) & 0x3FFFFFFF);
	DLOGV("\t- fbc_cr_idle_3d               : 0x%x \n", ((reg_val) >> 30) & 0x1);
	DLOGV("\t- fbc_cr_busy_3d               : 0x%x \n", ((reg_val) >> 31) & 0x1);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[8]);
	DLOGV("\t- outbuf_debug                 : 0x%x \n", reg_val);
	reg_val = ReadRegVCE(core_idx, vcore_idx, debug[9]);
	DLOGV("\t- fbcif_debug                  : 0x%x \n", reg_val);

	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
	DLOGV("\n");
}

void wave420l_print_vpu_status(unsigned long coreIdx, unsigned long productId)
{
	if (productId == PRODUCT_ID_420 || productId == PRODUCT_ID_420L)
	{
		int       rd, wr;
		Uint32    reg_val, num;
		int       vce_enc_debug[12] = {0, };
		int       set_mode;
		int       vcore_num = 0, vcore_idx = 0;
		Uint32    index;
		Uint32    vcpu_reg[31]= {0,};

		DLOGV("-------------------------------------------------------------------------------\n");
		DLOGV("------                            VCPU STATUS(ENC)                        -----\n");
		DLOGV("-------------------------------------------------------------------------------\n");
		rd = VpuReadReg(coreIdx, W4_BS_RD_PTR);
		wr = VpuReadReg(coreIdx, W4_BS_WR_PTR);
		DLOGV("RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x\n",
				rd, wr, VpuReadReg(coreIdx, W4_BS_OPTION), VpuReadReg(coreIdx, W4_BS_PARAM));

		// --------- VCPU register Dump
		DLOGV("[+] VCPU REG Dump\n");
		for (index = 0; index < 25; index++) {
			VpuWriteReg (coreIdx, W4_VPU_PDBG_IDX_REG, (1<<9) | (index & 0xff));
			vcpu_reg[index] = VpuReadReg (coreIdx, W4_VPU_PDBG_RDATA_REG);

			if (index < 16) {
				DLOGV("0x%08x\t",  vcpu_reg[index]);
				if ((index % 4) == 3) DLOGV("\n");
			}
			else {
				switch (index) {
					case 16: DLOGV("CR0: 0x%08x\t", vcpu_reg[index]); break;
					case 17: DLOGV("CR1: 0x%08x\n", vcpu_reg[index]); break;
					case 18: DLOGV("ML:  0x%08x\t", vcpu_reg[index]); break;
					case 19: DLOGV("MH:  0x%08x\n", vcpu_reg[index]); break;
					case 21: DLOGV("LR:  0x%08x\n", vcpu_reg[index]); break;
					case 22: DLOGV("PC:  0x%08x\n", vcpu_reg[index]);break;
					case 23: DLOGV("SR:  0x%08x\n", vcpu_reg[index]);break;
					case 24: DLOGV("SSP: 0x%08x\n", vcpu_reg[index]);break;
				}
			}
		}
		DLOGV("[-] VCPU REG Dump\n");
		DLOGV("vce run flag = %d\n", VpuReadReg(coreIdx, 0x1E8));
		// --------- BIT register Dump
		DLOGV("[+] BPU REG Dump\n");
		for (index=0;index < 30; index++)
		{
			int temp;
			temp = wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x18));
			DLOGV("BITPC = 0x%08x\n", temp);
			if ( temp == 0xffffffff)
				return;
		}

		// --------- BIT HEVC Status Dump
		DLOGV("==================================\n");
		DLOGV("[-] BPU REG Dump\n");
		DLOGV("==================================\n");

		DLOGV("DBG_FIFO_VALID		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx, (W4_REG_BASE + 0x8000 + 0x6C)));

		//SDMA debug information
		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20) | (1<<16)| 0x13c);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SDMA_DBG_INFO		[%08x]\n", reg_val);
		DLOGV("\t- [   28] need_more_update  : 0x%x \n", (reg_val>>28)&0x1 );
		DLOGV("\t- [27:25] tr_init_fsm       : 0x%x \n", (reg_val>>25)&0x7 );
		DLOGV("\t- [24:18] remain_trans_size : 0x%x \n", (reg_val>>18)&0x7F);
		DLOGV("\t- [17:13] wdata_out_cnt     : 0x%x \n", (reg_val>>13)&0x1F);
		DLOGV("\t- [12:10] wdma_wd_fsm       : 0x%x \n", (reg_val>>10)&0x1F);
		DLOGV("\t- [ 9: 7] wdma_wa_fsm       : 0x%x ", (reg_val>> 7)&0x7 );
		if (((reg_val>>7) &0x7) == 3) {
			DLOGV("-->WDMA_WAIT_ADDR  \n");
		}
		else {
			DLOGV("\n");
		}
		DLOGV("\t- [ 6: 5] sdma_init_fsm     : 0x%x \n", (reg_val>> 5)&0x3 );
		DLOGV("\t- [ 4: 1] save_fsm          : 0x%x \n", (reg_val>> 1)&0xF );
		DLOGV("\t- [    0] unalign_written   : 0x%x \n", (reg_val>> 0)&0x1 );

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16)| 0x13b);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SDMA_NAL_MEM_INF	[%08x]\n", reg_val);
		DLOGV("\t- [ 7: 1] nal_mem_empty_room : 0x%x \n", (reg_val>> 1)&0x3F);
		DLOGV("\t- [    0] ge_wnbuf_size      : 0x%x \n", (reg_val>> 0)&0x1 );

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x131);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SDMA_IRQ		[%08x]: [1]sdma_irq : 0x%x, [2]enable_sdma_irq : 0x%x\n",reg_val, (reg_val >> 1)&0x1,(reg_val &0x1));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x134);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SDMA_BS_BASE_ADDR [%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x135);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SDMA_NAL_STR_ADDR [%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x136);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SDMA_IRQ_ADDR     [%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x137);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SDMA_BS_END_ADDR	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x13A);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SDMA_CUR_ADDR		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x139);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SDMA_STATUS			[%08x]\n",reg_val);
		DLOGV("\t- [2] all_wresp_done : 0x%x \n", (reg_val>> 2)&0x1);
		DLOGV("\t- [1] sdma_init_busy : 0x%x \n", (reg_val>> 1)&0x1 );
		DLOGV("\t- [0] save_busy      : 0x%x \n", (reg_val>> 0)&0x1 );

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x164);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SHU_DBG				[%08x] : shu_unaligned_num (0x%x)\n",reg_val, reg_val);

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x169);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SHU_NBUF_WPTR		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x16A);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("SHU_NBUF_RPTR		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x16C);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78));
		DLOGV("SHU_NBUF_INFO		[%08x]\n",reg_val);
		DLOGV("\t- [5:1] nbuf_remain_byte : 0x%x \n", (reg_val>> 1)&0x1F);
		DLOGV("\t- [  0] nbuf_wptr_wrap   : 0x%x \n", (reg_val>> 0)&0x1 );

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x184);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("CTU_LAST_ENC_POS	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x187);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("CTU_POS_IN_PIC		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x110);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("MIB_EXTADDR			[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x111);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("MIB_INTADDR			[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x113);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("MIB_CMD				[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		wave420l_vdi_fio_write_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x74),(1<<20)| (1<<16) | 0x114);
		while((wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x7c))& 0x1) ==0 );
		DLOGV("MIB_BUSY			[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x78)));

		DLOGV("DBG_BPU_ENC_NB0		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x40)));
		DLOGV("DBG_BPU_CTU_CTRL0	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x44)));
		DLOGV("DBG_BPU_CAB_FSM0	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x48)));
		DLOGV("DBG_BPU_BIN_GEN0	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x4C)));
		DLOGV("DBG_BPU_CAB_MBAE0	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x50)));
		DLOGV("DBG_BPU_BUS_BUSY	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x68)));
		DLOGV("DBG_FIFO_VALID		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x6C)));
		DLOGV("DBG_BPU_CTU_CTRL1	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x54)));
		DLOGV("DBG_BPU_CTU_CTRL2	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x58)));
		DLOGV("DBG_BPU_CTU_CTRL3	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x5C)));

		for (index=0x80; index<0xA0; index+=4) {
			reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + index));
			num     = (index - 0x80)/2;
			DLOGV("DBG_BIT_STACK		[%08x] : Stack%02d (0x%04x), Stack%02d(0x%04x) \n",reg_val, num, reg_val>>16, num+1, reg_val & 0xffff);
		}

		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xA0));
		DLOGV("DGB_BIT_CORE_INFO	[%08x] : pc_ctrl_id (0x%04x), pfu_reg_pc(0x%04x)\n",reg_val,reg_val>>16, reg_val & 0xffff);
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xA4));
		DLOGV("DGB_BIT_CORE_INFO	[%08x] : ACC0 (0x%08x)\n",reg_val, reg_val);
		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xA8));
		DLOGV("DGB_BIT_CORE_INFO	[%08x] : ACC1 (0x%08x)\n",reg_val, reg_val);

		reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xAC));
		DLOGV("DGB_BIT_CORE_INFO	[%08x] : pfu_ibuff_id(0x%04x), pfu_ibuff_op(0x%04x)\n",reg_val,reg_val>>16, reg_val & 0xffff);

		for (num=0; num<5; num+=1) {
			reg_val = wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xB0));
			DLOGV("DGB_BIT_CORE_INFO	[%08x] : core_pram_rd_en(0x%04x), core_pram_rd_addr(0x%04x)\n",reg_val,reg_val>>16, reg_val & 0xffff);
		}

		DLOGV("SAO_LUMA_OFFSET	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xB4)));
		DLOGV("SAO_CB_OFFSET	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xB8)));
		DLOGV("SAO_CR_OFFSET	[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0xBC)));

		DLOGV("GDI_NO_MORE_REQ		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x8f0)));
		DLOGV("GDI_EMPTY_FLAG		[%08x]\n",wave420l_vdi_fio_read_register(coreIdx,(W4_REG_BASE + 0x8000 + 0x8f4)));

		if ( productId == PRODUCT_ID_420) {
			DLOGV("WAVE420_CODE VCE DUMP\n");
			vce_enc_debug[0] = 0x0bc8;//MODE SEL
			vce_enc_debug[1] = 0x0be0;
			vce_enc_debug[2] = 0x0bcc;
			vce_enc_debug[3] = 0x0be4;
			vce_enc_debug[4] = 0x0be8;
			vce_enc_debug[5] = 0x0bec;
			vce_enc_debug[6] = 0x0bc0;
			vce_enc_debug[7] = 0x0bc4;
			vce_enc_debug[8] = 0x0bf0;
			vce_enc_debug[9] = 0x0bf4;
			set_mode         = 0x0bc8;
			vcore_num        = 1;
		}
		else if (productId == PRODUCT_ID_420L) {
			DLOGV("WAVE420L_CODE VCE DUMP\n");
			vce_enc_debug[0] = 0x0bd0;//MODE SEL
			vce_enc_debug[1] = 0x0bd4;
			vce_enc_debug[2] = 0x0bd8;
			vce_enc_debug[3] = 0x0bdc;
			vce_enc_debug[4] = 0x0be0;
			vce_enc_debug[5] = 0x0be4;
			vce_enc_debug[6] = 0x0be8;
			vce_enc_debug[7] = 0x0bc4;
			vce_enc_debug[8] = 0x0bf0;
			vce_enc_debug[9] = 0x0bf4;
			set_mode         = 0x0bd0;
			vcore_num        = 1;
		}
		else {
			return;
		}

		for (vcore_idx = 0; vcore_idx < vcore_num ; vcore_idx++)
		{
			DLOGV("==========================================\n");
			DLOGV("[+] VCE REG Dump VCORE_IDX : %d\n",vcore_idx);
			DLOGV("==========================================\n");
			DisplayVceEncDebugCommon         (coreIdx, vcore_idx, set_mode, vce_enc_debug[0], vce_enc_debug[1], vce_enc_debug[2]);
			DisplayVceEncDebugDCI            (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugRDO            (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugLF             (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugSFU            (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugDCI2           (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugDCILast        (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugBigBufferCnt   (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugBigBufferAddr  (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugSubWb          (coreIdx, vcore_idx, set_mode, vce_enc_debug);
			DisplayVceEncDebugFBC            (coreIdx, vcore_idx, set_mode, vce_enc_debug);
		}

	}
}

void wave420l_vdi_make_log(unsigned long core_idx, const char *str, int step)
{
	int val;

	val = VpuReadReg(core_idx, W4_INST_INDEX);
	val &= 0xffff;
	if (step == 1)
		VLOG(INFO, "\n**%s start(%d)\n", str, val);
	else if (step == 2)	//
		VLOG(INFO, "\n**%s timeout(%d)\n", str, val);
	else
		VLOG(INFO, "\n**%s end(%d)\n", str, val);
}

void wave420l_vdi_log(unsigned long core_idx, int cmd, int step)
{
	int i;

	// BIT_RUN command
	enum {
		SEQ_INIT = 1,
		SEQ_END = 2,
		PIC_RUN = 3,
		SET_FRAME_BUF = 4,
		ENCODE_HEADER = 5,
		ENC_PARA_SET = 6,
		DEC_PARA_SET = 7,
		DEC_BUF_FLUSH = 8,
		RC_CHANGE_PARAMETER	= 9,
		VPU_SLEEP = 10,
		VPU_WAKE = 11,
		ENC_ROI_INIT = 12,
		FIRMWARE_GET = 0xf,
		VPU_RESET = 0x10,
	};

	if (core_idx >= MAX_NUM_VPU_CORE)
		return ;

	switch(cmd)
	{
		case INIT_VPU:
			wave420l_vdi_make_log(core_idx, "INIT_VPU", step);
			break;
		case DEC_PIC_HDR: //SET_PARAM for ENC
			wave420l_vdi_make_log(core_idx, "SET_PARAM(ENC), DEC_PIC_HDR(DEC)", step);
			break;
		case FINI_SEQ:
			wave420l_vdi_make_log(core_idx, "FINI_SEQ", step);
			break;
		case SET_FRAMEBUF:
			wave420l_vdi_make_log(core_idx, "SET_FRAMEBUF", step);
			break;
		case GET_FW_VERSION:
			wave420l_vdi_make_log(core_idx, "GET_FW_VERSION", step);
			break;
		case QUERY_DECODER:
			wave420l_vdi_make_log(core_idx, "QUERY_DECODER", step);
			break;
		case SLEEP_VPU:
			wave420l_vdi_make_log(core_idx, "SLEEP_VPU", step);
			break;
		case CREATE_INSTANCE:
			wave420l_vdi_make_log(core_idx, "CREATE_INSTANCE", step);
			break;
		case RESET_VPU:
			wave420l_vdi_make_log(core_idx, "RESET_VPU", step);
			break;
		default:
			wave420l_vdi_make_log(core_idx, "ANY_CMD", step);
			break;
	}

	for (i = 0; i < 0x200; i = i + 16)
	{
		VLOG(INFO, "0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
				wave420l_vdi_read_register(core_idx, i), wave420l_vdi_read_register(core_idx, i+4),
				wave420l_vdi_read_register(core_idx, i+8), wave420l_vdi_read_register(core_idx, i+0xc));
	}

	if (cmd == INIT_VPU || cmd == VPU_RESET || cmd == CREATE_INSTANCE)
		wave420l_vdi_print_vpu_status(core_idx);
}

#define VDI_128BIT_BUS_SYSTEM_ENDIAN 	VDI_128BIT_LITTLE_ENDIAN
void wave420l_byte_swap(unsigned char *data, int len)
{
	Uint8 temp;
	Int32 i;

	for (i=0; i<len; i+=2) {
		temp        = data[i];
		data[i]     = data[i + 1];
		data[i + 1] = temp;
	}
}

void wave420l_word_swap(unsigned char *data, int len)
{
	Uint16  temp;
	Uint16* ptr = (Uint16 *)data;
	Int32   i, size = len / sizeof(Uint16);

	for (i = 0; i < size; i += 2) {
		temp       = ptr[i];
		ptr[i]     = ptr[i + 1];
		ptr[i + 1] = temp;
	}
}

void wave420l_dword_swap(unsigned char *data, int len)
{
	Uint32  temp;
	Uint32 *ptr = (Uint32 *)data;
	Int32   i, size = len/sizeof(Uint32);

	for (i = 0; i < size; i += 2) {
		temp     = ptr[i];
		ptr[i]   = ptr[i+1];
		ptr[i+1] = temp;
	}
}

void wave420l_lword_swap(unsigned char *data, int len)
{
	Uint64  temp;
	Uint64 *ptr = (Uint64 *)data;
	Int32   i, size = len/sizeof(Uint64);

	for (i = 0; i < size; i += 2) {
		temp       = ptr[i];
		ptr[i]     = ptr[i + 1];
		ptr[i + 1] = temp;
	}
}

int wave420l_vdi_convert_endian(unsigned int coreIdx, unsigned int endian)
{
	switch (endian)
	{
		case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
		case VDI_BIG_ENDIAN:          endian = 0x0f; break;
		case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
		case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
	}
	return (endian & 0x0f);
}

int wave420l_swap_endian(unsigned int coreIdx, unsigned char *data, int len, int endian)
{
	int changes;
	int sys_endian;
	BOOL byteChange, wordChange, dwordChange, lwordChange;

	sys_endian = VDI_128BIT_BUS_SYSTEM_ENDIAN;

	endian     = wave420l_vdi_convert_endian(coreIdx, endian);
	sys_endian = wave420l_vdi_convert_endian(coreIdx, sys_endian);

	if (endian == sys_endian)
		return 0;

	changes     = endian ^ sys_endian;
	byteChange  = changes & 0x01;
	wordChange  = ((changes & 0x02) == 0x02);
	dwordChange = ((changes & 0x04) == 0x04);
	lwordChange = ((changes & 0x08) == 0x08);

	if (byteChange)  wave420l_byte_swap(data, len);
	if (wordChange)  wave420l_word_swap(data, len);
	if (dwordChange) wave420l_dword_swap(data, len);
	if (lwordChange) wave420l_lword_swap(data, len);

	return 1;
}

unsigned int cal_vpu_hevc_enc_framebuf_size (
		unsigned int width,
		unsigned int height,
		unsigned int frame_count,
		unsigned int bufAlignSize,
		unsigned int bufStartAddrAlign
		)
{
	unsigned int FrameBufferSize = 0;
	unsigned int width8, height8, stride;
	unsigned int LumaSize, ChromaSize;
	unsigned int DpbSize, mvColSize,fbcYTblSize, fbcCTblSize, subSampledSize;
	unsigned int heAlign = 4 * 1024;

	width8  = VPU_ALIGN8(width);
	height8 = VPU_ALIGN8(height);
	stride = VPU_ALIGN32(width);

	LumaSize = stride * height8;
	ChromaSize = VPU_ALIGN32(stride/2) * height8;	//maximum size
	// YUV420 = (VPU_ALIGN32(Stride/2) * height8) / 2
	// YUV422 or Packed = VPU_ALIGN32(Stride/2) * height8
	if (bufAlignSize < VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN) {
		bufAlignSize = VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN;
	}

	if (bufStartAddrAlign < VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN) {
		bufStartAddrAlign = VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN;
	}

	DpbSize = LumaSize + ChromaSize*2;
	DpbSize = VPU_ALIGN(DpbSize, bufAlignSize);

	mvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(width8, height8);    //(((width8+63)/64)*((height8+63)/64)*128);  HEVC_ENC_MVCOL_BUF_SIZE
	mvColSize = VPU_ALIGN(mvColSize, heAlign) + heAlign;    //mvColSize = VPU_ALIGN16(mvColSize); mvColSize = ((mvColSize*frame_count+4095)&~4095)+4096;   // 4096 is a margin
	mvColSize = VPU_ALIGN(mvColSize, bufAlignSize);

	fbcYTblSize = WAVE4_FBC_LUMA_TABLE_SIZE(width8, height8);    //(((height8+15)/16)*((width8+255)/256)*128);  HEVC_FBC_LUMA_TABLE_SIZE
	//fbcYTblSize = VPU_ALIGN16(fbcYTblSize); fbcYTblSize = ((fbcYTblSize*frame_count+4095)&~4095)+4096;
	fbcYTblSize = VPU_ALIGN(fbcYTblSize, heAlign) + heAlign;
	fbcYTblSize = VPU_ALIGN(fbcYTblSize, bufAlignSize);

	fbcCTblSize = WAVE4_FBC_CHROMA_TABLE_SIZE(width8, height8);  //(((height8+15)/16)*((width8/2+255)/256)*128);    HEVC_FBC_CHROMA_TABLE_SIZE
	//fbcCTblSize = VPU_ALIGN16(fbcCTblSize); fbcCTblSize = ((fbcCTblSize*frame_count+4095)&~4095)+4096;
	fbcCTblSize = VPU_ALIGN(fbcCTblSize, heAlign) + heAlign;
	fbcCTblSize = VPU_ALIGN(fbcCTblSize, bufAlignSize);

	subSampledSize = (VPU_ALIGN16(width8/4) * VPU_ALIGN8(height8/4));	//subSampledSize = ((((width8/4)+15)&~15)*(((height8/4)+7)&~7));
	//subSampledSize = ((subSampledSize*frame_count+4095)&~4095)+4096;
	subSampledSize = VPU_ALIGN(subSampledSize, heAlign) + heAlign;
	subSampledSize = VPU_ALIGN(subSampledSize, bufAlignSize);

	FrameBufferSize = DpbSize + mvColSize + fbcYTblSize + fbcCTblSize + subSampledSize;
	FrameBufferSize = FrameBufferSize * frame_count + bufStartAddrAlign;	// minFrameBufferCount = 2;
	FrameBufferSize = VPU_ALIGN(FrameBufferSize, bufAlignSize);

	return FrameBufferSize;
}

/* Calculate Frame Buffer Size
   unsigned int width [input]
   unsigned int height [input]
   unsigned int frame_count [input]
   unsigned int frame_buffer_size [output]
   */

unsigned int wave420l_CalcMinFrameBufferSize (unsigned int width, unsigned int height, unsigned int frame_count)
{
	unsigned int FrameBufferSize;
	unsigned int width8, height8, stride;
	unsigned int LumaSize, ChromaSize;
	unsigned int DpbSize, mvColSize,fbcYTblSize, fbcCTblSize, subSampledSize;

	if( frame_count == 0 )
		frame_count= 1;

	width8  = VPU_ALIGN8(width);
	height8 = VPU_ALIGN8(height);
	stride = VPU_ALIGN32(width);

	LumaSize = stride * height8;
	ChromaSize = VPU_ALIGN32(stride/2) * height8;	//maximum size
	// YUV420 = (VPU_ALIGN32(Stride/2) * height8) / 2
	// YUV422 or Packed = VPU_ALIGN32(Stride/2) * height8
	DpbSize = LumaSize + ChromaSize*2;
	DpbSize *= frame_count;

	mvColSize = WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(width8, height8);    //(((width8+63)/64)*((height8+63)/64)*128);
	mvColSize = VPU_ALIGN16(mvColSize);
	mvColSize = ((mvColSize*frame_count+4095)&~4095)+4096;   /* 4096 is a margin */

	fbcYTblSize = WAVE4_FBC_LUMA_TABLE_SIZE(width8, height8);    //(((height8+15)/16)*((width8+255)/256)*128);
	fbcYTblSize = VPU_ALIGN16(fbcYTblSize);
	fbcYTblSize = ((fbcYTblSize*frame_count+4095)&~4095)+4096;

	fbcCTblSize = WAVE4_FBC_CHROMA_TABLE_SIZE(width8, height8);  //(((height8+15)/16)*((width8/2+255)/256)*128);
	fbcCTblSize = VPU_ALIGN16(fbcCTblSize);
	fbcCTblSize = ((fbcCTblSize*frame_count+4095)&~4095)+4096;

	subSampledSize = ((((width8/4)+15)&~15)*(((height8/4)+7)&~7));
	subSampledSize = ((subSampledSize*frame_count+4095)&~4095)+4096;

	FrameBufferSize = DpbSize + mvColSize + fbcYTblSize + fbcCTblSize + subSampledSize;

	return FrameBufferSize;
}

int wave420l_HevcLevelCalculation(unsigned int picWidth, unsigned int picHeight, unsigned int rcEnable, unsigned int initialDelay, unsigned int targetRate, unsigned int frameRate)
{
	int i;
	unsigned int pic_sizeIn_samples_y;
	unsigned int cpb_size=0;
	unsigned int bit_rate=0;
	unsigned int luma_sr=0;

	pic_sizeIn_samples_y = picWidth*picHeight;

	if (rcEnable) {
		cpb_size = targetRate/1000;
		cpb_size = cpb_size*initialDelay;
		luma_sr  = pic_sizeIn_samples_y * frameRate;
		bit_rate = targetRate;
	}

	// VLOG(1, "cpb_sizd=%d, luma_sr=%d, bit_rate=%d \n", cpb_size, luma_sr, bit_rate);
	for (i = 0; i < 8; i++) //for (i = 0; i < 13; i++)
	{
		const hevc_level_t *p = &wave420l_HEVC_LEVEL_TABLE[i];

		if (pic_sizeIn_samples_y > p->max_luma_ps)
			continue;
		if (picWidth > p->max_sqrt_luma_ps_8)
			continue;
		if (picHeight > p->max_sqrt_luma_ps_8)
			continue;

		if (rcEnable) {
			if (cpb_size > p->max_cpb)
				continue;
			if (luma_sr > p->max_luma_sr)
				continue;
			if (bit_rate > p->max_br)
				continue;
		}

		break;
	}
	if (i > 7)  // max level is 5.0
		i = 7; // max value

	return wave420l_HEVC_LEVEL_TABLE[i].level;
}
