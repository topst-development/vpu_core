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

#include "vpuapifunc.h"
#include "regdefine.h"
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
#include "jpegtable.h"
#	ifdef USE_VPU_ENC_PUTHEADER_INTERRUPT
#include "vpu_core.h"
#	endif
#endif


#ifndef MIN
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

#define init_get_bits(CTX, BUFFER, SIZE) JpuGbuInit(CTX, BUFFER, SIZE)
#define show_bits(CTX, NUM) JpuGguShowBit(CTX, NUM)
#define get_bits(CTX, NUM) JpuGbuGetBit(CTX, NUM)
#define get_bits_left(CTX) JpuGbuGetLeftBitCount(CTX)
#define get_bits_count(CTX) JpuGbuGetUsedBitCount(CTX)

#if 0
#define MAX_LAVEL_IDX	16
static const int g_anLevel[MAX_LAVEL_IDX] =
{
	10, 11, 11, 12, 13,
	//10, 16, 11, 12, 13,
	20, 21, 22,
	30, 31, 32,
	40, 41, 42,
	50, 51
};

static const int g_anLevelMaxMBPS[MAX_LAVEL_IDX] =
{
	1485,   1485,   3000,   6000, 11880,
	11880,  19800,  20250,
	40500,  108000, 216000,
	245760, 245760, 522240,
	589824, 983040
};

static const int g_anLevelMaxFS[MAX_LAVEL_IDX] =
{
	99,    99,   396, 396, 396,
	396,   792,  1620,
	1620,  3600, 5120,
	8192,  8192, 8704,
	22080, 36864
};

static const int g_anLevelMaxBR[MAX_LAVEL_IDX] =
{
	64,     64,   192,  384, 768,
	2000,   4000,  4000,
	10000,  14000, 20000,
	20000,  50000, 50000,
	135000, 240000
};

static const int g_anLevelSliceRate[MAX_LAVEL_IDX] =
{
	0,  0,  0,  0,  0,
	0,  0,  0,
	22, 60, 60,
	60, 24, 24,
	24, 24
};

static const int g_anLevelMaxMbs[MAX_LAVEL_IDX] =
{
	28,   28,  56, 56, 56,
	56,   79, 113,
	113, 169, 202,
	256, 256, 263,
	420, 543
};
#endif

#endif	//TCC_VPU_2K_IP != 0x0630

/******************************************************************************
    Codec Instance Slot Management
******************************************************************************/

extern CodecInst codecInstPool[MAX_NUM_INSTANCE];
extern int gMaxInstanceNum;

/******************************************************************************
 Global Static Variables to prevent exceed the stack frame size.
 ******************************************************************************/
static int huffsize[256];
static int huffcode[256];

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

int JpuGbuInit(vpu_getbit_context_t *ctx, BYTE *buffer, int size)
{

	ctx->buffer = buffer;
	ctx->index = 0;
	ctx->size = size/8;

	return 1;
}

int JpuGbuGetUsedBitCount(vpu_getbit_context_t *ctx)
{
	return ctx->index*8;
}

int JpuGbuGetLeftBitCount(vpu_getbit_context_t *ctx)
{
	return (ctx->size*8) - JpuGbuGetUsedBitCount(ctx);
}

unsigned int JpuGbuGetBit(vpu_getbit_context_t *ctx, int bit_num)
{
	BYTE *p;
	unsigned int b = 0x0;

	if (bit_num > JpuGbuGetLeftBitCount(ctx))
		return -1;

	p = ctx->buffer + ctx->index;

	if (bit_num == 8)
	{
		b = *p;
		ctx->index++;
	}
	else if(bit_num == 16)
	{
		b = *p<<8; p++;
		b |= *p;
		ctx->index += 2;
	}
	else if(bit_num == 32)
	{
		b  = (*p<<24); p++;
		b |= (*p<<16); p++;
		b |= (*p<<8);  p++;
		b |= (*p<<0);
		ctx->index += 4;
	}

	return b;
}

unsigned int JpuGguShowBit(vpu_getbit_context_t *ctx, int bit_num)
{
	BYTE *p;
	unsigned int b = 0x0;

	if (bit_num > JpuGbuGetLeftBitCount(ctx))
		return -1;

	p = ctx->buffer + ctx->index;

	if (bit_num == 8)
	{
		b = *p;
	}
	else if(bit_num == 16)
	{
		b = *p++<<8;
		b |= *p;
	}
	else if(bit_num == 32)
	{
		b = *p++<<24;
		b |= (*p++<<16);
		b |= (*p++<<8);
		b |= (*p<<0);
	}

	return b;
}

static Uint32 tGetBits(DecInfo *pDecInfo, int endian, int byteCnt);

#ifdef MJPEG_ERROR_CONCEAL_DEBUG
//static int RstMakerRounding = 0;
#endif

const char lendian[4] = {0x49, 0x49, 0x2A, 0x00};
const char bendian[4] = {0x4D, 0x4D, 0x00, 0x2A};

const char *jfif = "JFIF";
const char *jfxx = "JFXX";
const char *exif = "Exif";

static BYTE * pThumbBuf;

#endif	//TCC_VPU_2K_IP != 0x0630

/*
 * GetCodecInstance() obtains a instance.
 * It stores a pointer to the allocated instance in *ppInst
 * and returns RETCODE_SUCCESS on success.
 * Failure results in 0(null pointer) in *ppInst and RETCODE_FAILURE.
 */

RetCode GetCodecInstance(CodecInst ** ppInst)
{
	int i;
	CodecInst * pCodecInst;

	pCodecInst = &codecInstPool[0];
	for (i = 0; i < gMaxInstanceNum; ++i, ++pCodecInst)
	{
		if (!pCodecInst->inUse)
			break;
	}

	if (i == gMaxInstanceNum)
	{
		*ppInst = 0;
		return RETCODE_FAILURE;
	}

	pCodecInst->inUse = 1;
	*ppInst = pCodecInst;

	return RETCODE_SUCCESS;
}

void FreeCodecInstance(CodecInst * pCodecInst)
{
	pCodecInst->inUse = 0;
}

RetCode CheckInstanceValidity(CodecInst * pci)
{
	CodecInst * pCodecInst;
	int i;

	pCodecInst = &codecInstPool[0];
	for (i = 0; i < gMaxInstanceNum; ++i, ++pCodecInst) {
		if (pCodecInst == pci)
		{
			if( pCodecInst->inUse != 0 )
				return RETCODE_SUCCESS;
		}
	}

	return RETCODE_INVALID_HANDLE;
}

/******************************************************************************
    API Subroutines
******************************************************************************/

RetCode BitLoadFirmware_tc(codec_addr_t codeBase, const Uint16 *codeWord, int codeSize)
{
	int i;

	for (i=0; i<codeSize; i+=4)
	{
		*(unsigned short *)(codeBase+i*2+0) = codeWord[i+3];
		*(unsigned short *)(codeBase+i*2+2) = codeWord[i+2];
		*(unsigned short *)(codeBase+i*2+4) = codeWord[i+1];
		*(unsigned short *)(codeBase+i*2+6) = codeWord[i+0];
	}

	return RETCODE_SUCCESS;
}

RetCode BitLoadFirmware(codec_addr_t codeBase, const Uint16 *codeWord, int codeSize)
{
	int i;
	Uint32 data;

	for (i=0; i<codeSize; i+=4)
	{
		*(unsigned short *)(codeBase+i*2+0) = codeWord[i+3];
		*(unsigned short *)(codeBase+i*2+2) = codeWord[i+2];
		*(unsigned short *)(codeBase+i*2+4) = codeWord[i+1];
		*(unsigned short *)(codeBase+i*2+6) = codeWord[i+0];
	}

	VpuWriteReg(BIT_INT_ENABLE, 0);
	VpuWriteReg(BIT_CODE_RUN, 0);

	for (i=0; i<2048; ++i) {
		data = codeWord[i];
		VpuWriteReg(BIT_CODE_DOWN, (i << 16) | data);
	}

	return RETCODE_SUCCESS;
}

void BitIssueCommand(CodecInst *inst, int cmd)
{
	int instIdx = 0;
	int cdcMode = 0;
	int auxMode = 0;

	if (inst != NULL) // command is specific to instance
	{
		instIdx = inst->instIndex;
		cdcMode = inst->codecMode;
		auxMode = inst->codecModeAux;
	}

	if (inst)
	{
		if (inst->codecMode < AVC_ENC)
			VpuWriteReg(BIT_WORK_BUF_ADDR, inst->CodecInfo.decInfo.workBufBaseAddr);
	#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
		else
			VpuWriteReg(BIT_WORK_BUF_ADDR, inst->CodecInfo.encInfo.workBufBaseAddr);
	#endif
	}

	VpuWriteReg(BIT_BUSY_FLAG, 1);
	VpuWriteReg(BIT_RUN_INDEX, instIdx);
	VpuWriteReg(BIT_RUN_COD_STD, cdcMode);
	VpuWriteReg(BIT_RUN_AUX_STD, auxMode);
	VpuWriteReg(BIT_RUN_COMMAND, cmd);
}

RetCode CheckDecInstanceValidity(DecHandle handle)
{
	CodecInst * pCodecInst;
	RetCode ret;

	pCodecInst = handle;
	ret = CheckInstanceValidity(pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}

	if (!pCodecInst->inUse) {
		return RETCODE_INVALID_HANDLE;
	}

	if ((pCodecInst->codecMode != AVC_DEC) &&
	    (pCodecInst->codecMode != VC1_DEC) &&
	    (pCodecInst->codecMode != MP2_DEC) &&
	    (pCodecInst->codecMode != MP4_DEC) &&
	#if TCC_AVS____DEC_SUPPORT == 1
	    (pCodecInst->codecMode != AVS_DEC) &&
	#endif
	#if TCC_MJPEG__DEC_SUPPORT == 1
	    (pCodecInst->codecMode != MJPG_DEC) &&
	#endif
	    (pCodecInst->codecMode != VPX_DEC))
	{
		return RETCODE_INVALID_HANDLE;
	}

	return RETCODE_SUCCESS;
}

RetCode CheckDecOpenParam(DecOpenParam * pop)
{
	if (pop == 0)
		return RETCODE_INVALID_PARAM;

	if (pop->bitstreamBuffer % 8)	//512
		return RETCODE_INVALID_PARAM;

	if (pop->bitstreamFormat != STD_MJPG)
	{
		if (pop->bitstreamBufferSize % 1024 ||
			pop->bitstreamBufferSize < 1024 ||
			pop->bitstreamBufferSize > (256*1024*1024-1) ) {
				return RETCODE_INVALID_PARAM;
		}
	}
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	else
	{
		if (!pop->jpgLineBufferMode)
		{
			if ((pop->bitstreamBufferSize % 1024) || (pop->bitstreamBufferSize < 1024))
				return RETCODE_INVALID_PARAM;
		}
	}
#endif
	if (pop->workBuffer % 8)
		return RETCODE_INVALID_PARAM;

	if (pop->workBufferSize < WORK_BUF_SIZE)
		return RETCODE_INVALID_PARAM;

	if (pop->bitstreamFormat != STD_AVC) {
		return RETCODE_CODEC_SPECOUT;	//RETCODE_INVALID_PARAM
	}

	return RETCODE_SUCCESS;
}

int DecBitstreamBufEmpty(DecInfo * pDecInfo)
{
	return (pDecInfo->streamRdPtr == (int)pDecInfo->streamWrPtr);
}

RetCode SetParaSet(DecHandle handle, int paraSetType, DecParamSet *para)
{
	CodecInst *pCodecInst;
	PhysicalAddress paraBuffer;
	int i;
	Uint32 *src;

	pCodecInst = handle;
	src = para->paraSet;
	paraBuffer = VpuReadReg(BIT_PARA_BUF_ADDR);
	for (i = 0; i < para->size; i += 4) {
		VpuWriteReg(paraBuffer + i, *src++);
	}
	VpuWriteReg(CMD_DEC_PARA_SET_TYPE, paraSetType); // 0: SPS, 1: PPS
	VpuWriteReg(CMD_DEC_PARA_SET_SIZE, para->size);

	BitIssueCommand(pCodecInst, DEC_PARA_SET);
	while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}


#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
int GetMvColBufSize(int framebufFormat, int picWidth, int picHeight, int mapType)
{

	int chr_vscale, chr_hscale;
	int size_dpb_lum, size_dpb_chr, size_dpb_all;
	int size_mvcolbuf, size_mvcolbuf_4k;
	const int interleave = 1;


	chr_hscale = (framebufFormat == FORMAT_420 || framebufFormat == FORMAT_422 ? 2 : 1);	// 4:2:0, 4:2:2h
	chr_vscale = (framebufFormat == FORMAT_420 || framebufFormat == FORMAT_224 ? 2 : 1);	// 4:2:0, 4:2:2v

	size_dpb_lum = picWidth*picHeight;
	if (framebufFormat == FORMAT_400)
		size_dpb_chr  = 0;
	else
		size_dpb_chr  = size_dpb_lum/chr_vscale/chr_hscale;

	size_dpb_all = (size_dpb_lum + 2*size_dpb_chr);

	// mvColBufSize is 1/5 of picture size in X264
	// normal case is 1/6
	size_mvcolbuf = (size_dpb_all+3) / 5;
	size_mvcolbuf = ((size_mvcolbuf+7) / 8) * 8;

	// base address should be aligned to 4k address in MS raster map (20 bit address).
	size_mvcolbuf_4k = ((size_mvcolbuf+(16384-1))/16384)*16384;


	if (mapType == TILED_FRAME_MB_RASTER_MAP)
		return (size_mvcolbuf_4k+7)/8*8;

	return (size_mvcolbuf+7)/8*8;
}

#else //TCC_VPU_2K_IP == 0x0630

