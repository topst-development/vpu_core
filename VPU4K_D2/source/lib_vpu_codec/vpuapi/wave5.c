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

#include "product.h"
#include "wave5.h"
#include "wave5_regdefine.h"
#include "wave5error.h"
#include "wave5_regdefine.h"
#include "wave5_core.h"
#include "vpu4k_d2_log.h"

extern int WAVE5_dec_reset( wave5_t *pVpu, int iOption );

#define WAVE5_TEMPBUF_OFFSET        (1024*1024)
#define WAVE5_TASK_BUF_OFFSET       (2*1024*1024)   // common mem = | codebuf(1M) | tempBuf(1M) | taskbuf0x0 ~ 0xF |

extern unsigned int g_uiBitCodeSize;

extern codec_addr_t gWave5VirtualBitWorkAddr;
extern PhysicalAddress gWave5PhysicalBitWorkAddr;

static CodecInst *pendingInstwave5;
#ifdef PROFILE_DECODING_TIME_INCLUDE_TICK
unsigned int g_uiLastPerformanceCycles = 0;
#endif

/*
 * [20191203] g_KeepPendingInstForSimInts is set to 1 &
 * insufficient bitstream at next decoding time in case that interrputs
 * for both INT_WAVE5_DEC_PIC and INT_WAVE5_BSBUF_EMPTY
 * happen at the same time.
*/
static int g_KeepPendingInstForSimInts = 0;

int WAVE5_ResetRegisters(Uint32 coreIdx, int option)
{
	int ret = RETCODE_FAILURE;

	if (option & 0x1) {
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x10, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x14, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x18, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x20, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x24, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x28, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x2C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x30, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x34, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x38, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x3C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x40, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x44, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x48, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x4C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x50, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x54, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x58, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x5C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x60, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x64, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x68, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x6C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x70, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x74, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x78, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x7C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x80, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x84, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x88, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x8C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x90, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x94, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x98, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x9C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xA0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xA4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xA8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xAC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xB0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xB4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xB8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xBC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xC0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xC4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xC8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xCC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xD0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xD4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xD8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xDC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xE0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xE4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xE8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xEC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xF0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xF4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xF8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0xFC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x100, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x104, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x108, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x10C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x110, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x114, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x118, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x11C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x120, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x124, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x128, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x12C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x130, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x134, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x138, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x13C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x140, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x144, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x148, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x14C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x150, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x154, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x158, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x15C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x160, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x164, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x168, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x16C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x170, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x174, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x178, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x17C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x180, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x184, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x188, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x18C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x190, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x194, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x198, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x19C, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1A0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1A4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1A8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1AC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1B0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1B4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1B8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1BC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1C0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1C4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1C8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1CC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1D0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1D4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1D8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1DC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1E0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1E4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1E8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1EC, 0x00);

		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1F0, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1F4, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1F8, 0x00);
		Wave5WriteReg(coreIdx, W5_CMD_REG_BASE+0x1FC, 0x00);

		ret = RETCODE_SUCCESS;
	}
	return ret;
}

void WAVE5_PeekPendingInstForSimInts(int keep)
{
	g_KeepPendingInstForSimInts = keep;
}

int WAVE5_PokePendingInstForSimInts()
{
	return g_KeepPendingInstForSimInts;
}

void WAVE5_SetPendingInst(Uint32 coreIdx, CodecInst *inst)
{
	pendingInstwave5 = inst;
}

void WAVE5_ClearPendingInst(Uint32 coreIdx)
{
	if(pendingInstwave5) {
		pendingInstwave5 = 0;
	}
}

CodecInst *WAVE5_GetPendingInst(Uint32 coreIdx)
{
	return pendingInstwave5;
}

void *WAVE5_GetPendingInstPointer(void)
{
	return ((void*)&pendingInstwave5);
}

Uint32 Wave5VpuIsInit(Uint32 coreIdx)
{
	Uint32 pc;

	pc = (Uint32)Wave5ReadReg(coreIdx, W5_VCPU_CUR_PC);
	return pc;
}

Int32 Wave5VpuIsBusy(Uint32 coreIdx)
{
	return Wave5ReadReg(coreIdx, W5_VPU_BUSY_STATUS);
}

Int32 WaveVpuGetProductId(Uint32  coreIdx)
{
	Uint32  productId = PRODUCT_ID_NONE;
	Uint32  val;

	if (coreIdx >= MAX_NUM_VPU_CORE) {
		return PRODUCT_ID_NONE;
	}

	val = Wave5ReadReg(coreIdx, W5_PRODUCT_NUMBER);

	switch (val) {
	case WAVE512_CODE:   productId = PRODUCT_ID_512;   break;
	case WAVE520_CODE:   productId = PRODUCT_ID_520;   break;
	default:
		//VLOG(ERR, "Check productId(%d)\n", val);
		break;
	}
	return productId;
}

void Wave5BitIssueCommand(CodecInst *instance, Uint32 cmd)
{
	Uint32 instanceIndex = 0;
	Uint32 codecMode     = 0;
	Uint32 coreIdx = 0;
	if (instance != NULL) {
		instanceIndex = instance->instIndex;
		codecMode     = instance->codecMode;
		coreIdx       = instance->coreIdx;
	}

	Wave5WriteReg(coreIdx, W5_CMD_INSTANCE_INFO,  (codecMode<<16)|(instanceIndex&0xffff));
	Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
	Wave5WriteReg(coreIdx, W5_COMMAND, cmd);

	#ifdef ENABLE_VDI_LOG
	if ((instance != NULL && instance->loggingEnable)) {
		vdi_log(coreIdx, cmd, 1);
	}
	#endif

	//VLOG(1, "Wave5BitIssueCommand cmd=%d, instanceIndex=%d \n", cmd, instanceIndex);
	Wave5WriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);
	return;
}

static RetCode SendQuery(CodecInst *instance, QUERY_OPT queryOpt)
{
	// Send QUERY cmd
	Wave5WriteReg(instance->coreIdx, W5_QUERY_OPTION, queryOpt);
	Wave5WriteReg(instance->coreIdx, W5_VPU_BUSY_STATUS, 1);
#ifdef USE_INTERRUPT_CALLBACK
	Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_DEC_QUERY));
#endif
#ifdef ENABLE_SENDQUERY_CHECK0809_TELPIC_134
	Wave5WriteReg(instance->coreIdx, W5_RET_SUCCESS, 0xffffffff);
#endif
	Wave5BitIssueCommand(instance, W5_QUERY);

#ifdef USE_INTERRUPT_CALLBACK
	while (1) {
		int iRet = 0;

		if (wave5_interrupt != NULL) {
			iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));
		}

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			Wave5VpuClearInterrupt(instance->coreIdx, iRet);
			#endif

			if (iRet & (1<<INT_WAVE5_DEC_QUERY)) {
				break;
			}
			else {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				return RETCODE_FAILURE;
			}
		}
	}
#else
	{
		int cnt = 0;

		while (1) {
			cnt++;
		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0)
				return RETCODE_CODEC_EXIT;
		#endif

		#if defined(ENABLE_WAIT_POLLING_USLEEP) && !defined(DISABLE_WAIT_POLLING_USLEEP_SENDQUERY)
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					#ifdef ENABLE_VDI_LOG
					if (instance->loggingEnable)
						vdi_log(instance->coreIdx, W5_QUERY, 2);
					#endif
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
				#ifdef ENABLE_VDI_LOG
				if (instance->loggingEnable)
					vdi_log(instance->coreIdx, W5_QUERY, 2);
				#endif
				return RETCODE_CODEC_EXIT;
			}
			if (Wave5ReadReg(instance->coreIdx, W5_VPU_BUSY_STATUS) == 0)
				break;
		}
	}
#endif

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {
		return RETCODE_FAILURE;
	}

	return RETCODE_SUCCESS;
}

static RetCode SetupWave5Properties(Uint32 coreIdx)
{
	VpuAttr*    pAttr = &g_VpuCoreAttributes[coreIdx];
	RetCode     ret = RETCODE_SUCCESS;

	pAttr->productNumber          = WAVE512_CODE;
	pAttr->productId              = PRODUCT_ID_512;
	pAttr->supportGDIHW           = TRUE;
	pAttr->supportDecoders        = 0;
	#if TCC_HEVC___DEC_SUPPORT == 1
	pAttr->supportDecoders        = (1<<STD_HEVC);
	#endif
	#if TCC_VP9____DEC_SUPPORT == 1
	pAttr->supportDecoders        |= (1<<STD_VP9);
	#endif
	pAttr->supportEncoders        = 0;
	pAttr->supportCommandQueue    = TRUE;
	pAttr->supportWTL             = TRUE;
	pAttr->supportTiled2Linear    = FALSE;
	pAttr->supportMapTypes        = FALSE;
	pAttr->support128bitBus       = TRUE;
	pAttr->supportThumbnailMode   = TRUE;
	pAttr->supportEndianMask      = (Uint32)((1<<VDI_LITTLE_ENDIAN) | (1<<VDI_BIG_ENDIAN) | (1<<VDI_32BIT_LITTLE_ENDIAN) | (1<<VDI_32BIT_BIG_ENDIAN) | (0xffff<<16));
	pAttr->supportBitstreamMode   = (1<<BS_MODE_INTERRUPT) | (1<<BS_MODE_PIC_END);
	pAttr->framebufferCacheType   = FramebufCacheNone;
	pAttr->bitstreamBufferMargin  = 4096;
	pAttr->numberOfVCores         = MAX_NUM_VCORE;
	pAttr->numberOfMemProtectRgns = 10;

	return ret;
}

RetCode Wave5VpuGetVersion(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision)
{
	Uint32 regVal;

#ifdef USE_INTERRUPT_CALLBACK
	Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_DEC_QUERY));
#endif
	Wave5WriteReg(coreIdx, W5_QUERY_OPTION, GET_VPU_INFO);
	Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
	Wave5WriteReg(coreIdx, W5_COMMAND, W5_QUERY);
	Wave5WriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

#ifdef USE_INTERRUPT_CALLBACK
	while (1) {
		int iRet = 0;

		if (wave5_interrupt != NULL) {
			iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
		}

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			Wave5VpuClearInterrupt(coreIdx, iRet);
			#endif

			if (iRet & (1<<INT_WAVE5_DEC_QUERY)) {
				break;
			}
			else {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				return RETCODE_FAILURE;
			}
		}
	}
#else
	{
		int cnt = 0;

		while (WAVE5_IsBusy(coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					return RETCODE_CODEC_EXIT;
				}
				#ifdef ENABLE_FORCE_ESCAPE
				if (gWave5InitFirst_exit > 0) {
					return RETCODE_CODEC_EXIT;
				}
				#endif
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
				return RETCODE_CODEC_EXIT;
			}
		}
	}
#endif

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		return RETCODE_CODEC_EXIT;
	}
	#endif

	if (Wave5ReadReg(coreIdx, W5_RET_SUCCESS) == FALSE) {
		return RETCODE_QUERY_FAILURE;
	}

	regVal = Wave5ReadReg(coreIdx, W5_RET_FW_VERSION);
	if (versionInfo != NULL) {
		*versionInfo = 0;
	}

	if (revision != NULL) {
		*revision = regVal;
	}

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		return RETCODE_CODEC_EXIT;
	}
	#endif

	return RETCODE_SUCCESS;
}

RetCode Wave5VpuInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size, Int32 cq_count)
{
	PhysicalAddress codeBase_PA, tempBase_PA;
	PhysicalAddress taskBufBase_PA;
	Uint32          codeSize, tempSize;
	Uint32          i, regVal, remapSize;
	Uint32          hwOption = 0;
	RetCode         ret = RETCODE_SUCCESS;
	CodecInstHeader hdr;

	if (wave5_memset) {
		wave5_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);
	}

	// ALIGN TO 4KB
	codeSize = (WAVE5_MAX_CODE_BUF_SIZE&~0xfff);
	codeBase_PA = workBuf;
	tempBase_PA = codeBase_PA + WAVE5_MAX_CODE_BUF_SIZE;
	tempSize = WAVE5_TEMPBUF_SIZE;

	codeBase_PA = codeBuf;

	regVal = 0;
	Wave5WriteReg(coreIdx, W5_PO_CONF, regVal);

	/* Reset All blocks */
	regVal = 0x7ffffff;
	Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);	// Reset All blocks
	/* Waiting reset done */

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
	#endif

	{
		int cnt = 0;
		while (Wave5ReadReg(coreIdx, W5_VPU_RESET_STATUS)) {
			cnt++;
		#if 0 //def ENABLE_WAIT_POLLING_USLEEP
			if(wave5_usleep != NULL)
			{
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					WAVE5_dec_reset(pVpu, (0 | (1<<16)));
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
			#endif
			if (cnt > 0x7FFFFFF) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
				return RETCODE_CODEC_EXIT;
			}
		}
	}

	Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

	// clear registers
	if (WAVE5_ResetRegisters(coreIdx, 1) != RETCODE_SUCCESS) {
		return RETCODE_FAILURE;
	}

	/* remap page size */
	remapSize = (codeSize >> 12) &0x1ff;
	regVal = 0x80000000 | (WAVE5_AXI_ID<<20) | (0 << 16) | (W5_REMAP_CODE_INDEX<<12) | (1<<11) | remapSize;
	Wave5WriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
	Wave5WriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);	/* DO NOT CHANGE! */
	Wave5WriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase_PA);
	Wave5WriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase_PA);
	Wave5WriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
	Wave5WriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_AXI_ID<<4) | 0);
	Wave5WriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase_PA);
	Wave5WriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
	Wave5WriteReg(coreIdx, W5_TIMEOUT_CNT,        0xffff);
	Wave5WriteReg(coreIdx, W5_HW_OPTION, hwOption);

	/* Interrupt */
	// decoder
