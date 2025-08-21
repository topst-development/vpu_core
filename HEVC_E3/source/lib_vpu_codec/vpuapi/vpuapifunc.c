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

#include "vpuapifunc.h"
#include "product.h"
#include "common_regdefine.h"
#include "wave4_regdefine.h"


#ifndef MIN
#define MIN(a, b)       (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif
#define MAX_LAVEL_IDX    16

/******************************************************************************
    define value
******************************************************************************/

/******************************************************************************
    Codec Instance Slot Management
******************************************************************************/

/*
 * GetCodecInstance() obtains a instance.
 * It stores a pointer to the allocated instance in *ppInst
 * and returns RETCODE_SUCCESS on success.
 * Failure results in 0(null pointer) in *ppInst and RETCODE_FAILURE.
 */
RetCode wave420l_GetCodecInstance(Uint32 coreIdx, CodecInst ** ppInst)
{
    int                     i;
    CodecInst*              pCodecInst = 0;

    //pCodecInst = gpCodecInstPoolWave420l;
	pCodecInst = &gsCodecInstPoolWave420l[0];
	for (i = 0; i < gHevcEncMaxInstanceNum; ++i, ++pCodecInst)
	{
		if (!pCodecInst->inUse)
			break;
	}

	if (i == gHevcEncMaxInstanceNum)
	{
		*ppInst = 0;
		return WAVE420L_RETCODE_FAILURE;
	}

	//pCodecInst->CodecInfo = &pCodecInst->CodecInfodata;
    pCodecInst->CodecInfo = (typeof(pCodecInst->CodecInfo))(&pCodecInst->CodecInfodata);
	pCodecInst->inUse = 1;
	*ppInst = pCodecInst;

    return WAVE420L_RETCODE_SUCCESS;
}

void wave420l_FreeCodecInstance(CodecInst * pCodecInst)
{
    pCodecInst->inUse = 0;
    pCodecInst->codecMode    = -1;
    pCodecInst->codecModeAux = -1;
    pCodecInst->CodecInfo = NULL;
}

RetCode wave420l_CheckInstanceValidity(CodecInst * pCodecInst)
{
    int i;

    CodecInst* pCodecInstPool = &gsCodecInstPoolWave420l[0];

	for (i = 0; i < gHevcEncMaxInstanceNum; ++i, ++pCodecInstPool) {
		if (pCodecInstPool == pCodecInst)
		{
		#ifdef TCC_ONBOARD
			if( pCodecInstPool->inUse != 0 )
				return RETCODE_SUCCESS;
		#else
			return WAVE420L_RETCODE_SUCCESS;
		#endif
		}
	}

    return WAVE420L_RETCODE_INVALID_HANDLE;
}

/******************************************************************************
    API Subroutines
******************************************************************************/

RetCode wave420l_CheckEncInstanceValidity(EncHandle handle)
{
    CodecInst * pCodecInst;
    RetCode ret;

    if (handle == NULL)
        return WAVE420L_RETCODE_INVALID_HANDLE;

    pCodecInst = handle;
    ret = wave420l_CheckInstanceValidity(pCodecInst);
    if (ret != WAVE420L_RETCODE_SUCCESS) {
        return WAVE420L_RETCODE_INVALID_HANDLE;
    }
    if (!pCodecInst->inUse) {
        return WAVE420L_RETCODE_INVALID_HANDLE;
    }

    return WAVE420L_RETCODE_SUCCESS;
}