int JpgDecHuffTabSetUp(DecInfo *pDecInfo)
{
	int i, j;
	int HuffData;	// 16BITS
	int HuffLength;
	int temp;
	JpgDecInfo *jpg = &pDecInfo->jpgInfo;


	// MIN Tables
	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x003);

	//DC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMin[0][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));	// 32-bit
	}

	//DC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMin[2][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));	// 32-bit
	}

	//AC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMin[1][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));	// 32-bit
	}

	//AC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMin[3][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));	// 32-bit
	}
	// MAX Tables
	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x403);
	VpuWriteReg(MJPEG_HUFF_ADDR_REG, 0x440);

	//DC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMax[0][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));
	}
	//DC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMax[2][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));
	}
	//AC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMax[1][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));
	}
	//AC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffMax[3][j];
		temp = (HuffData & 0x8000) >> 15;
		temp = (temp << 15) | (temp << 14) | (temp << 13) | (temp << 12) | (temp << 11) | (temp << 10) | (temp << 9) | (temp << 8) | (temp << 7 ) | (temp << 6) | (temp <<5) | (temp<<4) | (temp<<3) | (temp<<2) | (temp<<1)| (temp) ;
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFF) << 16) | HuffData));
	}

	// PTR Tables
	VpuWriteReg (MJPEG_HUFF_CTRL_REG, 0x803);
	VpuWriteReg (MJPEG_HUFF_ADDR_REG, 0x880);


	//DC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffPtr[0][j];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}
	//DC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffPtr[2][j];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}
	//AC Luma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffPtr[1][j];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}
	//AC Chroma
	for(j=0; j<16; j++)
	{
		HuffData = jpg->huffPtr[3][j];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}

	// VAL Tables
	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0xC03);

	// VAL DC Luma
	HuffLength = 0;
	for(i=0; i<12; i++)
		HuffLength += jpg->huffBits[0][i];

	if (HuffLength > 162)
		return 0;

	for (i=0; i<HuffLength; i++) {	// 8-bit, 12 row, 1 category (DC Luma)
		HuffData = jpg->huffVal[0][i];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}

	for (i=0; i<12-HuffLength; i++) {
		VpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
	}

	// VAL DC Chroma
	HuffLength = 0;
	for(i=0; i<12; i++)
		HuffLength += jpg->huffBits[2][i];

	if (HuffLength > 162)
		return 0;

	for (i=0; i<HuffLength; i++) {	// 8-bit, 12 row, 1 category (DC Chroma)
		HuffData = jpg->huffVal[2][i];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}
	for (i=0; i<12-HuffLength; i++) {
		VpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
	}

	// VAL AC Luma
	HuffLength = 0;
	for(i=0; i<162; i++)
		HuffLength += jpg->huffBits[1][i];

	if (HuffLength > 162)
		return 0;

	for (i=0; i<HuffLength; i++) {	// 8-bit, 162 row, 1 category (AC Luma)
		HuffData = jpg->huffVal[1][i];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}
	for (i=0; i<162-HuffLength; i++) {
		VpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);
	}

	// VAL AC Chroma
	HuffLength = 0;
	for(i=0; i<162; i++)
		HuffLength += jpg->huffBits[3][i];

	if (HuffLength > 162)
		return 0;

	for (i=0; i<HuffLength; i++) {	// 8-bit, 162 row, 1 category (AC Chroma)
		HuffData = jpg->huffVal[3][i];
		temp = (HuffData & 0x80) >> 7;
		temp = (temp<<23)|(temp<<22)|(temp<<21)|(temp<<20)|(temp<<19)|(temp<<18)|(temp<<17)|(temp<<16)|(temp<<15)|(temp<<14)|(temp<<13)|(temp<<12)|(temp<<11)|(temp<<10)|(temp<<9)|(temp<<8)|(temp<<7)|(temp<<6)|(temp<<5)|(temp<<4)|(temp<<3)|(temp<<2)|(temp<<1)|(temp);
		VpuWriteReg (MJPEG_HUFF_DATA_REG, (((temp & 0xFFFFFF) << 8) | HuffData));
	}

	for (i=0; i<162-HuffLength; i++)
		VpuWriteReg(MJPEG_HUFF_DATA_REG, 0xFFFFFFFF);

	// end SerPeriHuffTab
	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x000);

	return 1;
}

int JpgDecQMatTabSetUp(DecInfo *pDecInfo)
{

	int i;
	int table;
	int val;
	JpgDecInfo *jpg = &pDecInfo->jpgInfo;

	// SetPeriQMatTab
	// Comp 0
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x03);
	table = jpg->cInfoTab[0][3];
	if (table >= 4)
		return 0;
	for (i=0; i<64; i++) {
		val = jpg->qMatTab[table][i];
		VpuWriteReg(MJPEG_QMAT_DATA_REG, val);
	}
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);

	// Comp 1
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x43);
	table = jpg->cInfoTab[1][3];
	if (table >= 4)
		return 0;
	for (i=0; i<64; i++) {
		val = jpg->qMatTab[table][i];
		VpuWriteReg(MJPEG_QMAT_DATA_REG, val);
	}
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);

	// Comp 2
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x83);
	table = jpg->cInfoTab[2][3];
	if (table >= 4)
		return 0;
	for (i=0; i<64; i++) {
		val = jpg->qMatTab[table][i];
		VpuWriteReg(MJPEG_QMAT_DATA_REG, val);
	}
	VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x00);
	return 1;
}

void JpgDecGramSetup(DecInfo * pDecInfo)
{
	int dExtBitBufCurPos;
	int dExtBitBufBaseAddr;
	int dMibStatus;

	dMibStatus = 1;
	dExtBitBufCurPos = pDecInfo->jpgInfo.pagePtr;
	dExtBitBufBaseAddr = pDecInfo->streamBufStartAddr;

	VpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos);
	VpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, dExtBitBufBaseAddr + (dExtBitBufCurPos << 8));
	VpuWriteReg(MJPEG_BBC_INT_ADDR_REG, (dExtBitBufCurPos & 1) << 6);
	VpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 256 / 4);	// 64 * 4 byte == 32 * 8 byte
	VpuWriteReg(MJPEG_BBC_COMMAND_REG, (pDecInfo->openParam.streamEndian << 1) | 0);

	while (dMibStatus == 1)
		dMibStatus = VpuReadReg(MJPEG_BBC_BUSY_REG);

	dMibStatus = 1;
	dExtBitBufCurPos = dExtBitBufCurPos + 1;

	VpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos);
	VpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, dExtBitBufBaseAddr + (dExtBitBufCurPos << 8));
	VpuWriteReg(MJPEG_BBC_INT_ADDR_REG, (dExtBitBufCurPos & 1) << 6);
	VpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 256 / 4);	// 64 * 4 byte == 32 * 8 byte
	VpuWriteReg(MJPEG_BBC_COMMAND_REG, (pDecInfo->openParam.streamEndian << 1) | 0);

	while (dMibStatus == 1)
		dMibStatus = VpuReadReg(MJPEG_BBC_BUSY_REG);

	dMibStatus = 1;
	dExtBitBufCurPos = dExtBitBufCurPos + 1;

	VpuWriteReg(MJPEG_BBC_CUR_POS_REG, dExtBitBufCurPos);	// next uint page pointe
	VpuWriteReg(MJPEG_BBC_CTRL_REG, ((pDecInfo->openParam.streamEndian & 3) << 1) | 1);
	VpuWriteReg(MJPEG_GBU_WD_PTR_REG, pDecInfo->jpgInfo.wordPtr);
	VpuWriteReg(MJPEG_GBU_BBSR_REG, 0);
	VpuWriteReg(MJPEG_GBU_BBER_REG, ((256 / 4) * 2) - 1);
	if (pDecInfo->jpgInfo.pagePtr & 1) {
		VpuWriteReg(MJPEG_GBU_BBIR_REG, 0);
		VpuWriteReg(MJPEG_GBU_BBHR_REG, 0);
	}
	else {
		VpuWriteReg(MJPEG_GBU_BBIR_REG, 256 / 4);	// 64 * 4 byte == 32 * 8 byte
		VpuWriteReg(MJPEG_GBU_BBHR_REG, 256 / 4);	// 64 * 4 byte == 32 * 8 byte
	}

	VpuWriteReg(MJPEG_GBU_CTRL_REG, 4);
	VpuWriteReg(MJPEG_GBU_FF_RPTR_REG, pDecInfo->jpgInfo.bitPtr);

}

#define MAX_VSIZE		8192
#define MAX_HSIZE		8192

enum {
	SAMPLE_420 = 0xA,
	SAMPLE_H422 = 0x9,
	SAMPLE_V422 = 0x6,
	SAMPLE_444 = 0x5,
	SAMPLE_400 = 0x1,
	SAMPLE_NULL = 0x7FFFFFFF
};

const BYTE cDefHuffBits[4][16] =
{
	{	// DC index 0 (Luminance DC)
		0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	}
	,
	{	// AC index 0 (Luminance AC)
		0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
			0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D
	}
	,
	{	// DC index 1 (Chrominance DC)
		0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
	}
	,
	{	// AC index 1 (Chrominance AC)
		0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
			0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
	}
};

const BYTE cDefHuffVal[4][162] =
{
	{	// DC index 0 (Luminance DC)
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B
	}
	,
	{	// AC index 0 (Luminance AC)
		0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
		0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
		0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
		0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
		0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
		0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
		0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
		0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
		0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
		0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
		0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
		0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
		0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
		0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
		0xF9, 0xFA
	}
	,
	{	// DC index 1 (Chrominance DC)
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B
	}
	,
	{	// AC index 1 (Chrominance AC)
		0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
		0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
		0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
		0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
		0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
		0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
		0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
		0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
		0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
		0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
		0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
		0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
		0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
		0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
		0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
		0xF9, 0xFA
	}
};

enum {
	Marker			= 0xFF,
	FF_Marker		= 0x00,

	SOI_Marker		= 0xFFD8,			// Start of image
	EOI_Marker		= 0xFFD9,			// End of image

	JFIF_CODE		= 0xFFE0,			// Application
	EXIF_CODE		= 0xFFE1,

	DRI_Marker		= 0xFFDD,			// Define restart interval
	RST_Marker		= 0xD,				// 0xD0 ~0xD7

	DQT_Marker		= 0xFFDB,			// Define quantization table(s)
	DHT_Marker		= 0xFFC4,			// Define Huffman table(s)

	SOF_Marker		= 0xFFC0,			// Start of frame : Baseline DCT
	SOS_Marker		= 0xFFDA,			// Start of scan
	NULL_Marker		= 0x7FFFFFFF
};

int check_start_code(JpgDecInfo *jpg)
{
	if (show_bits(&jpg->gbc, 8) == 0xFF)
		return 1;
	else
		return 0;
}

int find_start_code(JpgDecInfo *jpg)
{
	int word;

	while(1)
	{
		if (get_bits_left(&jpg->gbc) <= 16)
		{
			////printf("hit end of stream\n");
			return 0;
		}

		word = show_bits(&jpg->gbc, 16);
		if ((word > 0xFF00) && (word < 0xFFFF))
			break;


		get_bits(&jpg->gbc, 8);
	}

	return word;
}

int find_start_soi_code(JpgDecInfo *jpg)
{
	int word;

	while(1)
	{
		if (get_bits_left(&jpg->gbc) <= 16)
		{
			////printf("hit end of stream\n");
			return 0;
		}

		word = show_bits(&jpg->gbc, 16);
		if ((word > 0xFF00) && (word < 0xFFFF))
		{
			if (word != SOI_Marker)
				get_bits(&jpg->gbc, 8);
			break;
		}


		get_bits(&jpg->gbc, 8);
	}

	return word;
}

#	ifdef SUPPORT_MJPEG_ERROR_CONCEAL
int find_restart_marker(JpgDecInfo *jpg)
{
	int word;

	while(1)
	{
		if (get_bits_left(&jpg->gbc) <= 16)
		{
			////printf("hit end of stream\n");
			return -1;
		}

		word = show_bits(&jpg->gbc, 16);
		if ( ((word >= 0xFFD0) && (word <= 0xFFD7)) || (word == 0xFFD8)  || (word == 0xFFD9))
			break;

		get_bits(&jpg->gbc, 8);
	}

	return word;
}
#	endif

int decode_app_header(JpgDecInfo *jpg)
{
	int length;

	if (get_bits_left(&jpg->gbc) < 16)
		return 0;
	length = get_bits(&jpg->gbc, 16);
	length -= 2;

	while(length-- > 0)
	{
		if (get_bits_left(&jpg->gbc) < 8)
			return 0;
		get_bits(&jpg->gbc, 8);
	}

	return 1;
}

int decode_dri_header(JpgDecInfo *jpg)
{
	//Length, Lr
	if (get_bits_left(&jpg->gbc) < 16*2)
		return 0;

	get_bits(&jpg->gbc, 16);
	jpg->rstIntval = get_bits(&jpg->gbc, 16);

	return 1;
}

int decode_dqt_header(JpgDecInfo *jpg)
{
	int Pq;
	int Tq;
	int i;
	int tmp;

	if (get_bits_left(&jpg->gbc) < 16)
		return 0;
	// Lq, Length of DQT
	get_bits(&jpg->gbc, 16);

	do {
		if (get_bits_left(&jpg->gbc) < 4+4+8*64)
			return 0;

		// Pq, Quantization Precision
	tmp = get_bits(&jpg->gbc, 8);
		// Tq, Quantization table destination identifier
		Pq = (tmp>>4) & 0xf;
		Tq = tmp&0xf;
		if (Tq > 3)
			return 0;
		for (i=0; i<64; i++)
			jpg->qMatTab[Tq][i] = get_bits(&jpg->gbc, 8);
	} while(!check_start_code(jpg));

	if (Pq != 0) // not 8-bit
	{
		////printf("pq is not set to zero\n");
		return 0;
	}
	return 1;
}

