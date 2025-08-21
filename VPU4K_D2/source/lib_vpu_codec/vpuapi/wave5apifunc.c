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

#include "wave5apifunc.h"
#include "product.h"
#include "wave5_regdefine.h"
#include "TCC_VPU_4K_D2.h"

#ifndef MIN
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif
#define MAX_LAVEL_IDX    16

extern CodecInst codecInstPoolWave5[WAVE5_MAX_NUM_INSTANCE];
extern int gWave5MaxInstanceNum;

/******************************************************************************
    define value
******************************************************************************/

/******************************************************************************
    Codec Instance Slot Management
******************************************************************************/


RetCode WAVE5_InitCodecInstancePool(Uint32 coreIdx)
{
	return RETCODE_SUCCESS;
}

/*
 * GetCodecInstance() obtains a instance.
 * It stores a pointer to the allocated instance in *ppInst
 * and returns RETCODE_SUCCESS on success.
 * Failure results in 0(null pointer) in *ppInst and RETCODE_FAILURE.
 */

RetCode WAVE5_GetCodecInstance(Uint32 coreIdx, CodecInst ** ppInst)
{
	int                     i;
	CodecInst*              pCodecInst;

	pCodecInst = &codecInstPoolWave5[0];
	for (i = 0; i < gWave5MaxInstanceNum; ++i, ++pCodecInst)
	{
		if (!pCodecInst->inUse)
			break;
	}

	if (i == gWave5MaxInstanceNum)
	{
		*ppInst = 0;
		return RETCODE_FAILURE;
	}

	pCodecInst->inUse = 1;
	*ppInst = pCodecInst;

	return RETCODE_SUCCESS;
}

void WAVE5_FreeCodecInstance(CodecInst * pCodecInst)
{
	pCodecInst->inUse = 0;
	pCodecInst->codecMode    = -1;
	pCodecInst->codecModeAux = -1;
}

RetCode WAVE5_CheckInstanceValidity(CodecInst * pci)
{
	int i;
	CodecInst * pCodecInst;

	pCodecInst = &codecInstPoolWave5[0];
	for (i = 0; i < gWave5MaxInstanceNum; ++i, ++pCodecInst) {
		if (pCodecInst == pci)
		{
		#ifdef TCC_ONBOARD
			if( pCodecInst->inUse != 0 )
				return RETCODE_SUCCESS;
		#else
			return RETCODE_SUCCESS;
		#endif
		}
	}

	return RETCODE_INVALID_HANDLE;
}

/******************************************************************************
    API Subroutines
******************************************************************************/
int WAVE5_DecBitstreamBufEmpty(DecInfo * pDecInfo)
{
	return (pDecInfo->streamRdPtr == pDecInfo->streamWrPtr);
}

#ifndef TEST_COMPRESSED_FBCTBL_INDEX_SET
RetCode AllocateLinearFrameBuffer(
	TiledMapType            mapType,
	FrameBuffer*            fbArr,
	PhysicalAddress		Addr,
	Uint32                  numOfFrameBuffers,
	Uint32                  sizeLuma,
	Uint32                  sizeChroma
	)
#else
RetCode AllocateLinearFrameBuffer(
	TiledMapType            mapType,
	FrameBuffer*            fbArr,
	PhysicalAddress		Addr,
	Uint32                  numOfFrameBuffers,
	Uint32                  sizeLuma,
	Uint32                  sizeChroma,
	Int32			fbcYsize,
	Int32			fbcCsize,
	Int32			mvColsize,
	Uint32			bufAlignSize,
	Uint32			bufStartAddrAlign
	)