RetCode wave420l_CheckEncParam(EncHandle handle, EncParam * param)
{
    CodecInst *pCodecInst;
    EncInfo *pEncInfo;

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    if (param == 0) {
        return WAVE420L_RETCODE_INVALID_PARAM;
    }
    if (param->skipPicture != 0 && param->skipPicture != 1) {
        return RETCODE_INVALID_PARAM;
    }
    if (param->skipPicture == 0) {
        if (param->sourceFrame == 0) {
            return RETCODE_INVALID_FRAME_BUFFER;
        }
        if (param->forceIPicture != 0 && param->forceIPicture != 1) {
            return WAVE420L_RETCODE_INVALID_PARAM;
        }
    }
    if (pEncInfo->openParam.bitRate == 0) { // no rate control
        if (pCodecInst->codecMode == HEVC_ENC) {
            if (param->forcePicQpEnable == 1) {
                if (param->forcePicQpI < 0 || param->forcePicQpI > 51)
                    return WAVE420L_RETCODE_INVALID_PARAM;

                if (param->forcePicQpP < 0 || param->forcePicQpP > 51)
                    return WAVE420L_RETCODE_INVALID_PARAM;

                if (param->forcePicQpB < 0 || param->forcePicQpB > 51)
                    return WAVE420L_RETCODE_INVALID_PARAM;
            }
            //if (pEncInfo->ringBufferEnable == 0)
            {
                if (param->picStreamBufferAddr % 16 || param->picStreamBufferSize == 0)
                    return WAVE420L_RETCODE_INVALID_PARAM;
            }
        }
        else {
                return WAVE420L_RETCODE_INVALID_PARAM;
        }
    }
    //if (pEncInfo->ringBufferEnable == 0)
    {
        if (param->picStreamBufferAddr % 8 || param->picStreamBufferSize == 0) {
            return WAVE420L_RETCODE_INVALID_PARAM;
        }
    }

    return WAVE420L_RETCODE_SUCCESS;
}

void wave420l_SetPendingInst(Uint32 coreIdx, CodecInst *inst)
{
    gpPendingInsthevcEnc = inst;
}

void wave420l_ClearPendingInst(Uint32 coreIdx)
{
    gpPendingInsthevcEnc = (CodecInst *)0;
}

CodecInst *wave420l_GetPendingInst(Uint32 coreIdx)
{
	return  (CodecInst *)gpPendingInsthevcEnc;
}

void *wave420l_GetPendingInstPointer( void )
{
	return ( (void*)&gpPendingInsthevcEnc );
}

RetCode wave420l_AllocateLinearFrameBuffer(
    TiledMapType            mapType,
    FrameBuffer*            fbArr,
    Uint32                  numOfFrameBuffers,
    Uint32                  sizeLuma,
    Uint32                  sizeChroma
    )
{
    Uint32      i;
    BOOL        yuv422Interleave = FALSE;
    BOOL        fieldFrame       = FALSE; //(BOOL)(mapType == LINEAR_FIELD_MAP);
    BOOL        cbcrInterleave   = (BOOL)(mapType == COMPRESSED_FRAME_MAP || fbArr[0].cbcrInterleave == TRUE);
    BOOL        reuseFb          = FALSE;


    if (mapType != COMPRESSED_FRAME_MAP) {
        switch (fbArr[0].format) {
        case FORMAT_YUYV:
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            yuv422Interleave = TRUE;
            break;
        default:
            yuv422Interleave = FALSE;
            break;
        }
    }

    for (i=0; i<numOfFrameBuffers; i++) {
        reuseFb = (fbArr[i].bufY != (Uint32)-1 && fbArr[i].bufCb != (Uint32)-1 && fbArr[i].bufCr != (Uint32)-1);
        if (reuseFb == FALSE) {
            if (yuv422Interleave == TRUE) {
                fbArr[i].bufCb = (PhysicalAddress)-1;
                fbArr[i].bufCr = (PhysicalAddress)-1;
            }
            else {
                if (fbArr[i].bufCb == (PhysicalAddress)-1) {
                    fbArr[i].bufCb = fbArr[i].bufY + (sizeLuma >> fieldFrame);
                }
                if (fbArr[i].bufCr == (PhysicalAddress)-1) {
                    if (cbcrInterleave == TRUE) {
                        fbArr[i].bufCr = (PhysicalAddress)-1;
                    }
                    else {
                        fbArr[i].bufCr = fbArr[i].bufCb + (sizeChroma >> fieldFrame);
                    }
                }
            }
        }
    }

    return WAVE420L_RETCODE_SUCCESS;
}