#ifdef USE_INTERRUPT_CALLBACK
	regVal  = (1<<W5_INT_INIT_VPU);
	regVal |= (1<<W5_INT_INIT_SEQ);
	regVal |= (1<<W5_INT_DEC_PIC);
	regVal |= (1<<W5_INT_BSBUF_EMPTY);

	Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE, regVal);
#endif

	#ifndef TEST_ADAPTIVE_CQ_NUM
	Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
	#else
	if (cq_count > 1) {
		Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
	}
	else {
		Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, cq_count);
	}
	#endif
	Wave5WriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, WAVE5_ONE_TASKBUF_SIZE_FOR_CQ);

	for(i = 0 ; i < cq_count ; i++) {
		taskBufBase_PA = (workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE) + (i*ONE_TASKBUF_SIZE_FOR_CQ);
		Wave5WriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase_PA);
	}

	Wave5WriteReg(coreIdx, W5_ADDR_SEC_AXI, workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE);
	Wave5WriteReg(coreIdx, W5_SEC_AXI_SIZE, WAVE5_SEC_AXI_BUF_SIZE);

	hdr.coreIdx = coreIdx;

	Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
	Wave5WriteReg(coreIdx, W5_COMMAND, W5_INIT_VPU);
	Wave5WriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
	#endif

#ifdef USE_INTERRUPT_CALLBACK
	while(1) {
		int iRet = 0;

		if (wave5_interrupt != NULL) {
			iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
		}

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			Wave5VpuClearInterrupt(coreIdx, iRet);
			#endif

			if (iRet & (1<<W5_INT_INIT_VPU)) {
				break;
			}
			else {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				return RETCODE_FAILURE;
			}
		}
		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif
	}
#else
	{
		int cnt = 0;

		while(WAVE5_IsBusy(coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			if(cnt > VPU_BUSY_CHECK_COUNT) {
				return RETCODE_CODEC_EXIT;
			}
		}
	}
#endif

	regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
	if (regVal == 0) {
		Uint32 reasonCode = Wave5ReadReg(coreIdx, W5_RET_FAIL_REASON);
		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif
		return RETCODE_FAILURE;
	}

	ret = SetupWave5Properties(coreIdx);
	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
	#endif
	return ret;
}

RetCode Wave5VpuBuildUpDecParam(CodecInst *instance, DecOpenParam *param)
{
	RetCode     ret = RETCODE_SUCCESS;
	DecInfo*    pDecInfo;
	//VpuAttr*    pAttr = &g_VpuCoreAttributes[instance->coreIdx];
	Uint32      bsEndian = 0;
	pDecInfo    = VPU_HANDLE_TO_DECINFO(instance);

	pDecInfo->streamRdPtrRegAddr      = W5_RET_DEC_BS_RD_PTR;
	pDecInfo->streamWrPtrRegAddr      = W5_BS_WR_PTR;
	pDecInfo->frameDisplayFlagRegAddr = W5_RET_DEC_DISP_FLAG;
	pDecInfo->currentPC               = W5_VCPU_CUR_PC;
	pDecInfo->busyFlagAddr            = W5_VPU_BUSY_STATUS;
	pDecInfo->seqChangeMask           = (param->bitstreamFormat == STD_HEVC) ? SEQ_CHANGE_ENABLE_ALL_HEVC : SEQ_CHANGE_ENABLE_ALL_VP9;
	pDecInfo->scaleWidth              = 0;
	pDecInfo->scaleHeight             = 0;
	pDecInfo->targetSubLayerId        = HEVC_MAX_SUB_LAYER_ID;
	pDecInfo->vbWork.size = WAVE5_WORKBUF_SIZE;

#ifdef TEST_SEQCHANGEMASK_ZERO
	if (pDecInfo->openParam.FullSizeFB == 1) {
		pDecInfo->seqChangeMask = 0;
	}
#endif

	Wave5WriteReg(instance->coreIdx, W5_ADDR_WORK_BASE, pDecInfo->vbWork.phys_addr);
	Wave5WriteReg(instance->coreIdx, W5_WORK_SIZE,      pDecInfo->vbWork.size);

	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_BS_START_ADDR, pDecInfo->streamBufStartAddr);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_BS_SIZE, pDecInfo->streamBufSize);

	switch(param->streamEndian)
	{
		case VDI_LITTLE_ENDIAN:       bsEndian = 0x00; break;
		case VDI_BIG_ENDIAN:          bsEndian = 0x0f; break;
		case VDI_32BIT_LITTLE_ENDIAN: bsEndian = 0x04; break;
		case VDI_32BIT_BIG_ENDIAN:    bsEndian = 0x03; break;
		default:                      bsEndian = param->streamEndian; break;
	}
	bsEndian = bsEndian & 0x0f;

	/* NOTE: When endian mode is 0, SDMA reads MSB first */
	bsEndian = (~bsEndian&VDI_128BIT_ENDIAN_MASK);
	Wave5WriteReg(instance->coreIdx, W5_CMD_BS_PARAM, bsEndian);

	Wave5WriteReg(instance->coreIdx, W5_VPU_BUSY_STATUS, 1);
	Wave5WriteReg(instance->coreIdx, W5_RET_SUCCESS, 0);	//for debug

#ifdef USE_INTERRUPT_CALLBACK
	Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_CREATE_INSTANCE));
#endif
	Wave5BitIssueCommand(instance, W5_CREATE_INSTANCE);

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
	#endif

#ifdef USE_INTERRUPT_CALLBACK
	while (1) {
		int iRet = 0;

		if (wave5_interrupt != NULL)
			iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			#ifdef ENABLE_VDI_LOG
			if (instance->loggingEnable)
				vdi_log(instance->coreIdx, W5_CREATE_INSTANCE, 2);
			#endif
			return RETCODE_CODEC_EXIT;
		}

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			Wave5VpuClearInterrupt(instance->coreIdx, iRet);
			#endif

			if (iRet & (1<<W5_INT_CREATE_INSTANCE)) {
				break;
			}
			else {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				return RETCODE_FAILURE;
			}
		}
		#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
		#endif
	}
#else
	{
		int cnt = 0;

		while (WAVE5_IsBusy(instance->coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					#ifdef ENABLE_VDI_LOG
					if (instance->loggingEnable)
						vdi_log(instance->coreIdx, W5_CREATE_INSTANCE, 2);
					#endif

					WAVE5_dec_reset(NULL, (0 | (1<<16)));
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
			#ifdef ENABLE_VDI_LOG
				if (instance->loggingEnable)
					vdi_log(instance->coreIdx, W5_CREATE_INSTANCE, 2);
			#endif
				return RETCODE_CODEC_EXIT;
			}
		}
	}
#endif

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {           // FAILED for adding into VCPU QUEUE
		ret = RETCODE_FAILURE;
	}
#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0) {
		WAVE5_dec_reset(NULL, (0 | (1<<16)));
		return RETCODE_CODEC_EXIT;
	}
#endif

	return ret;
}

RetCode Wave5VpuDecInitSeq(CodecInst *instance)
{
	RetCode     ret = RETCODE_SUCCESS;
	DecInfo*    pDecInfo;
	Uint32      cmdOption = INIT_SEQ_NORMAL, bsOption;
	Uint32      regVal;

	if(instance == NULL)
		return RETCODE_INVALID_PARAM;

	pDecInfo = VPU_HANDLE_TO_DECINFO(instance);
	if(pDecInfo->thumbnailMode)
		cmdOption = INIT_SEQ_W_THUMBNAIL;


	/* Set attributes of bitstream buffer controller */
	bsOption = 0;
	switch(pDecInfo->openParam.bitstreamMode)
	{
		case BS_MODE_INTERRUPT:
			if(pDecInfo->seqInitEscape == TRUE)
				bsOption = BSOPTION_ENABLE_EXPLICIT_END;
			break;
		case BS_MODE_PIC_END:
			bsOption = BSOPTION_ENABLE_EXPLICIT_END;
			break;
		default:
			return RETCODE_INVALID_PARAM;
	}

	if(pDecInfo->streamEndflag == 1)
		bsOption = 3;

	Wave5WriteReg(instance->coreIdx, W5_BS_RD_PTR, pDecInfo->streamRdPtr);
	Wave5WriteReg(instance->coreIdx, W5_BS_WR_PTR, pDecInfo->streamWrPtr);

	Wave5WriteReg(instance->coreIdx, W5_BS_OPTION, (1<<31) | bsOption);

	Wave5WriteReg(instance->coreIdx, W5_COMMAND_OPTION, cmdOption);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_USER_MASK, pDecInfo->userDataEnable);

#ifdef USE_INTERRUPT_CALLBACK
	regVal  = (1<<W5_INT_INIT_SEQ);
	regVal |= (1<<W5_INT_DEC_PIC);
	regVal |= (1<<W5_INT_BSBUF_EMPTY);
	Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, regVal);
#endif

	Wave5BitIssueCommand(instance, W5_INIT_SEQ);

#ifdef USE_INTERRUPT_CALLBACK
	while (1) {
		int iRet = 0;

		if (wave5_interrupt != NULL)
			iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			#ifdef ENABLE_VDI_LOG
			if (instance->loggingEnable)
				vdi_log(instance->coreIdx, W5_INIT_SEQ, 2);
			#endif
			return RETCODE_CODEC_EXIT;
		}

	#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
	#else
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
	#endif
		if (iRet) {
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);

			if (iRet & (1<<INT_WAVE5_INIT_SEQ)) {
			#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(instance->coreIdx, iRet);
			#endif
				break;
			}
			else {
				if (iRet & (1<<INT_WAVE5_BSBUF_EMPTY)) {
					WAVE5_SetPendingInst(instance->coreIdx, 0);
					return RETCODE_FRAME_NOT_COMPLETE;
				}
				else {
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
					WAVE5_SetPendingInst(instance->coreIdx, 0);
					return RETCODE_FAILURE;
				}
			}
		}
	#ifdef ENABLE_FORCE_ESCAPE
		if (gWave5InitFirst_exit > 0) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}
	#endif
	}
#else
	{
		int cnt = 0;

		while (WAVE5_IsBusy(instance->coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					#ifdef ENABLE_VDI_LOG
					if (instance->loggingEnable)
						vdi_log(instance->coreIdx, W5_INIT_SEQ, 2);
					#endif
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
				#ifdef ENABLE_VDI_LOG
				if (instance->loggingEnable)
					vdi_log(instance->coreIdx, W5_INIT_SEQ, 2);
				#endif
				return RETCODE_CODEC_EXIT;
			}
		}
	}
#endif

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_QUEUE_STATUS);

	pDecInfo->instanceQueueCount = (regVal>>16)&0xff;
	pDecInfo->totalQueueCount    = (regVal & 0xffff);

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {           // FAILED for adding a command into VCPU QUEUE
		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);
		if ( regVal == WAVE5_QUEUEING_FAIL)
			ret = RETCODE_QUEUEING_FAILURE;
		else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
			ret =  RETCODE_MEMORY_ACCESS_VIOLATION;
		else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
			ret =  RETCODE_VPU_RESPONSE_TIMEOUT;
		else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL)
			ret = RETCODE_VLC_BUF_FULL;
		else
			ret = RETCODE_FAILURE;
	}

	return ret;
}

static void GetDecSequenceResult(CodecInst *instance, DecInitialInfo *info)
{
	DecInfo*   pDecInfo   = &instance->CodecInfo.decInfo;
	Uint32     regVal;
	Uint32     profileCompatibilityFlag;
	Uint32     left, right, top, bottom;
	Uint32     progressiveFlag, interlacedFlag;

	pDecInfo->streamRdPtr = info->rdPtr = ProductVpuDecGetRdPtr(instance);

	pDecInfo->frameDisplayFlag = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DISP_FLAG);
	/*regVal = Wave5ReadReg(instance->coreIdx, W4_BS_OPTION);
	pDecInfo->streamEndflag    = (regVal&0x02) ? TRUE : FALSE;*/

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SIZE);
	info->picWidth            = ( (regVal >> 16) & 0xffff );
	info->picHeight           = ( regVal & 0xffff );
	info->minFrameBufferCount = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_FRAMEBUF_NEEDED);
	info->frameBufDelay       = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_NUM_REORDER_DELAY);

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_CROP_LEFT_RIGHT);
	left   = (regVal >> 16) & 0xffff;
	right  = regVal & 0xffff;
	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_CROP_TOP_BOTTOM);
	top    = (regVal >> 16) & 0xffff;
	bottom = regVal & 0xffff;

	info->picCropRect.left   = left;
	info->picCropRect.right  = info->picWidth - right;
	info->picCropRect.top    = top;
	info->picCropRect.bottom = info->picHeight - bottom;

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_SEQ_PARAM);
	info->level                  = regVal & 0xff;
	interlacedFlag               = (regVal>>10) & 0x1;
	progressiveFlag              = (regVal>>11) & 0x1;
	profileCompatibilityFlag     = (regVal>>12)&0xff;
	info->profile                = (regVal >> 24)&0x1f;
	info->tier                   = (regVal >> 29)&0x01;
	info->maxSubLayers           = (regVal >> 21)&0x07;

	info->fRateNumerator         = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_FRAME_RATE_NR);
	info->fRateDenominator       = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_FRAME_RATE_DR);

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_COLOR_SAMPLE_INFO);
	info->lumaBitdepth           = (regVal>>0)&0x0f;
	info->chromaBitdepth         = (regVal>>4)&0x0f;
	info->chromaFormatIDC        = (regVal>>8)&0x0f;
	info->aspectRateInfo         = (regVal>>16)&0xff;
	info->isExtSAR               = (info->aspectRateInfo == 255 ? TRUE : FALSE);
	if (info->isExtSAR == TRUE)
		info->aspectRateInfo     = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ASPECT_RATIO);  /* [0:15] - vertical size, [16:31] - horizontal size */

	info->bitRate                = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_BIT_RATE);

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (instance->codecMode == W_HEVC_DEC) {
		/* Guessing Profile */
		if (info->profile == 0) {
			if ((profileCompatibilityFlag&0x06) == 0x06)        info->profile = 1;      /* Main profile */
			else if ((profileCompatibilityFlag&0x04) == 0x04)   info->profile = 2;      /* Main10 profile */
			else if ((profileCompatibilityFlag&0x08) == 0x08)   info->profile = 3;      /* Main Still Picture profile */
			else                                                info->profile = 1;      /* For old version HM */
		}

		if (progressiveFlag == 1 && interlacedFlag == 0)
			info->interlace = 0;
		else
			info->interlace = 1;
	}
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (instance->codecMode == W_VP9_DEC) {
		info->vp9PicInfo.color_space = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_VP9_COLOR_SPACE);
		info->vp9PicInfo.color_range = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_VP9_COLOR_RANGE);
		// VLOG(INFO, "%s color_space=0x%x, color_range=0x%x \n",
		// __FUNCTION__, info->vp9PicInfo.color_space, info->vp9PicInfo.color_range);
	}
	#endif

	return;
}