int decode_dth_header(JpgDecInfo *jpg)
{
	int Tc;
	int Th;
	int ThTc;
	int bitCnt;
	int i;
	int tmp;

	// Length, Lh
	if (get_bits_left(&jpg->gbc) < 16)
		return 0;
	get_bits(&jpg->gbc, 16);

	do {
		if (get_bits_left(&jpg->gbc) < 8 + 8*16)
			return 0;

		// Table class - DC, AC
		tmp = get_bits(&jpg->gbc, 8);
		// Table destination identifier
		Tc = (tmp>>4) & 0xf;
		Th = tmp&0xf;
		if (Tc > 1 || Th > 3) {
			return 0;
		}

		// DC_ID0 (0x00) -> 0
		// AC_ID0 (0x10) -> 1
		// DC_ID1 (0x01) -> 2
		// AC_ID1 (0x11) -> 3
		ThTc = ((Th&1)<<1) | (Tc&1);

		// Get Huff Bits list
		bitCnt = 0;
		for (i=0; i<16; i++)
		{
			jpg->huffBits[ThTc][i] = get_bits(&jpg->gbc, 8);
			bitCnt += jpg->huffBits[ThTc][i];
			if (cDefHuffBits[ThTc][i] != jpg->huffBits[ThTc][i])
				jpg->userHuffTab = 1;
		}

		if ( bitCnt > 162 )
			return 0;

		if (get_bits_left(&jpg->gbc) <  8*bitCnt)
			return 0;
		// Get Huff Val list
		for (i=0; i<bitCnt; i++)
		{
			jpg->huffVal[ThTc][i] = get_bits(&jpg->gbc, 8);
			if (cDefHuffVal[ThTc][i] != jpg->huffVal[ThTc][i])
				jpg->userHuffTab = 1;
		}
	} while(!check_start_code(jpg));

	return 1;
}

int decode_sof_header(JpgDecInfo *jpg)
{
	int samplePrecision;
	int sampleFactor;
	int i;
	int Tqi;
	int compID;
	int hSampFact[3]={0};
	int vSampFact[3]={0};
	int picX, picY;
	int numComp;
	int tmp;

	if (get_bits_left(&jpg->gbc) < 16+8+16+16+8)
		return 0;
	// LF, Length of SOF
	get_bits(&jpg->gbc, 16);

	// Sample Precision: Baseline(8), P
	samplePrecision = get_bits(&jpg->gbc, 8);

	if (samplePrecision != 8)
	{
		//printf("Sample Precision is not 8\n");
		return 0;
	}

	picY = get_bits(&jpg->gbc, 16);
	if (picY > MAX_VSIZE)
	{
		//printf("Picture Vertical Size limits Maximum size\n");
		return 0;
	}

	picX = get_bits(&jpg->gbc, 16);
	if (picX > MAX_HSIZE)
	{
		//printf("Picture Horizontal Size limits Maximum size\n");
		return 0;
	}

	//Number of Components in Frame: Nf
	numComp = get_bits(&jpg->gbc, 8);
	if (numComp > 3)
	{
		//printf("Picture Horizontal Size limits Maximum size\n");
		return 0;
	}

	if (get_bits_left(&jpg->gbc) < numComp*(8+4+4+8))
		return 0;

	for (i=0; i<numComp; i++)
	{
		// Component ID, Ci 0 ~ 255
		compID = get_bits(&jpg->gbc, 8);

		tmp = get_bits(&jpg->gbc, 8);
		// Horizontal Sampling Factor, Hi
		hSampFact[i] = (tmp>>4) & 0xf;
		// Vertical Sampling Factor, Vi
		vSampFact[i] = tmp&0xf;

		// Quantization Table Selector, Tqi
		Tqi = get_bits(&jpg->gbc, 8);

		jpg->cInfoTab[i][0] = compID;
		jpg->cInfoTab[i][1] = hSampFact[i];
		jpg->cInfoTab[i][2] = vSampFact[i];
		jpg->cInfoTab[i][3] = Tqi;
	}

	//if ( hSampFact[0]>2 || vSampFact[0]>2 || ( numComp == 3 && ( hSampFact[1]!=1 || hSampFact[2]!=1 || vSampFact[1]!=1 || vSampFact[2]!=1) ) )
		//printf("Not Supported Sampling Factor\n");

	if (numComp == 1)
		sampleFactor = SAMPLE_400;
	else
		sampleFactor = ((hSampFact[0]&3)<<2) | (vSampFact[0]&3);

	switch(sampleFactor) {
	case SAMPLE_420:
		jpg->format = FORMAT_420;
		break;
	case SAMPLE_H422:
		jpg->format = FORMAT_422;
		break;
	case SAMPLE_V422:
		jpg->format = FORMAT_224;
		break;
	case SAMPLE_444:
		jpg->format = FORMAT_444;
		break;
	default:	// 4:0:0
		jpg->format = FORMAT_400;
	}

	jpg->picWidth = picX;
	jpg->picHeight = picY;

	return 1;
}

int decode_sos_header(JpgDecInfo *jpg)
{
	int i, j;
	int len;
	int numComp;
	int compID;
	int ecsPtr;
	int ss, se, ah, al;
	int dcHufTblIdx[3];
	int acHufTblIdx[3];
	int tmp;

	if (get_bits_left(&jpg->gbc) < 8)
		return 0;
	// Length, Ls
	len = get_bits(&jpg->gbc, 16);

	jpg->ecsPtr = get_bits_count(&jpg->gbc)/8 + len - 2 ;

	ecsPtr = jpg->ecsPtr+jpg->frameOffset;

	jpg->pagePtr = ecsPtr/256;
	jpg->wordPtr = (ecsPtr % 256) / 4;	// word unit
	if (jpg->pagePtr & 1)
		jpg->wordPtr += 64;
	if (jpg->wordPtr & 1)
		jpg->wordPtr -= 1; // to make even.

	jpg->bitPtr = (ecsPtr % 4) * 8;	// bit unit
	if (((ecsPtr % 256) / 4) & 1)
		jpg->bitPtr += 32;

	if (get_bits_left(&jpg->gbc) < 8)
		return 0;
	//Number of Components in Scan: Ns
	numComp = get_bits(&jpg->gbc, 8);
	if (numComp > 3)
		return 0;

	if (get_bits_left(&jpg->gbc) < numComp*(8+4+4))
		return 0;
	for (i=0; i<numComp; i++) {
		// Component ID, Csj 0 ~ 255
		compID = get_bits(&jpg->gbc, 8);

		tmp = get_bits(&jpg->gbc, 8);
		// dc entropy coding table selector, Tdj
		dcHufTblIdx[i] = (tmp>>4) & 0xf;
		// ac entropy coding table selector, Taj
		acHufTblIdx[i] = tmp&0xf;


		for (j=0; j<numComp; j++)
		{
			if (compID == jpg->cInfoTab[j][0])
			{
				jpg->cInfoTab[j][4] = dcHufTblIdx[i];
				jpg->cInfoTab[j][5] = acHufTblIdx[i];
			}
		}
	}

	if (get_bits_left(&jpg->gbc) < 8+8+4+4)
		return 0;
	// Ss 0
	ss = get_bits(&jpg->gbc, 8);
	// Se 3F
	se = get_bits(&jpg->gbc, 8);

	tmp = get_bits(&jpg->gbc, 8);
	// Ah 0
	ah = (i>>4) & 0xf;
	// Al 0
	al = tmp&0xf;

	if ((ss != 0) || (se != 0x3F) || (ah != 0) || (al != 0))
	{
		//printf("The Jpeg Image must be another profile\n");
		return 0;
	}

	return 1;
}

static void genDecHuffTab(JpgDecInfo *jpg, int tabNum)
{
	unsigned char *huffPtr, *huffBits;
	unsigned int *huffMax, *huffMin;

	int ptrCnt =0;
	int huffCode = 0;
	int zeroFlag = 0;
	int dataFlag = 0;
	int i;

	huffBits	= jpg->huffBits[tabNum];
	huffPtr		= jpg->huffPtr[tabNum];
	huffMax		= jpg->huffMax[tabNum];
	huffMin		= jpg->huffMin[tabNum];

	for (i=0; i<16; i++)
	{
		if (huffBits[i]) // if there is bit cnt value
		{
			huffPtr[i] = ptrCnt;
			ptrCnt += huffBits[i];
			huffMin[i] = huffCode;
			huffMax[i] = huffCode + (huffBits[i] - 1);
			dataFlag = 1;
			zeroFlag = 0;
		}
		else
		{
			huffPtr[i] = 0xFF;
			huffMin[i] = 0xFFFF;
			huffMax[i] = 0xFFFF;
			zeroFlag = 1;
		}

		if (dataFlag == 1)
		{
			if (zeroFlag == 1)
				huffCode <<= 1;
			else
				huffCode = (huffMax[i] + 1) << 1;
		}
	}
}

#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
int JpegDecodeConcealError(DecInfo *pDecInfo)
{
	unsigned int code;
	int ret, foced_stop = 0;
	BYTE *b;
	Uint32 ecsPtr = 0;
	Uint32 size, wrOffset;
	Uint32 nextMarkerPtr, chunkSize;
	Uint32 iRstIdx;
	int numMcu = 0, numMcuCol = 0;
	JpgDecInfo *jpg = &pDecInfo->jpgInfo;


	// "nextOffset" is distance between frameOffset and next_restart_index that exist.
	nextMarkerPtr = jpg->nextOffset + jpg->frameOffset;

	if(pDecInfo->jpgInfo.lineBufferMode == 1)
	{
		chunkSize = VpuReadReg(MJPEG_BBC_END_ADDR_REG);
		size =  chunkSize - nextMarkerPtr;
	}
	else
	{

		if (pDecInfo->streamWrPtr == pDecInfo->streamBufStartAddr)
		{
			size    = pDecInfo->streamBufSize - nextMarkerPtr;
			wrOffset= pDecInfo->streamBufSize;
		}
		else
		{
			size    = pDecInfo->streamWrPtr - nextMarkerPtr;
			wrOffset= (pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr);
		}
	}

	//pointing to find next_restart_marker
	b = jpg->pBitStream + nextMarkerPtr;
	init_get_bits(&jpg->gbc, b, size*8);

	while(1)
	{

		ret = find_restart_marker(jpg);
	#ifdef MJPEG_ERROR_CONCEAL_WORKAROUND
		jpg->errorRstIdx = (ret & 0xF);
	#endif
		if ( ret < 0)
		{
			// if next_start_maker meet end of picture but decoding is not completed yet,
			// To prevent over picture boundary in ringbuffer mode,
			// finding previous maker and make decoding forced to stop.
			if(pDecInfo->jpgInfo.lineBufferMode)
			{
				size = chunkSize - jpg->currOffset;
				b    = jpg->pBitStream + jpg->currOffset;
				init_get_bits(&jpg->gbc, b, size*8);

				nextMarkerPtr = jpg->currOffset;
				foced_stop = 1;

				continue;
			}
			// ringbuffer case.
			else
				break;
		}

		code = get_bits(&jpg->gbc, 16);
		if ((code == 0xFFD8) || (code == 0xFFD9))
		{
			// if next_start_maker meet EOI(end of picture) but decoding is not completed yet,
			// To prevent over picture boundary in ringbuffer mode,
			// finding previous maker and make decoding forced to stop.
			b = jpg->pBitStream + jpg->currOffset + jpg->frameOffset;
			init_get_bits(&jpg->gbc, b, size*8);

			nextMarkerPtr = jpg->currOffset + jpg->frameOffset;
			foced_stop = 1;
			continue;
		}

		iRstIdx = (code & 0xF);
		// you can find next restart marker which will be.
		if( iRstIdx >= 0 && iRstIdx <= 7)
		{
		#ifdef MJPEG_ERROR_CONCEAL_DEBUG
			int numMcuRow;
			//int numRstMakerRounding = RstMakerRounding;
		#endif
			if (get_bits_left(&jpg->gbc) < 8)
				return (-1);

			jpg->ecsPtr = get_bits_count(&jpg->gbc)/8;
			// set ecsPtr that restart marker is founded.
			ecsPtr = jpg->ecsPtr + nextMarkerPtr;

			jpg->pagePtr = ecsPtr/256;
		#ifdef MJPEG_ERROR_CONCEAL_DEBUG
			//jpg->wordPtr = (ecsPtr % 256) / 4;	                     // word unit
			jpg->wordPtr = ((ecsPtr % 256) & 0xF0) / 4;	                     // word unit
		#endif
			if (jpg->pagePtr & 1)
				jpg->wordPtr += 64;
			if (jpg->wordPtr & 1)
				jpg->wordPtr -= 1;                                  // to make even.
		#ifdef MJPEG_ERROR_CONCEAL_DEBUG
			jpg->bitPtr = (ecsPtr & 0xF) * 8;	                       // bit unit
			/*
			jpg->bitPtr = (ecsPtr % 4) * 8;	                        // bit unit
			if (((ecsPtr % 256) / 4) & 1)
				jpg->bitPtr += 32;
			*/
		#endif

			numMcuRow = (jpg->alignedWidth / jpg->mcuWidth);
			numMcuCol = (jpg->alignedHeight/ jpg->mcuHeight);

			// num of restart marker that is rounded can be calculated from error position.
		#ifdef MJPEG_ERROR_CONCEAL_DEBUG
			//numRstMakerRounding = ((jpg->errInfo.errPosY*numMcuRow + jpg->errInfo.errPosX) / (jpg->rstIntval*8));
		#endif

			jpg->curRstIdx = iRstIdx;
			if(jpg->curRstIdx < jpg->nextRstIdx)
				jpg->numRstMakerRounding++;

			// find mcu position to restart.
			numMcu = (jpg->numRstMakerRounding*jpg->rstIntval*8) + (jpg->curRstIdx + 1)*jpg->rstIntval;
			jpg->setPosX = (numMcu % numMcuRow);
			jpg->setPosY = (numMcu / numMcuRow);
			jpg->gbuStartPtr = ((ecsPtr- jpg->frameOffset)/256)*256;

			// if setPosY is greater than Picture Height, set mcu position to last mcu of picture to finish decoding.
			if((jpg->setPosY > numMcuCol) || foced_stop)
			{
				jpg->setPosX = numMcuRow - 1;
				jpg->setPosY = numMcuCol - 1;
			}

			// update restart_index to find next.
			jpg->nextRstIdx = iRstIdx++;

		#ifdef MJPG_CONCEAL_DEBUG
			printf("[MJPG CONCEAL] search_ptr : 0x%x,  ecs_ptr ; 0x%x restart Idx : %d MCU pos : %d x : %d y : %d\n",
				nextMarkerPtr, ecsPtr, jpg->curRstIdx, numMcu, jpg->setPosX, jpg->setPosY);
		#endif
			ret = 0;
			break;
		}
	}

	// prevent to find same position.
	jpg->currOffset = jpg->nextOffset;

	// if the distance between ecsPtr and streamBufEndAddr is less than 512byte, that rdPtr run over than streamBufEndAddr without interrupt.
	// bellow is workaround to avoid this case.
	if(pDecInfo->jpgInfo.lineBufferMode == 0 && (pDecInfo->streamBufEndAddr - ecsPtr < JPU_GBU_SIZE) )
	{
		BYTE *dst;
		BYTE *src;
		BYTE *data;
		int data_size;
		int src_size;
		int dst_size;

		data_size = pDecInfo->streamWrPtr - pDecInfo->streamBufStartAddr;
		data = (BYTE *)malloc(data_size);

		if (data_size && data)
			VpuReadMem(pDecInfo->streamBufStartAddr, data, data_size, pDecInfo->openParam.streamEndian);

		// cut in remaining stream buffer and paste it to streamBufStartAddr.
		src_size = pDecInfo->streamBufSize - nextMarkerPtr;
		src = (BYTE *)malloc(src_size);
		dst_size = ((src_size+(JPU_GBU_SIZE-1))&~(JPU_GBU_SIZE-1));
		dst = (BYTE *)malloc(dst_size);
		if (dst && src)
		{
			VpuReadMem(pDecInfo->streamBufStartAddr + nextMarkerPtr, src, src_size, pDecInfo->openParam.streamEndian);
			memcpy(dst+(dst_size-src_size), src, src_size);
			VpuWriteMem(pDecInfo->streamBufStartAddr, dst, dst_size, pDecInfo->openParam.streamEndian);
			if (data_size && data)
				VpuWriteMem(pDecInfo->streamBufStartAddr+dst_size, data, data_size, pDecInfo->openParam.streamEndian);
			free(src);
			free(dst);
		}

		if (data_size && data)
			free(data);

		pDecInfo->streamWrPtr = pDecInfo->streamBufStartAddr+dst_size+data_size;
		ret = (-1);
	}
	return ret;
}
#endif

