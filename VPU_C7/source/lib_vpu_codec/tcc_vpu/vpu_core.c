/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#include "vpu_core.h"

void (*g_pfPrintCb_VpuC7)(const char *, ...) = NULL; /**< printk callback */
#define LOG_VAR(level) g_bLog##level##_VpuC7
// Log level variables for each prefix
int LOG_VAR(Verbose) = 0;
int LOG_VAR(Debug) = 0;
int LOG_VAR(Info) = 0;
int LOG_VAR(Warn) = 0;
int LOG_VAR(Error) = 0;
int LOG_VAR(Assert) = 0;
int LOG_VAR(Func) = 0;
int LOG_VAR(Trace) = 0;
int LOG_VAR(DecodeSuccess) = 0;

extern int gMaxInstanceNum;

static vpu_t gsVpuHandlePool[MAX_NUM_INSTANCE];


void vpu_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int type)
{
	unsigned int i;
	unsigned char *pSrcAddr = src_addr;
	for (i=0; i < count; i++) {
		*pSrcAddr++ = val;
	}
	return;
}

void *vpu_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int type)
{
	unsigned int i;
	unsigned char *pTmpAddr = dst_addr;
	const unsigned char *pSrcAddr = src_addr;
	for (i = 0; i < count; i++) {
		*pTmpAddr++ = *pSrcAddr++;
	}
	return dst_addr;
}

int check_vpu_handle_addr(vpu_t *pVpu)
{
	int ret = RETCODE_FAILURE;

	if (pVpu != NULL) {
		int i;
		vpu_t *pVpuRef = &gsVpuHandlePool[0];

		for (i = 0; i < gMaxInstanceNum; ++i, ++pVpuRef) {
			if (pVpu == pVpuRef) {
				i = gMaxInstanceNum;
				ret = RETCODE_SUCCESS;
			}
		}
	}
	return ret;
}

int check_vpu_handle_use(vpu_t *pVpu)
{
	int ret = RETCODE_FAILURE;

	if (pVpu != NULL) {
		if (pVpu->m_bInUse == 1) {
			ret = RETCODE_SUCCESS;	//success
		}
	}
	return ret;
}

int get_vpu_handle(vpu_t **ppVpu)
{
	int i;
	vpu_t* p_vpu;

	p_vpu = &gsVpuHandlePool[0];
	for (i = 0; i < gMaxInstanceNum; ++i, ++p_vpu) {
		if (!p_vpu->m_bInUse) {
			break;
		}
	}

	if (i == gMaxInstanceNum) {
		*ppVpu = 0;
		return RETCODE_FAILURE;
	}

	p_vpu->m_bInUse = 1;
	p_vpu->m_iVpuInstIndex = i;
	p_vpu->m_iPrevDispIdx = -1;
	p_vpu->m_iIndexFrameDisplayPrev = -1;

	{
		vpu_field_info_t *pstFieldinfo = &p_vpu->m_stFieldInfo;
		pstFieldinfo->m_lPicType = 0;
		pstFieldinfo->m_lDispIndex = -3;
		pstFieldinfo->m_lDecIndex = -1;
		pstFieldinfo->m_lOutputStatus = 0;
		pstFieldinfo->m_lConsumedBytes = 0;
	}

	p_vpu->m_pPendingHandle = GetPendingInstPointer();

	*ppVpu = p_vpu;
	return RETCODE_SUCCESS;
}

int get_vpu_instance_number(void)
{
	int i, count;
	vpu_t* p_vpu;

	p_vpu = &gsVpuHandlePool[0];
	count = 0;
	for (i = 0; i < gMaxInstanceNum; ++i, ++p_vpu) {
		if (p_vpu->m_bInUse) {
			count++;
		}
	}
	return count;
}

void free_vpu_handle(vpu_t* pVpu)
{
	int i, inuse = 0;
	vpu_t* p_vpu;

	if (pVpu != NULL) {
		pVpu->m_bInUse = 0;
		if (vpu_memset != NULL) {
			vpu_memset(pVpu, 0, sizeof(*pVpu), 0);
		}
	}
	p_vpu = &gsVpuHandlePool[0];
	for (i = 0; i < gMaxInstanceNum; ++i, ++p_vpu) {
		if (p_vpu->m_bInUse) {
			inuse = 1;
			break;
		}
	}
	if (inuse == 0) {
		gInitFirst = 0;
	}
}

void reset_vpu_global_var(int iOption)
{
	if (iOption == 0) {
		//extern CodecInst codecInstPool[MAX_NUM_INSTANCE];
		// changed in coda960

		int i;
		for (i=0; i< gMaxInstanceNum; i++) {
			//codecInstPool[i].inUse = 0;
			// changed in coda960
		}
	} else if (iOption == 1) {
		gInitFirst = 0;

		if (vpu_memset != NULL) {
			vpu_memset(gsVpuHandlePool, 0, (sizeof(vpu_t) * gMaxInstanceNum), 0);
		}

		{
			int ret;
			unsigned int opt = 0x14; //inUse = (1<<2), codecMode = (1<<4);

			ret = resetAllCodecInstanceMember(opt);	//inUse = (1<<2), codecMode = (1<<4);
			/*
			extern CodecInst codecInstPool[MAX_NUM_INSTANCE];
			int i;
			CodecInst * pCodecInst;

			pCodecInst = &codecInstPool[0];
			for( i = 0; i < gMaxInstanceNum; ++i, ++pCodecInst )
			{
				pCodecInst->instIndex = i;
				pCodecInst->inUse = 0;
			}
			*/
			// changed in coda960
		}
	}
}

void set_register_bit(unsigned int uiBit, unsigned int uiRegisterAddr)
{
	unsigned int val = VpuReadReg(uiRegisterAddr);
	val |= uiBit;
	VpuWriteReg(uiRegisterAddr, val);
}

#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
int check_aarch_vpu_d6(void)
#else
int check_aarch_vpu_c7(void)
#endif
{
	int ret = 4;
	int sb;

	sb = (int)sizeof(long);	//4 for ARM32, 8 for ARM64
	if (sb <= 0) {
		ret = 4;
	} else if (sb > 256) {
		ret = 4;
	} else {
		ret = sb;
	}
	return ret;
}