RetCode Wave5VpuDecGetSeqInfo(CodecInst *instance, DecInitialInfo *info)
{
	RetCode     ret = RETCODE_SUCCESS;
	Uint32      regVal, i;
	DecInfo*    pDecInfo;

	pDecInfo = VPU_HANDLE_TO_DECINFO(instance);

	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_ADDR_REPORT_BASE, pDecInfo->userDataBufAddr);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_REPORT_SIZE,      pDecInfo->userDataBufSize);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_REPORT_PARAM,     VPU_USER_DATA_ENDIAN&VDI_128BIT_ENDIAN_MASK);

	// Send QUERY cmd
	ret = SendQuery(instance, GET_RESULT);
	if (ret != RETCODE_SUCCESS) {
		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);
		if (regVal == WAVE5_RESULT_NOT_READY)
			return RETCODE_REPORT_NOT_READY;
		//else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
		//	return RETCODE_MEMORY_ACCESS_VIOLATION;
		//else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
		//	return RETCODE_VPU_RESPONSE_TIMEOUT;
		//else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL)
		//	return RETCODE_VLC_BUF_FULL;
		else
			return RETCODE_QUERY_FAILURE;
	}

	#ifdef ENABLE_VDI_LOG
	if (instance->loggingEnable)
		vdi_log(instance->coreIdx, W5_INIT_SEQ, 0);
	#endif

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_QUEUE_STATUS);

	pDecInfo->instanceQueueCount = (regVal>>16)&0xff;
	pDecInfo->totalQueueCount    = (regVal & 0xffff);

	if (Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODING_SUCCESS) != 1) {
		info->seqInitErrReason = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ERR_INFO);
		ret = RETCODE_FAILURE;
		if (info->seqInitErrReason == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
			ret = RETCODE_MEMORY_ACCESS_VIOLATION;
		else
			ret = RETCODE_FAILURE;
	}
	else {
		info->warnInfo = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_WARN_INFO);
	}

	// Get Sequence Info
	info->userDataSize   = 0;
	info->userDataNum    = 0;
	info->userDataBufFull= 0;
	info->userDataHeader = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_USERDATA_IDC);
	if (info->userDataHeader != 0) {
		regVal = info->userDataHeader;
		for (i=0; i<32; i++) {
			if (i == 1) {
				if (regVal & (1<<i))
					info->userDataBufFull = 1;
			}
			else {
				if (regVal & (1<<i))
					info->userDataNum++;
			}
		}
		info->userDataSize = pDecInfo->userDataBufSize;
	}

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DONE_INSTANCE_INFO);

	GetDecSequenceResult(instance, info);

	if (ret ==  RETCODE_SUCCESS) {
		if ((info->picWidth == 0) || (info->picHeight == 0)) {
			ret = RETCODE_INVALID_STRIDE;
			info->seqInitErrReason = WAVE5_ETCERR_INIT_SEQ_KEY_FRAME_NOT_FOUND;
		}
	}

	return ret;
}

RetCode Wave5VpuDecRegisterFramebuffer(CodecInst *inst, FrameBuffer *fbArr, TiledMapType mapType, Uint32 count)
{
	RetCode      ret = RETCODE_SUCCESS;
	DecInfo*     pDecInfo = &inst->CodecInfo.decInfo;
	DecInitialInfo* sequenceInfo = &inst->CodecInfo.decInfo.initialInfo;
	Int32        q, j, i, remain, idx;
	Int32        coreIdx, startNo, endNo;
	Uint32       regVal, cbcrInterleave, nv21, picSize;
	Uint32       endian, yuvFormat = 0;
	Uint32       addrY, addrCb, addrCr;
	Uint32       mvColSize, fbcYTblSize, fbcCTblSize;
	vpu_buffer_t vbBuffer;
	Uint32       stride;
	Uint32       colorFormat  = 0;
	Uint32       outputFormat = 0;
	Uint32       axiID;

	coreIdx        = inst->coreIdx;
	axiID          = pDecInfo->openParam.virtAxiID;
	cbcrInterleave = pDecInfo->openParam.cbcrInterleave;
	nv21           = pDecInfo->openParam.nv21;
	mvColSize      = fbcYTblSize = fbcCTblSize = 0;

	if (mapType == COMPRESSED_FRAME_MAP) {
		cbcrInterleave = 0;
		nv21           = 0;

		if ((inst->codecMode != W_HEVC_DEC) && (inst->codecMode != W_VP9_DEC))
			return RETCODE_NOT_SUPPORTED_FEATURE;	/* Unknown codec */

		if (pDecInfo->openParam.FullSizeFB) {
			#if TCC_HEVC___DEC_SUPPORT == 1
			if (inst->codecMode == W_HEVC_DEC) {
				mvColSize   = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight);
				fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight);
				fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight);
			}
			#endif

			#if TCC_VP9____DEC_SUPPORT == 1
			if (inst->codecMode == W_VP9_DEC) {
				mvColSize   = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight);
				fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), WAVE5_ALIGN64(pDecInfo->openParam.maxheight));
				fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->openParam.maxwidth), WAVE5_ALIGN64(pDecInfo->openParam.maxheight));
			}
			#endif

			picSize = (WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)<<16)|(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight));
			DLOGD("FullSizeFB maxwidth:%d, maxheight:%d", pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight);
		}
		else {
			#if TCC_HEVC___DEC_SUPPORT == 1
			if (inst->codecMode == W_HEVC_DEC) {
				mvColSize   = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
				fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
				fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
			}
			#endif

			#if TCC_VP9____DEC_SUPPORT == 1
			if (inst->codecMode == W_VP9_DEC) {
				mvColSize   = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
				fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
				fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
			}
			#endif

			picSize = (WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)<<16)|(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight));
			DLOGD("initialInfo picWidth:%d, picHeight:%d",pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
		}

		DLOGD("mvcolsize:%u, fbcYTblSize:%u, fbcCTblSize:%u, picSize:%u", mvColSize, fbcYTblSize, fbcCTblSize, picSize);

		mvColSize     = WAVE5_ALIGN16(mvColSize);
		vbBuffer.size = ((mvColSize+4095)&~4095)+4096;   /* 4096 is a margin */
		mvColSize = ((mvColSize+4095)&~4095)+4096;

		fbcYTblSize   = WAVE5_ALIGN16(fbcYTblSize);
		vbBuffer.size = ((fbcYTblSize+4095)&~4095)+4096;
		fbcYTblSize = ((fbcYTblSize+4095)&~4095)+4096;

		fbcCTblSize   = WAVE5_ALIGN16(fbcCTblSize);
		vbBuffer.size = ((fbcCTblSize+4095)&~4095)+4096;
		fbcCTblSize = ((fbcCTblSize+4095)&~4095)+4096;

	#ifndef TEST_COMPRESSED_FBCTBL_INDEX_SET
		pDecInfo->vbFbcYTbl[0].phys_addr = fbArr[0].bufY + pDecInfo->framebufSizeCompressedLinear * count;
		pDecInfo->vbFbcCTbl[0].phys_addr = pDecInfo->vbFbcYTbl[0].phys_addr + ((fbcYTblSize*count+4095)&~4095)+4096;
		pDecInfo->vbMV[0].phys_addr = pDecInfo->vbFbcCTbl[0].phys_addr + ((fbcCTblSize*count+4095)&~4095)+4096;

		for (i = 1 ; i < count ; i++) {
			pDecInfo->vbMV[i].phys_addr = pDecInfo->vbMV[i-1].phys_addr + mvColSize;
			pDecInfo->vbFbcYTbl[i].phys_addr = pDecInfo->vbFbcYTbl[i-1].phys_addr + fbcYTblSize;
			pDecInfo->vbFbcCTbl[i].phys_addr = pDecInfo->vbFbcCTbl[i-1].phys_addr + fbcCTblSize;
		}
	#else
		for (i = 0 ; i < count ; i++) {
			pDecInfo->vbFbcYTbl[i].phys_addr = fbArr[i].bufFBCY;
			pDecInfo->vbFbcCTbl[i].phys_addr = fbArr[i].bufFBCC;
			pDecInfo->vbMV[i].phys_addr = fbArr[i].bufMVCOL;
			DLOGD("[%d] vbFbcYTbl:0x%x, vbFbcCTbl:0x%x, vbMV:0x%x", i, pDecInfo->vbFbcYTbl[i].phys_addr, pDecInfo->vbFbcCTbl[i].phys_addr, pDecInfo->vbMV[i].phys_addr);
		}
	#endif
	}
	else {
		picSize = (WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)<<16)|(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight));
		if (pDecInfo->scalerEnable == TRUE) {
			picSize = (pDecInfo->scaleWidth << 16) | (pDecInfo->scaleHeight);
		}
	}

	switch (fbArr[0].endian)
	{
		case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
		case VDI_BIG_ENDIAN:          endian = 0x0f; break;
		case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
		case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
		default:                      endian = fbArr[0].endian; break;
	}
	endian = endian&0xf;

	DLOGD("picSize:%u", picSize);
	Wave5WriteReg(coreIdx, W5_PIC_SIZE, picSize);

	yuvFormat = 0; /* YUV420 8bit */
	if (mapType == LINEAR_FRAME_MAP) {
		BOOL   justified = WTL_RIGHT_JUSTIFIED;
		Uint32 formatNo  = WTL_PIXEL_8BIT;
		switch (pDecInfo->wtlFormat) {
		case FORMAT_420_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
			break;
		case FORMAT_420_P10_16BIT_LSB:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
			break;
		case FORMAT_420_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_32BIT;
			break;
		case FORMAT_420_P10_32BIT_LSB:
		case FORMAT_422_P10_32BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_32BIT;
			break;
		default:
			break;
		}
		yuvFormat = justified<<2 | formatNo;
	}

	stride = fbArr[0].stride;
	DLOGD("mapType:%d, stride:%u, chFbcFrameIdx:%d", mapType, stride, pDecInfo->chFbcFrameIdx);
	if (mapType == COMPRESSED_FRAME_MAP) {
		if ( pDecInfo->chFbcFrameIdx != -1 )
			stride = fbArr[pDecInfo->chFbcFrameIdx].stride;
	} else {
		if ( pDecInfo->chBwbFrameIdx != -1 )
			stride = fbArr[pDecInfo->chBwbFrameIdx].stride;
	}
	DLOGD("mapType:%d, stride:%u, chFbcFrameIdx:%d", mapType, stride, pDecInfo->chFbcFrameIdx);

	if (mapType == LINEAR_FRAME_MAP) {
		switch (pDecInfo->wtlFormat) {
		case FORMAT_422:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 0;
			outputFormat |= (nv21 << 1);
			outputFormat |= (cbcrInterleave << 0);
			break;
		case FORMAT_YUYV:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
			colorFormat   = 1;
			outputFormat  = 4;
			break;
		case FORMAT_YVYU:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 6;
			break;
		case FORMAT_UYVY:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
			colorFormat   = 1;
			outputFormat  = 5;
			break;
		case FORMAT_VYUY:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 7;
			break;
		default:
			colorFormat   = 0;
			outputFormat  = 0;
			outputFormat |= (nv21 << 1);
			outputFormat |= (cbcrInterleave << 0);
			break;
		}
	}

	if (pDecInfo->enableAfbce) {
		BOOL   justified;
		Uint32 formatNo;
		if (pDecInfo->afbceFormat == 1) { /* AFBCE_FORMAT_420 */
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_8BIT;
		}
		else { /* AFBCE_FORMAT_420_P10_16BIT_LSB */
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
		}
		yuvFormat = justified<<2 | formatNo;
	}

	regVal =
		((mapType == LINEAR_FRAME_MAP) ? (pDecInfo->enableAfbce << 30) : 0)  |
		((mapType == LINEAR_FRAME_MAP) ? (pDecInfo->scalerEnable << 29) : 0) |
		((mapType == LINEAR_FRAME_MAP) << 28)   |
		(axiID << 24)                           |
		(1<< 23)                                |   /* PIXEL ORDER in 128bit. first pixel in low address */
		(yuvFormat     << 20)                   |
		(colorFormat  << 19)                    |
		(outputFormat << 16)                    |
		(stride);

	DLOGD("regVal:%u, maptype:%d, axiID:%d, yuvFormat:%d, colorFormat:%d, outputFormat:%d, stride:%d", regVal, mapType, axiID, yuvFormat, colorFormat, outputFormat, stride);
	Wave5WriteReg(coreIdx, W5_COMMON_PIC_INFO, regVal);

	remain = count;
	q      = (remain+7)/8;
	idx    = 0;
	for (j=0; j<q; j++) {
		regVal = (pDecInfo->openParam.fbc_mode<<20)|(endian<<16) | (j==q-1)<<4 | ((j==0)<<3) ;
		Wave5WriteReg(coreIdx, W5_SFB_OPTION, regVal);
		startNo = j*8;
		endNo   = startNo + (remain>=8 ? 8 : remain) - 1;

		Wave5WriteReg(coreIdx, W5_SET_FB_NUM, (startNo<<8)|endNo);

		DLOGD("q:%d, j:%d, regVal:%d, fb_num:%d", q, j, ((startNo<<8)|endNo));

		for (i=0; i<8 && i<remain; i++) {
			if (mapType == LINEAR_FRAME_MAP && pDecInfo->openParam.cbcrOrder == CBCR_ORDER_REVERSED) {
				addrY  = fbArr[i+startNo].bufY;
				addrCb = fbArr[i+startNo].bufCr;
				addrCr = fbArr[i+startNo].bufCb;
			}
			else {
				addrY  = fbArr[i+startNo].bufY;
				addrCb = fbArr[i+startNo].bufCb;
				addrCr = fbArr[i+startNo].bufCr;
			}
			Wave5WriteReg(coreIdx, W5_ADDR_LUMA_BASE0  + (i<<4), addrY);
			Wave5WriteReg(coreIdx, W5_ADDR_CB_BASE0    + (i<<4), addrCb);
			if (mapType == COMPRESSED_FRAME_MAP) {
				DLOGD("compressed [%d] addrY:0x%x, addrCb:0x%x, y table:0x%x, c table:0x%x, mvcol:0x%x", i, addrY, addrCb, pDecInfo->vbFbcYTbl[idx].phys_addr, pDecInfo->vbFbcCTbl[idx].phys_addr, pDecInfo->vbMV[idx].phys_addr);
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_Y_OFFSET0 + (i<<4), pDecInfo->vbFbcYTbl[idx].phys_addr); /* Luma FBC offset table */
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_C_OFFSET0 + (i<<4), pDecInfo->vbFbcCTbl[idx].phys_addr);        /* Chroma FBC offset table */
				Wave5WriteReg(coreIdx, W5_ADDR_MV_COL0  + (i<<2), pDecInfo->vbMV[idx].phys_addr);
			}
			else {
				DLOGD("linear [%d] addrY:0x%x, addrCb:0x%x, addrCr:0x%x", i, addrY, addrCb, addrCr);
				Wave5WriteReg(coreIdx, W5_ADDR_CR_BASE0 + (i<<4), addrCr);
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_C_OFFSET0 + (i<<4), 0);
				Wave5WriteReg(coreIdx, W5_ADDR_MV_COL0  + (i<<2), 0);
			}
			idx++;
		}
		remain -= i;

	#ifdef USE_INTERRUPT_CALLBACK
		#if TCC_HEVC___DEC_SUPPORT == 1
		if (inst->codecMode == W_HEVC_DEC) {	//HEVC side effect Test then it's necessary to change
			unsigned int regVal_int;

			regVal_int  = (1<<W5_INT_SET_FRAMEBUF);
			regVal_int |= (1<<W5_INT_DEC_PIC);
			Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE, regVal_int);
		}
		#endif
	#endif

		Wave5BitIssueCommand(inst, W5_SET_FB);
	#if TCC_VP9____DEC_SUPPORT == 1
		if (inst->codecMode == W_VP9_DEC) {
			int cnt = 0;

			while(WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if( cnt > VPU_BUSY_CHECK_USLEEP_CNT )
						return RETCODE_CODEC_EXIT;
				}
			#endif
				if(cnt > VPU_BUSY_CHECK_COUNT)
					return RETCODE_CODEC_EXIT;
			}
		}
	#endif
	#if TCC_HEVC___DEC_SUPPORT == 1
		if (inst->codecMode == W_HEVC_DEC) {
			//HEVC side effect Test then it's necessary to change

		#ifdef USE_INTERRUPT_CALLBACK
			while (1) {
				int iRet = 0;

				if (wave5_interrupt != NULL)
					iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));

				if (iRet == RETCODE_CODEC_EXIT) 	{
					WAVE5_dec_reset(NULL, (0 | (1<<16)));
					return RETCODE_CODEC_EXIT;
				}

				#ifndef TEST_INTERRUPT_REASON_REGISTER
				iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
				#else
				iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
				#endif
				if (iRet) {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					#ifndef TEST_INTERRUPT_REASON_REGISTER
					Wave5VpuClearInterrupt(coreIdx, iRet);
					#endif

					if (iRet & (1<<INT_WAVE5_SET_FRAMEBUF)) {
						break;
					}
					else {
						Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
						Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
						return RETCODE_FAILURE;
					}
				}
			}
		#else
			{
				int cnt = 0;
				while(WAVE5_IsBusy(coreIdx)) {
					cnt++;
				#ifdef ENABLE_WAIT_POLLING_USLEEP
					if (wave5_usleep != NULL) {
						wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
						if (cnt > VPU_BUSY_CHECK_USLEEP_CNT)
							return RETCODE_CODEC_EXIT;
					}
				#endif
					if(cnt > VPU_BUSY_CHECK_COUNT)
						return RETCODE_CODEC_EXIT;
				}
			}
		#endif
		}
	#endif
	}

	regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
	if (regVal == 0)
		return RETCODE_FAILURE;

	if (pDecInfo->openParam.FullSizeFB) {
		Uint32 profile, level;
		if (pDecInfo->openParam.maxbit > 8)
			profile = VP9_PROFILE_2;
		else
			profile = VP9_PROFILE_0;
		level = 30;
		if (ConfigSecAXIWave(coreIdx, inst->codecMode,
			&pDecInfo->secAxiInfo, pDecInfo->openParam.maxwidth, pDecInfo->openParam.maxheight,
			profile, level) == 0) {
				return RETCODE_INSUFFICIENT_SECAXI_BUF;
		}
	}
	else {
		if (ConfigSecAXIWave(coreIdx, inst->codecMode,
			&pDecInfo->secAxiInfo, pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight,
			sequenceInfo->profile, sequenceInfo->level) == 0) {
				return RETCODE_INSUFFICIENT_SECAXI_BUF;
		}
	}
	return ret;
}