static void thumbRaw(DecInfo *pDecInfo, BYTE pal[][3], int picX, int picY)
{
	int i;
	int idx;

	for (i = 0; i < 256; i++)
	{
		pal[i][0] = get_bits(&pDecInfo->jpgInfo.gbc, 8);
		pal[i][1] = get_bits(&pDecInfo->jpgInfo.gbc, 8);
		pal[i][2] = get_bits(&pDecInfo->jpgInfo.gbc, 8);
	}

	for (i=0; i<picX*picY*3; i++)
	{
		idx = get_bits(&pDecInfo->jpgInfo.gbc, 8);
	}
}


int ParseJFIF(DecInfo *pDecInfo, int jfif, int length)
{
	int exCode;
	int pal = 0;
	int picX, picY;
	THUMB_INFO *pThumbInfo;
	pThumbInfo = &(pDecInfo->jpgInfo.ThumbInfo);

	if (pThumbInfo->ThumbType == EXIF_JPG)
	{
		if(jfif)
		{
			get_bits(&pDecInfo->jpgInfo.gbc, 16);
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
			get_bits(&pDecInfo->jpgInfo.gbc, 16);
			get_bits(&pDecInfo->jpgInfo.gbc, 16);
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
			get_bits(&pDecInfo->jpgInfo.gbc, 8);

			length -= 9;
		}
		else
		{
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
			length -= 3;
		}

		return length;

	}
	if (jfif)						//JFIF
	{
		pThumbInfo->ThumbType = JFIF;
		pThumbInfo->Version = get_bits(&pDecInfo->jpgInfo.gbc, 16);
		get_bits(&pDecInfo->jpgInfo.gbc, 8);
		get_bits(&pDecInfo->jpgInfo.gbc, 16);
		get_bits(&pDecInfo->jpgInfo.gbc, 16);

		picX = pDecInfo->jpgInfo.picWidth = get_bits(&pDecInfo->jpgInfo.gbc, 8);
		picY = pDecInfo->jpgInfo.picHeight = get_bits(&pDecInfo->jpgInfo.gbc, 8);

		if (pDecInfo->jpgInfo.picWidth != 0 && pDecInfo->jpgInfo.picHeight != 0)
		{
			pDecInfo->jpgInfo.thumbInfo.MbNumX = (picX + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbNumY = (picY + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbSize = 192;
			pDecInfo->jpgInfo.thumbInfo.DecFormat = FORMAT_444;
		}

		length -= 9;
	}
	else							//JFXX
	{
		exCode = get_bits(&pDecInfo->jpgInfo.gbc, 8);
		length -= 1;
		if (exCode == 0x10)
		{
			pThumbInfo->ThumbType = JFXX_JPG;
		}
		else if (exCode == 0x11)
		{
			picX = pDecInfo->jpgInfo.picWidth = get_bits(&pDecInfo->jpgInfo.gbc, 8);
			picY = pDecInfo->jpgInfo.picHeight = get_bits(&pDecInfo->jpgInfo.gbc, 8);

			pDecInfo->jpgInfo.thumbInfo.MbNumX = (picX + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbNumY = (picY + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbSize = 192;
			pThumbInfo->ThumbType = JFXX_PAL;
			pDecInfo->jpgInfo.thumbInfo.DecFormat = FORMAT_444;
			length -= 2;

		}
		else if (exCode == 0x13)
		{
			picX = pDecInfo->jpgInfo.picWidth = get_bits(&pDecInfo->jpgInfo.gbc, 8);
			picY = pDecInfo->jpgInfo.picHeight = get_bits(&pDecInfo->jpgInfo.gbc, 8);

			pDecInfo->jpgInfo.thumbInfo.MbNumX = (picX + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbNumY = (picY + 7)/8;
			pDecInfo->jpgInfo.thumbInfo.MbSize = 192;
			pThumbInfo->ThumbType = JFXX_RAW;
			pDecInfo->jpgInfo.thumbInfo.DecFormat = FORMAT_444;
			length -= 2;
		}
		else
			return length;

	}

	return length;
}

int ParseEXIF(DecInfo *pDecInfo, int length)
{
	int i;
	Uint32 iFDValOffset = 0;
	Uint32 runIdx = 0;
	Uint32 j;
	Uint32 ifdSize;
	Uint32 nextIFDOffset;
	Uint32 size;
	TAG tags;
	BYTE id;
	int endian;
	BYTE big_e = 1;
	BYTE little_e = 1;
	THUMB_INFO *pThumbInfo;
	pThumbInfo = &(pDecInfo->jpgInfo.ThumbInfo);

	//------------------------------------------------------------------------------
	// TIFF HEADER, {endian[1:0],0x002A,offset(4bytes)}
	//------------------------------------------------------------------------------

	for (i = 0; i < 2; i++)
	{
		id = (BYTE) get_bits(&pDecInfo->jpgInfo.gbc, 8);

		if (id != lendian[i])
			little_e = 0;
		if (id != bendian[i])
			big_e = 0;
	}
	length -= 2;

//	if (little_e == 0 && big_e == 0)
//		printf("ERROR\n");

	endian = (little_e) ? VDI_LITTLE_ENDIAN : VDI_BIG_ENDIAN;

	tGetBits(pDecInfo, endian, 2);
	length -= 2;

	size = tGetBits(pDecInfo, endian, 4) -8;
	length -= 4;

	for (j = 0; j < size; j++)
	{
		get_bits(&pDecInfo->jpgInfo.gbc, 8);
		length -= 1;
	}

	//------------------------------------------------------------------------------
	// 0TH IFD
	//------------------------------------------------------------------------------

	ifdSize = tGetBits(pDecInfo, endian, 2);
	length -= 2;

	for (j = 0; j < ifdSize; j++)
	{
		tGetBits(pDecInfo, endian, 2); //Tag
		tGetBits(pDecInfo, endian, 2); //Type
		tGetBits(pDecInfo, endian, 4); //count
		tGetBits(pDecInfo, endian, 4); //offset
		length -= 12;
	}

	nextIFDOffset = tGetBits(pDecInfo, endian, 4);
	length -= 4;

	if(nextIFDOffset == 0x00)
	{
		while (length--)
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
		return length;
	}
	else if ((int) nextIFDOffset > length)
	{
		while (length--)
			get_bits(&pDecInfo->jpgInfo.gbc, 8);
		return length;
	}
	nextIFDOffset -= (ifdSize *12 + 10 + size + 4);

	for (j = 0; j < nextIFDOffset; j++)
	{
		get_bits(&pDecInfo->jpgInfo.gbc, 8);
		length -= 1;
	}
	runIdx += (8 + size + 2 + ifdSize*12 + 4 + nextIFDOffset);

	//------------------------------------------------------------------------------
	// 1TH IFD, thumbnail
	//------------------------------------------------------------------------------

	ifdSize = tGetBits(pDecInfo, endian, 2);
	length -= 2;
	for (j=0; j<ifdSize; j++) {
		tags.tag    = tGetBits(pDecInfo,endian,2); // Tag
		tags.type   = tGetBits(pDecInfo, endian,2); // Type
		tags.count  = tGetBits(pDecInfo, endian,4); // count
		tags.offset = tGetBits(pDecInfo, endian,4); // offset
		length -= 12;

		if (endian != VDI_LITTLE_ENDIAN) {
			if (tags.type == 1 && tags.count < 4)
				tags.offset >>= (4 - tags.count) * 8;
			if (tags.type == 3 && tags.count < 2)
				tags.offset >>= (2 - tags.count) * 16;
		}

		switch(tags.tag&0xFFFF) {
			case IMAGE_WIDTH :
				pThumbInfo->ExifInfo.PicX = tags.offset;
				break;
			case IMAGE_HEIGHT :
				pThumbInfo->ExifInfo.PicY = tags.offset;
				break;
			case BITS_PER_SAMPLE :
				pThumbInfo->ExifInfo.BitPerSample[0] = tags.offset;
				break;
			case COMPRESSION_SCHEME :
				pThumbInfo->ThumbType = EXIF_JPG;
				pThumbInfo->ExifInfo.Compression = tags.offset & 0xffff;
				break;
			case PIXEL_COMPOSITION :
				pThumbInfo->ExifInfo.PixelComposition = tags.offset;
				break;
			case SAMPLE_PER_PIXEL :
				pThumbInfo->ExifInfo.SamplePerPixel = tags.offset;
				break;
			case YCBCR_SUBSAMPLING : // 2, 1 4:2:2 / 2, 2 4:2:0
				pThumbInfo->ExifInfo.YCbCrSubSample = tags.offset;
				break;
			case JPEG_IC_FORMAT :
				pThumbInfo->ExifInfo.JpegOffset = tags.offset;
				break;
			case PLANAR_CONFIG :
				pThumbInfo->ExifInfo.PlanrConfig = tags.offset;
				break;
			default :
				break;
		}

		if (tags.type == 2)
			iFDValOffset += tags.count;
		else if (tags.type == 3 && tags.count > 2)
			iFDValOffset += (tags.count*2);
		else if (tags.type == 5 || tags.type == 0xA)
			iFDValOffset += (tags.count*8);
	}

	if (pThumbInfo->ExifInfo.Compression == 6) { // jpeg
		runIdx += (2 + ifdSize*12);
		iFDValOffset = pThumbInfo->ExifInfo.JpegOffset - runIdx;
	}

	for (j=0; j<iFDValOffset; j++)
	{
		get_bits(&pDecInfo->jpgInfo.gbc, 8);
		length -= 1;
	}

	return length;
}

static Uint32 tGetBits(DecInfo *pDecInfo, int endian, int byteCnt)
{
	int i;
	BYTE byte;
	Uint32 retData = 0;

	for (i=0; i<byteCnt; i++) {

		byte = (BYTE)get_bits(&pDecInfo->jpgInfo.gbc, 8);

		if (endian)
			retData = (retData<<8) | byte;
		else
			retData = retData | (byte<<((i&3)*8));
	}

	return retData;
}

int CheckThumbNail(DecInfo *pDecInfo)
{
	BYTE id;
	BYTE jfifFlag = 1;
	BYTE jfxxFlag = 1;
	BYTE exifFlag = 1;
	int i;
	int length;
	int initLength;

	THUMB_INFO *pThumbInfo;
	pThumbInfo = &(pDecInfo->jpgInfo.ThumbInfo);

	length = get_bits(&pDecInfo->jpgInfo.gbc, 16);
	length -= 2;

	initLength = length;

	for (i = 0; i < 4; i++)
	{
		id = (BYTE) get_bits(&pDecInfo->jpgInfo.gbc, 8);

		if (id != jfif[i])
			jfifFlag = 0;
		if (id != jfxx[i])
			jfxxFlag = 0;
		if (id != exif[i])
			exifFlag = 0;
	}
	get_bits(&pDecInfo->jpgInfo.gbc, 8);
	length -= 5;

	if (exifFlag)
	{
		get_bits(&pDecInfo->jpgInfo.gbc, 8);
		length -= 1;
	}
	if (initLength >= 5)
	{
		if (jfifFlag | jfxxFlag)	//JFIF
		{
			length = ParseJFIF(pDecInfo, jfifFlag, length);
			if (pThumbInfo->ThumbType != EXIF_JPG)
			{
				if(pThumbInfo->ThumbType != JFXX_JPG)
				{
					//RAW data
					thumbRaw (pDecInfo, pThumbInfo->Pallette, pDecInfo->jpgInfo.picWidth, pDecInfo->jpgInfo.picHeight);
				}
			}
		}
		else if (exifFlag)			//EXIF
		{
			length = ParseEXIF(pDecInfo, length);
			if (length == -1)
				return 0;
		}
	}
	else
	{
		while(length--)
			get_bits(&pDecInfo->jpgInfo.gbc, 8);

	}

	return 0;
}

int JpegDecodeHeader(DecInfo *pDecInfo)
{

	unsigned int code;
	int ret;
	int i;
	int temp;
	int wrOffset=0;
	BYTE *b;
	int size;

	ret = 1;

	if (pDecInfo->jpgInfo.lineBufferMode)
	{
		b = pDecInfo->jpgInfo.pJpgChunkBase;
		size = pDecInfo->jpgInfo.chunkSize;
	}
	else
	{
		b = pDecInfo->jpgInfo.pBitStream+pDecInfo->jpgInfo.frameOffset;
		if (pDecInfo->streamWrPtr == pDecInfo->streamBufStartAddr)
		{
			size = pDecInfo->streamBufSize-pDecInfo->jpgInfo.frameOffset;
			wrOffset = pDecInfo->streamBufSize;
		}
		else
		{
			if ((pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr) < pDecInfo->jpgInfo.frameOffset)
				size = pDecInfo->streamBufSize - pDecInfo->jpgInfo.frameOffset;
			else
				size = (pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr)-pDecInfo->jpgInfo.frameOffset;
			wrOffset = (pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr);
		}

		if (!b || !size) {
			ret = -1;
			goto DONE_DEC_HEADER;
		}

		// find start code of next frame
		if (!pDecInfo->jpgInfo.ecsPtr)
		{
			int nextOffset = 0;
			int soiOffset = 0;

			if (pDecInfo->jpgInfo.consumeByte != 0)	// meaning is frameIdx > 0
			{
			#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
				nextOffset = pDecInfo->jpgInfo.nextOffset;
			#else
				nextOffset = pDecInfo->jpgInfo.consumeByte;
			#endif
				if (nextOffset <= 0)
					nextOffset = 2;	//in order to consume start code.
			}

			//consume to find the start code of next frame.
			b += nextOffset;

			if (b > pDecInfo->jpgInfo.pBitStream+pDecInfo->streamBufSize)
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}

			size -= nextOffset;

			if (size < 0)
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}

			init_get_bits(&pDecInfo->jpgInfo.gbc, b, size*8);

			for (;;)
			{
				code = find_start_soi_code(&pDecInfo->jpgInfo);
				if (code == 0)
				{
					ret = -1;
					goto DONE_DEC_HEADER;
				}

				if (code == SOI_Marker)
					break;

			}

			soiOffset = get_bits_count(&pDecInfo->jpgInfo.gbc)/8;

			b += soiOffset;
			size -= soiOffset;
			pDecInfo->jpgInfo.frameOffset += (soiOffset+ nextOffset);
		}
	}

	init_get_bits(&pDecInfo->jpgInfo.gbc, b, size*8);

	// Initialize component information table
	for (i=0; i<4; i++)
	{
		pDecInfo->jpgInfo.cInfoTab[i][0] = 0;
		pDecInfo->jpgInfo.cInfoTab[i][1] = 0;
		pDecInfo->jpgInfo.cInfoTab[i][2] = 0;
		pDecInfo->jpgInfo.cInfoTab[i][3] = 0;
		pDecInfo->jpgInfo.cInfoTab[i][4] = 0;
		pDecInfo->jpgInfo.cInfoTab[i][5] = 0;
	}

	for (;;)
	{
		if (find_start_code(&pDecInfo->jpgInfo) == 0)
		{
			ret = -1;
			goto DONE_DEC_HEADER;
		}

		code = get_bits(&pDecInfo->jpgInfo.gbc, 16);

		switch (code) {
		case SOI_Marker:
			break;
		case JFIF_CODE:
		case EXIF_CODE:
			if ((pDecInfo->jpgInfo.thumbNailEn) == 1)
			{
				CheckThumbNail(pDecInfo);
				if(pDecInfo->jpgInfo.ThumbInfo.ThumbType != JFXX_JPG && pDecInfo->jpgInfo.ThumbInfo.ThumbType != EXIF_JPG)
				{
//					printf("[INFO] Unable to decode thumbnail from JPEG image.\n");
				}
			}
			else
			{
				if (!decode_app_header(&pDecInfo->jpgInfo))
				{
					ret = -1;
					goto DONE_DEC_HEADER;
				}
			}
			break;
		case DRI_Marker:
			if (!decode_dri_header(&pDecInfo->jpgInfo))
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}
			break;
		case DQT_Marker:
			if (!decode_dqt_header(&pDecInfo->jpgInfo))
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}
			break;
		case DHT_Marker:
			if (!decode_dth_header(&pDecInfo->jpgInfo))
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}
			break;
		case SOF_Marker:
			if (!decode_sof_header(&pDecInfo->jpgInfo))
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}
			break;
		case SOS_Marker:
			if (!decode_sos_header(&pDecInfo->jpgInfo))
			{
				ret = -1;
				goto DONE_DEC_HEADER;
			}
			goto DONE_DEC_HEADER;
			break;
		case EOI_Marker:
			goto DONE_DEC_HEADER;
		default:
			switch (code&0xFFF0)
			{
			case 0xFFE0:	// 0xFFEX
			case 0xFFF0:	// 0xFFFX
				if (get_bits_left(&pDecInfo->jpgInfo.gbc) <=0 ) {
					{
						ret = -1;
						goto DONE_DEC_HEADER;
					}
				}
				else
				{
					if (!decode_app_header(&pDecInfo->jpgInfo))
					{
						ret = -1;
						goto DONE_DEC_HEADER;
					}
					break;
				}
			default:
				//printf("code = [%x]\n", code);
				return	0;
			}
			break;
		}
	}

