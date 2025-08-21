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
#include "product.h"
#include "common.h"
#include "common_vpuconfig.h"
#include "vpuerror.h"
#include "common_regdefine.h"

#define COMMAND_TIMEOUT 0xffff

void wave420l_Wave4BitIssueCommand(CodecInst *instance, Uint32 cmd)
{
	Uint32 instanceIndex = 0;
	Uint32 codecMode = 0;
	Uint32 coreIdx;

	if (instance != NULL) {
		instanceIndex = instance->instIndex;
		codecMode = instance->codecMode;
	}

	coreIdx = instance->coreIdx;

	VpuWriteReg(coreIdx, W4_VPU_BUSY_STATUS, 1);
	VpuWriteReg(coreIdx, W4_RET_SUCCESS, 0); // for debug
	VpuWriteReg(coreIdx, W4_CORE_INDEX, 0);

	VpuWriteReg(coreIdx, W4_INST_INDEX, (instanceIndex & 0xffff) | (codecMode << 16));

	VpuWriteReg(coreIdx, W4_COMMAND, cmd);

	if ((instance != NULL && instance->loggingEnable)) {
		wave420l_vdi_log(coreIdx, cmd, 1);
	}

	if (cmd != INIT_VPU) {
		VpuWriteReg(coreIdx, W4_VPU_HOST_INT_REQ, 1);
	}

	return;
}

Int32 WaveVpuGetProductId(Uint32 coreIdx)
{
	Uint32 productId = PRODUCT_ID_NONE;
	Uint32 val;

	if (coreIdx >= MAX_NUM_VPU_CORE) {
		return PRODUCT_ID_NONE;
	}

	val = VpuReadReg(coreIdx, W4_PRODUCT_NUMBER);

	switch (val)
	{
	case WAVE420L_CODE:
		productId = PRODUCT_ID_420L;
		break;
	default:
	#if !defined(VLOG_DISABLE)
		VLOG(ERR, "Check productId(%d)\n", val);
	#endif
		break;
	}

	return productId;
}

RetCode wave420l_Wave4VpuGetVersion(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision)
{
	Uint32 regVal;
	CodecInstHeader hdr = {0};

	/* GET FIRMWARE&HARDWARE INFORMATION */
	hdr.coreIdx = 0;
	hdr.instIndex = 0;
	hdr.loggingEnable = 0;

	wave420l_Wave4BitIssueCommand((CodecInst *)&hdr, GET_FW_VERSION);
	if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}

	regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
	if (regVal == 0) {
		return RETCODE_FAILURE;
	}

	if (versionInfo != NULL) {
		regVal = VpuReadReg(coreIdx, W4_RET_FW_VERSION);
		*versionInfo = regVal;
	#if !defined(VLOG_DISABLE)
		VLOG(INFO, "\nget firmware version %d !!!\n", regVal);
	#endif
	}

	if (revision != NULL) {
		regVal = VpuReadReg(coreIdx, W4_RET_CONFIG_REVISION);
		*revision = regVal;
	#if !defined(VLOG_DISABLE)
		VLOG(INFO, "\nget hw revision %d !!!\n", regVal);
	#endif
	}

	return RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size)
{
	PhysicalAddress codeBase;
	Uint32 codeSize;
	Uint32 i, regVal, remapSize;
	Uint32 hwOption = 0;
	CodecInstHeader hdr;

	wave420l_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);

#if !defined(VLOG_DISABLE)
	VLOG(INFO, "\nVPU INIT Start!!!\n");