#ifdef ADD_FBREG_2
RetCode Wave5VpuDecRegisterFramebuffer2(CodecInst *inst, FrameBuffer *fbArr, TiledMapType mapType, Uint32 count)
{
	RetCode      ret = RETCODE_SUCCESS;
	DecInfo*     pDecInfo = &inst->CodecInfo->decInfo;
	DecInitialInfo* sequenceInfo = &inst->CodecInfo->decInfo.initialInfo;
	Int32        q, j, i, remain, idx;
	Int32        coreIdx, startNo, endNo;
	Uint32       regVal, cbcrInterleave, nv21, picSize;
	Uint32       endian, yuvFormat = 0;
	Uint32       addrY, addrCb, addrCr;
	Uint32       mvColSize, fbcYTblSize, fbcCTblSize;
	vpu_buffer_t vbBuffer;
	Uint32       stride;
	Uint32       colorFormat  = 0;
	Uint32       outputFormat = 0;
	Uint32       axiID;

	coreIdx        = inst->coreIdx;
	axiID          = pDecInfo->openParam.virtAxiID;
	cbcrInterleave = pDecInfo->openParam.cbcrInterleave;
	nv21           = pDecInfo->openParam.nv21;
	mvColSize      = fbcYTblSize = fbcCTblSize = 0;
	if (mapType == COMPRESSED_FRAME_MAP) {
		cbcrInterleave = 0;
		nv21           = 0;

		if ((inst->codecMode != W_HEVC_DEC) && (inst->codecMode != W_VP9_DEC))
			return RETCODE_NOT_SUPPORTED_FEATURE;	/* Unknown codec */

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (inst->codecMode == W_HEVC_DEC) {
			mvColSize = WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_VP9____DEC_SUPPORT == 1
		if(inst->codecMode == W_VP9_DEC) {
			mvColSize = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
		}
		#endif

		mvColSize = WAVE5_ALIGN16(mvColSize);
		vbBuffer.size = ((mvColSize*count+4095)&~4095)+4096;   /* 4096 is a margin */


		#if TCC_HEVC___DEC_SUPPORT == 1
		if (inst->codecMode == W_HEVC_DEC){
			fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_VP9____DEC_SUPPORT == 1
		//VP9 Decoded size : 64 aligned.
		if (inst->codecMode == W_VP9_DEC) {
			fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
		}
		#endif

		fbcYTblSize = WAVE5_ALIGN16(fbcYTblSize);
		vbBuffer.size = ((fbcYTblSize+4095)&~4095)+4096;

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (inst->codecMode == W_HEVC_DEC) {
			fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
		}
		#endif

		#if TCC_VP9____DEC_SUPPORT == 1
		if (inst->codecMode == W_VP9_DEC) {
			fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(WAVE5_ALIGN64(pDecInfo->initialInfo.picWidth), WAVE5_ALIGN64(pDecInfo->initialInfo.picHeight));
		}
		#endif

		fbcCTblSize = WAVE5_ALIGN16(fbcCTblSize);
		vbBuffer.size = ((fbcCTblSize+4095)&~4095)+4096;

		picSize = (WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)<<16)|(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight));
	}
	else {
		picSize = (WAVE5_ALIGN8(pDecInfo->initialInfo.picWidth)<<16)|(WAVE5_ALIGN8(pDecInfo->initialInfo.picHeight));
		if (pDecInfo->scalerEnable == TRUE) {
			picSize = (pDecInfo->scaleWidth << 16) | (pDecInfo->scaleHeight);
		}
	}

	switch (fbArr[0].endian)
	{
		case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
		case VDI_BIG_ENDIAN:          endian = 0x0f; break;
		case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
		case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
		default:                      endian = fbArr[0].endian; break;
	}
	endian = endian&0xf;

	Wave5WriteReg(coreIdx, W5_PIC_SIZE, picSize);

	yuvFormat = 0; /* YUV420 8bit */
	if (mapType == LINEAR_FRAME_MAP) {
		BOOL   justified = WTL_RIGHT_JUSTIFIED;
		Uint32 formatNo  = WTL_PIXEL_8BIT;
		switch (pDecInfo->wtlFormat) {
		case FORMAT_420_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
			break;
		case FORMAT_420_P10_16BIT_LSB:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
			break;
		case FORMAT_420_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_32BIT;
			break;
		case FORMAT_420_P10_32BIT_LSB:
		case FORMAT_422_P10_32BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_32BIT;
			break;
		default:
			break;
		}
		yuvFormat = justified<<2 | formatNo;
	}

	stride = fbArr[0].stride;
	if (mapType == COMPRESSED_FRAME_MAP) {
		if ( pDecInfo->chFbcFrameIdx != -1 )
			stride = fbArr[pDecInfo->chFbcFrameIdx].stride;
	} else {
		if ( pDecInfo->chBwbFrameIdx != -1 )
			stride = fbArr[pDecInfo->chBwbFrameIdx].stride;
	}

	if (mapType == LINEAR_FRAME_MAP) {
		switch (pDecInfo->wtlFormat) {
		case FORMAT_422:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 0;
			outputFormat |= (nv21 << 1);
			outputFormat |= (cbcrInterleave << 0);
			break;
		case FORMAT_YUYV:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
			colorFormat   = 1;
			outputFormat  = 4;
			break;
		case FORMAT_YVYU:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 6;
			break;
		case FORMAT_UYVY:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
			colorFormat   = 1;
			outputFormat  = 5;
			break;
		case FORMAT_VYUY:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
			colorFormat   = 1;
			outputFormat  = 7;
			break;
		default:
			colorFormat   = 0;
			outputFormat  = 0;
			outputFormat |= (nv21 << 1);
			outputFormat |= (cbcrInterleave << 0);
			break;
		}
	}

	if (pDecInfo->enableAfbce) {
		BOOL   justified;
		Uint32 formatNo;
		if (pDecInfo->afbceFormat == 1) { /* AFBCE_FORMAT_420 */
			justified = WTL_RIGHT_JUSTIFIED;
			formatNo  = WTL_PIXEL_8BIT;
		}
		else { /* AFBCE_FORMAT_420_P10_16BIT_LSB */
			justified = WTL_LEFT_JUSTIFIED;
			formatNo  = WTL_PIXEL_16BIT;
		}
		yuvFormat = justified<<2 | formatNo;
	}

	regVal =
		((mapType == LINEAR_FRAME_MAP) ? (pDecInfo->enableAfbce << 30) : 0)  |
		((mapType == LINEAR_FRAME_MAP) ? (pDecInfo->scalerEnable << 29) : 0) |
		((mapType == LINEAR_FRAME_MAP) << 28)   |
		(axiID << 24)                           |
		(1<< 23)                                |   /* PIXEL ORDER in 128bit. first pixel in low address */
		(yuvFormat     << 20)                   |
		(colorFormat  << 19)                    |
		(outputFormat << 16)                    |
		(stride);

	Wave5WriteReg(coreIdx, W5_COMMON_PIC_INFO, regVal);

	remain = count;
	q      = (remain+7)/8;
	idx    = 0;
	for (j=0; j<q; j++) {
		regVal = (pDecInfo->openParam.fbc_mode<<20)|(endian<<16) | (j==q-1)<<4 | ((j==0)<<3) ;
		Wave5WriteReg(coreIdx, W5_SFB_OPTION, regVal);
		startNo = j*8;
		endNo   = startNo + (remain>=8 ? 8 : remain) - 1;

		Wave5WriteReg(coreIdx, W5_SET_FB_NUM, (startNo<<8)|endNo);

		for (i=0; i<8 && i<remain; i++) {
			if (mapType == LINEAR_FRAME_MAP && pDecInfo->openParam.cbcrOrder == CBCR_ORDER_REVERSED) {
				addrY  = fbArr[i+startNo].bufY;
				addrCb = fbArr[i+startNo].bufCr;
				addrCr = fbArr[i+startNo].bufCb;
			}
			else {
				addrY  = fbArr[i+startNo].bufY;
				addrCb = fbArr[i+startNo].bufCb;
				addrCr = fbArr[i+startNo].bufCr;
			}
			Wave5WriteReg(coreIdx, W5_ADDR_LUMA_BASE0  + (i<<4), addrY);
			Wave5WriteReg(coreIdx, W5_ADDR_CB_BASE0    + (i<<4), addrCb);

			if (mapType == COMPRESSED_FRAME_MAP) {
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_Y_OFFSET0 + (i<<4), pDecInfo->vbFbcYTbl[idx].phys_addr); /* Luma FBC offset table */
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_C_OFFSET0 + (i<<4), pDecInfo->vbFbcCTbl[idx].phys_addr);        /* Chroma FBC offset table */
				Wave5WriteReg(coreIdx, W5_ADDR_MV_COL0  + (i<<2), pDecInfo->vbMV[idx].phys_addr);
			}
			else {
				Wave5WriteReg(coreIdx, W5_ADDR_CR_BASE0 + (i<<4), addrCr);
				Wave5WriteReg(coreIdx, W5_ADDR_FBC_C_OFFSET0 + (i<<4), 0);
				Wave5WriteReg(coreIdx, W5_ADDR_MV_COL0  + (i<<2), 0);
			}
			idx++;
		}
		remain -= i;

	#ifdef USE_INTERRUPT_CALLBACK
		Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_SET_FRAMEBUF));
	#endif

		Wave5BitIssueCommand(inst, W5_SET_FB);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;
			//Int32  reason = -1;

			if (wave5_interrupt != NULL)
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));

			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset( pVpu, ( 0 | (1<<16) ) );
				return RETCODE_CODEC_EXIT;
			}

			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(coreIdx, iRet);
				#endif

				if(iRet & (1<<INT_WAVE5_SET_FRAMEBUF)) {
					break;
				}
				else {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;

			while(WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT)
						return RETCODE_CODEC_EXIT;
				}
			#endif
				if(cnt > VPU_BUSY_CHECK_COUNT)
					return RETCODE_CODEC_EXIT;
			}
		}
	#endif
    }

	regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
	if (regVal == 0) {
		return RETCODE_FAILURE;
	}

	if (ConfigSecAXIWave(coreIdx, inst->codecMode,
		&pDecInfo->secAxiInfo, pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight,
		sequenceInfo->profile, sequenceInfo->level) == 0) {
			return RETCODE_INSUFFICIENT_RESOURCE;
	}

	return ret;
}
#endif