DONE_DEC_HEADER:

	if (pDecInfo->jpgInfo.lineBufferMode)
	{
		if (ret == -1)
			return -1;
	}
	else
	{
		if (ret == -1)
		{
			if (wrOffset < pDecInfo->jpgInfo.frameOffset)
				return -2;

			return -1;
		}
	}

	if (!pDecInfo->jpgInfo.ecsPtr)
		return 0;

	// Generate Huffman table information
	for(i=0; i<4; i++)
		genDecHuffTab(&pDecInfo->jpgInfo, i);

	// Q Idx
	temp =             pDecInfo->jpgInfo.cInfoTab[0][3];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[1][3];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[2][3];
	pDecInfo->jpgInfo.Qidx = temp;

	// Huff Idx[DC, AC]
	temp =             pDecInfo->jpgInfo.cInfoTab[0][4];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[1][4];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[2][4];
	pDecInfo->jpgInfo.huffDcIdx = temp;

	temp =             pDecInfo->jpgInfo.cInfoTab[0][5];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[1][5];
	temp = temp << 1 | pDecInfo->jpgInfo.cInfoTab[2][5];
	pDecInfo->jpgInfo.huffAcIdx = temp;

	switch(pDecInfo->jpgInfo.format)
	{
	case FORMAT_420:
		pDecInfo->jpgInfo.busReqNum = 2;
		pDecInfo->jpgInfo.mcuBlockNum = 6;
		pDecInfo->jpgInfo.compNum = 3;
		pDecInfo->jpgInfo.compInfo[0] = 10;
		pDecInfo->jpgInfo.compInfo[1] = 5;
		pDecInfo->jpgInfo.compInfo[2] = 5;
		pDecInfo->jpgInfo.alignedWidth = ((pDecInfo->jpgInfo.picWidth+15)&~15);
		pDecInfo->jpgInfo.alignedHeight = ((pDecInfo->jpgInfo.picHeight+15)&~15);
	#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
		pDecInfo->jpgInfo.mcuWidth  = 16;
		pDecInfo->jpgInfo.mcuHeight = 16;
	#endif
		break;
	case FORMAT_422:
		pDecInfo->jpgInfo.busReqNum = 3;
		pDecInfo->jpgInfo.mcuBlockNum = 4;
		pDecInfo->jpgInfo.compNum = 3;
		pDecInfo->jpgInfo.compInfo[0] = 9;
		pDecInfo->jpgInfo.compInfo[1] = 5;
		pDecInfo->jpgInfo.compInfo[2] = 5;
		pDecInfo->jpgInfo.alignedWidth = ((pDecInfo->jpgInfo.picWidth+15)&~15);
		pDecInfo->jpgInfo.alignedHeight = ((pDecInfo->jpgInfo.picHeight+7)&~7);
	#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
		pDecInfo->jpgInfo.mcuWidth  = 16;
		pDecInfo->jpgInfo.mcuHeight = 8;
	#endif
		break;
	case FORMAT_224:
		pDecInfo->jpgInfo.busReqNum = 3;
		pDecInfo->jpgInfo.mcuBlockNum = 4;
		pDecInfo->jpgInfo.compNum = 3;
		pDecInfo->jpgInfo.compInfo[0] = 6;
		pDecInfo->jpgInfo.compInfo[1] = 5;
		pDecInfo->jpgInfo.compInfo[2] = 5;
		pDecInfo->jpgInfo.alignedWidth = ((pDecInfo->jpgInfo.picWidth+7)&~7);
		pDecInfo->jpgInfo.alignedHeight = ((pDecInfo->jpgInfo.picHeight+15)&~15);
	#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
		pDecInfo->jpgInfo.mcuWidth  = 8;
		pDecInfo->jpgInfo.mcuHeight = 16;
	#endif
		break;
	case FORMAT_444:
		pDecInfo->jpgInfo.busReqNum = 4;
		pDecInfo->jpgInfo.mcuBlockNum = 3;
		pDecInfo->jpgInfo.compNum = 3;
		pDecInfo->jpgInfo.compInfo[0] = 5;
		pDecInfo->jpgInfo.compInfo[1] = 5;
		pDecInfo->jpgInfo.compInfo[2] = 5;
		pDecInfo->jpgInfo.alignedWidth = ((pDecInfo->jpgInfo.picWidth+7)&~7);
		pDecInfo->jpgInfo.alignedHeight = ((pDecInfo->jpgInfo.picHeight+7)&~7);
	#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
		pDecInfo->jpgInfo.mcuWidth  = 8;
		pDecInfo->jpgInfo.mcuHeight = 8;
	#endif
		break;
	case FORMAT_400:
		pDecInfo->jpgInfo.busReqNum = 4;
		pDecInfo->jpgInfo.mcuBlockNum = 1;
		pDecInfo->jpgInfo.compNum = 1;
		pDecInfo->jpgInfo.compInfo[0] = 5;
		pDecInfo->jpgInfo.compInfo[1] = 0;
		pDecInfo->jpgInfo.compInfo[2] = 0;
		pDecInfo->jpgInfo.alignedWidth = ((pDecInfo->jpgInfo.picWidth+7)&~7);
		pDecInfo->jpgInfo.alignedHeight = ((pDecInfo->jpgInfo.picHeight+7)&~7);
	#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
		pDecInfo->jpgInfo.mcuWidth  = 8;
		pDecInfo->jpgInfo.mcuHeight = 8;
	#endif
		break;
	}

	return 1;
}

RetCode CheckEncInstanceValidity(EncHandle handle)
{
	CodecInst * pCodecInst;
	RetCode ret;

	pCodecInst = handle;
	ret = CheckInstanceValidity(pCodecInst);
	if (ret != RETCODE_SUCCESS) {
		return RETCODE_INVALID_HANDLE;
	}
	if (!pCodecInst->inUse) {
		return RETCODE_INVALID_HANDLE;
	}

	if (pCodecInst->codecMode != AVC_ENC)
	{
		return RETCODE_INVALID_HANDLE;
	}
	return RETCODE_SUCCESS;
}

RetCode CheckEncOpenParam(EncOpenParam * pop)
{
	int picWidth;
	int picHeight;

	if (pop == 0) {
		return RETCODE_INVALID_PARAM;
	}
	picWidth = pop->picWidth;
	picHeight = pop->picHeight;

	if (pop->bitstreamBuffer % 8) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->bitstreamBufferSize % 1024 ||
			pop->bitstreamBufferSize < 1024 ||
			pop->bitstreamBufferSize > (256*1024*1024-1) ) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->workBuffer % 8) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->workBufferSize < WORK_BUF_SIZE) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->bitstreamFormat != STD_AVC)
	{
		return RETCODE_INVALID_PARAM;
	}
	if (pop->bitRate > 32767 || pop->bitRate < 0) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->bitRate !=0 && pop->initialDelay > 32767) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->bitRate !=0 && pop->initialDelay != 0 && pop->vbvBufferSize < 0) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->enableAutoSkip != 0 && pop->enableAutoSkip != 1) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->gopSize > 32767/*64*/) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->slicemode.sliceMode != 0 && pop->slicemode.sliceMode != 1) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->slicemode.sliceMode == 1) {
		if (pop->slicemode.sliceSizeMode != 0 && pop->slicemode.sliceSizeMode != 1) {
			return RETCODE_INVALID_PARAM;
		}
		if (pop->slicemode.sliceSizeMode == 1 && pop->slicemode.sliceSize == 0 ) {
			return RETCODE_INVALID_PARAM;
		}
	}
	if (pop->intraRefresh < 0) {
		return RETCODE_INVALID_PARAM;
	}

	if (pop->MEUseZeroPmv != 0 && pop->MEUseZeroPmv != 1) {
		return RETCODE_INVALID_PARAM;
	}
	if (pop->IntraCostWeight < 0 || pop->IntraCostWeight >= 65535) {
		return RETCODE_INVALID_PARAM;
	}

	if (pop->MESearchRange < 0 || pop->MESearchRange >= 4) {
		return RETCODE_INVALID_PARAM;
	}

	if (pop->bitstreamFormat == STD_AVC) {
		EncAvcParam * param = &pop->EncStdParam.avcParam;
		if (param->avc_constrainedIntraPredFlag != 0 && param->avc_constrainedIntraPredFlag != 1) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_disableDeblk != 0 && param->avc_disableDeblk != 1 && param->avc_disableDeblk != 2) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_deblkFilterOffsetAlpha < -6 || 6 < param->avc_deblkFilterOffsetAlpha) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_deblkFilterOffsetBeta < -6 || 6 < param->avc_deblkFilterOffsetBeta) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_chromaQpOffset < -12 || 12 < param->avc_chromaQpOffset) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_audEnable != 0 && param->avc_audEnable != 1) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_frameCroppingFlag != 0 &&param->avc_frameCroppingFlag != 1) {
			return RETCODE_INVALID_PARAM;
		}
		if (param->avc_frameCropLeft & 0x01 ||
			param->avc_frameCropRight & 0x01 ||
			param->avc_frameCropTop & 0x01 ||
			param->avc_frameCropBottom & 0x01) {
			return RETCODE_INVALID_PARAM;
		}
		if (picWidth < 64 || picWidth > MAX_ENC_PIC_WIDTH ) {
			return RETCODE_INVALID_PARAM;
		}
		if (picHeight < 16 || picHeight > MAX_ENC_PIC_HEIGHT ) {
			return RETCODE_INVALID_PARAM;
		}
	}

	return RETCODE_SUCCESS;
}