#endif

	/* ALIGN TO 4KB */
	codeSize = (WAVE4_MAX_CODE_BUF_SIZE & ~0xfff);
	codeBase = codeBuf;
	codeSize = (codeSize + 0xfff) & ~0xfff;

	regVal = 0;
	VpuWriteReg(coreIdx, W4_PO_CONF, regVal);

	/* Reset All blocks */
	regVal = 0x7ffffff;
	VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, regVal); // Reset All blocks
	/* Waiting reset done */

	if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_RESET_STATUS) == -1) {
	#if !defined(VLOG_DISABLE)
		VLOG(ERR, "VPU init(W4_VPU_RESET_REQ) timeout\n");
	#endif
		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}

	VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);

	/* clear registers */
	for (i = W4_CMD_REG_BASE; i < W4_CMD_REG_END; i += 4)
		VpuWriteReg(coreIdx, i, 0x00);

	/* remap page size */
	remapSize = (codeSize >> 12) & 0x1ff;
	regVal = 0x80000000 | (0 << 16) | (W4_REMAP_CODE_INDEX << 12) | (1 << 11) | remapSize;
	VpuWriteReg(coreIdx, W4_VPU_REMAP_CTRL, regVal);
	VpuWriteReg(coreIdx, W4_VPU_REMAP_VADDR, 0x00000000); /* DO NOT CHANGE! */
	VpuWriteReg(coreIdx, W4_VPU_REMAP_PADDR, codeBase);
	VpuWriteReg(coreIdx, W4_ADDR_CODE_BASE, codeBase);
	VpuWriteReg(coreIdx, W4_CODE_SIZE, codeSize);
	VpuWriteReg(coreIdx, W4_CODE_PARAM, 0);
	// timeoutTicks = COMMAND_TIMEOUT*VCPU_CLOCK_IN_MHZ*(1000000>>15);
	VpuWriteReg(coreIdx, W4_TIMEOUT_CNT, 0xffffffff);

	VpuWriteReg(coreIdx, W4_HW_OPTION, hwOption);
	/* Interrupt */
	// for encoder interrupt
	regVal = (1 << W4_INT_ENC_PIC);
	regVal |= (1 << W4_INT_SLEEP_VPU);
	regVal |= (1 << W4_INT_SET_PARAM);

	// for decoder interrupt
	regVal |= (1 << W4_INT_DEC_PIC_HDR);
	regVal |= (1 << W4_INT_DEC_PIC);
	regVal |= (1 << W4_INT_QUERY_DEC);
	regVal |= (1 << W4_INT_SLEEP_VPU);
	regVal |= (1 << W4_INT_BSBUF_EMPTY);

	VpuWriteReg(coreIdx, W4_VPU_VINT_ENABLE, regVal);

	hdr.coreIdx = coreIdx;
	wave420l_Wave4BitIssueCommand((CodecInst *)&hdr, INIT_VPU);
	VpuWriteReg(coreIdx, W4_VPU_REMAP_CORE_START, 1);

	if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
	#if !defined(VLOG_DISABLE)
		VLOG(ERR, "VPU init(W4_VPU_REMAP_CORE_START) timeout\n");
	#endif
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}

	regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);

	if (regVal == 0) {
	#if !defined(VLOG_DISABLE)
		Uint32 reasonCode = VpuReadReg(coreIdx, W4_RET_FAIL_REASON);

		VLOG(ERR, "VPU init(W4_RET_SUCCESS) failed(%d) REASON CODE(%08x)\n", regVal, reasonCode);
	#endif
		return RETCODE_FAILURE;
	}

	return RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuReInit(Uint32 coreIdx, PhysicalAddress workBuf, PhysicalAddress codeBuf, void *firmware, Uint32 size)
{
	PhysicalAddress codeBase;
	Uint32 codeSize;
	Uint32 regVal, remapSize;
	CodecInstHeader hdr;

	wave420l_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);
	codeBase = workBuf;

	/* ALIGN TO 4KB */
	codeSize = (WAVE4_MAX_CODE_BUF_SIZE & ~0xfff);
	codeBase = codeBuf;

	{
		// ALIGN TO 4KB
		codeSize = (WAVE4_MAX_CODE_BUF_SIZE & ~0xfff);

		regVal = 0;
		VpuWriteReg(coreIdx, W4_PO_CONF, regVal);

		// Waiting for completion of bus transaction
		// Step1 : disable request
		wave420l_vdi_fio_write_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL, 0x100); // WAVE410 GDI added new feature: disable_request

		// Step2 : Waiting for completion of bus transaction
		if (wave420l_vdi_wait_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_GDI_VCORE0_BUS_STATUS) == -1) {
			wave420l_vdi_fio_write_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL, 0x00);
			wave420l_vdi_log(coreIdx, RESET_VPU, 2);
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		// MACRO_WAVE4_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);

		/* Reset All blocks */
		regVal = 0x7ffffff;
		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, regVal); // Reset All blocks
		/* Waiting reset done */

		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_RESET_STATUS) == -1) {
			VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
		    int cnt = 0;
			    while(VpuReadReg(coreIdx, W4_VPU_RESET_STATUS))
			    {
				    cnt++;
				    if (cnt > 0x7FFFFFF)
					    return RETCODE_CODEC_EXIT;
			    }
		}*/

		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);

		// Step3 : must clear GDI_BUS_CTRL after done SW_RESET
		wave420l_vdi_fio_write_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL, 0x00);

		/* not needed to clear registers
		for (i=W4_CMD_REG_BASE; i<W4_CMD_REG_END; i+=4)
		VpuWriteReg(coreIdx, i, 0x00)
		*/

		/* remap page size */
		remapSize = (codeSize >> 12) & 0x1ff;
		regVal = 0x80000000 | (W4_REMAP_CODE_INDEX << 12) | (0 << 16) | (1 << 11) | remapSize;
		VpuWriteReg(coreIdx, W4_VPU_REMAP_CTRL, regVal);
		VpuWriteReg(coreIdx, W4_VPU_REMAP_VADDR, 0x00000000); /* DO NOT CHANGE! */
		VpuWriteReg(coreIdx, W4_VPU_REMAP_PADDR, codeBase);
		VpuWriteReg(coreIdx, W4_ADDR_CODE_BASE, codeBase);
		VpuWriteReg(coreIdx, W4_CODE_SIZE, codeSize);
		VpuWriteReg(coreIdx, W4_CODE_PARAM, 0);
		VpuWriteReg(coreIdx, W4_TIMEOUT_CNT, COMMAND_TIMEOUT);
		VpuWriteReg(coreIdx, W4_HW_OPTION, 0);
		/* Interrupt */
		// for encoder interrupt
		regVal = (1 << W4_INT_ENC_PIC);
		regVal |= (1 << W4_INT_SLEEP_VPU);
		regVal |= (1 << W4_INT_SET_PARAM);
		// for decoder interrupt
		regVal |= (1 << W4_INT_DEC_PIC_HDR);
		regVal |= (1 << W4_INT_DEC_PIC);
		regVal |= (1 << W4_INT_QUERY_DEC);
		regVal |= (1 << W4_INT_SLEEP_VPU);
		regVal |= (1 << W4_INT_BSBUF_EMPTY);
		VpuWriteReg(coreIdx, W4_VPU_VINT_ENABLE, regVal);

		hdr.coreIdx = coreIdx;

		wave420l_Wave4BitIssueCommand((CodecInst *)&hdr, INIT_VPU);
		VpuWriteReg(coreIdx, W4_VPU_REMAP_CORE_START, 1);

		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}

		regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
		if (regVal == 0)
			return RETCODE_FAILURE;
	}

	return RETCODE_SUCCESS;
}