RetCode Wave5VpuDecode(CodecInst *instance, DecParam *option)
{
	Uint32      modeOption = DEC_PIC_NORMAL,  bsOption, regVal;
	DecOpenParam*   pOpenParam;
	Int32       forceLatency = -1;
	RetCode     ret = 0;
	DecInfo*    pDecInfo = &instance->CodecInfo.decInfo;
	int waitInterruptMode = 0; //0: polling mode 1: interrupt mode

	pOpenParam = &pDecInfo->openParam;


	if (option->vp9SuperFrameParseEnable == TRUE) {
		if (pOpenParam->bitstreamMode != BS_MODE_PIC_END) {
			return RETCODE_INVALID_PARAM;
		}
	}

	if (pDecInfo->thumbnailMode) {
		modeOption = DEC_PIC_W_THUMBNAIL;
	}
	else if (option->skipframeMode) {
		switch (option->skipframeMode) {
		case 1:
			modeOption   = SKIP_NON_IRAP;
		#ifdef SUPPORT_NON_IRAP_DELAY_ENABLE
			if (option->skipframeDelayEnable != 1)
				forceLatency = 0;
			//if (option->skipframeDelayEnable == 1)   forceLatency = -1;
			//else		                           forceLatency = 0;
		#else
			forceLatency = 0;
		#endif
			break;
		case 2:
			modeOption = SKIP_NON_REF_PIC;
			break;
		default:
			// skip off
			break;
		}
	}

	if (pDecInfo->targetSubLayerId < (pDecInfo->initialInfo.maxSubLayers-1))
		modeOption = SKIP_TEMPORAL_LAYER;

	if (option->craAsBlaFlag == TRUE)
		modeOption |= (1<<1);

	// set disable reorder
	if (pDecInfo->reorderEnable == FALSE)
		forceLatency = 0;

	// Bandwidth optimization
	modeOption |= (pDecInfo->openParam.bwOptimization<< 31);

	#if TCC_VP9____DEC_SUPPORT == 1
	if (instance->codecMode == W_VP9_DEC) {
		if (option->vp9SuperFrameParseEnable == TRUE)
			modeOption |= (1<<8);
	}
	#endif

	/* Set attributes of bitstream buffer controller */
	bsOption = 0;
	regVal = 0;
	switch (pOpenParam->bitstreamMode) {
	case BS_MODE_INTERRUPT:
		bsOption = 0;
		break;
	case BS_MODE_PIC_END:
		bsOption = BSOPTION_ENABLE_EXPLICIT_END;
		break;
	default:
		return RETCODE_INVALID_PARAM;
	}

	Wave5WriteReg(instance->coreIdx, W5_BS_RD_PTR,     pDecInfo->streamRdPtr);
	Wave5WriteReg(instance->coreIdx, W5_BS_WR_PTR,     pDecInfo->streamWrPtr);

	if (pDecInfo->streamEndflag == 1)
		bsOption = 3;   // (streamEndFlag<<1) | EXPLICIT_END

	if (pOpenParam->bitstreamMode == BS_MODE_PIC_END)
		bsOption |= (1<<31);

	Wave5WriteReg(instance->coreIdx, W5_BS_OPTION, bsOption);

	/* Secondary AXI */
	regVal = (pDecInfo->secAxiInfo.u.wave.useBitEnable<<0)    |
             (pDecInfo->secAxiInfo.u.wave.useIpEnable<<9)     |
             (pDecInfo->secAxiInfo.u.wave.useLfRowEnable<<15);
	regVal |= (pDecInfo->secAxiInfo.u.wave.useSclEnable<<5);

	if (pDecInfo->secAxiInfo.u.wave.useSclEnable == TRUE) {
		switch (pDecInfo->wtlFormat) {
		case FORMAT_YUYV:
		case FORMAT_YVYU:
		case FORMAT_UYVY:
		case FORMAT_VYUY:
			regVal |= (pDecInfo->secAxiInfo.u.wave.useSclPackedModeEnable<<6);
			break;
		default:
			break;
		}
	}
	Wave5WriteReg(instance->coreIdx, W5_USE_SEC_AXI,  regVal);


	DLOGV("skipframeMode(%d),skipframeDelayEnable(%d),reorderEnable(%d),forceLatency(%d)",
		option->skipframeMode,
		option->skipframeDelayEnable,
		pDecInfo->reorderEnable,
		forceLatency);

	/* Set attributes of User buffer */
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_USER_MASK, pDecInfo->userDataEnable);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_VCORE_LIMIT, 1);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_TEMPORAL_ID_PLUS1, pDecInfo->targetSubLayerId+1);
	Wave5WriteReg(instance->coreIdx, W5_CMD_SEQ_CHANGE_ENABLE_FLAG, pDecInfo->seqChangeMask);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_FORCE_FB_LATENCY_PLUS1, forceLatency+1);
	Wave5WriteReg(instance->coreIdx, W5_COMMAND_OPTION, modeOption);

	waitInterruptMode = 1;

	if (waitInterruptMode) //0: polling mode 1: interrupt mode
		Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_DEC_PIC)  | (1<<W5_INT_BSBUF_EMPTY)  );

	Wave5BitIssueCommand(instance, W5_DEC_PIC);

	waitInterruptMode = 0;

#if 1
	if (waitInterruptMode == 0) {	//0: polling mode 1: interrupt mode
		int cnt = 0;
		{
			Uint32 busy_flag;
			//int i;

		#ifdef ENABLE_WAIT_POLLING_USLEEP_AFTER
			for (cnt=0 ; cnt< ENABLE_WAIT_POLLING_USLEEP_AFTER; cnt++) {
				busy_flag = Wave5ReadReg(instance->coreIdx, W5_VPU_BUSY_STATUS);
				if( busy_flag == 0 )
					break;
			}

			if (busy_flag != 0) {
				for( cnt=0 ; cnt< ENABLE_WAIT_POLLING_USLEEP_AFTER; cnt++) {
					busy_flag = Wave5ReadReg(instance->coreIdx, W5_VPU_BUSY_STATUS);
					if (busy_flag == 0) {
						if (Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON) & (1<<INT_WAVE5_DEC_PIC)) {
							pDecInfo->fb_cleared = 1;
						}
						break;
					}

					if (wave5_usleep != NULL) {
						wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
						if( cnt > VPU_BUSY_CHECK_USLEEP_CNT )
							return RETCODE_CODEC_EXIT;
					}
				}
			}

			if (busy_flag != 0) {
				cnt = 0;
				while (1) {
					busy_flag = Wave5ReadReg(instance->coreIdx, W5_VPU_BUSY_STATUS);
					if( busy_flag == 0 )
						break;
				#ifdef ENABLE_FORCE_ESCAPE
					if( gWave5InitFirst_exit > 0 )
						return RETCODE_CODEC_EXIT;
				#endif

					if (cnt > 0x7FFFFFF) {
						#ifdef ENABLE_VDI_LOG
						if (instance->loggingEnable)
							vdi_log(instance->coreIdx, W5_DEC_PIC, 2);
						#endif
						return RETCODE_CODEC_EXIT;
					}
				}
			}
		#else //#ifdef ENABLE_WAIT_POLLING_USLEEP_AFTER
			while (1) {
				busy_flag = Wave5ReadReg(instance->coreIdx, W5_VPU_BUSY_STATUS);
				if( busy_flag == 0 )
					break;

				cnt++;
				#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT)
						return RETCODE_CODEC_EXIT;
				}
				#endif
				if (cnt > 0x7FFFFFF) {
					#ifdef ENABLE_VDI_LOG
					if (instance->loggingEnable)
						vdi_log(instance->coreIdx, W5_DEC_PIC, 2);
					#endif
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif //#ifdef ENABLE_WAIT_POLLING_USLEEP_AFTER
		}
	}
#else
	#ifdef USE_INTERRUPT_CALLBACK
	if (1) {
		while (1) {
			int iRet = 0;

			if (Wave5ReadReg(instance->coreIdx, W5_VCPU_CUR_PC) == 0x0) {
				//WAVE5_dec_reset( pVpu, ( 0 | (1<<16) ) );
				return RETCODE_CODEC_EXIT;
			}
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				WAVE5_ClearInterrupt(instance->coreIdx);
				#endif

				if (iRet & (1<<INT_WAVE5_DEC_PIC)) {
					break;
				}
				else {
					if (iRet & (1<<INT_WAVE5_BSBUF_EMPTY)) {
						if (instance->CodecInfo.decInfo.openParam.bitstreamMode != BS_MODE_INTERRUPT) {
							WAVE5_SetPendingInst(instance->coreIdx, 0);
							return RETCODE_FRAME_NOT_COMPLETE;
						}
						else {
							int read_ptr, write_ptr, room;

							ret = WAVE5_DecGetBitstreamBuffer(instance, (PhysicalAddress *)&read_ptr, (PhysicalAddress *)&write_ptr, (int *)&room);
							if( ret != RETCODE_SUCCESS )
								return ret;
							instance->CodecInfo.decInfo.prev_streamWrPtr = write_ptr;

							WAVE5_SetPendingInst(instance->coreIdx, instance);
							instance->CodecInfo.decInfo.m_iPendingReason = (1<<INT_WAVE5_BSBUF_EMPTY);
							return RETCODE_INSUFFICIENT_BITSTREAM;
						}
					}
					else if (iRet & (1<<INT_WAVE5_FLUSH_INSTANCE)) {
						break;
					}
					else if(iRet & (	 (1<<INT_WAVE5_INIT_SEQ)
//										|(1<<INT_WAVE_DEC_PIC_HDR)
										|(1<<INT_WAVE5_SET_FRAMEBUF))) {
						return RETCODE_FAILURE;
					}
				}
			}
		}
	}
	#else
	{
		int cnt = 0;
		{
			Uint32 int_reason;
			while (1) {
			#ifndef TEST_INTERRUPT_REASON_REGISTER
				int_reason = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
			#else
				int_reason = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
			#endif
				if (int_reason == 8) {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, int_reason);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
					WAVE5_ClearInterrupt(coreIdx);
				#endif
					break;
				}
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						if (int_reason) {
							Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, int_reason);
							Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
						#ifndef TEST_INTERRUPT_REASON_REGISTER
							WAVE5_ClearInterrupt(coreIdx);
						#endif
						}
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > VPU_BUSY_CHECK_COUNT) {
					if (int_reason) {
						Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, int_reason);
						Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					#ifndef TEST_INTERRUPT_REASON_REGISTER
						WAVE5_ClearInterrupt(coreIdx);
					#endif
					}
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	}
	#endif
#endif

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_QUEUE_STATUS);

	pDecInfo->instanceQueueCount = (regVal>>16)&0xff;
	pDecInfo->totalQueueCount    = (regVal & 0xffff);

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {           // FAILED for adding a command into VCPU QUEUE
		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);
		if ( regVal == WAVE5_QUEUEING_FAIL)
			return RETCODE_QUEUEING_FAILURE;
		else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
			return RETCODE_MEMORY_ACCESS_VIOLATION;
		else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL)
			return RETCODE_VLC_BUF_FULL;
		else if (regVal == WAVE5_FIND_SUPER_FRAME) {
			int i;
			pDecInfo->superframe.nframes = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SUPERFRAME_NUM);
			for (i=0; i <pDecInfo->superframe.nframes; i++) {
				pDecInfo->superframe.frames[i] = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SUPERFRAME_0_ADDR + (i*2*4));
				pDecInfo->superframe.frameSize[i] = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SUPERFRAME_0_SIZE + (i*2*4));
			}
			pDecInfo->superframe.currentIndex = 0;
			pDecInfo->superframe.doneIndex = pDecInfo->superframe.nframes;
			return RETCODE_FIND_SUPERFRAME;
		}
		else {
			return RETCODE_FAILURE;
		}
	}

	return RETCODE_SUCCESS;
}