#endif
{
	Uint32      i, sizeFb;
	BOOL        yuv422Interleave = FALSE;
	BOOL        fieldFrame       = (BOOL)(mapType == LINEAR_FIELD_MAP);
	BOOL        cbcrInterleave   = (BOOL)(mapType == COMPRESSED_FRAME_MAP || fbArr[0].cbcrInterleave == TRUE);
	PhysicalAddress addrY;
	Uint32          deltaAligned = 0;

	if (mapType != COMPRESSED_FRAME_MAP && mapType != ARM_COMPRESSED_FRAME_MAP) {
		switch (fbArr[0].format) {
		case FORMAT_YUYV:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_YVYU:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
		case FORMAT_UYVY:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
			yuv422Interleave = TRUE;
			break;
		default:
			//yuv422Interleave = FALSE;
			break;
		}
	}

	addrY  = Addr;
	sizeFb = sizeLuma + sizeChroma*2;
	for (i=0; i<numOfFrameBuffers; i++)
	{
		deltaAligned = 0;
		{
			fbArr[i].bufY = addrY;
			fbArr[i].bufY = WAVE5_ALIGN(addrY, bufStartAddrAlign);
			deltaAligned += (fbArr[i].bufY - addrY);
			if (yuv422Interleave == TRUE)
			{
				fbArr[i].bufCb = (PhysicalAddress)-1;
				fbArr[i].bufCr = (PhysicalAddress)-1;
			}
			else
			{
				//if (fbArr[i].bufCb == (PhysicalAddress)-1)
				{
					fbArr[i].bufCb = fbArr[i].bufY + (sizeLuma >> fieldFrame);
				}
				//if (fbArr[i].bufCr == (PhysicalAddress)-1)
				{
					if (cbcrInterleave == TRUE) {
						fbArr[i].bufCr = (PhysicalAddress)-1;
					}
					else {
						fbArr[i].bufCr = fbArr[i].bufCb + (sizeChroma >> fieldFrame);
					}
				}
			}
			addrY += (sizeFb + deltaAligned);
			#ifdef TEST_COMPRESSED_FBCTBL_INDEX_SET
			if ( mapType == COMPRESSED_FRAME_MAP )
			{
				fbArr[i].bufFBCY = addrY;
				fbArr[i].bufFBCC = fbArr[i].bufFBCY + fbcYsize;
				fbArr[i].bufMVCOL = fbArr[i].bufFBCC + fbcCsize;
				addrY += (fbcYsize + fbcCsize + mvColsize);
			}
			#endif
		}
	}

	return RETCODE_SUCCESS;
}