Uint32 wave420l_Wave4VpuIsInit(Uint32 coreIdx)
{
	Uint32 pc;

	pc = (Uint32)VpuReadReg(coreIdx, W4_VCPU_CUR_PC);

	return pc;
}

Int32 wave420l_Wave4VpuIsBusy(Uint32 coreIdx)
{
	return VpuReadReg(coreIdx, W4_VPU_BUSY_STATUS);
}

Int32 wave420l_Wave4VpuWaitInterrupt(CodecInst *handle, Int32 timeout)
{
	Int32 reason = -1;
	Uint32 coreIdx = 0;

	if ((reason = wave420l_vdi_wait_interrupt(0, 0x7FFF /*VPU_ENC_TIMEOUT*/, W4_VPU_VINT_REASON_USR, 0 /*W4_VPU_VPU_INT_STS*/, 1)) > 0) {
		VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_CLR, reason);
		VpuWriteReg(coreIdx, W4_VPU_VINT_CLEAR, 1);
	}

	return reason;
}

RetCode wave420l_Wave4VpuClearInterrupt(Uint32 coreIdx, Uint32 flags)
{
	Uint32 interruptReason;

	interruptReason = VpuReadReg(coreIdx, W4_VPU_VINT_REASON_USR); // W4_VPU_VINT_REASON
	interruptReason &= ~flags;
	VpuWriteReg(coreIdx, W4_VPU_VINT_REASON_USR, interruptReason); // W4_VPU_VINT_REASON

	return RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuSleepWake(Uint32 coreIdx, int iSleepWake, const Uint16 *code, Uint32 size)
{
	CodecInstHeader hdr;
	Uint32 regVal;
	PhysicalAddress codeBase;
	Uint32 codeSize;
	Uint32 remapSize;

	wave420l_memset((void *)&hdr, 0x00, sizeof(CodecInstHeader), 0);
	hdr.coreIdx = coreIdx;

	if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}

	if (iSleepWake == 1) // saves
	{
		wave420l_Wave4BitIssueCommand((CodecInst *)&hdr, SLEEP_VPU);
		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
		    int cnt = 0;
			while(VpuReadReg(coreIdx, W4_VPU_BUSY_STATUS))
		    {
			cnt++;
			if (cnt > 0x7FFFFFF)
			    return RETCODE_CODEC_EXIT;
		    }
		}*/

		regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
		if (regVal == 0) {
			APIDPRINT("SLEEP_VPU failed [0x%x]", VpuReadReg(coreIdx, W4_RET_FAIL_REASON));
			return RETCODE_FAILURE;
		}
	}
	else // restore
	{
		Uint32 hwOption = 0;

		size = gsHevcEncBitCodeSize; // sizeof(vpu_hevc_enc_bit_code) / sizeof(vpu_hevc_enc_bit_code[0]);

		codeBase = gHevcEncFWAddr;

		/* ALIGN TO 4KB */
		codeSize = (WAVE4_MAX_CODE_BUF_SIZE & ~0xfff);

		regVal = 0;
		VpuWriteReg(coreIdx, W4_PO_CONF, regVal);

		/* SW_RESET_SAFETY */
		regVal = W4_RST_BLOCK_ALL;
		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, regVal); // Reset All blocks

		/* Waiting reset done */
		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_RESET_STATUS) == -1) {
		#if !defined(VLOG_DISABLE)
			VLOG(ERR, "VPU Wakeup(W4_VPU_RESET_REQ) timeout\n");
		#endif
			VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
		    int cnt = 0;
			while(VpuReadReg(coreIdx, W4_VPU_RESET_STATUS))
		    {
			cnt++;
			if (cnt > 0x7FFFFFF)
			    return RETCODE_CODEC_EXIT;
		    }
		}*/

		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);

		/* remap page size */
		remapSize = (codeSize >> 12) & 0x1ff;
		regVal = 0x80000000 | (0 << 16) | (W4_REMAP_CODE_INDEX << 12) | (1 << 11) | remapSize;
		VpuWriteReg(coreIdx, W4_VPU_REMAP_CTRL, regVal);
		VpuWriteReg(coreIdx, W4_VPU_REMAP_VADDR, 0x00000000); /* DO NOT CHANGE! */
		VpuWriteReg(coreIdx, W4_VPU_REMAP_PADDR, codeBase);
		VpuWriteReg(coreIdx, W4_ADDR_CODE_BASE, codeBase);
		VpuWriteReg(coreIdx, W4_CODE_SIZE, codeSize);
		VpuWriteReg(coreIdx, W4_CODE_PARAM, 0);
		VpuWriteReg(coreIdx, W4_TIMEOUT_CNT, COMMAND_TIMEOUT);

		VpuWriteReg(coreIdx, W4_HW_OPTION, hwOption);

		/* Interrupt */
		// for encoder interrupt
		regVal = (1 << W4_INT_ENC_PIC);
		regVal |= (1 << W4_INT_SLEEP_VPU);
		regVal |= (1 << W4_INT_SET_PARAM);
		// for decoder interrupt
		regVal |= (1 << W4_INT_DEC_PIC_HDR);
		regVal |= (1 << W4_INT_DEC_PIC);
		regVal |= (1 << W4_INT_QUERY_DEC);
		regVal |= (1 << W4_INT_SLEEP_VPU);
		regVal |= (1 << W4_INT_BSBUF_EMPTY);

		VpuWriteReg(coreIdx, W4_VPU_VINT_ENABLE, regVal);

		hdr.coreIdx = coreIdx;

		VpuWriteReg(coreIdx, W4_VPU_BUSY_STATUS, 1);
		VpuWriteReg(coreIdx, W4_RET_SUCCESS, 0); // for debug
		VpuWriteReg(coreIdx, W4_CORE_INDEX, 0);

		VpuWriteReg(coreIdx, W4_INST_INDEX, (hdr.instIndex & 0xffff) | (hdr.codecMode << 16));

		VpuWriteReg(coreIdx, W4_COMMAND, INIT_VPU);

		if (hdr.loggingEnable) {
			wave420l_vdi_log(coreIdx, INIT_VPU, 1);
		}

		VpuWriteReg(coreIdx, W4_VPU_REMAP_CORE_START, 1);

		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
		#if !defined(VLOG_DISABLE)
			VLOG(ERR, "VPU Wakeup (W4_VPU_REMAP_CORE_START) timeout\n");
		#endif
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
		    int cnt = 0;
			while(VpuReadReg(coreIdx, W4_VPU_BUSY_STATUS))
		    {
			cnt++;
			if (cnt > 0x7FFFFFF)
			    return RETCODE_CODEC_EXIT;
		    }
		}*/

		regVal = VpuReadReg(coreIdx, W4_RET_SUCCESS);
		if (regVal == 0) {
		#if !defined(VLOG_DISABLE)
			Uint32 reasonCode = VpuReadReg(coreIdx, W4_RET_FAIL_REASON);

			VLOG(ERR, "VPU Wakeup(W4_RET_SUCCESS) failed(%d) REASON CODE(%08x)\n", regVal, reasonCode);
		#endif
			return RETCODE_FAILURE;
		}
	}

	return RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuReset(Uint32 coreIdx, SWResetMode resetMode)
{
	Uint32 val = 0;
	RetCode ret = RETCODE_FAILURE;

	// VPU doesn't send response. Force to set BUSY flag to 0.
	VpuWriteReg(coreIdx, W4_VPU_BUSY_STATUS, 0);

	val = wave420l_vdi_fio_read_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL);
	if ((val >> 24) == 0x01) {
		/* VPU has a bus transaction controller */
		wave420l_vdi_fio_write_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL, 0x11);
	}

	if (wave420l_vdi_wait_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_GDI_VCORE0_BUS_STATUS) == -1) {
		wave420l_vdi_log(coreIdx, RESET_VPU, 2);
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}
	// MACRO_WAVE4_GDI_BUS_WAITING_EXIT(coreIdx, 0x7FFFF);

	if (resetMode == SW_RESET_SAFETY) {
		if ((ret = wave420l_Wave4VpuSleepWake(coreIdx, TRUE, NULL, 0)) != RETCODE_SUCCESS) {
			return ret;
		}
	}

	switch (resetMode) {
	case SW_RESET_ON_BOOT:
	case SW_RESET_FORCE:
		val = W4_RST_BLOCK_ALL;
		break;
	case SW_RESET_SAFETY:
		val = W4_RST_BLOCK_ACLK_ALL | W4_RST_BLOCK_BCLK_ALL | W4_RST_BLOCK_CCLK_ALL;
		break;
	default:
		return RETCODE_INVALID_PARAM;
	}

	if (val) {
		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, val);

		if (wave420l_vdi_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_RESET_STATUS) == -1) {
			VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);
			return RETCODE_VPU_RESPONSE_TIMEOUT;
		}
		/*
		{  //polling
		    int cnt = 0;
			while(VpuReadReg(coreIdx, W4_VPU_RESET_STATUS))
		    {
			cnt++;
			if (cnt > 0x7FFFFFF)
			    return RETCODE_CODEC_EXIT;
		    }
		}*/

		VpuWriteReg(coreIdx, W4_VPU_RESET_REQ, 0);
	}

	val = wave420l_vdi_fio_read_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL);
	if ((val >> 24) == 0x01) {
		/* VPU has a bus transaction controller */
		wave420l_vdi_fio_write_register(coreIdx, W4_GDI_VCORE0_BUS_CTRL, 0);
	}

	if (resetMode == SW_RESET_SAFETY || resetMode == SW_RESET_FORCE) {
		ret = wave420l_Wave4VpuSleepWake(coreIdx, FALSE, NULL, 0);
	}

	return RETCODE_SUCCESS;
}