#ifdef DBG_DECSUCCESS
static int logCnt = 0;
#endif
RetCode Wave5VpuDecGetResult(CodecInst* instance, DecOutputInfo* result)
{
	RetCode     ret = RETCODE_SUCCESS;
	Uint32      regVal, index, nalUnitType;
	DecInfo*    pDecInfo;

	pDecInfo = VPU_HANDLE_TO_DECINFO(instance);

	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_ADDR_REPORT_BASE, pDecInfo->userDataBufAddr);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_REPORT_SIZE,      pDecInfo->userDataBufSize);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_REPORT_PARAM,     VPU_USER_DATA_ENDIAN&VDI_128BIT_ENDIAN_MASK);

	// Send QUERY cmd
	ret = SendQuery(instance, GET_RESULT);
	if (ret != RETCODE_SUCCESS) {
		#ifdef ENABLE_FORCE_ESCAPE
		if( ret == RETCODE_CODEC_EXIT )
			return RETCODE_CODEC_EXIT;
		#endif

		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);
		if (regVal == WAVE5_RESULT_NOT_READY)
			return RETCODE_REPORT_NOT_READY;
		//else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
		//	return RETCODE_MEMORY_ACCESS_VIOLATION;
		//else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
		//	return RETCODE_VPU_RESPONSE_TIMEOUT;
		//else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL)
		//	return RETCODE_VLC_BUF_FULL;
		else
			return RETCODE_QUERY_FAILURE;
	}

	#ifdef ENABLE_VDI_LOG
	if (instance->loggingEnable)
		vdi_log(instance->coreIdx, W5_DEC_PIC, 0);
	#endif

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_QUEUE_STATUS);

	pDecInfo->instanceQueueCount = (regVal>>16)&0xff;
	pDecInfo->totalQueueCount    = (regVal & 0xffff);

	result->decodingSuccess = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODING_SUCCESS);

 	result->refMissingFrameFlag = 0;

	if (result->decodingSuccess == FALSE) {
		result->errorReason = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ERR_INFO);
		if (result->errorReason == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
			return RETCODE_MEMORY_ACCESS_VIOLATION;
	}
	else {
		result->warnInfo = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_WARN_INFO);
		if (instance->codecMode == W_HEVC_DEC) {
			// only for HEVC 2019.07.12 (TELMIC-240)
			// (VP9 does not have the concept of MISSING_REFERENCE. If it is missing due to frame loss, it becomes a problem from the entropy decoding stage.)
			//#define WAVE4_ETCERR_MISSING_REFERENCE_PICTURE   0x00020000  //wave410
			//#define WAVE5_ETCWARN_MISSING_REFERENCE_PICTURE  0x00000002 //wave512
			unsigned int refMissingFrame_Mask = 0x00000002;
			if (result->warnInfo & refMissingFrame_Mask) {
      				result->refMissingFrameFlag = 1; //detect
			}
		}
	}

	result->decOutputExtData.userDataSize   = 0;
	result->decOutputExtData.userDataNum    = 0;
	result->decOutputExtData.userDataBufFull= 0;
	result->decOutputExtData.userDataHeader = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_USERDATA_IDC);

	if (result->decOutputExtData.userDataHeader != 0) {
		regVal = result->decOutputExtData.userDataHeader;
		for (index=0; index<32; index++) {
			if (index == 1) {
				if (regVal & (1<<index))
					result->decOutputExtData.userDataBufFull = 1;
			}
			else {
				if (regVal & (1<<index))
					result->decOutputExtData.userDataNum++;
			}
		}
		result->decOutputExtData.userDataSize = pDecInfo->userDataBufSize;
	}

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_TYPE);

	#if TCC_VP9____DEC_SUPPORT == 1
	if (instance->codecMode == W_VP9_DEC) {
		if      (regVal&0x01) result->picType = PIC_TYPE_I;
		else if (regVal&0x02) result->picType = PIC_TYPE_P;
		else if (regVal&0x04) result->picType = PIC_TYPE_REPEAT;
		else                  result->picType = PIC_TYPE_MAX;
	}
	#endif
	#if TCC_HEVC___DEC_SUPPORT == 1
	if (instance->codecMode == W_HEVC_DEC) {
		if      (regVal&0x04) result->picType = PIC_TYPE_B;
		else if (regVal&0x02) result->picType = PIC_TYPE_P;
		else if (regVal&0x01) result->picType = PIC_TYPE_I;
		else                  result->picType = PIC_TYPE_MAX;
	}
	#endif
	result->outputFlag      = (regVal>>31)&0x1;

	nalUnitType = (regVal & 0x3f0) >> 4;
	if ((nalUnitType == 19 || nalUnitType == 20) && result->picType == PIC_TYPE_I) {
		/* IDR_W_RADL, IDR_N_LP */
		result->picType = PIC_TYPE_IDR;
	}
	result->nalType                   = nalUnitType;
	result->ctuSize                   = 16<<((regVal>>10)&0x3);
	index                             = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DISPLAY_INDEX);
	result->indexFrameDisplay         = index;
	result->indexFrameDisplayForTiled = index;
	index                             = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODED_INDEX);
	result->indexFrameDecoded         = index;
	result->indexFrameDecodedForTiled = index;

	#if TCC_HEVC___DEC_SUPPORT == 1
	if (instance->codecMode == W_HEVC_DEC) {
		result->h265Info.decodedPOC = -1;
		result->h265Info.displayPOC = -1;
		if (result->indexFrameDecoded >= 0)
			result->h265Info.decodedPOC = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_POC);
		result->h265Info.temporalId = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_SUB_LAYER_INFO) & 0xff;
	}
	#endif

	#if TCC_VP9____DEC_SUPPORT == 1
	if (instance->codecMode == W_VP9_DEC) {
		result->vp9PicInfo.color_space = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_VP9_COLOR_SPACE);
		result->vp9PicInfo.color_range = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_VP9_COLOR_RANGE);
		// VLOG(INFO, "%s color_space=0x%x, color_range=0x%x \n",
		// __FUNCTION__, result->vp9PicInfo.color_space, result->vp9PicInfo.color_range);
	}
	#endif

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_NOTIFICATION);
	result->sequenceChanged  = regVal & ~(1<<31);

	/*
	* If current picture is the last of the current sequence and sequence-change flag is not 0, then
	* the width and height of the current picture is set to the width and height of the current sequence.
	*/
	if (result->sequenceChanged == 0) {
		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SIZE);
		result->decPicWidth   = regVal>>16;
		result->decPicHeight  = regVal&0xffff;
	}
	else {
		if (result->indexFrameDecoded < 0) {
			result->decPicWidth   = 0;
			result->decPicHeight  = 0;
		}
		else {
			result->decPicWidth   = pDecInfo->initialInfo.picWidth;
			result->decPicHeight  = pDecInfo->initialInfo.picHeight;
		}

		if ( instance->codecMode == W_VP9_DEC ) {
			if ( result->sequenceChanged & SEQ_CHANGE_INTER_RES_CHANGE) {
				regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PIC_SIZE);
				result->decPicWidth   = regVal>>16;
				result->decPicHeight  = regVal&0xffff;
				result->indexInterFrameDecoded = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_REALLOC_INDEX);
			}
		}

		wave5_memcpy((void*)&pDecInfo->newSeqInfo, (void*)&pDecInfo->initialInfo, sizeof(DecInitialInfo), 0);

		GetDecSequenceResult(instance, &pDecInfo->newSeqInfo);
		#ifdef ENABLE_FORCE_ESCAPE
		if( gWave5InitFirst_exit > 0 )
		{
			return RETCODE_CODEC_EXIT;
		}
		#endif
	}
	//     The cases adding 4 more lines on top of picture for HEVC LF filtering
	//     if((f->filt_en==1) && !((f->height <= 16) || ( f->height <= 32 && f->ctu_size>= 32)) ){
	//         f_enc->width  = align16(f->width);
	//         f_enc->height = align16(f->height+4);
	//
	//         f_enc->mbw = f_enc->width/16;
	//         f_enc->mbh = f_enc->height/16;
	//     }
	if(WAVE5_ALIGN8(result->decPicHeight) <= result->ctuSize) {
		if (result->ctuSize == 64) {
			if (WAVE5_ALIGN8(result->decPicHeight) <= 32) {
				result->lfEnable = 0;
			}
			else {
				regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_COLOR_SAMPLE_INFO);
				result->lfEnable = (regVal >> 12) & 0xf;
			}
		}
		else {
			result->lfEnable = 0;
		}
	}
	else {
		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_COLOR_SAMPLE_INFO);
		result->lfEnable = (regVal >> 12) & 0xf;
	}
	result->numOfErrMBs       = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ERR_CTB_NUM)>>16;
	result->numOfTotMBs       = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ERR_CTB_NUM)&0xffff;
	result->bytePosFrameStart = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_AU_START_POS);
	result->bytePosFrameEnd   = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_AU_END_POS);

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_RECOVERY_POINT);
	result->h265RpSei.recoveryPocCnt = regVal & 0xFFFF;            // [15:0]
	result->h265RpSei.exactMatchFlag = (regVal >> 16)&0x01;        // [16]
	result->h265RpSei.brokenLinkFlag = (regVal >> 17)&0x01;        // [17]
	result->h265RpSei.exist =  (regVal >> 18)&0x01;                // [18]
	if (result->h265RpSei.exist == 0) {
		result->h265RpSei.recoveryPocCnt = 0;
		result->h265RpSei.exactMatchFlag = 0;
		result->h265RpSei.brokenLinkFlag = 0;
	}

#ifdef PROFILE_DECODING_TIME_INCLUDE_TICK
	result->decHostCmdTick     = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_HOST_CMD_TICK);
	result->decSeekStartTick   = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_SEEK_START_TICK);
	result->decSeekEndTick     = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_SEEK_END__TICK);
	result->decParseStartTick  = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PARSING_START_TICK);
	result->decParseEndTick    = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PARSING_END_TICK);
	result->decDecodeStartTick = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODING_START_TICK);
	result->decDecodeEndTick   = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODING_END_TICK);

	if (result->indexFrameDecodedForTiled != -1) {
		if (pDecInfo->firstCycleCheck == FALSE) {
			result->frameCycle = (result->decDecodeEndTick - result->decHostCmdTick)*512;
			pDecInfo->firstCycleCheck = TRUE;
		}
		else {
			if (g_uiLastPerformanceCycles < result->decHostCmdTick) {
				result->frameCycle = (result->decDecodeEndTick - result->decHostCmdTick)*512;
			} else {
				result->frameCycle = (result->decDecodeEndTick - g_uiLastPerformanceCycles)*512;
			}
			g_uiLastPerformanceCycles = result->decDecodeEndTick;
		}
	}
	else {
		result->frameCycle = 0;
	}
	result->seekCycle    = (result->decSeekEndTick   - result->decSeekStartTick)*512;
	result->parseCycle   = (result->decParseEndTick  - result->decParseStartTick)*512;
	result->decodeCycle  = (result->decDecodeEndTick - result->decDecodeStartTick)*512;
	if ((0 == pDecInfo->instanceQueueCount) && (0 == pDecInfo->totalQueueCount)) {
		// No remaining command. Reset frame cycle.
		pDecInfo->firstCycleCheck = FALSE;
	}
#else
	result->frameCycle         = Wave5ReadReg(instance->coreIdx, W5_FRAME_CYCLE);
	result->seekCycle          = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_SEEK_CYCLE);
	result->parseCycle         = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_PARSING_CYCLE);
	result->decodeCycle        = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_DECODING_CYCLE);
#endif

	if (result->sequenceChanged  && (instance->codecMode != W_VP9_DEC)) {
		pDecInfo->scaleWidth  = pDecInfo->newSeqInfo.picWidth;
		pDecInfo->scaleHeight = pDecInfo->newSeqInfo.picHeight;
	}

	regVal = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_COLOR_SAMPLE_INFO);
	result->lumaBitdepth = (regVal >> 0) & 0x0f;
	result->chromaBitdepth = (regVal >> 4) & 0x0f;
	result->chromaFormatIDC = (regVal >> 8) & 0x0f;
	result->aspectRateInfo = (regVal >> 16) & 0xff;
	result->isExtSAR = (result->aspectRateInfo == 255 ? TRUE : FALSE);
	if (result->isExtSAR == TRUE) {
		int sar_width, sar_height;

		result->aspectRateInfo = Wave5ReadReg(instance->coreIdx, W5_RET_DEC_ASPECT_RATIO); /* [0:15] - vertical size, [16:31] - horizontal size */
		sar_width = (result->aspectRateInfo >> 16);
		sar_height = (result->aspectRateInfo & 0xFFFF);
	}

	return RETCODE_SUCCESS;
}

RetCode Wave5VpuDecFlush(CodecInst *instance, FramebufferIndex *framebufferIndexes, Uint32 size)
{
	RetCode ret = RETCODE_SUCCESS;
	int waitInterruptMode = 0; //0: polling mode 1: interrupt mode

	if (instance->codecMode == W_HEVC_DEC) {
		if( instance->CodecInfo.decInfo.openParam.bitstreamMode == BS_MODE_INTERRUPT) { //BS_MODE_PIC_END
			//only for HEVC && ring buffer mode case
			waitInterruptMode = 1; //interrupt mode
		}
	}

	if (waitInterruptMode)
		Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_FLUSH_INSTANCE));

	Wave5BitIssueCommand(instance, W5_FLUSH_INSTANCE);

	#ifdef ENABLE_FORCE_ESCAPE
	if (gWave5InitFirst_exit > 0)
		return RETCODE_CODEC_EXIT;
	#endif

	if (waitInterruptMode) {
		while(1) {
			int iRet = 0;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));
			}

			if (iRet == RETCODE_CODEC_EXIT) {
				return RETCODE_CODEC_EXIT;
			}

			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(instance->coreIdx, iRet);
				#endif

				if (iRet & (1<<W5_INT_FLUSH_INSTANCE)) {
					break;
				}
				else {
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
			#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				return RETCODE_CODEC_EXIT;
			}
			#endif
		}
	}
	else {	//polling mode
		int cnt = 0;

		while(WAVE5_IsBusy(instance->coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
		#ifdef ENABLE_FORCE_ESCAPE
			if (gWave5InitFirst_exit > 0) {
				return RETCODE_CODEC_EXIT;
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
				return RETCODE_CODEC_EXIT;
			}
		}
	}

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {
		Uint32 regVal;

		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);

		#ifdef ENABLE_FORCE_ESCAPE
		if( gWave5InitFirst_exit > 0 )
			return RETCODE_CODEC_EXIT;
		#endif

		if (regVal == WAVE5_VPU_STILL_RUNNING) {
			return RETCODE_VPU_STILL_RUNNING;
		}
		else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW) {
			return RETCODE_MEMORY_ACCESS_VIOLATION;
		}
		else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT) {
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL) {
			return RETCODE_VLC_BUF_FULL;
		}
		else {
			return RETCODE_QUERY_FAILURE;
		}
	}
	return ret;
}


