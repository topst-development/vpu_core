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

#ifndef VPUAPI_UTIL_H_INCLUDED
#define VPUAPI_UTIL_H_INCLUDED

#include "vpuapi.h"
#include "wave4_regdefine.h"

// COD_STD
enum {
	HEVC_ENC                = 1,
	MAX_CODECS,
};

// BIT_RUN command
enum {
	ENC_SEQ_INIT            = 1,
	ENC_SEQ_END             = 2,
	PIC_RUN                 = 3,
	SET_FRAME_BUF           = 4,
	ENCODE_HEADER           = 5,
	ENC_PARA_SET            = 6,
	RC_CHANGE_PARAMETER     = 9,
	VPU_SLEEP               = 10,
	VPU_WAKE                = 11,
	ENC_ROI_INIT            = 12,
	FIRMWARE_GET            = 0xf
};

enum {
	SRC_BUFFER_EMPTY        = 0,    //!< source buffer doesn't allocated.
	SRC_BUFFER_ALLOCATED    = 1,    //!< source buffer has been allocated.
	SRC_BUFFER_SRC_LOADED   = 2,    //!< source buffer has been allocated.
	SRC_BUFFER_USE_ENCODE   = 3     //!< source buffer was sent to VPU. but it was not used for encoding.
};
#define MAX(_a, _b)             (_a >= _b ? _a : _b)

#define HEVC_MAX_SUB_LAYER_ID   6

//#define API_DEBUG
#ifdef API_DEBUG
#ifdef _MSC_VER
#define APIDPRINT(_fmt, ...)            printf(_fmt, __VA_ARGS__)
#else
#define APIDPRINT(_fmt, ...)            printf(_fmt, ##__VA_ARGS__)
#endif
#else
#define APIDPRINT(_fmt, ...)
#endif

extern Uint32 __VPU_BUSY_TIMEOUT;
/**
 * PRODUCT: CODA960/CODA980/WAVE320
 */
typedef struct {
	union {
		struct {
			int             useIpEnable;
			int             useSclEnable;
			int             useSclPackedModeEnable;
			int             useLfRowEnable;
			int             useBitEnable;
			PhysicalAddress bufIp;
			PhysicalAddress bufLfRow;
			PhysicalAddress bufBit;
			PhysicalAddress bufScaler;
			PhysicalAddress bufScalerPackedData;
			int             useEncImdEnable;    //Wave420L does not support EncImd secondary AXI (TELMON-28)
			int             useEncRdoEnable;
			int             useEncLfEnable;
			PhysicalAddress bufImd;             //Wave420L does not support EncImd secondary AXI (TELMON-28)
			PhysicalAddress bufRdo;
			PhysicalAddress bufLf;
		} wave4;
	} u;
	int             bufSize;
	PhysicalAddress bufBase;
} SecAxiInfo;

typedef struct {
	EncOpenParam        openParam;
	EncInitialInfo      initialInfo;
	PhysicalAddress     streamRdPtr;
	PhysicalAddress     streamWrPtr;
	int                 streamEndflag;
	PhysicalAddress     streamRdPtrRegAddr;
	PhysicalAddress     streamWrPtrRegAddr;
	PhysicalAddress     streamBufStartAddr;
	PhysicalAddress     streamBufEndAddr;
	PhysicalAddress     currentPC;
	PhysicalAddress     busyFlagAddr;
	int                 streamBufSize;
	int                 mapType;
	FrameBuffer         frameBufPool[MAX_REG_FRAME];
	vpu_buffer_t        vbFrame;
	vpu_buffer_t        vbPPU;
	int                 frameAllocExt;
	int                 ppuAllocExt;
	int                 numFrameBuffers;
	int                 stride;
	int                 frameBufferHeight;
	//int                 rotationAngle;
	int                 initialInfoObtained;
	//int                 ringBufferEnable;
	SecAxiInfo          secAxiInfo;

	vpu_buffer_t     secAxiBuf;

	int                 sliceIntEnable;       /*!<< WAVE420 only */

	int                 frameIdx;               /*!<< CODA980 */
	int                 lineBufIntEn;
	vpu_buffer_t        vbWork;
	vpu_buffer_t        vbScratch;

	vpu_buffer_t        vbTemp;                 //!< Temp buffer (WAVE420)
	vpu_buffer_t        vbMV;                   //!< colMV buffer (WAVE420)
	vpu_buffer_t        vbFbcYTbl;              //!< FBC Luma table buffer (WAVE420)
	vpu_buffer_t        vbFbcCTbl;              //!< FBC Chroma table buffer (WAVE420)
	vpu_buffer_t        vbSubSamBuf;            //!< Sub-sampled buffer for ME (WAVE420)

	//    DRAMConfig          dramCfg;                /*!<< CODA960 */

	Uint32 prefixSeiNalEnable;
	Uint32 prefixSeiDataSize;
	Uint32 prefixSeiDataEncOrder;
	PhysicalAddress prefixSeiNalAddr;
	Uint32 suffixSeiNalEnable;
	Uint32 suffixSeiDataSize;
	Uint32 suffixSeiDataEncOrder;
	PhysicalAddress suffixSeiNalAddr;

	Int32   errorReasonCode;
#if !defined(TCC_ONBOARD)
	Uint64          curPTS;             /**! Current timestamp in 90KHz */
	Uint64          ptsMap[32];         /**! PTS mapped with source frame index */
#endif
} EncInfo;