Int32 wave420l_ConfigSecAXIWave(Uint32 coreIdx, Int32 codecMode, SecAxiInfo *sa, Uint32 width, Uint32 height, Uint32 profile, Uint32 levelIdc)
{
    vpu_buffer_t vb;
    int offset;
    Uint32 size;
    Uint32 lumaSize;
    Uint32 chromaSize;

    UNREFERENCED_PARAMETER(codecMode);
    UNREFERENCED_PARAMETER(height);

    vb.size = sa->bufSize; //0x2E000;  //in vdi.c define VDI_WAVE420L_SRAM_SIZE		0x2E000     // 8Kx8X MAIN10 MAX size
    vb.phys_addr = sa->bufBase;
    vb.base = 0;
    vb.virt_addr = 0;

    if (!vb.size) {
        sa->bufSize                = 0;
        sa->u.wave4.useIpEnable    = 0;
        sa->u.wave4.useLfRowEnable = 0;
        sa->u.wave4.useBitEnable   = 0;
        sa->u.wave4.useSclEnable   = 0;
        sa->u.wave4.useSclPackedModeEnable = FALSE;
        sa->u.wave4.useEncImdEnable   = 0;
        sa->u.wave4.useEncLfEnable    = 0;
        sa->u.wave4.useEncRdoEnable   = 0;
        return 0;
    }

    sa->bufBase = vb.phys_addr;
    offset      = 0;
    /* Intra Prediction */
    if (sa->u.wave4.useIpEnable == TRUE) {
        sa->u.wave4.bufIp = sa->bufBase + offset;
        return 0;

        offset     = lumaSize + chromaSize;
        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    /* Loopfilter row */
    if (sa->u.wave4.useLfRowEnable == TRUE) {
        sa->u.wave4.bufLfRow = sa->bufBase + offset;
         {
            Uint32 level = levelIdc/30;
            if (level >= 5) {
                size = VPU_ALIGN32(width)/2 * 13 + VPU_ALIGN64(width)*4;
            }
            else {
                size = VPU_ALIGN64(width)*13;
            }
            offset += size;
        }
        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    if (sa->u.wave4.useBitEnable == TRUE) {
        sa->u.wave4.bufBit = sa->bufBase + offset;
        {
            size = 34*1024; /* Fixed size */
        }
        offset += size;
        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }
    if (sa->u.wave4.useSclEnable == TRUE) {
        sa->u.wave4.bufScaler = sa->bufBase + offset;
        {
            size = VPU_ALIGN128(width*10)/8 * 4;
        }
        offset += size;

        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }
    if (sa->u.wave4.useSclPackedModeEnable == TRUE) {
        sa->u.wave4.bufScalerPackedData = sa->bufBase + offset;
        {
            // size = Ceil128(width*10)/8 * 1
            size += VPU_ALIGN128(width*10)/8;
        }
        offset += size;

        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    if (sa->u.wave4.useEncImdEnable == TRUE) {
         /* Main   profile(8bit) : Align32(picWidth)
          */
        sa->u.wave4.bufImd = sa->bufBase + offset;
        offset    += VPU_ALIGN32(width);

        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    if (sa->u.wave4.useEncLfEnable == TRUE) {
        /* Main   profile(8bit) :
         *              Luma   = Align64(picWidth) * 5
         *              Chroma = Align64(picWidth) * 3
         */
        Uint32 luma   = (profile == HEVC_PROFILE_MAIN10 ? 7 : 5);
        Uint32 chroma = (profile == HEVC_PROFILE_MAIN10 ? 5 : 3);

        sa->u.wave4.bufLf = sa->bufBase + offset;

        lumaSize   = VPU_ALIGN64(width) * luma;
        chromaSize = VPU_ALIGN64(width) * chroma;
        offset    += lumaSize + chromaSize;

        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    if (sa->u.wave4.useEncRdoEnable == TRUE) {
        sa->u.wave4.bufRdo = sa->bufBase + offset;
        offset    += (VPU_ALIGN64(width)/32) * 22*16;

        if (offset > vb.size) {
            sa->bufSize = 0;
            return 0;
        }
    }

    // Since FW is using part of temp buffer, you need to use tempbuffer size by adding 5Kbytes (5x1024) to the corresponding calculation size. (TELMON-41)
    offset += 5*1024;

    if (offset > vb.size) {
        sa->bufSize = 0;
        return 0;
    }

    sa->bufSize = offset;

    return 1;
}

Int32 wave420l_CalcStride(
    Uint32              width,
    Uint32              height,
    FrameBufferFormat   format,
    BOOL                cbcrInterleave,
    TiledMapType        mapType,
    BOOL                isVP9
    )
{
    Uint32  lumaStride   = 0;

    lumaStride = VPU_ALIGN32(width);

    if (mapType == LINEAR_FRAME_MAP ) {
        switch (format) {
        case FORMAT_420:
            /* nothing to do */
            break;
        case FORMAT_422: /* nothing to do */
            break;
        case FORMAT_YUYV:       // 4:2:2 8bit packed
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            lumaStride = VPU_ALIGN32(width) * 2;
            break;
        default:
            break;
        }
    }
    else if (mapType == COMPRESSED_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
        case FORMAT_422:
        case FORMAT_YUYV:
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            break;
        default:
            return -1;
        }
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

Int32 wave420l_CalcLumaSize(
    Int32           productId,
    Int32           stride,
    Int32           height,
    FrameBufferFormat format,
    BOOL            cbcrIntl,
    TiledMapType    mapType,
    DRAMConfig      *pDramCfg
    )
{
    Int32 unit_size_hor_lum, unit_size_ver_lum, size_dpb_lum, field_map; //, size_dpb_lum_4k;
    UNREFERENCED_PARAMETER(cbcrIntl);

    {
        field_map = 0;
    }

    if (mapType == LINEAR_FRAME_MAP) {
        size_dpb_lum = stride * height;
    }
    else if (mapType == COMPRESSED_FRAME_MAP) {
        size_dpb_lum = stride * height;
    }
    else {
        {  // productId != 960
            unit_size_hor_lum = stride;
            unit_size_ver_lum = (((height>>field_map)+63)/64) * 64; // unit vertical size is 64 pixel (4MB)
            size_dpb_lum      = unit_size_hor_lum * (unit_size_ver_lum<<field_map);
        }
    }

    return size_dpb_lum;
}

Int32 wave420l_CalcChromaSize(
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
    Int32 size_dpb_chr; //, size_dpb_chr_4k;
    Int32 field_map;

    unit_size_hor_chr = 0;
    unit_size_ver_chr = 0;

    chr_hscale = 1;
    chr_vscale = 1;

    switch (format) {
    case FORMAT_420:
        chr_hscale = 2;
        chr_vscale = 2;
        break;
    case FORMAT_422:
        chr_hscale = 2;
        break;
    case FORMAT_YUYV:
    case FORMAT_YVYU:
    case FORMAT_UYVY:
    case FORMAT_VYUY:
        break;
    default:
        return 0;
    }

    {
        field_map = 0;
    }

    if (mapType == LINEAR_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
            unit_size_hor_chr = stride/2;
            unit_size_ver_chr = height/2;
            break;
        case FORMAT_422:
            unit_size_hor_chr = VPU_ALIGN32(stride/2);
            unit_size_ver_chr = height;
            break;
        case FORMAT_YUYV:
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            unit_size_hor_chr = 0;
            unit_size_ver_chr = 0;
            break;
        default:
            break;
        }
        size_dpb_chr = (format == FORMAT_400) ? 0 : unit_size_ver_chr * unit_size_hor_chr;
    }
    else if (mapType == COMPRESSED_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
        case FORMAT_YUYV:       // 4:2:2 8bit packed
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            size_dpb_chr = VPU_ALIGN16(stride/2)*height;
            break;
        default:
            /* 10bit */
            stride = VPU_ALIGN64(stride/2)+12;
            size_dpb_chr = VPU_ALIGN32(stride)*VPU_ALIGN4(height);
            break;
        }
        size_dpb_chr = size_dpb_chr / 2;
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

