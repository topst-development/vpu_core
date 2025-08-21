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

/*!
 ***********************************************************************
 *
 * \file
 *		vpu_err.h : This file is part of libvpu
 * \date
 *		2009/03/23
 * \brief
 *		vpu err
 *
 ***********************************************************************
 */
#ifndef VPU_ERR_H_INCLUDED
#define VPU_ERR_H_INCLUDED

#define VPU_ERR_SEQ_HEADER_NOT_FOUND                       31	//	Sequence header not found. (AVC, VC1, H263, MPEG4, MPEG2, AVS)


/* H.264 */

//      ERROR_TYPE                                          	Category Description
#define VPU_AVCERR_SEQINITSTOP                             0	//	Host suspends sequence parameter set searching.
#define VPU_AVCERR_MEETSEQEND_SPSINIT                      1	//	Do not meet SPS in feeding stream.
#define VPU_AVCERR_MEETSEQEND_PPSINIT                      2	//	Do not meet PPS in feeding stream.
#define VPU_AVCERR_NAL_REF_IDR                             3	//SPS	nal_ref_idc shall not be equal to 0 when NAL unit contains a sequence parameter set.
#define VPU_AVCERR_RESERVED_ZERO_4BITS                     4	//SPS	reserved_zero_4bits shall be 0.
#define VPU_AVCERR_BIT_DEPTH_LUMA_MINUS8                   5	//SPS	bit_depth_luma_minus8 shall be 0.
#define VPU_AVCERR_BIT_DEPTH_CHROMA_MINUS8                 6	//SPS	bit_depth_chroma_minus8 shall be 0.
#define VPU_AVCERR_LOG2_MAX_FRAME_NUM_MINUS4               7	//SPS	log2_max_frame_num_minus4 shall be less than 12.
#define VPU_AVCERR_PIC_ORDER_CNT_TYPE                      8	//SPS	pic_order_cnt_type shall be less than 2.
#define VPU_AVCERR_LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4       9	//SPS	log2_max_pic_order_cnt_lsb_minus4 shall be less than 12 .
#define VPU_AVCERR_NUM_REF_FRAME_IN_PIC_ORDER_CNT_CYCLE    10	//SPS	num_ref_frame_in_pic_order_cnt_cycle shall be less than 255.
#define VPU_AVCERR_OFFSET_FOR_REF_FRAME                    11	//SPS	offset_for_ref_frame shall be in the range of -2^31 to 2^31 - 1.
#define VPU_AVCERR_NUM_REF_FRAMES                          12	//SPS	num_ref_frames shall be less than or equal to 16.
#define VPU_AVCERR_PIC_WIDTH_IN_MBS_MINUS1                 13	//SPS	pic_width_in_mbs_minus1 shall be less than or equal to MAX_MB_NUM_X -1.
#define VPU_AVCERR_PIC_HEIGHT_IN_MAP_MINUS1                14	//SPS	pic_height_in_map_minus1 shall be less than or equal to MAX_MB_NUM_Y -1.
#define VPU_AVCERR_DIRECT_8X8_INFERENCE_FLAG               15	//SPS	direct_8x8_inference_flag when frame_mbs_only_fag is equal to 0, direct_8x8_inference_flag shall be equal to 1.
#define VPU_AVCERR_FRAME_CROP_OFFSET                       16	//SPS	frame_crop_left_offset, frame_crop_right_offset, frame_crop_top_offset, frame_crop_bottom_offset
#define VPU_AVCERR_OVER_MAX_MB_SIZE                        17	//SPS	pic_width_in_mbs and pic_height_in_map shall be less than 2048.
#define VPU_AVCERR_NOT_SUPPORT_PROFILEIDC                  18	//SPS	66(base), 77(main), 100(high), 118(mvc), and 128 (mvc) Supported.
#define VPU_AVCERR_NOT_SUPPORT_LEVELIDC                    19	//SPS	Supported up to 51.



#endif//VPU_ERR_H_INCLUDED