typedef struct CodecInst {
	Int32   inUse;
	Int32   instIndex;
	Int32   coreIdx;
	Int32   codecMode;
	Int32   codecModeAux;
	Int32   productId;
	Int32   loggingEnable;
	Uint32  isDecoder;
	union {
		EncInfo encInfo;
	}* CodecInfo;
	union {
		EncInfo encInfo;
	} CodecInfodata;
} CodecInst;

/*******************************************************************************
 * H.265 USER DATA(VUI and SEI)                                           *
 *******************************************************************************/
#define H265_MAX_DPB_SIZE               17
#define H265_MAX_NUM_SUB_LAYER          8
#define H265_MAX_NUM_ST_RPS             64
#define H265_MAX_CPB_CNT                32
#define H265_MAX_NUM_VERTICAL_FILTERS   5
#define H265_MAX_NUM_HORIZONTAL_FILTERS 3
#define H265_MAX_TAP_LENGTH             32
#define H265_MAX_NUM_KNEE_POINT         999

typedef struct
{
	Uint32   offset;
	Uint32   size;
} user_data_entry_t;

typedef struct
{
	Int16   left;
	Int16   right;
	Int16   top;
	Int16   bottom;
} win_t;

typedef struct
{
	Uint8    nal_hrd_param_present_flag;
	Uint8    vcl_hrd_param_present_flag;
	Uint8    sub_pic_hrd_params_present_flag;
	Uint8    tick_divisor_minus2;
	Int8     du_cpb_removal_delay_inc_length_minus1;
	Int8     sub_pic_cpb_params_in_pic_timing_sei_flag;
	Int8     dpb_output_delay_du_length_minus1;
	Int8     bit_rate_scale;
	Int8     cpb_size_scale;
	Int8     initial_cpb_removal_delay_length_minus1;
	Int8     cpb_removal_delay_length_minus1;
	Int8     dpb_output_delay_length_minus1;

	Uint8    fixed_pic_rate_gen_flag[H265_MAX_NUM_SUB_LAYER];
	Uint8    fixed_pic_rate_within_cvs_flag[H265_MAX_NUM_SUB_LAYER];
	Uint8    low_delay_hrd_flag[H265_MAX_NUM_SUB_LAYER];
	Int8     cpb_cnt_minus1[H265_MAX_NUM_SUB_LAYER];
	Int16    elemental_duration_in_tc_minus1[H265_MAX_NUM_SUB_LAYER];

	Uint32   nal_bit_rate_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
	Uint32   nal_cpb_size_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
	Uint32   nal_cpb_size_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
	Uint32   nal_bit_rate_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
	Uint8    nal_cbr_flag[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];

	Uint32   vcl_bit_rate_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
	Uint32   vcl_cpb_size_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
	Uint32   vcl_cpb_size_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
	Uint32   vcl_bit_rate_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
	Uint8    vcl_cbr_flag[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
} h265_hrd_param_t;

typedef struct
{
	Uint8    aspect_ratio_info_present_flag;
	Uint8    aspect_ratio_idc;
	Uint8    overscan_info_present_flag;
	Uint8    overscan_appropriate_flag;

	Uint8    video_signal_type_present_flag;
	Int8     video_format;

	Uint8    video_full_range_flag;
	Uint8    colour_description_present_flag;

	Uint16   sar_width;
	Uint16   sar_height;

	Uint8    colour_primaries;
	Uint8    transfer_characteristics;
	Uint8    matrix_coefficients;

	Uint8    chroma_loc_info_present_flag;
	Int8     chroma_sample_loc_type_top_field;
	Int8     chroma_sample_loc_type_bottom_field;

	Uint8    neutral_chroma_indication_flag;

	Uint8    field_seq_flag;

	Uint8    frame_field_info_present_flag;
	Uint8    default_display_window_flag;
	Uint8    vui_timing_info_present_flag;
	Uint8    vui_poc_proportional_to_timing_flag;

	Uint32   vui_num_units_in_tick;
	Uint32   vui_time_scale;

	Uint8    vui_hrd_parameters_present_flag;
	Uint8    bitstream_restriction_flag;

	Uint8    tiles_fixed_structure_flag;
	Uint8    motion_vectors_over_pic_boundaries_flag;
	Uint8    restricted_ref_pic_lists_flag;
	Int8     min_spatial_segmentation_idc;
	Int8     max_bytes_per_pic_denom;
	Int8     max_bits_per_mincu_denom;

	Int16    vui_num_ticks_poc_diff_one_minus1;
	Int8     log2_max_mv_length_horizontal;
	Int8     log2_max_mv_length_vertical;

	win_t    def_disp_win;
	h265_hrd_param_t hrd_param;
} h265_vui_param_t;

typedef struct
{
	Uint32   display_primaries_x[3];
	Uint32   display_primaries_y[3];
	Uint32   white_point_x                   : 16;
	Uint32   white_point_y                   : 16;
	Uint32   max_display_mastering_luminance : 32;
	Uint32   min_display_mastering_luminance : 32;
} h265_mastering_display_colour_volume_t;

typedef struct
{
	Uint32   ver_chroma_filter_idc               : 8;
	Uint32   hor_chroma_filter_idc               : 8;
	Uint32   ver_filtering_field_processing_flag : 1;
	Uint32   target_format_idc                   : 2;
	Uint32   num_vertical_filters                : 3;
	Uint32   num_horizontal_filters              : 3;
	Uint8    ver_tap_length_minus1[H265_MAX_NUM_VERTICAL_FILTERS];
	Uint8    hor_tap_length_minus1[H265_MAX_NUM_HORIZONTAL_FILTERS];
	Int32    ver_filter_coeff[H265_MAX_NUM_VERTICAL_FILTERS][H265_MAX_TAP_LENGTH];
	Int32    hor_filter_coeff[H265_MAX_NUM_HORIZONTAL_FILTERS][H265_MAX_TAP_LENGTH];
} h265_chroma_resampling_filter_hint_t;

typedef struct
{
	Uint32   knee_function_id;
	Uint8    knee_function_cancel_flag;

	Uint8    knee_function_persistence_flag;
	Uint32   input_disp_luminance;
	Uint32   input_d_range;
	Uint32   output_d_range;
	Uint32   output_disp_luminance;
	Uint16   num_knee_points_minus1;
	Uint16   input_knee_point[H265_MAX_NUM_KNEE_POINT];
	Uint16   output_knee_point[H265_MAX_NUM_KNEE_POINT];
} h265_knee_function_info_t;

typedef struct
{
	Int8   status;           // 0 : empty, 1 : occupied
	Int8   pic_struct;
	Int8   source_scan_type;
	Int8   duplicate_flag;
} h265_sei_pic_timing_t;

#ifdef __cplusplus
extern "C" {
#endif

RetCode wave420l_GetCodecInstance(Uint32 coreIdx, CodecInst **ppInst);

void    wave420l_FreeCodecInstance(CodecInst *pCodecInst);

Int32 wave420l_ConfigSecAXIWave(
								Uint32      coreIdx,
								Int32       codecMode,
								SecAxiInfo* sa,
								Uint32      width,
								Uint32      height,
								Uint32      profile,
								Uint32      level
							   );

RetCode wave420l_AllocateLinearFrameBuffer(
										   TiledMapType            mapType,
										   FrameBuffer*            fbArr,
										   Uint32                  numOfFrameBuffers,
										   Uint32                  sizeLuma,
										   Uint32                  sizeChroma
										  );

RetCode wave420l_CheckEncInstanceValidity(EncHandle handle);
RetCode CheckEncOpenParam(EncOpenParam *pop);
RetCode wave420l_CheckEncParam(EncHandle handle, EncParam *param);

void wave420l_SetPendingInst(Uint32 coreIdx, CodecInst *inst);
void wave420l_ClearPendingInst(Uint32 coreIdx);
CodecInst *wave420l_GetPendingInst(Uint32 coreIdx);
void *wave420l_GetPendingInstPointer(void);


/**
 * \brief   It returns the stride of framebuffer in byte.
 *
 * \param   width           picture width in pixel.
 * \param   format          YUV format. see FrameBufferFormat structure in vpuapi.h
 * \param   cbcrInterleave
 * \param   mapType         map type. see TiledMapType in vpuapi.h
 */
Int32 wave420l_CalcStride(
						  Uint32              width,
						  Uint32              height,
						  FrameBufferFormat   format,
						  BOOL                cbcrInterleave,
						  TiledMapType        mapType,
						  BOOL                isVP9
						 );

Int32 wave420l_CalcLumaSize(
							Int32               productId,
							Int32               stride,
							Int32               height,
							FrameBufferFormat   format,
							BOOL                cbcrIntl,
							TiledMapType        mapType,
							DRAMConfig*         pDramCfg
						   );

Int32 wave420l_CalcChromaSize(
							  Int32               productId,
							  Int32               stride,
							  Int32               height,
							  FrameBufferFormat   format,
							  BOOL                cbcrIntl,
							  TiledMapType        mapType,
							  DRAMConfig*         pDramCfg
							 );

#ifdef __cplusplus
}
#endif

#endif // endif VPUAPI_UTIL_H_INCLUDED

