/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#include "wave5_core.h"
#include "wave5.h"

extern int gWave5MaxInstanceNum;

wave5_t gsWave5HandlePool[WAVE5_MAX_NUM_INSTANCE];

void wave5_local_mem_set(void *src_addr, int val, unsigned int count, unsigned int isIO)
{
	unsigned int i;
	unsigned char *pSrcAddr = src_addr;

	if( src_addr == NULL )
		return;
	for(i=0; i< count; i++)
		*pSrcAddr++ = val;
	return;
}

void *wave5_local_mem_cpy(void *dst_addr, const void *src_addr, unsigned int count, unsigned int isIO)
{
	unsigned int i;
	unsigned char *pTmpAddr = dst_addr;
	const unsigned char *pSrcAddr = src_addr;

	for(i=0;i<count;i++)
		*pTmpAddr++ = *pSrcAddr++;
	return dst_addr;
}

int check_wave5_handle_addr(wave5_t *pVpu)
{
	int ret = 0;

	if (pVpu != NULL) {
		int i;
		wave5_t *pVpuRef = &gsWave5HandlePool[0];

		for (i = 0; i < gWave5MaxInstanceNum; ++i, ++pVpuRef) {
			if (pVpu == pVpuRef) {
				i = gWave5MaxInstanceNum;
				ret = 1;	//success
			}
		}
	}
	return ret;
}

int check_wave5_handle_use(wave5_t *pVpu)
{
	int ret = 0;

	if (pVpu != NULL) {
		if (pVpu->m_bInUse == 1) {
			ret = 1;	//success
		}
	}
	return ret;
}


int wave5_get_vpu_handle(wave5_t **ppVpu)
{
	int i;
	wave5_t* p_vpu;

	p_vpu = &gsWave5HandlePool[0];

	for (i = 0; i < gWave5MaxInstanceNum; ++i, ++p_vpu) {
		if (!p_vpu->m_bInUse)
			break;
	}

	if (i == gWave5MaxInstanceNum) {
		*ppVpu = 0;
		return RETCODE_FAILURE;
	}

	p_vpu->m_bInUse = 1;
	p_vpu->m_iVpuInstIndex = i;
	p_vpu->m_iPrevDispIdx = -1;
	p_vpu->m_iIndexFrameDisplayPrev = -1;
	p_vpu->m_pPendingHandle = WAVE5_GetPendingInstPointer( );

	*ppVpu = p_vpu;
	return RETCODE_SUCCESS;
}

void wave5_free_vpu_handle(wave5_t *pVpu)
{
	int i, inuse = 0;
	wave5_t* p_vpu;

	pVpu->m_bInUse = 0;

	if( wave5_memset )
		wave5_memset( pVpu, 0, sizeof(*pVpu), 0 );

	p_vpu = &gsWave5HandlePool[0];

	for (i = 0; i < gWave5MaxInstanceNum; ++i, ++p_vpu) {
		if (p_vpu->m_bInUse) {
			inuse = 1;
			break;
		}
	}

	if (inuse == 0) {
		gWave5InitFirst = 0;
	#ifdef ENABLE_FORCE_ESCAPE
		gWave5InitFirst_exit = 0;
	#endif
	}
}

void wave5_reset_global_var(int iOption)
{
	if (iOption == 1) {
		gWave5InitFirst = 0;
	#ifdef ENABLE_FORCE_ESCAPE
		gWave5InitFirst_exit =0;
	#endif

		if (wave5_memset) {
			if ((void *)gsWave5HandlePool != NULL)
				wave5_memset( gsWave5HandlePool, 0, sizeof(wave5_t)*gWave5MaxInstanceNum, 0 );
		}
	}
}