RetCode Wave5VpuReInit_tc(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size, Int32 cq_count)
{
	PhysicalAddress codeBase_PA, tempBase_PA, taskBufBase_PA; //, secaxiBase, taskBufBaseAddr;
	PhysicalAddress tempSize; //oldCodeBase
	Uint32          codeSize; 	//secaxiSize;
	Uint32          regVal, remapSize, i=0;
	CodecInstHeader hdr;
	Uint32          hwOption = 0;

	if (wave5_memset)
		wave5_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);

	workBuf = gWave5PhysicalBitWorkAddr;

	// ALIGN TO 4KB
	codeSize = (WAVE5_MAX_CODE_BUF_SIZE&~0xfff);

	codeBase_PA = workBuf;
	tempBase_PA = codeBase_PA + WAVE5_MAX_CODE_BUF_SIZE;
	tempSize = WAVE5_TEMPBUF_SIZE;

	codeBase_PA = codeBuf;

	regVal = 0;
	Wave5WriteReg(coreIdx, W5_PO_CONF, regVal);

	{
	#if 1 //This part does not exist in Wave5VpuInit.

		// Step1 : disable request
		{
			unsigned int ctrl;
			unsigned int data = 0x100;
			unsigned int addr = W5_GDI_BUS_CTRL;

			Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
			ctrl  = (addr&0xffff);
			ctrl |= (1<<16);    /* write operation */
			Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
		}

		MACRO_WAVE5_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);
	#endif

		/* Reset All blocks */
		regVal = 0x7ffffff;
		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);    // Reset All blocks
		/* Waiting reset done */

		{
			int cnt = 0;
			while (Wave5ReadReg(coreIdx, W5_VPU_RESET_STATUS)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > 0x7FFFFFF) {
					Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
					return RETCODE_CODEC_EXIT;
				}
			}
		}

		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

	#if 1 //This part does not exist in Wave5VpuInit.
		// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
		{
			unsigned int ctrl;
			unsigned int data = 0x00;
			unsigned int addr = W5_GDI_BUS_CTRL;

			Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
			ctrl  = (addr&0xffff);
			ctrl |= (1<<16);    /* write operation */
			Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
		}
	#endif

		/* remap page size */
		remapSize = (codeSize >> 12) &0x1ff;
		regVal = 0x80000000 | (WAVE5_AXI_ID<<20) | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remapSize;
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase_PA);
		Wave5WriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase_PA);
		Wave5WriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
		Wave5WriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_AXI_ID<<4) | 0);
		Wave5WriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase_PA);
		Wave5WriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
		Wave5WriteReg(coreIdx, W5_TIMEOUT_CNT,        0xffff);
		Wave5WriteReg(coreIdx, W5_HW_OPTION, hwOption);

	#ifdef USE_INTERRUPT_CALLBACK
		/* Interrupt */
		regVal  = (1<<W5_INT_INIT_SEQ);
		regVal |= (1<<W5_INT_DEC_PIC);
		regVal |= (1<<W5_INT_BSBUF_EMPTY);
		regVal |= (1<<W5_INT_SLEEP_VPU);
		regVal |= (1<<W5_INT_INIT_VPU);

		#ifdef USE_INTERRUPT_CALLBACK
		Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);
		#endif
	#endif

		#ifndef TEST_ADAPTIVE_CQ_NUM
		Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		#else
		if (cq_count > 1) {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		}
		else {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, cq_count);
		}
		#endif
		Wave5WriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, WAVE5_ONE_TASKBUF_SIZE_FOR_CQ);

		for(i = 0 ; i < cq_count ; i++) {
			taskBufBase_PA = (workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE) + (i*ONE_TASKBUF_SIZE_FOR_CQ);
			Wave5WriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase_PA);
		}

		Wave5WriteReg(coreIdx, W5_ADDR_SEC_AXI, workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE);
		Wave5WriteReg(coreIdx, W5_SEC_AXI_SIZE, WAVE5_SEC_AXI_BUF_SIZE);

		hdr.coreIdx = coreIdx;

		Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
		Wave5WriteReg(coreIdx, W5_COMMAND, W5_INIT_VPU);
		Wave5WriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;
			//Int32  reason = -1;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
			}

			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}

			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(coreIdx, iRet);
				#endif

				if (iRet & (1<<W5_INT_INIT_VPU)) {
					break;
				}
				else {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;
			while (WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > VPU_BUSY_CHECK_COUNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	#endif

		regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
		if (regVal == 0) {
			return RETCODE_FAILURE;
		}
	}

	RetCode	ret = SetupWave5Properties(coreIdx);
	return ret;
}


RetCode Wave5VpuReInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size, Int32 cq_count)
{
	PhysicalAddress codeBase, tempBase, taskBufBase, secaxiBase, taskBufBaseAddr;
	PhysicalAddress oldCodeBase, tempSize;
	Uint32          codeSize, secaxiSize;
	Uint32          regVal, remapSize, i;
	CodecInstHeader hdr;

	if (wave5_memset) {
		wave5_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);
	}

	codeBase = workBuf;

	/* ALIGN TO 4KB */
	codeSize = (WAVE5_MAX_CODE_BUF_SIZE&~0xfff);
	if (codeSize < size * 2) {
		return RETCODE_INSUFFICIENT_RESOURCE;
	}

	tempBase = codeBase + codeSize;
	tempSize = WAVE5_TEMPBUF_SIZE;

	oldCodeBase = Wave5ReadReg(coreIdx, W5_VPU_REMAP_PADDR);

	codeBase = codeBuf;

	if (oldCodeBase != codeBase) {

		regVal = 0;
		Wave5WriteReg(coreIdx, W5_PO_CONF, regVal);

		// Step1 : disable request
		//vdi_fio_write_register(coreIdx, W5_GDI_BUS_CTRL, 0x100);
		{
			unsigned int ctrl;
			unsigned int data = 0x100;
			unsigned int addr = W5_GDI_BUS_CTRL;

			Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
			ctrl  = (addr&0xffff);
			ctrl |= (1<<16);    /* write operation */
			Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
		}

		// Step2 : Waiting for completion of bus transaction
		MACRO_WAVE5_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);

		/* Reset All blocks */
		regVal = 0x7ffffff;
		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);    // Reset All blocks
		/* Waiting reset done */

		{
			int cnt = 0;
			while (Wave5ReadReg(coreIdx, W5_VPU_RESET_STATUS)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > 0x7FFFFFF) {
					Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
					return RETCODE_CODEC_EXIT;
				}
			}
		}

		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

		// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
		//vdi_fio_write_register(coreIdx, W5_GDI_BUS_CTRL, 0x00);
		{
			unsigned int ctrl;
			unsigned int data = 0x00;
			unsigned int addr = W5_GDI_BUS_CTRL;

			Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
			ctrl  = (addr&0xffff);
			ctrl |= (1<<16);    /* write operation */
			Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
		}

		/* remap page size */
		remapSize = (codeSize >> 12) &0x1ff;
		regVal = 0x80000000 | (WAVE5_AXI_ID<<20) | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remapSize;
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase);
		Wave5WriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase);
		Wave5WriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
		Wave5WriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_AXI_ID<<4) | 0);
		Wave5WriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase);
		Wave5WriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
		Wave5WriteReg(coreIdx, W5_TIMEOUT_CNT,   0);

		Wave5WriteReg(coreIdx, W5_HW_OPTION, 0);
	#ifdef USE_INTERRUPT_CALLBACK
		/* Interrupt */
		regVal  = (1<<W5_INT_INIT_SEQ);
		regVal |= (1<<W5_INT_DEC_PIC);
		regVal |= (1<<W5_INT_BSBUF_EMPTY);
		regVal |= (1<<W5_INT_SLEEP_VPU);
		regVal |= (1<<W5_INT_INIT_VPU);

		Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);
	#endif

		#ifndef TEST_ADAPTIVE_CQ_NUM
		Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		#else
		if (cq_count > 1) {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		}
		else {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, cq_count);
		}
		#endif

		taskBufBaseAddr = workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE;
		for (i = 0; i < cq_count; i++) {
			taskBufBase = taskBufBaseAddr + (i*WAVE5_ONE_TASKBUF_SIZE_FOR_CQ);
			Wave5WriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase);
		}

		secaxiBase = workBuf + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE;
		secaxiSize = WAVE5_SEC_AXI_BUF_SIZE;
		Wave5WriteReg(coreIdx, W5_ADDR_SEC_AXI, secaxiBase);
		Wave5WriteReg(coreIdx, W5_SEC_AXI_SIZE, secaxiSize);

		hdr.coreIdx = coreIdx;

		Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
		Wave5WriteReg(coreIdx, W5_COMMAND, W5_INIT_VPU);
		Wave5WriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;
			//Int32  reason = -1;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
			}
			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}

			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(coreIdx, iRet);
				#endif

				if (iRet & (1<<INT_WAVE5_INIT_VPU)) {
					break;
				}
				else {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;

			while (WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if(cnt > VPU_BUSY_CHECK_COUNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	#endif

		regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
		if (regVal == 0) {
			return RETCODE_FAILURE;
		}
	}
	SetupWave5Properties(coreIdx);

	return RETCODE_SUCCESS;
}

RetCode Wave5VpuSleepWake(Uint32 coreIdx, int iSleepWake, const Uint16 *code, Uint32 size, BOOL reset, Int32 cq_count)
{
	CodecInstHeader hdr;
	Uint32          regVal;
	PhysicalAddress codeBase, tempBase;
	Uint32          codeSize, tempSize;
	Uint32          remapSize;

	if (wave5_memset) {
		wave5_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);
	}

	hdr.coreIdx = coreIdx;

	if (iSleepWake==1) {	//saves
		int cnt = 0;

		while (WAVE5_IsBusy(coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT)
					return RETCODE_CODEC_EXIT;
			}
		#endif

		#ifdef ENABLE_FORCE_ESCAPE_2
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif

			if (cnt > 0x7FFFFFF)
				return RETCODE_CODEC_EXIT;
		}

		Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
		#ifdef USE_INTERRUPT_CALLBACK
		Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_SLEEP_VPU));
		#endif
		Wave5WriteReg(coreIdx, W5_COMMAND, W5_SLEEP_VPU);
		Wave5WriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
			}

			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}

		#ifdef ENABLE_FORCE_ESCAPE_2
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif

		#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(coreIdx, iRet);
			#endif

				if (iRet & (1<<INT_WAVE5_SLEEP_VPU)) {
					break;
				}
				else {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;
			while (WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > VPU_BUSY_CHECK_COUNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	#endif

		regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
		if (regVal == 0) {
			return RETCODE_FAILURE;
		}
	}
	else {	//restore
		Uint32  hwOption  = 0;
		Uint32  i;
		BOOL	reset_int = (reset==TRUE ? INT_WAVE5_WAKEUP_VPU : INT_WAVE5_INIT_VPU);
		PhysicalAddress taskBufBase;

		/* ALIGN TO 4KB */
		codeSize = (WAVE5_MAX_CODE_BUF_SIZE&~0xfff);

		codeBase = gWave5PhysicalBitWorkAddr;
		tempBase = codeBase + WAVE5_MAX_CODE_BUF_SIZE;
		tempSize = WAVE5_TEMPBUF_SIZE;

		regVal = 0;
		Wave5WriteReg(coreIdx, W5_PO_CONF, regVal);

		/* SW_RESET_SAFETY */
		regVal = W5_RST_BLOCK_ALL;
		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);	// Reset All blocks

		/* Waiting reset done */
		{
			int cnt = 0;
			while (Wave5ReadReg(coreIdx, W5_VPU_RESET_STATUS)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif

			#ifdef ENABLE_FORCE_ESCAPE_2
				if (gWave5InitFirst_exit > 0) {
					WAVE5_dec_reset(NULL, (0 | (1<<16)));
					return RETCODE_CODEC_EXIT;
				}
			#endif

				if (cnt > 0x7FFFFFF) {
					Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
					return RETCODE_CODEC_EXIT;
				}
			}
		}

		codeBase = gWave5FWAddr;

		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

		/* remap page size */
		remapSize = (codeSize >> 12) &0x1ff;
		regVal = 0x80000000 | (WAVE5_AXI_ID<<20) | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remapSize;
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase);
		Wave5WriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase);
		Wave5WriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
		Wave5WriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_AXI_ID<<4) | 0);
		Wave5WriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase);
		Wave5WriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
		Wave5WriteReg(coreIdx, W5_TIMEOUT_CNT,   0);

		codeBase = gWave5PhysicalBitWorkAddr;

		Wave5WriteReg(coreIdx, W5_HW_OPTION, hwOption);

		#ifdef USE_INTERRUPT_CALLBACK	//2019.08.10 Isn't this what reset_int should do?  Need to compare with wait time. In the case of INT_WAVE5_INIT_VPU, there may be a problem with not waiting.
		/* Interrupt */
		regVal  = (1<<W5_INT_INIT_SEQ);
		regVal |= (1<<W5_INT_DEC_PIC);
		regVal |= (1<<W5_INT_BSBUF_EMPTY);
		regVal |= (1<<W5_INT_WAKEUP_VPU);
		Wave5WriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);
		#else
		regVal  = (1<<W5_INT_DEC_PIC);
		regVal |= (1<<W5_INT_BSBUF_EMPTY);
		#endif

		#ifndef TEST_ADAPTIVE_CQ_NUM
		Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		#else
		if (cq_count > 1) {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, (ENABLE_TCC_WAVE410_FLOW<<16) | cq_count);
		}
		else {
			Wave5WriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, cq_count);
		}
		#endif
		Wave5WriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, WAVE5_ONE_TASKBUF_SIZE_FOR_CQ);

		for (i = 0; i < cq_count; i++) {
			taskBufBase = codeBase + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE + WAVE5_SEC_AXI_BUF_SIZE + (i*ONE_TASKBUF_SIZE_FOR_CQ);
			Wave5WriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase);
		}

		Wave5WriteReg(coreIdx, W5_ADDR_SEC_AXI, (codeBase + WAVE5_MAX_CODE_BUF_SIZE + WAVE5_TEMPBUF_SIZE));
		Wave5WriteReg(coreIdx, W5_SEC_AXI_SIZE, WAVE5_SEC_AXI_BUF_SIZE);
		Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
		Wave5WriteReg(coreIdx, W5_COMMAND, (reset==TRUE ? W5_INIT_VPU : W5_WAKEUP_VPU));
		Wave5WriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;
			//Int32  reason = -1;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(coreIdx, W5_VPU_VINT_ENABLE));
			}

			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}

		#ifdef ENABLE_FORCE_ESCAPE_2
			if (gWave5InitFirst_exit > 0) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}
		#endif


		#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
		#else
			iRet = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON);
		#endif
			if (iRet) {
				Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(coreIdx, iRet);
			#endif

				if (iRet & (1<<reset_int)) {
					break;
				}
				else {
					Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;

			while (WAVE5_IsBusy(coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > VPU_BUSY_CHECK_COUNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	#endif

		regVal = Wave5ReadReg(coreIdx, W5_RET_SUCCESS);
		if (regVal == 0) {
			return RETCODE_FAILURE;
		}

		Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0xffff);
	#ifndef TEST_INTERRUPT_REASON_REGISTER
		Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_USR, 0);
 	#endif
		#ifdef TEST_INTERRUPT_REASON_REGISTER
		Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON, 0);
		#endif
		Wave5WriteReg(coreIdx, W5_VPU_VINT_CLEAR, 0x1);
	}

	return RETCODE_SUCCESS;
}