RetCode CheckEncParam(EncHandle handle, EncParam * param)
{
	CodecInst *pCodecInst;
	EncInfo *pEncInfo;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if (param == 0) {
		return RETCODE_INVALID_PARAM;
	}
	if (param->skipPicture != 0 && param->skipPicture != 1) {
		return RETCODE_INVALID_PARAM;
	}
	if (param->skipPicture == 0) {
		if (param->sourceFrame == 0) {
			return RETCODE_INVALID_FRAME_BUFFER;
		}
		if (param->forceIPicture != 0 && param->forceIPicture != 1) {
			return RETCODE_INVALID_PARAM;
		}
	}
	if (pEncInfo->openParam.bitRate == 0) { // no rate control
		if (pCodecInst->codecMode == MP4_ENC) {
			if (param->quantParam < 1 || param->quantParam > 31) {
				return RETCODE_INVALID_PARAM;
			}
		}
		else { // AVC_ENC
			if (param->quantParam < 0 || param->quantParam > 51) {
				return RETCODE_INVALID_PARAM;
			}
		}
	}
	if (pEncInfo->ringBufferEnable == 0) {
		if (param->picStreamBufferAddr % 8 || param->picStreamBufferSize == 0) {
			return RETCODE_INVALID_PARAM;
		}
	}
	return RETCODE_SUCCESS;
}
static int OverrideProfileLevel(int headerCode, int forcedProfileLevel)
{
	// Bit[4] : UserProfileLevel Enable
	headerCode |= (1 << 4);

	/**
	 * Bit[15:08] level_idc in H.264/AVC SPS header.
	 * Bit[15:08] level_indication in MPEG-4 VOS.
	*/
	headerCode |= (forcedProfileLevel << 8);

	return headerCode;
}

RetCode GetEncHeader(EncHandle handle, EncHeaderParam *encHeaderParam)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	EncOpenParam *encOP;

	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;
	int flag=0;
	RetCode ret = RETCODE_SUCCESS;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;
	encOP = &(pEncInfo->openParam);

	if (pCodecInst->m_iVersionRTL == 0)
	{
	#if 1
		MACRO_VPU_SWRESET_EXIT
	#else
		VPU_SWReset(SW_RESET_SAFETY);
		while(VPU_IsBusy());
	#endif
	}

	if (pEncInfo->ringBufferEnable == 0) {
		VpuWriteReg(CMD_ENC_HEADER_BB_START, encHeaderParam->buf);
		VpuWriteReg(CMD_ENC_HEADER_BB_SIZE, encHeaderParam->size/1024);
	}

	if( encHeaderParam->headerType == SPS_RBSP && pEncInfo->openParam.bitstreamFormat == STD_AVC) {
		Uint32 CropV, CropH;
		if (encOP->EncStdParam.avcParam.avc_frameCroppingFlag == 1) {
			flag = 1;
			CropH = encOP->EncStdParam.avcParam.avc_frameCropLeft << 16;
			CropH |= encOP->EncStdParam.avcParam.avc_frameCropRight;
			CropV = encOP->EncStdParam.avcParam.avc_frameCropTop << 16;
			CropV |= encOP->EncStdParam.avcParam.avc_frameCropBottom;
			VpuWriteReg( CMD_ENC_HEADER_FRAME_CROP_H, CropH );
			VpuWriteReg( CMD_ENC_HEADER_FRAME_CROP_V, CropV );
		}
	}

	{
		int headerCode;

		headerCode = encHeaderParam->headerType;
		headerCode |= (flag << 3);

		#ifdef USE_HOST_PROFILE_LEVEL
		{
			unsigned int profileLevel = 0;

			if(pEncInfo->openParam.bitstreamFormat == STD_AVC)
			{
				profileLevel = encOP->EncStdParam.avcParam.avc_level;
			}
			else if (pEncInfo->openParam.bitstreamFormat == STD_MPEG4)
			{
				profileLevel = encOP->EncStdParam.mp4Param.mp4_profileAndIndication;
			}

		headerCode = OverrideProfileLevel(headerCode, profileLevel);
		}
		#endif

		#ifdef USER_OVERRIDE_H264_PROFILE_LEVEL
		if (pEncInfo->openParam.bitstreamFormat == STD_AVC)
		{
			if (encOP->EncStdParam.avcParam.avc_level > 0)
			{
				headerCode = OverrideProfileLevel(headerCode, encOP->EncStdParam.avcParam.avc_level);
			}
		}
		#endif

		#ifdef USER_OVERRIDE_MPEG4_PROFILE_LEVEL
		if (pEncInfo->openParam.bitstreamFormat == STD_MPEG4)
		{
			if (encOP->EncStdParam.mp4Param.mp4_profileAndIndication > 0)
			{
				headerCode = OverrideProfileLevel(headerCode, encOP->EncStdParam.mp4Param.mp4_profileAndIndication);
			}
		}
		#endif

		VpuWriteReg(CMD_ENC_HEADER_CODE, headerCode); // 0: SPS, 1: PPS
	}

	VpuWriteReg(pEncInfo->streamRdPtrRegAddr, encHeaderParam->buf);
	VpuWriteReg(pEncInfo->streamWrPtrRegAddr, encHeaderParam->buf);
    #ifdef USE_VPU_ENC_PUTHEADER_INTERRUPT
	VpuWriteReg(BIT_INT_ENABLE, (1<<INT_BIT_ENC_HEADER));
    #endif

	BitIssueCommand(pCodecInst, ENCODE_HEADER);
	//while(VPU_IsBusy())	;
    #ifdef USE_VPU_ENC_PUTHEADER_INTERRUPT
	while( 1 )
	{
		int iRet = 0;

	#ifdef USE_VPU_SWRESET_CHECK_PC_ZERO
		if(VpuReadReg(BIT_CUR_PC) == 0x0)
		{
			return RETCODE_CODEC_EXIT;
		}
	#endif
		if( vpu_interrupt != NULL )
		{
			iRet = vpu_interrupt();
		}
		if( iRet == RETCODE_CODEC_EXIT )
		{
			MACRO_VPU_BIT_INT_ENABLE_RESET1
			return RETCODE_CODEC_EXIT;
		}

		iRet = VpuReadReg(BIT_INT_REASON);
		if( iRet )
		{
			VpuWriteReg(BIT_INT_REASON, 0);
			VpuWriteReg(BIT_INT_CLEAR, 1);
			if( iRet & (1<<INT_BIT_ENC_HEADER) )
			{
				break;
			}else
			{
				return RETCODE_FAILURE;
			}
		}
	}
    #else	//USE_VPU_ENC_PUTHEADER_INTERRUPT
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);
    #endif	//USE_VPU_ENC_PUTHEADER_INTERRUPT

	if (pEncInfo->ringBufferEnable == 0) {
		rdPtr = encHeaderParam->buf;
		wrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
		// encHeaderParam->buf = rdPtr;
		encHeaderParam->size = wrPtr - rdPtr;
	}
	else {
		rdPtr = VpuReadReg(pEncInfo->streamRdPtrRegAddr);
		wrPtr = VpuReadReg(pEncInfo->streamWrPtrRegAddr);
		encHeaderParam->buf = rdPtr;
		encHeaderParam->size = wrPtr - rdPtr;
	}

	pEncInfo->streamWrPtr = wrPtr;
	pEncInfo->streamRdPtr = rdPtr;



	return RETCODE_SUCCESS;
}

/**
 * EncParaSet()
 *  1. Setting encoder header option
 *  2. Get RBSP format header in PARA_BUF
 * @param handle      : encoder handle
 * @param paraSetType : encoder header type >> SPS: 0, PPS: 1, VOS: 1, VO: 2, VOL: 0
 * @return none
 */
void EncParaSet(EncHandle handle, int paraSetType)
{
	CodecInst * pCodecInst;
	EncInfo * pEncInfo;
	int flag = 0;
	int encHeaderCode;

	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	if( paraSetType == 0 && pEncInfo->openParam.bitstreamFormat == STD_AVC) {
		EncOpenParam *encOP;
		Uint32 CropV, CropH;
		encOP = &(pEncInfo->openParam);

		if (encOP->EncStdParam.avcParam.avc_frameCroppingFlag == 1) {
			flag = 1;
			CropH = encOP->EncStdParam.avcParam.avc_frameCropLeft << 16;
			CropH |= encOP->EncStdParam.avcParam.avc_frameCropRight;
			CropV = encOP->EncStdParam.avcParam.avc_frameCropTop << 16;
			CropV |= encOP->EncStdParam.avcParam.avc_frameCropBottom;
			VpuWriteReg( CMD_ENC_HEADER_FRAME_CROP_H, CropH );
			VpuWriteReg( CMD_ENC_HEADER_FRAME_CROP_V, CropV );
		}
	}
	encHeaderCode = paraSetType| (flag<<2); //paraSetType>> SPS: 0, PPS: 1, VOS: 1, VO: 2, VOL: 0

	VpuWriteReg(CMD_ENC_PARA_SET_TYPE, encHeaderCode);


	BitIssueCommand(pCodecInst, ENC_PARA_SET);
	while(VPU_IsBusy())
		;
}

RetCode SetGopNumber(EncHandle handle, Uint32 *pGopNumber)
{
	CodecInst * pCodecInst;
	int data =0;
	Uint32 gopNumber = *pGopNumber;

	pCodecInst = handle;
	data = 1;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_RC_GOP, gopNumber);

	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy())	;

	return RETCODE_SUCCESS;
}

RetCode SetIntraQp(EncHandle handle, Uint32 *pIntraQp)
{
	CodecInst * pCodecInst;
	int data =1<<1;
	Uint32 intraQp = *pIntraQp;

	pCodecInst = handle;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_RC_INTRA_QP, intraQp);

	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

RetCode SetBitrate(EncHandle handle, Uint32 *pBitrate)
{
	CodecInst * pCodecInst;
	int data = 1<<2;
	Uint32 bitrate = *pBitrate;

	pCodecInst = handle;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_RC_BITRATE, bitrate);
	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

RetCode SetFramerate(EncHandle handle, Uint32 *pFramerate)
{
	CodecInst * pCodecInst;
	int data = 1<<3;
	Uint32 framerate = *pFramerate;

	pCodecInst = handle;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_RC_FRAME_RATE, framerate);
	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

RetCode SetIntraRefreshNum(EncHandle handle, Uint32 *pIntraRefreshNum)
{
	CodecInst * pCodecInst;
	Uint32 intraRefreshNum = *pIntraRefreshNum;
	int data = 1<<4;

	pCodecInst = handle;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_INTRA_MB_NUM, intraRefreshNum);

	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

RetCode SetSliceMode(EncHandle handle, EncSliceMode *pSliceMode)
{
	CodecInst * pCodecInst;
	Uint32 data = 0;
	int data2 = 1<<5;

	pCodecInst = handle;
	data = pSliceMode->sliceSize<<2 | pSliceMode->sliceSizeMode<<1 | pSliceMode->sliceMode;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data2);
	VpuWriteReg(CMD_ENC_SEQ_PARA_SLICE_MODE, data);
	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

RetCode SetHecMode(EncHandle handle, int mode)
{
	CodecInst * pCodecInst;
	Uint32 HecMode = mode;
	int data = 1 << 6;

	pCodecInst = handle;

	VpuWriteReg(CMD_ENC_SEQ_PARA_CHANGE_ENABLE, data);
	VpuWriteReg(CMD_ENC_SEQ_PARA_HEC_MODE, HecMode);
	BitIssueCommand(pCodecInst, RC_CHANGE_PARAMETER);
	MACRO_VPU_BUSY_WAITING_EXIT(0x7FFFFFF);	//while(VPU_IsBusy());

	return RETCODE_SUCCESS;
}

void EncSetHostParaAddr(PhysicalAddress baseAddr, PhysicalAddress paraAddr)
{
	BYTE tempBuf[8];					// 64bit bus & endian
	Uint32 val;

	val = paraAddr;
	tempBuf[0] = 0;
	tempBuf[1] = 0;
	tempBuf[2] = 0;
	tempBuf[3] = 0;
	tempBuf[4] = (val >> 24) & 0xff;
	tempBuf[5] = (val >> 16) & 0xff;
	tempBuf[6] = (val >> 8) & 0xff;
	tempBuf[7] = (val >> 0) & 0xff;
}

int JpgEncGenHuffTab(EncInfo * pEncInfo, int tabNum)
{
	int p, i, l, lastp, si, maxsymbol;
	int code;

	vpu_memset(huffsize, 0, sizeof(huffsize), 0);
	vpu_memset(huffcode, 0, sizeof(huffcode), 0);

	BYTE *bitleng, *huffval;
	unsigned int *ehufco, *ehufsi;

	if (tabNum > 3)
		return 0;

	bitleng	= pEncInfo->jpgInfo.pHuffBits[tabNum];
	huffval	= pEncInfo->jpgInfo.pHuffVal[tabNum];
	ehufco	= pEncInfo->jpgInfo.huffCode[tabNum];
	ehufsi	= pEncInfo->jpgInfo.huffSize[tabNum];

	maxsymbol = tabNum & 1 ? 256 : 16;

	/* Figure C.1: make table of Huffman code length for each symbol */

	p = 0;
	for (l=1; l<=16; l++) {
		i = bitleng[l-1];
		if (i < 0 || p + i > maxsymbol)
			return 0;
		while (i--)
			huffsize[p++] = l;
	}
	lastp = p;

	/* Figure C.2: generate the codes themselves */
	/* We also validate that the counts represent a legal Huffman code tree. */

	code = 0;
	si = huffsize[0];
	p = 0;
	while (p < lastp) {
		while (huffsize[p] == si) {
			huffcode[p++] = code;
			code++;
			if (p >= 256)
				return 0;
		}
		if (code >= (1 << si))
			return 0;
		code <<= 1;
		si++;
	}

	/* Figure C.3: generate encoding tables */
	/* These are code and size indexed by symbol value */

	for(i=0; i<256; i++)
		ehufsi[i] = 0x00;

	for(i=0; i<256; i++)
		ehufco[i] = 0x00;

	if (lastp > 162)
		return 0;

	for (p=0; p<lastp; p++) {
		i = huffval[p];
		if (i < 0 || i >= maxsymbol || ehufsi[i])
			return 0;
		ehufco[i] = huffcode[p];
		ehufsi[i] = huffsize[p];
	}

	return 1;
}