Int32 ConfigSecAXIWave(Uint32 coreIdx, Int32 codecMode, SecAxiInfo *sa, Uint32 width, Uint32 height, Uint32 profile, Uint32 levelIdc)
{
	int offset;
	Uint32 size = 0;
	int lumaSize;
	int chromaSize;

	UNREFERENCED_PARAMETER(codecMode);
	UNREFERENCED_PARAMETER(height);

	offset      = 0;
	/* Intra Prediction */
	if (sa->u.wave.useIpEnable == TRUE) {
		sa->u.wave.bufIp = sa->bufBase + offset;

		#if TCC_VP9____DEC_SUPPORT == 1
		if (codecMode == W_VP9_DEC) {
			lumaSize   = WAVE5_ALIGN128(width) * 10/8;
			chromaSize = WAVE5_ALIGN128(width) * 10/8;
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (codecMode == W_HEVC_DEC) {
			if (profile == HEVC_PROFILE_MAIN) {
				lumaSize   = WAVE5_ALIGN32(width);
				chromaSize = WAVE5_ALIGN32(width);
			}
			else {
				lumaSize   = WAVE5_ALIGN128(WAVE5_ALIGN16(width)*10)/8;
				chromaSize = WAVE5_ALIGN128(WAVE5_ALIGN16(width)*10)/8;
			}
		}
		#endif
		offset = lumaSize + chromaSize;
	}

	/* Loopfilter row */
	if (sa->u.wave.useLfRowEnable == TRUE) {
		sa->u.wave.bufLfRow = sa->bufBase + offset;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (codecMode == W_VP9_DEC) {
			if (profile == VP9_PROFILE_2) {
				lumaSize   = WAVE5_ALIGN64(width) * 8 * 10/8; /* lumaLIne   : 8 */
				chromaSize = WAVE5_ALIGN64(width) * 8 * 10/8; /* chromaLine : 8 */
				lumaSize *= 2;
			}
			else {
				lumaSize   = WAVE5_ALIGN64(width) * 8; /* lumaLIne   : 8 */
				chromaSize = WAVE5_ALIGN64(width) * 8; /* chromaLine : 8 */
				lumaSize *= 2;
			}
			offset += lumaSize+chromaSize;
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (codecMode == W_HEVC_DEC) {
			if (profile == HEVC_PROFILE_MAIN) {
				size = WAVE5_ALIGN32(width)*8;
			}
			else {
				Uint32 level = levelIdc/30;
				if (level >= 5) {
					size = WAVE5_ALIGN32(width)/2 * 13 + WAVE5_ALIGN64(width)*4;
				}
				else {
					size = WAVE5_ALIGN64(width)*13;
				}
			}
			offset += size;
		}
		#endif

	}

	if (sa->u.wave.useBitEnable == TRUE) {
		sa->u.wave.bufBit = sa->bufBase + offset;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (codecMode == W_VP9_DEC) {
			size = WAVE5_ALIGN64(width)/64 * (70*8);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (codecMode == W_HEVC_DEC) {
			size = WAVE5_ALIGN128(WAVE5_ALIGN32(width)/32*9*8);
		}
		#endif
		offset += size;
	}

	if (sa->u.wave.useSclEnable == TRUE) {
		sa->u.wave.bufScaler = sa->bufBase + offset;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (codecMode == W_VP9_DEC) {
			/* Scaler Line buffer - luma 3 line, chroma 1 line */
			size = WAVE5_ALIGN16(WAVE5_ALIGN32(width) * 10/8) * (3+1);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (codecMode == W_HEVC_DEC) {
			size = WAVE5_ALIGN128(width*10)/8 * 4;
		}
		#endif
		offset += size;
	}

	if (sa->u.wave.useSclPackedModeEnable == TRUE) {
		sa->u.wave.bufScalerPackedData = sa->bufBase + offset;
		#if TCC_VP9____DEC_SUPPORT == 1
		if (codecMode == W_VP9_DEC) {
			/* Scaler Packed Mode temp buffer - packed line buffer 1 line */
			size = WAVE5_ALIGN16(WAVE5_ALIGN32(width) * 10/8) * (1);
		}
		#endif

		#if TCC_HEVC___DEC_SUPPORT == 1
		if (codecMode == W_HEVC_DEC) {
			// size = Ceil128(width*10)/8 * 1
			size = WAVE5_ALIGN128(width*10)/8;
		}
		#endif
		offset += size;
	}

	if (offset > WAVE5_SEC_AXI_BUF_SIZE)
		return 0;

	sa->bufSize = offset;

	return 1;
}

Int32 CalcStride(
	Uint32              width,
	Uint32              height,
	FrameBufferFormat   format,
	BOOL                cbcrInterleave,
	TiledMapType        mapType,
	BOOL                isVP9
	)
{
	Uint32  lumaStride   = 0;
	Uint32  chromaStride = 0;

	lumaStride = WAVE5_ALIGN32(width);

	if (height > width) {
		if ((mapType >= TILED_FRAME_V_MAP && mapType <= TILED_MIXED_V_MAP) ||
			mapType == TILED_FRAME_NO_BANK_MAP ||
			mapType == TILED_FIELD_NO_BANK_MAP)
			width = WAVE5_ALIGN16(height);	// TiledMap constraints
	}
	if (mapType == LINEAR_FRAME_MAP || mapType == LINEAR_FIELD_MAP) {
		Uint32 twice = 0;

		twice = cbcrInterleave == TRUE ? 2 : 1;
		switch (format) {
		case FORMAT_420:
			/* nothing to do */
			break;
		case FORMAT_420_P10_16BIT_LSB:
		case FORMAT_420_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_LSB:
			lumaStride = WAVE5_ALIGN32(width)*2;
			break;
		case FORMAT_420_P10_32BIT_LSB:
		case FORMAT_420_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_LSB:
			if ( isVP9 == TRUE ) {
				lumaStride   = WAVE5_ALIGN32(((width+11)/12)*16);
				chromaStride = (((width/2)+11)*twice/12)*16;
			}
			else {
				width = WAVE5_ALIGN32(width);
				lumaStride   = ((WAVE5_ALIGN16(width)+11)/12)*16;
				chromaStride = ((WAVE5_ALIGN16(width/2)+11)*twice/12)*16;
			//if ( isWAVE410 == TRUE ) {
				if ( (chromaStride*2) > lumaStride)
				{
					lumaStride = chromaStride * 2;
//					VLOG(ERR, "double chromaStride size is bigger than lumaStride\n");
				}
			//}
			}
			if (cbcrInterleave == TRUE) {
				lumaStride = MAX(lumaStride, chromaStride);
			}
			break;
		case FORMAT_422:
			/* nothing to do */
			break;
		case FORMAT_YUYV:       // 4:2:2 8bit packed
		case FORMAT_YVYU:
		case FORMAT_UYVY:
		case FORMAT_VYUY:
			lumaStride = WAVE5_ALIGN32(width) * 2;
			break;
		case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
			lumaStride = WAVE5_ALIGN32(width) * 4;
			break;
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
			lumaStride = WAVE5_ALIGN32(width*2)*2;
			break;
		default:
		break;
		}
	}
	else if (mapType == COMPRESSED_FRAME_MAP) {
		switch (format)
		{
		case FORMAT_420:
		case FORMAT_422:
		case FORMAT_YUYV:
		case FORMAT_YVYU:
		case FORMAT_UYVY:
		case FORMAT_VYUY:
			break;
		case FORMAT_420_P10_16BIT_LSB:
		case FORMAT_420_P10_16BIT_MSB:
		case FORMAT_420_P10_32BIT_LSB:
		case FORMAT_420_P10_32BIT_MSB:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_422_P10_32BIT_MSB:
		case FORMAT_422_P10_32BIT_LSB:
		case FORMAT_YUYV_P10_16BIT_MSB:
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
			lumaStride = WAVE5_ALIGN32(WAVE5_ALIGN16(width)*5)/4;
			lumaStride = WAVE5_ALIGN32(lumaStride);
			break;
		default:
			return -1;
		}
	}
	else if (mapType == ARM_COMPRESSED_FRAME_MAP) {
		switch (format) {
			case FORMAT_420_ARM:
				lumaStride = WAVE5_ALIGN32(width);
				break;
			case FORMAT_420_P10_16BIT_LSB_ARM:
				lumaStride = WAVE5_ALIGN32(width) * 2;
				break;
			default:
				return -1;
		}
	}
	else if (mapType == TILED_FRAME_NO_BANK_MAP || mapType == TILED_FIELD_NO_BANK_MAP) {
		lumaStride = (width > 4096) ? 8192 :
					 (width > 2048) ? 4096 :
					 (width > 1024) ? 2048 :
					 (width >  512) ? 1024 : 512;
	}
	else if (mapType == TILED_FRAME_MB_RASTER_MAP || mapType == TILED_FIELD_MB_RASTER_MAP) {
		lumaStride = WAVE5_ALIGN32(width);
	}
	else {
		width = (width < height) ? height : width;

		lumaStride = (width > 4096) ? 8192 :
					 (width > 2048) ? 4096 :
					 (width > 1024) ? 2048 :
					 (width >  512) ? 1024 : 512;
	}

	return lumaStride;
}

Int32 CalcLumaSize(
	Int32           productId,
	Int32           stride,
	Int32           height,
	FrameBufferFormat format,
	BOOL            cbcrIntl,
	TiledMapType    mapType,
	DRAMConfig      *pDramCfg
	)
{
	Int32 unit_size_hor_lum, unit_size_ver_lum, size_dpb_lum, field_map, size_dpb_lum_4k;
	//UNREFERENCED_PARAMETER(cbcrIntl);

	if (mapType == TILED_FIELD_V_MAP || mapType == TILED_FIELD_NO_BANK_MAP || mapType == LINEAR_FIELD_MAP)
		field_map = 1;
	else
		field_map = 0;

	if (mapType == LINEAR_FRAME_MAP || mapType == LINEAR_FIELD_MAP) {
		size_dpb_lum = stride * height;
	}
	else if (mapType == COMPRESSED_FRAME_MAP) {
		size_dpb_lum = stride * height;
	}
	else if (mapType == ARM_COMPRESSED_FRAME_MAP) {
		Uint32   mbw, mbh, header_size, uncomp_b16_size;

		mbw = WAVE5_ALIGN16(stride)>>4;
		mbh = WAVE5_ALIGN16(height + 4)>>4;
		header_size = WAVE5_ALIGN128(16*mbw*mbh);
		uncomp_b16_size = (format == FORMAT_420_ARM) ? 384 : 512;

		size_dpb_lum = header_size + (uncomp_b16_size*mbw*mbh);
	}
	else if (mapType == TILED_FRAME_NO_BANK_MAP || mapType == TILED_FIELD_NO_BANK_MAP) {
		unit_size_hor_lum = stride;
		unit_size_ver_lum = (((height>>field_map)+127)/128) * 128; // unit vertical size is 128 pixel (8MB)
		size_dpb_lum      = unit_size_hor_lum * (unit_size_ver_lum<<field_map);
	}
	else if (mapType == TILED_FRAME_MB_RASTER_MAP || mapType == TILED_FIELD_MB_RASTER_MAP) {
		if (productId == PRODUCT_ID_960) {
			size_dpb_lum   = stride * height;

			// aligned to 8192*2 (0x4000) for top/bot field
			// use upper 20bit address only
			size_dpb_lum_4k =  ((size_dpb_lum + 16383)/16384)*16384;

			if (mapType == TILED_FIELD_MB_RASTER_MAP) {
				size_dpb_lum_4k = ((size_dpb_lum_4k+(0x8000-1))&~(0x8000-1));
			}

			size_dpb_lum = size_dpb_lum_4k;
		}
		else {
			size_dpb_lum    = stride * (height>>field_map);
			size_dpb_lum_4k = ((size_dpb_lum+(16384-1))/16384)*16384;
			size_dpb_lum    = size_dpb_lum_4k<<field_map;
		}
	}
	else {
		unit_size_hor_lum = stride;
		unit_size_ver_lum = (((height>>field_map)+63)/64) * 64; // unit vertical size is 64 pixel (4MB)
		size_dpb_lum      = unit_size_hor_lum * (unit_size_ver_lum<<field_map);
	}

	return size_dpb_lum;
}

Int32 CalcChromaSize(
	Int32               productId,
	Int32               stride,
	Int32               height,
	FrameBufferFormat   format,
	BOOL                cbcrIntl,
	TiledMapType        mapType,
	DRAMConfig*         pDramCfg
	)
{
	Int32 chr_size_y, chr_size_x;
	Int32 chr_vscale, chr_hscale;
	Int32 unit_size_hor_chr, unit_size_ver_chr;
	Int32 size_dpb_chr, size_dpb_chr_4k;
	Int32 field_map;

	unit_size_hor_chr = 0;
	unit_size_ver_chr = 0;

	chr_hscale = 1;
	chr_vscale = 1;

	switch (format) {
	case FORMAT_420_P10_16BIT_LSB:
	case FORMAT_420_P10_16BIT_MSB:
	case FORMAT_420_P10_32BIT_LSB:
	case FORMAT_420_P10_32BIT_MSB:
	case FORMAT_420:
		chr_hscale = 2;
		chr_vscale = 2;
		break;
	case FORMAT_224:
		chr_vscale = 2;
		break;
	case FORMAT_422:
	case FORMAT_422_P10_16BIT_LSB:
	case FORMAT_422_P10_16BIT_MSB:
	case FORMAT_422_P10_32BIT_LSB:
	case FORMAT_422_P10_32BIT_MSB:
		chr_hscale = 2;
		break;
	case FORMAT_444:
	case FORMAT_400:
#ifdef SUPPORT_PACKED_STREAM_FORMAT
	case FORMAT_YUYV:
	case FORMAT_YVYU:
	case FORMAT_UYVY:
	case FORMAT_VYUY:
	case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
	case FORMAT_YUYV_P10_16BIT_LSB:
	case FORMAT_YVYU_P10_16BIT_MSB:
	case FORMAT_YVYU_P10_16BIT_LSB:
	case FORMAT_UYVY_P10_16BIT_MSB:
	case FORMAT_UYVY_P10_16BIT_LSB:
	case FORMAT_VYUY_P10_16BIT_MSB:
	case FORMAT_VYUY_P10_16BIT_LSB:
	case FORMAT_YUYV_P10_32BIT_MSB:
	case FORMAT_YUYV_P10_32BIT_LSB:
	case FORMAT_YVYU_P10_32BIT_MSB:
	case FORMAT_YVYU_P10_32BIT_LSB:
	case FORMAT_UYVY_P10_32BIT_MSB:
	case FORMAT_UYVY_P10_32BIT_LSB:
	case FORMAT_VYUY_P10_32BIT_MSB:
	case FORMAT_VYUY_P10_32BIT_LSB:
		break;
#endif
	default:
		return 0;
	}

	if (mapType == TILED_FIELD_V_MAP || mapType == TILED_FIELD_NO_BANK_MAP || mapType == LINEAR_FIELD_MAP) {
		field_map = 1;
	}
	else {
		field_map = 0;
	}

	if (mapType == LINEAR_FRAME_MAP || mapType == LINEAR_FIELD_MAP) {
		switch (format) {
		case FORMAT_420:
			unit_size_hor_chr = stride/2;
			unit_size_ver_chr = height/2;
			break;
		case FORMAT_420_P10_16BIT_LSB:
		case FORMAT_420_P10_16BIT_MSB:
			unit_size_hor_chr = stride/2;
			unit_size_ver_chr = height/2;
			break;
		case FORMAT_420_P10_32BIT_LSB:
		case FORMAT_420_P10_32BIT_MSB:
			unit_size_hor_chr = WAVE5_ALIGN16(stride/2);
			unit_size_ver_chr = height/2;
			break;
		case FORMAT_422:
		case FORMAT_422_P10_16BIT_LSB:
		case FORMAT_422_P10_16BIT_MSB:
		case FORMAT_422_P10_32BIT_LSB:
		case FORMAT_422_P10_32BIT_MSB:
			unit_size_hor_chr = WAVE5_ALIGN32(stride/2);
			unit_size_ver_chr = height;
			break;
#ifdef SUPPORT_PACKED_STREAM_FORMAT
		case FORMAT_YUYV:
		case FORMAT_YVYU:
		case FORMAT_UYVY:
		case FORMAT_VYUY:
		case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
		case FORMAT_YUYV_P10_16BIT_LSB:
		case FORMAT_YVYU_P10_16BIT_MSB:
		case FORMAT_YVYU_P10_16BIT_LSB:
		case FORMAT_UYVY_P10_16BIT_MSB:
		case FORMAT_UYVY_P10_16BIT_LSB:
		case FORMAT_VYUY_P10_16BIT_MSB:
		case FORMAT_VYUY_P10_16BIT_LSB:
		case FORMAT_YUYV_P10_32BIT_MSB:
		case FORMAT_YUYV_P10_32BIT_LSB:
		case FORMAT_YVYU_P10_32BIT_MSB:
		case FORMAT_YVYU_P10_32BIT_LSB:
		case FORMAT_UYVY_P10_32BIT_MSB:
		case FORMAT_UYVY_P10_32BIT_LSB:
		case FORMAT_VYUY_P10_32BIT_MSB:
		case FORMAT_VYUY_P10_32BIT_LSB:
			unit_size_hor_chr = 0;
			unit_size_ver_chr = 0;
			break;
#endif
		default:
			break;
		}
		size_dpb_chr = (format == FORMAT_400) ? 0 : unit_size_ver_chr * unit_size_hor_chr;
	}
	else if (mapType == COMPRESSED_FRAME_MAP) {
		switch (format) {
		case FORMAT_420:
#ifdef SUPPORT_PACKED_STREAM_FORMAT
		case FORMAT_YUYV:       // 4:2:2 8bit packed
		case FORMAT_YVYU:
		case FORMAT_UYVY:
		case FORMAT_VYUY:
#endif
			size_dpb_chr = WAVE5_ALIGN16(stride/2)*height;
			break;
		default:
			/* 10bit */
			stride = WAVE5_ALIGN64(stride/2)+12; /* FIXME: need width information */
			size_dpb_chr = WAVE5_ALIGN32(stride)*WAVE5_ALIGN4(height);
			break;
		}
		size_dpb_chr = size_dpb_chr / 2;
	}
	else if (mapType == ARM_COMPRESSED_FRAME_MAP) {
		size_dpb_chr = 0;
	}
	else if (mapType == TILED_FRAME_NO_BANK_MAP || mapType == TILED_FIELD_NO_BANK_MAP) {
		chr_size_y = (height>>field_map)/chr_hscale;
		chr_size_x = stride/chr_vscale;

		unit_size_hor_chr = (chr_size_x > 4096) ? 8192:
							(chr_size_x > 2048) ? 4096 :
							(chr_size_x > 1024) ? 2048 :
							(chr_size_x >  512) ? 1024 : 512;
		unit_size_ver_chr = ((chr_size_y+127)/128) * 128; // unit vertical size is 128 pixel (8MB)

		size_dpb_chr = (format==FORMAT_400) ? 0 : (unit_size_hor_chr * (unit_size_ver_chr<<field_map));
	}
	else if (mapType == TILED_FRAME_MB_RASTER_MAP || mapType == TILED_FIELD_MB_RASTER_MAP) {
		{
			size_dpb_chr    = (format==FORMAT_400) ? 0 : ((stride * (height>>field_map))/(chr_hscale*chr_vscale));
			size_dpb_chr_4k = ((size_dpb_chr+(16384-1))/16384)*16384;
			size_dpb_chr    = size_dpb_chr_4k<<field_map;
		}
	}
	else {
		{  // productId != 960
			chr_size_y = (height>>field_map)/chr_hscale;
			chr_size_x = cbcrIntl == TRUE ? stride : stride/chr_vscale;

			unit_size_hor_chr = (chr_size_x> 4096) ? 8192:
								(chr_size_x> 2048) ? 4096 :
								(chr_size_x > 1024) ? 2048 :
								(chr_size_x >  512) ? 1024 : 512;
			unit_size_ver_chr = ((chr_size_y+63)/64) * 64; // unit vertical size is 64 pixel (4MB)

			size_dpb_chr  = (format==FORMAT_400) ? 0 : unit_size_hor_chr * (unit_size_ver_chr<<field_map);
			size_dpb_chr /= (cbcrIntl == TRUE ? 2 : 1);
		}
	}
	return size_dpb_chr;
}