RetCode Wave5VpuReset(Uint32 coreIdx, SWResetMode resetMode, Int32 cq_count)
{
	Uint32  val = 0;
	RetCode ret = RETCODE_SUCCESS;

	// VPU doesn't send response. Force to set BUSY flag to 0.
	Wave5WriteReg(coreIdx, W5_VPU_BUSY_STATUS, 0);

	// Waiting for completion of bus transaction
	// Step1 : disable request
	//vdi_fio_write_register(coreIdx, W5_GDI_BUS_CTRL, 0x100);
	{
		unsigned int ctrl;
		unsigned int data = 0x100;
		unsigned int addr = W5_GDI_BUS_CTRL;

		Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
		ctrl  = (addr&0xffff);
		ctrl |= (1<<16);    /* write operation */
		Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
	}

	MACRO_WAVE5_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);

	if (resetMode == SW_RESET_SAFETY) {
		if ((ret=Wave5VpuSleepWake(coreIdx, TRUE, NULL, 0, TRUE, cq_count)) != RETCODE_SUCCESS) {
			return ret;
		}
	}

	switch (resetMode) {
	case SW_RESET_ON_BOOT:
	case SW_RESET_FORCE:
	case SW_RESET_SAFETY:
		val = W5_RST_BLOCK_ALL;
		break;
	default:
		return RETCODE_INVALID_PARAM;
	}

	if (val) {
		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, val);
		{
			int cnt = 0;
			while (Wave5ReadReg(coreIdx, W5_VPU_RESET_STATUS)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif

			#ifdef ENABLE_FORCE_ESCAPE_2
				if (gWave5InitFirst_exit > 0) {
					WAVE5_dec_reset(NULL, (0 | (1<<16)));
					return RETCODE_CODEC_EXIT;
				}
			#endif

				if (cnt > 0x7FFFFFF) {
					Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
					#ifdef ENABLE_VDI_LOG
					vdi_log(coreIdx, W5_RESET_VPU, 2);
					#endif
					return RETCODE_CODEC_EXIT;
				}
			}
		}

		Wave5WriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
	}

	// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
	// vdi_fio_write_register(coreIdx, W5_GDI_BUS_CTRL, 0x00);
	{
		unsigned int ctrl;
		unsigned int data = 0x00;
		unsigned int addr = W5_GDI_BUS_CTRL;

		Wave5WriteReg(coreIdx, W5_VPU_FIO_DATA, data);
		ctrl  = (addr&0xffff);
		ctrl |= (1<<16);    /* write operation */
		Wave5WriteReg(coreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl);
	}

	if (resetMode == SW_RESET_SAFETY || resetMode == SW_RESET_FORCE) {
		ret = Wave5VpuSleepWake(coreIdx, FALSE, NULL, 0, TRUE, cq_count);
	}

	return ret;
}

RetCode Wave5VpuDecFiniSeq(CodecInst *instance)
{
	RetCode ret = RETCODE_SUCCESS;

#ifdef USE_INTERRUPT_CALLBACK
	Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_DESTORY_INSTANCE));
#endif

	Wave5BitIssueCommand(instance, W5_DESTROY_INSTANCE);

#ifdef USE_INTERRUPT_CALLBACK
	while (1) {
		int iRet = 0;
		Int32  reason = -1;

		if (wave5_interrupt != NULL) {
			iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));
		}

		if (iRet == RETCODE_CODEC_EXIT) {
			WAVE5_dec_reset(NULL, (0 | (1<<16)));
			return RETCODE_CODEC_EXIT;
		}

		#ifndef TEST_INTERRUPT_REASON_REGISTER
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
		#else
		iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
		#endif
		if (iRet) {
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
			Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
			#ifndef TEST_INTERRUPT_REASON_REGISTER
			Wave5VpuClearInterrupt(instance->coreIdx, iRet);
			#endif

			if (iRet & (1<<INT_WAVE5_DESTORY_INSTANCE)) {
				break;
			}
			else {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				return RETCODE_FAILURE;
			}
		}
	}
#else
	{
		int cnt = 0;

		while (WAVE5_IsBusy(instance->coreIdx)) {
			cnt++;
		#ifdef ENABLE_WAIT_POLLING_USLEEP
			if (wave5_usleep != NULL) {
				wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
				if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		#endif
			if (cnt > VPU_BUSY_CHECK_COUNT) {
				return RETCODE_CODEC_EXIT;
			}
		}
	}
#endif

	if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == FALSE) {
		Uint32 regVal;

		regVal = Wave5ReadReg(instance->coreIdx, W5_RET_FAIL_REASON);
		if (regVal == WAVE5_VPU_STILL_RUNNING) {
			ret = RETCODE_VPU_STILL_RUNNING;
		}
		else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW) {
			return RETCODE_MEMORY_ACCESS_VIOLATION;
		}
		else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT) {
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		else if (regVal == WAVE5_SYSERR_DEC_VLC_BUF_FULL) {
			return RETCODE_VLC_BUF_FULL;
		}
		else {
			ret = RETCODE_FAILURE;
		}
	}

	return ret;
}

RetCode Wave5VpuDecSetBitstreamFlag(CodecInst *instance, BOOL running, BOOL eos, BOOL explictEnd)
{
	DecInfo* pDecInfo = &instance->CodecInfo.decInfo;
	BitStreamMode bsMode = (BitStreamMode)pDecInfo->openParam.bitstreamMode;
	pDecInfo->streamEndflag = (eos == 1) ? TRUE : FALSE;

	if (bsMode == BS_MODE_INTERRUPT) {
		if (pDecInfo->streamEndflag == TRUE) {
			explictEnd = TRUE;
		}

		Wave5WriteReg(instance->coreIdx, W5_BS_OPTION, (pDecInfo->streamEndflag<<1) | explictEnd);
		Wave5WriteReg(instance->coreIdx, W5_BS_WR_PTR, pDecInfo->streamWrPtr);

	#ifdef USE_INTERRUPT_CALLBACK
		Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_ENABLE, (1<<W5_INT_BSBUF_EMPTY));
	#endif
		Wave5BitIssueCommand(instance, W5_UPDATE_BS);

	#ifdef USE_INTERRUPT_CALLBACK
		while (1) {
			int iRet = 0;
			Int32  reason = -1;

			if (wave5_interrupt != NULL) {
				iRet = wave5_interrupt(Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_ENABLE));
			}

			if (iRet == RETCODE_CODEC_EXIT) {
				WAVE5_dec_reset(NULL, (0 | (1<<16)));
				return RETCODE_CODEC_EXIT;
			}

			#ifndef TEST_INTERRUPT_REASON_REGISTER
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON_USR);
			#else
			iRet = Wave5ReadReg(instance->coreIdx, W5_VPU_VINT_REASON);
			#endif
			if (iRet) {
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, iRet);
				Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
				#ifndef TEST_INTERRUPT_REASON_REGISTER
				Wave5VpuClearInterrupt(instance->coreIdx, iRet);
				#endif

				if (iRet & (1<<INT_WAVE5_BSBUF_EMPTY)) {
					break;
				}
				else {
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_REASON_CLR, 0);
					Wave5WriteReg(instance->coreIdx, W5_VPU_VINT_CLEAR, 1);
					return RETCODE_FAILURE;
				}
			}
		}
	#else
		{
			int cnt = 0;

			while (WAVE5_IsBusy(instance->coreIdx)) {
				cnt++;
			#ifdef ENABLE_WAIT_POLLING_USLEEP
				if (wave5_usleep != NULL) {
					wave5_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX);
					if (cnt > VPU_BUSY_CHECK_USLEEP_CNT) {
						return RETCODE_CODEC_EXIT;
					}
				}
			#endif
				if (cnt > VPU_BUSY_CHECK_COUNT) {
					return RETCODE_CODEC_EXIT;
				}
			}
		}
	#endif

		if (Wave5ReadReg(instance->coreIdx, W5_RET_SUCCESS) == 0)
			return RETCODE_FAILURE;
	}

	return RETCODE_SUCCESS;
}

RetCode Wave5DecClrDispFlag(CodecInst *instance, Uint32 index)
{
	return Wave5DecClrDispFlagEx(instance, (1 << index));
}

RetCode Wave5DecSetDispFlag(CodecInst *instance, Uint32 index)
{
	return Wave5DecSetDispFlagEx(instance, (1 << index));
}

RetCode Wave5DecClrDispFlagEx(CodecInst *instance, Uint32 dispFlag)
{
	RetCode ret = RETCODE_SUCCESS;
	DecInfo *pDecInfo;
	pDecInfo = &instance->CodecInfo.decInfo;

	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_CLR_DISP_IDC, dispFlag);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_SET_DISP_IDC, 0);

	ret = SendQuery(instance, UPDATE_DISP_FLAG);

#ifdef ENABLE_FORCE_ESCAPE
	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			return RETCODE_CODEC_EXIT;
		}
		return RETCODE_QUERY_FAILURE;
	}
#else
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_QUERY_FAILURE;
	}
#endif

	pDecInfo->frameDisplayFlag = Wave5ReadReg(instance->coreIdx, pDecInfo->frameDisplayFlagRegAddr);
	return RETCODE_SUCCESS;
}

RetCode Wave5DecSetDispFlagEx(CodecInst *instance, Uint32 dispFlag)
{
	RetCode ret = RETCODE_SUCCESS;
	DecInfo *pDecInfo;

	pDecInfo   = &instance->CodecInfo.decInfo;
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_CLR_DISP_IDC, 0);
	Wave5WriteReg(instance->coreIdx, W5_CMD_DEC_SET_DISP_IDC, dispFlag);
	ret = SendQuery(instance, UPDATE_DISP_FLAG);
	pDecInfo->frameDisplayFlag = Wave5ReadReg(instance->coreIdx, pDecInfo->frameDisplayFlagRegAddr);
	return ret;
}

RetCode Wave5VpuClearInterrupt(Uint32 coreIdx, Uint32 flags)
{
	Uint32 interruptReason;

	interruptReason = Wave5ReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
	interruptReason &= ~flags;
	Wave5WriteReg(coreIdx, W5_VPU_VINT_REASON_USR, interruptReason);
	return RETCODE_SUCCESS;
}

RetCode Wave5VpuDecGetRdPtr(CodecInst *instance, PhysicalAddress *rdPtr)
{
	RetCode ret = RETCODE_SUCCESS;

	ret = SendQuery(instance, GET_BS_RD_PTR);
#ifdef ENABLE_FORCE_ESCAPE_2
	if (ret != RETCODE_SUCCESS) {
		if (ret == RETCODE_CODEC_EXIT) {
			return ret;
		}
		return RETCODE_QUERY_FAILURE;
	}
#else
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_QUERY_FAILURE;
	}
#endif
	*rdPtr = Wave5ReadReg(instance->coreIdx, W5_RET_QUERY_DEC_BS_RD_PTR);

	return RETCODE_SUCCESS;
}

RetCode Wave5VpuDecSetRdPtr(CodecInst *instance, PhysicalAddress rdPtr)
{
	RetCode ret = RETCODE_SUCCESS;

	Wave5WriteReg(instance->coreIdx, W5_RET_QUERY_DEC_SET_BS_RD_PTR, rdPtr);
	ret = SendQuery(instance, GET_BS_SET_RD_PTR);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_QUERY_FAILURE;
	}

	return RETCODE_SUCCESS;
}