int JpgEncLoadHuffTab(EncInfo * pEncInfo)
{
	int i, j, t;
	int huffData;

	for (i=0; i<4; i++)
		JpgEncGenHuffTab(pEncInfo, i);

	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x3);

	for (j=0; j<4; j++)
	{

		t = (j==0) ? AC_TABLE_INDEX0 : (j==1) ? AC_TABLE_INDEX1 : (j==2) ? DC_TABLE_INDEX0 : DC_TABLE_INDEX1;

		for (i=0; i<256; i++)
		{
			if ((t==DC_TABLE_INDEX0 || t==DC_TABLE_INDEX1) && (i>15))	// DC
				break;

			if ((pEncInfo->jpgInfo.huffSize[t][i] == 0) && (pEncInfo->jpgInfo.huffCode[t][i] == 0))
				huffData = 0;
			else
			{
				huffData =                    (pEncInfo->jpgInfo.huffSize[t][i] - 1);	// Code length (1 ~ 16), 4-bit
				huffData = (huffData << 16) | (pEncInfo->jpgInfo.huffCode[t][i]    );	// Code word, 16-bit
			}
			VpuWriteReg(MJPEG_HUFF_DATA_REG, huffData);
		}
	}
	VpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x0);
	return 1;
}

int JpgEncLoadQMatTab(EncInfo * pEncInfo)
{
	int quantID;
	int comp;
	int i, t;

	for (comp=0; comp<3; comp++) {
		quantID = pEncInfo->jpgInfo.pCInfoTab[comp][3];
		if (quantID >= 4)
			return 0;
		t = (comp==0)? Q_COMPONENT0 :
		(comp==1)? Q_COMPONENT1 : Q_COMPONENT2;
		VpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x3 + t);
		for (i=0; i<64; i++) {
			VpuWriteReg(MJPEG_QMAT_DATA_REG, (int) QMAT_DATA_TAB[pEncInfo->jpgInfo.format][2][comp][i]);
		}
		VpuWriteReg(MJPEG_QMAT_CTRL_REG, t);
	}

	return 1;
}

#define PUT_BYTE(_p, _b) \
	if (tot++ > len) return 0; \
	*_p++ = (unsigned char)(_b);

#ifdef JPEG_THUMBNAIL_EN
int JpgEncEncodeThumbnailHeader(EncHandle handle, EncParamSet * para)
{
	CodecInst *pCodecInst;
	EncInfo *pEncInfo;
	BYTE *p;
	const char *jfif = "JFIF";
	const char *jfxx = "JFXX";
	int i, tot, len, pad;

	tot = 0;
	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	p = para->pParaSet;
	len = para->size;

	// APP0 Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xE0);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x10);

	PUT_BYTE(p, jfif[0]);
	PUT_BYTE(p, jfif[1]);
	PUT_BYTE(p, jfif[2]);
	PUT_BYTE(p, jfif[3]);
	PUT_BYTE(p, jfif[4]);

	PUT_BYTE(p, 0x01);//version
	PUT_BYTE(p, 0x02);//version

	PUT_BYTE(p, 0x01);//units

	PUT_BYTE(p, 0x00);//horizontal pixel density
	PUT_BYTE(p, 0x90);//horizontal pixel density
	PUT_BYTE(p, 0x00);//vertical pixel density
	PUT_BYTE(p, 0x90);//vertical pixel density

	PUT_BYTE(p, 0x00); //thumbnail horizontal pixel count
	PUT_BYTE(p, 0x00); //thumbnail vertical pixel count

	//APP0 Extension
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xE0);
	//PUT_BYTE(p, length); length of the thumbnail APP for 2-bytes
	PUT_BYTE(p, jfxx[0]);
	PUT_BYTE(p, jfxx[1]);
	PUT_BYTE(p, jfxx[2]);
	PUT_BYTE(p, jfxx[3]);
	PUT_BYTE(p, jfxx[4]);
	PUT_BYTE(p, 0x10); //ExCode = JPEG

	// SOI Header
	PUT_BYTE(p, 0xff);
	PUT_BYTE(p, 0xD8);

	// APP9 Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xE9);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x04);

	PUT_BYTE(p, (pEncInfo->jpgInfo.frameIdx >> 8));
	PUT_BYTE(p, (pEncInfo->jpgInfo.frameIdx & 0xFF));

	// DRI header
	if (pEncInfo->jpgInfo.rstIntval) {

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xDD);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x04);

		PUT_BYTE(p, (pEncInfo->jpgInfo.rstIntval >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.rstIntval & 0xff));

	}

	// DQT Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xDB);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x43);

	PUT_BYTE(p, 0x00);

	for (i=0; i<64; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pQMatTab[0][i]);
	}

	if (pEncInfo->jpgInfo.format != FORMAT_400) {
		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xDB);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x43);

		PUT_BYTE(p, 0x01);

		for (i=0; i<64; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pQMatTab[1][i]);
		}
	}

	// DHT Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC4);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x1F);

	PUT_BYTE(p, 0x00);

	for (i=0; i<16; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[0][i]);
	}

	for (i=0; i<12; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[0][i]);
	}

	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC4);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0xB5);

	PUT_BYTE(p, 0x10);

	for (i=0; i<16; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[1][i]);
	}

	for (i=0; i<162; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[1][i]);
	}


	if (pEncInfo->jpgInfo.format != FORMAT_400) {

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xC4);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x1F);

		PUT_BYTE(p, 0x01);

		for (i=0; i<16; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[2][i]);
		}
		for (i=0; i<12; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[2][i]);
		}

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xC4);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0xB5);

		PUT_BYTE(p, 0x11);

		for (i=0; i<16; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[3][i]);
		}

		for (i=0; i<162; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[3][i]);
		}
	}

	// SOF header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC0);

	PUT_BYTE(p, (((8+(pEncInfo->jpgInfo.compNum*3)) >> 8) & 0xFF));
	PUT_BYTE(p, ((8+(pEncInfo->jpgInfo.compNum*3)) & 0xFF));

	PUT_BYTE(p, 0x08);

	if (pEncInfo->rotationEnable && (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270)) {
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth & 0xFF));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight & 0xFF));
	} else {
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight & 0xFF));

		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth & 0xFF));
	}

	PUT_BYTE(p, pEncInfo->jpgInfo.compNum);

	for (i=0; i<pEncInfo->jpgInfo.compNum; i++) {
		PUT_BYTE(p, (i+1));
		PUT_BYTE(p, ((pEncInfo->jpgInfo.pCInfoTab[i][1]<<4) & 0xF0) + (pEncInfo->jpgInfo.pCInfoTab[i][2] & 0x0F));
		PUT_BYTE(p, pEncInfo->jpgInfo.pCInfoTab[i][3]);
	}

	//tot = p - para->pParaSet;
	pad = 0;
	if (tot % 8) {
		pad = tot % 8;
		pad = 8-pad;
		for (i=0; i<pad; i++) {
			PUT_BYTE(p, 0x00);
		}
	}

	pEncInfo->jpgInfo.frameIdx++;
	para->size = tot;
	return tot;
}
#endif//JPEG_THUMBNAIL_EN

int JpgEncEncodeHeader(EncHandle handle, EncParamSet * para)
{
	CodecInst *pCodecInst;
	EncInfo *pEncInfo;
	BYTE *p;
	int i, tot, len, pad;

	tot = 0;
	pCodecInst = handle;
	pEncInfo = &pCodecInst->CodecInfo.encInfo;

	p = para->pParaSet;
	len = para->size;

	// SOI Header
	PUT_BYTE(p, 0xff);
	PUT_BYTE(p, 0xD8);

	// APP9 Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xE9);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x04);

	PUT_BYTE(p, (pEncInfo->jpgInfo.frameIdx >> 8));
	PUT_BYTE(p, (pEncInfo->jpgInfo.frameIdx & 0xFF));

	// DRI header
	if (pEncInfo->jpgInfo.rstIntval) {

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xDD);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x04);

		PUT_BYTE(p, (pEncInfo->jpgInfo.rstIntval >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.rstIntval & 0xff));

	}

	// DQT Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xDB);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x43);

	PUT_BYTE(p, 0x00);

	for (i=0; i<64; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pQMatTab[0][i]);
	}

	if (pEncInfo->jpgInfo.format != FORMAT_400) {
		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xDB);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x43);

		PUT_BYTE(p, 0x01);

		for (i=0; i<64; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pQMatTab[1][i]);
		}
	}

	// DHT Header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC4);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0x1F);

	PUT_BYTE(p, 0x00);

	for (i=0; i<16; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[0][i]);
	}

	for (i=0; i<12; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[0][i]);
	}

	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC4);

	PUT_BYTE(p, 0x00);
	PUT_BYTE(p, 0xB5);

	PUT_BYTE(p, 0x10);

	for (i=0; i<16; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[1][i]);
	}

	for (i=0; i<162; i++) {
		PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[1][i]);
	}

	if (pEncInfo->jpgInfo.format != FORMAT_400) {

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xC4);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0x1F);

		PUT_BYTE(p, 0x01);

		for (i=0; i<16; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[2][i]);
		}
		for (i=0; i<12; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[2][i]);
		}

		PUT_BYTE(p, 0xFF);
		PUT_BYTE(p, 0xC4);

		PUT_BYTE(p, 0x00);
		PUT_BYTE(p, 0xB5);

		PUT_BYTE(p, 0x11);

		for (i=0; i<16; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffBits[3][i]);
		}

		for (i=0; i<162; i++) {
			PUT_BYTE(p, pEncInfo->jpgInfo.pHuffVal[3][i]);
		}
	}

	// SOF header
	PUT_BYTE(p, 0xFF);
	PUT_BYTE(p, 0xC0);

	PUT_BYTE(p, (((8+(pEncInfo->jpgInfo.compNum*3)) >> 8) & 0xFF));
	PUT_BYTE(p, ((8+(pEncInfo->jpgInfo.compNum*3)) & 0xFF));

	PUT_BYTE(p, 0x08);

	if (pEncInfo->rotationEnable && (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270)) {
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth & 0xFF));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight & 0xFF));
	} else {
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picHeight & 0xFF));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth >> 8));
		PUT_BYTE(p, (pEncInfo->jpgInfo.picWidth & 0xFF));
	}

	PUT_BYTE(p, pEncInfo->jpgInfo.compNum);

	for (i=0; i<pEncInfo->jpgInfo.compNum; i++) {
		PUT_BYTE(p, (i+1));
		PUT_BYTE(p, ((pEncInfo->jpgInfo.pCInfoTab[i][1]<<4) & 0xF0) + (pEncInfo->jpgInfo.pCInfoTab[i][2] & 0x0F));
		PUT_BYTE(p, pEncInfo->jpgInfo.pCInfoTab[i][3]);
	}

	//tot = p - para->pParaSet;
	pad = 0;
	if (tot % 8) {
		pad = tot % 8;
		pad = 8-pad;
		for (i=0; i<pad; i++) {
			PUT_BYTE(p, 0x00);
		}
	}

	pEncInfo->jpgInfo.frameIdx++;
	para->size = tot;
	return tot;
}

int EncGetFrameBufSize(int framebufFormat, int picWidth, int picHeight, int mapType)
{
	int framebufSize = 0;
	int fbWidth16, fbHeight32;

	fbWidth16 = VALIGN16(picWidth);	//((picWidth+15)&~15);
	fbHeight32 = VALIGN32(picHeight);	//((picHeight+31)&~31);

	if (mapType == LINEAR_FRAME_MAP)
	{
		switch (framebufFormat)
		{
		case FORMAT_420:
			//framebufSize = fbWidth16 * ((fbHeight32+1)/2*2) + ((fbWidth16+1)/2)*((fbHeight32+1)/2)*2;
			framebufSize = fbWidth16 * fbHeight32 + fbWidth16 * fbHeight32 / 2;
			break;
		case FORMAT_224:	//framebufSize = fbWidth16 * ((fbHeight32+1)/2*2) + fbWidth16*((fbHeight32+1)/2)*2;
		case FORMAT_422:	//framebufSize = fbWidth16 * fbHeight32 + ((fbWidth16+1)/2)*fbHeight32*2;
			framebufSize = fbWidth16 * fbHeight32 + fbWidth16 * fbHeight32;
			break;
		case FORMAT_444:
			framebufSize = fbWidth16 * fbHeight32 * 3;
			break;
		case FORMAT_400:
			framebufSize = fbWidth16 * fbHeight32;
			break;
		}
		//framebufSize = (framebufSize+7)/8*8;	//Since the aligned size has already been calculated to be larger than a multiple of 8, there is no need for 8-byte alignment.
	}
	else if (mapType == TILED_FRAME_MB_RASTER_MAP)
	{
		int divX, divY;
		int lum_size, chr_size;
		int dpbsizeluma_aligned;
		int dpbsizechroma_aligned;
		int dpbsizeall_aligned;

		divX = framebufFormat == FORMAT_420 || framebufFormat == FORMAT_422 ? 2 : 1;
		divY = framebufFormat == FORMAT_420 || framebufFormat == FORMAT_224 ? 2 : 1;

		lum_size   = fbWidth16 * fbHeight32;
		chr_size   = lum_size / divX / divY;

		dpbsizeluma_aligned   =  VALIGNKB(lum_size, 16);	//((lum_size + 16383)/16384)*16384;
		dpbsizechroma_aligned =  VALIGNKB(chr_size, 16);	//((chr_size + 16383)/16384)*16384;
		dpbsizeall_aligned    =  dpbsizeluma_aligned + 2 * dpbsizechroma_aligned;

		framebufSize = dpbsizeall_aligned;
		//framebufSize = (framebufSize+7)/8*8;	//Since the aligned size has already been calculated to be larger than a multiple of 8, there is no need for 8-byte alignment.
	}

	return framebufSize;
}