RetCode Wave4VpuEncFiniSeq(CodecInst *instance)
{
	EncInfo *pEncInfo = VPU_HANDLE_TO_ENCINFO(instance);

	VpuWriteReg(instance->coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(instance->coreIdx, W4_WORK_SIZE, pEncInfo->vbWork.size);
	VpuWriteReg(instance->coreIdx, W4_WORK_PARAM, 0);

	wave420l_Wave4BitIssueCommand(instance, FINI_SEQ);
	if (wave420l_vdi_wait_vpu_busy(instance->coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1)
		return RETCODE_VPU_RESPONSE_TIMEOUT;

	if (VpuReadReg(instance->coreIdx, W4_RET_SUCCESS) == FALSE)
		return RETCODE_FAILURE;

	return RETCODE_SUCCESS;
}

RetCode wave420l_Wave4VpuBuildUpEncParam(CodecInst *instance, EncOpenParam *param)
{
	RetCode ret = RETCODE_SUCCESS;
	EncInfo *pEncInfo = &instance->CodecInfo->encInfo;

	pEncInfo->streamRdPtrRegAddr = W4_BS_RD_PTR;
	pEncInfo->streamWrPtrRegAddr = W4_BS_WR_PTR;
	pEncInfo->currentPC = W4_VCPU_CUR_PC;
	pEncInfo->busyFlagAddr = W4_VPU_BUSY_STATUS;

	instance->codecMode = HEVC_ENC;

	pEncInfo->vbWork.size = WAVE4ENC_WORKBUF_SIZE;
	pEncInfo->vbWork.base = param->vbWork.base;
	pEncInfo->vbWork.phys_addr = param->vbWork.phys_addr;
	pEncInfo->vbWork.size = param->vbWork.size;
	pEncInfo->vbWork.virt_addr = param->vbWork.virt_addr;
	pEncInfo->vbTemp.base = param->vbTemp.base;
	pEncInfo->vbTemp.phys_addr = param->vbTemp.phys_addr;
	pEncInfo->vbTemp.size = param->vbTemp.size;
	pEncInfo->vbTemp.virt_addr = param->vbTemp.virt_addr;

	VpuWriteReg(instance->coreIdx, W4_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
	VpuWriteReg(instance->coreIdx, W4_WORK_SIZE, pEncInfo->vbWork.size);
	VpuWriteReg(instance->coreIdx, W4_WORK_PARAM, 0);

	wave420l_Wave4BitIssueCommand(instance, CREATE_INSTANCE);
	if (wave420l_vdi_wait_vpu_busy(instance->coreIdx, __VPU_BUSY_TIMEOUT, W4_VPU_BUSY_STATUS) == -1) {
		return RETCODE_VPU_RESPONSE_TIMEOUT;
	}
	/*
	{  //polling
	    int cnt = 0;
		while(VpuReadReg(coreIdx, W4_VPU_BUSY_STATUS))
	    {
		cnt++;
		if (cnt > 0x7FFFFFF)
		    return RETCODE_CODEC_EXIT;
	    }
	}
	*/

	if (VpuReadReg(instance->coreIdx, W4_RET_SUCCESS) == FALSE) {
		ret = RETCODE_FAILURE;
	}

	pEncInfo->streamRdPtr = param->bitstreamBuffer;
	pEncInfo->streamWrPtr = param->bitstreamBuffer;
	pEncInfo->lineBufIntEn = 0; // disable
	pEncInfo->streamBufStartAddr = param->bitstreamBuffer;
	pEncInfo->streamBufSize = param->bitstreamBufferSize;
	pEncInfo->streamBufEndAddr = param->bitstreamBuffer + param->bitstreamBufferSize;
	pEncInfo->stride = 0;
	pEncInfo->vbFrame.size = 0;
	pEncInfo->vbPPU.size = 0;
	pEncInfo->frameAllocExt = 0;
	pEncInfo->ppuAllocExt = 0;
	pEncInfo->secAxiInfo.u.wave4.useBitEnable = 0;
	pEncInfo->secAxiInfo.u.wave4.useIpEnable = 0;
	pEncInfo->secAxiInfo.u.wave4.useLfRowEnable = 0;
	// pEncInfo->rotationAngle         = 0;
	pEncInfo->initialInfoObtained = 0;
	pEncInfo->sliceIntEnable = 0;

	return ret;
}