int GetMvColBufSize(int framebufFormat, int picWidth, int picHeight, int mapType)
{

	int chr_vscale, chr_hscale;
	int size_dpb_lum, size_dpb_chr, size_dpb_all;
	int size_mvcolbuf;
	const int interleave = 1;
	int fbWidth16, fbHeight32;

	chr_hscale  = (framebufFormat == FORMAT_420 || framebufFormat == FORMAT_422 ? 2 : 1);  // 4:2:0, 4:2:2h
	chr_vscale = (framebufFormat == FORMAT_420 || framebufFormat == FORMAT_224 ? 2 : 1);// 4:2:0, 4:2:2v

	fbWidth16 = VALIGN16(picWidth);
	fbHeight32 = VALIGN32(picHeight);
	size_dpb_lum = fbWidth16*fbHeight32;

	if (framebufFormat == FORMAT_400)
		size_dpb_chr  = 0;
	else
		size_dpb_chr  = size_dpb_lum / chr_vscale / chr_hscale;

	size_dpb_all = (size_dpb_lum + 2 * size_dpb_chr);

	// mvColBufSize is 1/5 of picture size in X264
	// normal case is 1/6
	size_mvcolbuf = (size_dpb_all + 4) / 5;
	size_mvcolbuf = VALIGN08(size_mvcolbuf);	//((size_mvcolbuf+7) / 8) * 8;

	if (mapType == TILED_FRAME_MB_RASTER_MAP)
	{
		// base address should be aligned to 4k address in MS raster map (20 bit address).
		size_mvcolbuf = VALIGNKB(size_mvcolbuf, 16);	//((size_mvcolbuf+(16384-1))/16384)*16384; //15Kbytes align.
	}

	// need to check bufAlignSize and bufStartAddrAlign
	return size_mvcolbuf;
}


#if 0
// 32 bit / 16 bit ==> 32-n bit remainder, n bit quotient
static int fixDivRq(int a, int b, int n)
{
	Int64 c;
	Int64 a_36bit;
	Int64 mask, signBit, signExt;
	int  i;

	// DIVS emulation for BPU accumulator size
	// For SunOS build
	mask = 0x0F; mask <<= 32; mask |= 0x00FFFFFFFF; // mask = 0x0FFFFFFFFF;
	signBit = 0x08; signBit <<= 32;                 // signBit = 0x0800000000;
	signExt = 0xFFFFFFF0; signExt <<= 32;           // signExt = 0xFFFFFFF000000000;

	a_36bit = (Int64) a;

	for (i=0; i<n; i++) {
		c =  a_36bit - (b << 15);
		if (c >= 0)
			a_36bit = (c << 1) + 1;
		else
			a_36bit = a_36bit << 1;

	a_36bit = a_36bit & mask;
	if (a_36bit & signBit)
		a_36bit |= signExt;
	}

	a = (int) a_36bit;
	return a;	// R = [31:n], Q = [n-1:0]
}

static int fixDivRnd(int a, int b)
{
	int  c;
	c = fixDivRq(a, b, 17);	// R = [31:17], Q = [16:0]
	c = c & 0xFFFF;
	c = (c + 1) >> 1;	// round
	return (c & 0xFFFF);
}

int RcFixDivRnd(int res, int div)
{
	return fixDivRnd(res, div);
}

int LevelCalculation(int MbNumX, int MbNumY, int frameRateInfo, int interlaceFlag, int BitRate, int SliceNum)
{
	int mbps;
	int frameRateDiv, frameRateRes, frameRate;
	int mbPicNum = (MbNumX*MbNumY);
	int mbFrmNum;
	int MaxSliceNum;

	int LevelIdc = 0;
	int i, maxMbs;

	if (interlaceFlag)	{
		mbFrmNum = mbPicNum*2;
		MbNumY   *=2;
	}
	else	mbFrmNum = mbPicNum;

	frameRateDiv = (frameRateInfo >> 16) + 1;
	frameRateRes  = frameRateInfo & 0xFFFF;
	frameRate = fixDivRnd(frameRateRes, frameRateDiv);
	mbps = mbFrmNum*frameRate;

	for(i=0; i<MAX_LAVEL_IDX; i++)
	{
		maxMbs = g_anLevelMaxMbs[i];
		if ( mbps <= g_anLevelMaxMBPS[i]
		&& mbFrmNum <= g_anLevelMaxFS[i]
		&& MbNumX   <= maxMbs
		&& MbNumY   <= maxMbs
		&& BitRate  <= g_anLevelMaxBR[i] )
		{
			LevelIdc = g_anLevel[i];
			break;
		}
	}

	if (i==MAX_LAVEL_IDX)
		i = MAX_LAVEL_IDX-1;

	if (SliceNum)
	{
		SliceNum =  fixDivRnd(mbPicNum, SliceNum);

		if (g_anLevelSliceRate[i])
		{
			MaxSliceNum = fixDivRnd( MAX( mbPicNum, g_anLevelMaxMBPS[i]/( 172/( 1+interlaceFlag ) )), g_anLevelSliceRate[i] );

			if ( SliceNum> MaxSliceNum)
				return -1;
		}
	}

	return LevelIdc;
}
#else

	#ifdef USER_OVERRIDE_MPEG4_PROFILE_LEVEL
	// 0x08 8'b0000 1000(L0 )  128x 96@15fps  8x 6=  48 mbps(15)=   720
	// 0x01 8'b0000 0001(L1 )  176x144@15fps 11x 9=  99 mbps(15)=  1485 or 128×96@30fps or 144×96@25fps or 160×96@24fps
	// 0x02 8'b0000 0010(L2 )  352x288@15fps 22x18= 396 mbps(15)=  5940
	// 0x03 8'b0000 0011(L3 )  352x288@30fps 22x18= 396 mbps(30)= 11880
	// 0x04 8'b0000 0100(L4a)  640x480@30fps 40x30=1200 mbps(30)= 36000
	// 0x05 8'b0000 0101(L5 )  720x576@25fps 45x36=1620 mbps(25)= 40500
	//                         720x480@30fps 45x30=1350 mbps(25)= 40500
	// 0x06 8'b0000 0110(L6 ) 1280x720@25fps 80x45=3600 mbps(25)= 108000

	#define MAX_LAVEL_IDX_MP4   7

	static const int mp4encLevelMaxVcv[MAX_LAVEL_IDX_MP4] =         //g_anLevelMp4MaxVcv
	{
		99, 99, 396,    396,    1200,   1620,    3600
	};

	static const int mp4encLevelMp4MaxVcvRate[MAX_LAVEL_IDX_MP4] =    //g_anLevelMp4MaxVcvRate
	{
		720/*1485*/,    1485,   5940, 11880,    36000, 40500,  108000
	};
	static const int mp4encLevelMp4MaxBitRate[MAX_LAVEL_IDX_MP4] =    //g_anLevelMp4MaxBitRate
	{
		64,     64,     128,    384,     4000,  8000,   12000
	};
	static const int mp4encLevelMp4LevelCode[MAX_LAVEL_IDX_MP4] =     //g_anLevelMp4LevelCode
	{
	//      L0      L1      L2      L3      L4      L5      L6      (Table G.1)
		0x08,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06
	};

	int LevelCalculationMp4(int width, int height, int frameRate, int BitRatekbps)
	{
		int MbNumX, MbNumY, mbFrmNum, mbps;
		int LevelIdc = 0;
		int i, curLevel;

		MbNumX = (width + 15) >> 4;       //MB size = 16x16 pixels
		MbNumY = (height + 15) >> 4;
		mbFrmNum = (MbNumX * MbNumY);
		mbps = mbFrmNum * frameRate;

		// mbFrmNum, mbps, BitRate(kbps)
		curLevel = 6;
		for(i = (MAX_LAVEL_IDX_MP4-1); i >= 0; i--)
		{
			if (   (mbFrmNum     <= mp4encLevelMaxVcv[i])
				&& (mbps         <= mp4encLevelMp4MaxVcvRate[i])
				&& (BitRatekbps  <= mp4encLevelMp4MaxBitRate[i]))
			{
				curLevel = i;
			}
		}
		LevelIdc = mp4encLevelMp4LevelCode[curLevel];

		return LevelIdc;
	}
#endif // USER_OVERRIDE_MPEG4_PROFILE_LEVEL


#ifdef USER_OVERRIDE_H264_PROFILE_LEVEL
	#define MAX_LAVEL_IDX_AVC           16

	static const int avcencLevel[MAX_LAVEL_IDX_AVC] = //g_anLevel
	{
		10, 11, 11, 12, 13,
		20, 21, 22,
		30, 31, 32,
		40, 41, 42,
		50, 51
	};

	static const int avcencLevelMaxMBPS[MAX_LAVEL_IDX_AVC] =  //g_anLevelMaxMBPS
	{
		1485,   1485,   3000,   6000, 11880,
		11880,  19800,  20250,
		40500,  108000, 216000,
		245760, 245760, 522240,
		589824, 983040
	};

	static const int avcencLevelMaxMBPSdiv172[MAX_LAVEL_IDX_AVC] =  //g_anLevelMaxMBPS / 172 (only for progressive, interlaceFlag = 0 in TELBB-571)
	{
			8,  8,  17, 34, 69,
			69, 115,    117,
			235,    627,    1255,
			1428,   1428,   3036,
			3429,   5715
	};

	static const int avcencLevelMaxFS[MAX_LAVEL_IDX_AVC] =    //g_anLevelMaxFS
	{
		99,    99,   396, 396, 396,
		396,   792,  1620,
		1620,  3600, 5120,
		8192,  8192, 8704,
		22080, 36864
	};

	static const int avcencLevelMaxBR[MAX_LAVEL_IDX_AVC] =    //g_anLevelMaxBR
	{
		64,     128,   192,  384, 768,
		2000,   4000,  4000,
		10000,  14000, 20000,
		20000,  50000, 50000,
		135000, 240000
	};

	static const int avcencLevelSliceRate[MAX_LAVEL_IDX_AVC] =        //g_anLevelSliceRate
	{
		0,  0,  0,  0,  0,
		0,  0,  0,
		22, 60, 60,
		60, 24, 24,
		24, 24
	};

	static const int avcencLevelMaxMbs[MAX_LAVEL_IDX_AVC] =   //g_anLevelMaxMbs
	{
		28,   28,  56, 56, 56,
		56,   79, 113,
		113, 169, 202,
		256, 256, 263,
		420, 543
	};

	static int avcfixDivRq(int a, int b, int n) // 32 bit / 16 bit ==> 32-n bit remainder, n bit quotient
	{
		long long c;
		long long a_36bit;
		long long mask, signBit, signExt;
		int  i;

		// DIVS emulation for BPU accumulator size
		// For SunOS build
		mask = 0x0F; mask <<= 32; mask |= 0x00FFFFFFFF; // mask = 0x0FFFFFFFFF;
		signBit = 0x08; signBit <<= 32;           // signBit = 0x0800000000;
		signExt = 0xFFFFFFF0; signExt <<= 32;     // signExt = 0xFFFFFFF000000000;

		a_36bit = (long long) a;

		for (i=0; i < n; i++) {
			c =  a_36bit - (b << 15);
			if (c >= 0)
				a_36bit = (c << 1) + 1;
			else
				a_36bit = a_36bit << 1;

			a_36bit = a_36bit & mask;
			if (a_36bit & signBit)
				a_36bit |= signExt;
		}

		a = (int) a_36bit;
		return a;   // R = [31:n], Q = [n-1:0]
	}

	static int avcMathDiv(int number, int denom)
	{
		int  c;
		c = avcfixDivRq(number, denom, 17); // R = [31:17], Q = [16:0]
		c = c & 0xFFFF;
		c = (c + 1) >> 1; // round
		return (c & 0xFFFF);
	}

	int LevelCalculationAvc(int width, int height, int frameRate, int BitRatekbps, int SliceNum)
	{
		int mbps;
			int MbNumX, MbNumY;
		int mbPicNum, mbFrmNum;
		int MaxSliceNum;

		int LevelIdc = 0;
		int i, maxMbs;

		MbNumX = (width  + 15) >> 4;       //MB size = 16x16 pixels
		MbNumY = (height + 15) >> 4;
			mbPicNum = mbFrmNum = MbNumX * MbNumY;
			mbps = mbFrmNum * frameRate;

		for(i = 0; i < MAX_LAVEL_IDX_AVC; i++)
		{
			maxMbs = avcencLevelMaxMbs[i];
			if (   mbps        <= avcencLevelMaxMBPS[i]
				&& mbFrmNum    <= avcencLevelMaxFS[i]
				&& MbNumX      <= maxMbs
				&& MbNumY      <= maxMbs
				&& BitRatekbps <= avcencLevelMaxBR[i])
			{
				LevelIdc = avcencLevel[i];
				break;
			}
		}

		if (i == MAX_LAVEL_IDX_AVC)
			i = MAX_LAVEL_IDX_AVC - 1;

		if (SliceNum)
		{
			SliceNum = avcMathDiv(mbPicNum, SliceNum);
					if (avcencLevelSliceRate[i])
					{
							MaxSliceNum = avcMathDiv(MAX(mbPicNum, avcencLevelMaxMBPSdiv172[i]), avcencLevelSliceRate[i]);     //avcencLevelMaxMBPS[i]/172
							if (SliceNum > MaxSliceNum)
									return -1;
					}
			}

		return LevelIdc;
	}

#endif // USER_OVERRIDE_H264_PROFILE_LEVEL
#endif

#endif	//TCC_VPU_2K_IP == 0x0630
