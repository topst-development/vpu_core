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

#ifndef VPUAPI_H_INCLUDED
#define VPUAPI_H_INCLUDED

#include "wave5config.h"
#include "wave5_pre_define.h"

enum {NONE=0, INFO, WARN, ERR, TRACE, MAX_LOG_LEVEL};

typedef struct vpu_buffer_t {
	int size;
	PhysicalAddress phys_addr;
	PhysicalAddress base;
	unsigned long virt_addr;
} vpu_buffer_t;

typedef enum {
	VDI_LITTLE_ENDIAN           = 0,      /* 64bit LE */
	VDI_BIG_ENDIAN,                       /* 64bit BE */
	VDI_32BIT_LITTLE_ENDIAN,
	VDI_32BIT_BIG_ENDIAN,
	/* WAVE PRODUCTS */
	VDI_128BIT_LITTLE_ENDIAN    = 16,
	VDI_128BIT_LE_BYTE_SWAP,
	VDI_128BIT_LE_WORD_SWAP,
	VDI_128BIT_LE_WORD_BYTE_SWAP,
	VDI_128BIT_LE_DWORD_SWAP,
	VDI_128BIT_LE_DWORD_BYTE_SWAP,
	VDI_128BIT_LE_DWORD_WORD_SWAP,
	VDI_128BIT_LE_DWORD_WORD_BYTE_SWAP,
	VDI_128BIT_BE_DWORD_WORD_BYTE_SWAP,
	VDI_128BIT_BE_DWORD_WORD_SWAP,
	VDI_128BIT_BE_DWORD_BYTE_SWAP,
	VDI_128BIT_BE_DWORD_SWAP,
	VDI_128BIT_BE_WORD_BYTE_SWAP,
	VDI_128BIT_BE_WORD_SWAP,
	VDI_128BIT_BE_BYTE_SWAP,
	VDI_128BIT_BIG_ENDIAN       = 31,
	VDI_ENDIAN_MAX
} EndianMode;

#define VDI_128BIT_ENDIAN_MASK      0xf

#ifdef TCC_ONBOARD
	#ifdef ARM_ADS
		#define Wave5WriteReg(CORE, ADDR, DATA) *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
		#define Wave5ReadReg(CORE, ADDR)        *(volatile unsigned int *)(ADDR)
		#define Wave5WriteMem(CORE, ADDR, DATA) *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
		#define Wave5ReadMem(CORE, ADDR)        *(volatile unsigned int *)(ADDR)
		#define VirtualWriteReg(ADDR, DATA)	*((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
//	#elif defined(_WIN32_WCE) || defined(__LINUX__) || defined(__ANDROID__)
	#elif defined(WIN32) || defined(__LINUX__) || defined(__ANDROID__)
		extern void Wave5WriteReg(int idx, unsigned int ADDR, unsigned int DATA);
		extern unsigned int Wave5ReadReg(int idx, unsigned int ADDR);
		#define Wave5WriteMem(a,b,c,d)
		#define Wave5ReadMem(a,b,c,d)
		#define VirtualWriteReg( ADDR, DATA )   *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
	#endif
#else
	#define Wave5WriteReg(CORE, ADDR, DATA)              vdi_write_register(CORE, ADDR, DATA)		// system register write
	#define Wave5ReadReg(CORE, ADDR)                     vdi_read_register(CORE, ADDR)			// system register write
	#define Wave5WriteMem(CORE, ADDR, DATA, LEN, ENDIAN) vdi_write_memory(CORE, ADDR, DATA, LEN, ENDIAN)	// system memory write
	#define Wave5ReadMem(CORE, ADDR, DATA, LEN, ENDIAN)  vdi_read_memory(CORE, ADDR, DATA, LEN, ENDIAN)	// system memory write
#endif

#define WAVE5_DEC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (((_w+63)/64)*((_h+63)/64)*256+ 64)
#define WAVE5_DEC_VP9_MVCOL_BUF_SIZE(_w, _h)    (((WAVE5_ALIGN64(_w)*WAVE5_ALIGN64(_h))>>2))

#define WAVE5_ENC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (WAVE5_ALIGN64(_w)/64*WAVE5_ALIGN64(_h)/64*128)
#define WAVE5_FBC_LUMA_TABLE_SIZE(_w, _h)       (WAVE5_ALIGN16(_h)*WAVE5_ALIGN256(_w)/32)
#define WAVE5_FBC_CHROMA_TABLE_SIZE(_w, _h)     (WAVE5_ALIGN16(_h)*WAVE5_ALIGN256(_w/2)/32)

#define AFBCE_DISALBE                  0
#define AFBCE_FORMAT_420               1
#define AFBCE_FORMAT_420_P10_16BIT_LSB 2

//------------------------------------------------------------------------------
// common struct and definition
//------------------------------------------------------------------------------

/**
* @brief
@verbatim
This is an enumeration for declaring the operation mode of DEC_PIC_HDR command. (WAVE decoder only)

@endverbatim
*/
typedef enum {
	INIT_SEQ_NORMAL             = 0x01,  /**< It initializes some parameters (i.e. buffer mode) required for decoding sequence, performs sequence header, and returns information on the sequence. */
	INIT_SEQ_W_THUMBNAIL        = 0x11, /**< It decodes only the first I picture of sequence to get thumbnail. */
	DEC_PIC_HDR_OPTION_MAX      = 0x7FFFFFFF
} DEC_PIC_HDR_OPTION;

/**
* @brief
@verbatim
This is an enumeration for declaring the running option of DEC_PIC command. (WAVE decoder only)

@endverbatim
*/
typedef enum {
	DEC_PIC_NORMAL              = 0x00, /**< It is normal mode of DEC_PIC command. */
	DEC_PIC_W_THUMBNAIL         = 0x10, /**< It handles CRA picture as BLA picture not to use reference from the previously decoded pictures. */
	SKIP_NON_IRAP               = 0x11, /**< It is thumbnail mode (skip non-IRAP without reference reg.) */
	SKIP_NON_RECOVERY           = 0x12, /**< It skips to decode non-IRAP pictures. */
	SKIP_NON_REF_PIC            = 0x13, /**< It skips to decode non-reference pictures which correspond to sub-layer non-reference picture with MAX_DEC_TEMP_ID. (The sub-layer non-reference picture is the one whose nal_unit_type equal to TRAIL_N, TSA_N, STSA_N, RADL_N, RASL_N, RSV_VCL_N10, RSV_VCL_N12, or RSV_VCL_N14. )*/
	SKIP_TEMPORAL_LAYER         = 0x14, /**< It decodes only frames whose temporal id is equal to or less than MAX_DEC_TEMP_ID. */
	DEC_PIC_OPTION_MAX          = 0x7FFFFFFF
} DEC_PIC_OPTION;

/************************************************************************/
/* Limitations                                                          */
/************************************************************************/
#define MAX_GDI_IDX                             31	/* The 32nd framebuffer is reserved for WTL */
#define MAX_REG_FRAME                           MAX_GDI_IDX*2 // 2 for WTL


/************************************************************************/
/* PROFILE & LEVEL                                                      */
/************************************************************************/
/* HEVC */
#define HEVC_PROFILE_MAIN           1
#define HEVC_PROFILE_MAIN10         2
#define HEVC_PROFILE_STILLPICTURE   3
/* VP9 */
#define VP9_PROFILE_0               0
#define VP9_PROFILE_1               1
#define VP9_PROFILE_2               2
#define VP9_PROFILE_3               3
/* Tier */
#define HEVC_TIER_MAIN              0
#define HEVC_TIER_HIGH              1
/* Level */
#define HEVC_LEVEL(_Major, _Minor)  (_Major*10+_Minor)

/* H.264 */
#define H264_PROFILE_BASELINE       66
#define H264_PROFILE_MAIN           77
#define H264_PROFILE_EXTENDED       88
#define H264_PROFILE_HIGH           100
#define H264_PROFILE_HIGH10         110
#define H264_PROFILE_HIGH10_INTRA   120
#define H264_PROFILE_HIGH422        122
#define H264_PROFILE_HIGH444        244
#define H264_PROFILE_CAVLC_444_INTRA 44

/* H265 USER_DATA(SPS & SEI) ENABLE FLAG */
#define H265_USERDATA_FLAG_RESERVED_0                           (0)
#define H265_USERDATA_FLAG_RESERVED_1                           (1)
#define H265_USERDATA_FLAG_VUI                                  (2)
#define H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS (3)  /* SEI Prefix: alternative_transfer_characteristics */
#define H265_USERDATA_FLAG_PIC_TIMING                           (4)
#define H265_USERDATA_FLAG_ITU_T_T35_PRE                        (5)  /* SEI Prefix: user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_UNREGISTERED_PRE                     (6)  /* SEI Prefix: user_data_unregistered */
#define H265_USERDATA_FLAG_ITU_T_T35_SUF                        (7)  /* SEI Suffix: user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_UNREGISTERED_SUF                     (8)  /* SEI Suffix: user_data_unregistered */
#define H265_USERDATA_FLAG_RESERVED_9                           (9)  /* SEI RESERVED */
#define H265_USERDATA_FLAG_MASTERING_COLOR_VOL                 (10)  /* SEI Prefix: mastering_display_color_volume */
#define H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT       (11)  /* SEI Prefix: chroma_resampling_filter_hint */
#define H265_USERDATA_FLAG_KNEE_FUNCTION_INFO                  (12)  /* SEI Prefix: knee_function_info */
#define H265_USERDATA_FLAG_TONE_MAPPING_INFO                   (13)  /* SEI Prefix: tone_mapping_info */
#define H265_USER_DATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO    (14)  /* SEI Prefix: film_grain_characteristics_info */
#define H265_USER_DATA_FLAG_CONTENT_LIGHT_LEVEL_INFO           (15)  /* SEI Prefix: content_light_level_info */
#define H265_USER_DATA_FLAG_COLOUR_REMAPPING_INFO              (16)  /* SEI Prefix: content_light_level_info */
#define H265_USERDATA_FLAG_ITU_T_T35_PRE_1                     (28)  /* SEI Prefix: additional user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_ITU_T_T35_PRE_2                     (29)  /* SEI Prefix: additional user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_ITU_T_T35_SUF_1                     (30)  /* SEI Suffix: additional user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_ITU_T_T35_SUF_2                     (31)  /* SEI Suffix: additional user_data_registered_itu_t_t35 */

#define AFBC_VERSION                4
#define AFBC_HEADER_SIZE            16
#define AFBC_FILEHEADER_SIZE        32
#define AFBC_SUBSAMPLING_420        1


/************************************************************************/
/* Error codes                                                          */
/************************************************************************/

/**
* @brief    This is an enumeration for declaring return codes from API function calls.
The meaning of each return code is the same for all of the API functions, but the reasons of
non-successful return might be different. Some details of those reasons are
briefly described in the API definition chapter. In this chapter, the basic meaning
of each return code is presented.
*/
typedef enum {
	RETCODE_SUCCESS,                    /**< This means that operation was done successfully.  */  /* 0  */
	RETCODE_FAILURE,                    /**< This means that operation was not done successfully. When un-recoverable decoder error happens such as header parsing errors, this value is returned from VPU API.  */
	RETCODE_INVALID_HANDLE,             /**< This means that the given handle for the current API function call was invalid (for example, not initialized yet, improper function call for the given handle, etc.).  */
	RETCODE_INVALID_PARAM,              /**< This means that the given argument parameters (for example, input data structure) was invalid (not initialized yet or not valid anymore). */
	RETCODE_INVALID_COMMAND,            /**< This means that the given command was invalid (for example, undefined, or not allowed in the given instances).  */
	RETCODE_ROTATOR_OUTPUT_NOT_SET,     /**< This means that rotator output buffer was not allocated even though postprocessor (rotation, mirroring, or deringing) is enabled. */  /* 5  */
	RETCODE_ROTATOR_STRIDE_NOT_SET,     /**< This means that rotator stride was not provided even though postprocessor (rotation, mirroring, or deringing) is enabled.  */
	RETCODE_FRAME_NOT_COMPLETE,         /**< This means that frame decoding operation was not completed yet, so the given API function call cannot be allowed.  */
	RETCODE_INVALID_FRAME_BUFFER,       /**< This means that the given source frame buffer pointers were invalid in encoder (not initialized yet or not valid anymore).  */
	RETCODE_INSUFFICIENT_FRAME_BUFFERS, /**< This means that the given numbers of frame buffers were not enough for the operations of the given handle. This return code is only received when calling VPU_DecRegisterFrameBuffer() or VPU_EncRegisterFrameBuffer() function. */
	RETCODE_INVALID_STRIDE,             /**< This means that the given stride was invalid (for example, 0, not a multiple of 8 or smaller than picture size). This return code is only allowed in API functions which set stride.  */   /* 10 */
	RETCODE_WRONG_CALL_SEQUENCE,        /**< This means that the current API function call was invalid considering the allowed sequences between API functions (for example, missing one crucial function call before this function call).  */
	RETCODE_CALLED_BEFORE,              /**< This means that multiple calls of the current API function for a given instance are invalid. */
	RETCODE_NOT_INITIALIZED,            /**< This means that VPU was not initialized yet. Before calling any API functions, the initialization API function, VPU_Init(), should be called at the beginning.  */
	RETCODE_USERDATA_BUF_NOT_SET,       /**< This means that there is no memory allocation for reporting userdata. Before setting user data enable, user data buffer address and size should be set with valid value. */
	RETCODE_MEMORY_ACCESS_VIOLATION,    /**< This means that access violation to the protected memory has been occurred. */   /* 15 */
	RETCODE_VPU_RESPONSE_TIMEOUT,       /**< This means that VPU response time is too long, time out. */
	RETCODE_INSUFFICIENT_RESOURCE,      /**< This means that VPU cannot allocate memory due to lack of memory. */
	RETCODE_NOT_FOUND_BITCODE_PATH,     /**< This means that BIT_CODE_FILE_PATH has a wrong firmware path or firmware size is 0 when calling VPU_InitWithBitcode() function.  */
	RETCODE_NOT_SUPPORTED_FEATURE,      /**< This means that HOST application uses an API option that is not supported in current hardware.  */
	RETCODE_NOT_FOUND_VPU_DEVICE,       /**< This means that HOST application uses the undefined product ID. */   /* 20 */
	RETCODE_CP0_EXCEPTION,              /**< This means that coprocessor exception has occurred. (WAVE only) */
	RETCODE_STREAM_BUF_FULL,            /**< This means that stream buffer is full in encoder. */
	RETCODE_ACCESS_VIOLATION_HW,        /**< This means that GDI access error has occurred. It might come from violation of write protection region or spec-out GDI read/write request. (WAVE only) */
	RETCODE_QUERY_FAILURE,              /**< This means that query command was not successful (WAVE5 only) */
	RETCODE_QUEUEING_FAILURE,           /**< This means that commands cannot be queued (WAVE5 only) */
	RETCODE_VPU_STILL_RUNNING,          /**< This means that VPU cannot be flushed or closed now. because VPU is running (WAVE5 only) */
	RETCODE_REPORT_NOT_READY,           /**< This means that report is not ready for Query(GET_RESULT) command (WAVE5 only) */
	RETCODE_VLC_BUF_FULL,               /**< This means that vlc buffer is full in encoder. (WAVE5 only) */
	RETCODE_FIND_SUPERFRAME,            /**< This means that the current frame consists of super frame. (WAVE5 VP9 only) */
	RETCODE_KEY_FRAME_NOT_FOUND,
	RETCODE_NULL = 0x7FFFFFFF
} RetCode;

/************************************************************************/
/* Utility macros                                                       */
/************************************************************************/
#define WAVE5_ALIGN4(_x)              ((_x+0x03)&~0x03)
#define WAVE5_ALIGN8(_x)              ((_x+0x07)&~0x07)
#define WAVE5_ALIGN16(_x)             ((_x+0x0f)&~0x0f)
#define WAVE5_ALIGN32(_X)             ((_X+0x1f)&~0x1f)
#define WAVE5_ALIGN64(_X)             ((_X+0x3f)&~0x3f)
#define WAVE5_ALIGN128(_x)            (((_x)+0x7f)&~0x7f)
#define WAVE5_ALIGN256(_x)            (((_x)+0xff)&~0xff)
#define WAVE5_ALIGN512(_x)            (((_x)+0x1ff)&~0x1ff)
#define WAVE5_ALIGN4096(_x)           (((_x)+0xfff)&~0xfff)
#define WAVE5_ALIGN16384(_x)          (((_x)+0x3fff)&~0x3fff)

#define WAVE5_ALIGN_MASK(x, mask)     (((x) + (mask)) & ~(mask))
#define WAVE5_ALIGN(x, a)             WAVE5_ALIGN_MASK(x, (typeof(x))(a) - 1)

// VP9: 64, HEVC: 32
#define WAVE5_W_ALIGN(opParam, W) \
	((opParam.bitstreamFormat == STD_VP9) ? ((W + 0x3f) & ~0x3f) : ((W + 0x1f) & ~0x1f))
// VP9: 64, HEVC: 32
#define WAVE5_H_ALIGN(opParam, H) \
	((opParam.bitstreamFormat == STD_VP9) ? ((H + 0x3f) & ~0x3f) : ((H + 0x1f) & ~0x1f))

/************************************************************************/
/*                                                                      */
/************************************************************************/
#define INTERRUPT_TIMEOUT_VALUE     (Uint32)-1

/**
 * \brief   parameters of DEC_SET_SEQ_CHANGE_MASK
 */
#define SEQ_CHANGE_ENABLE_PROFILE   (1<<5)
#define SEQ_CHANGE_ENABLE_SIZE      (1<<16)
#define SEQ_CHANGE_ENABLE_BITDEPTH  (1<<18)
#define SEQ_CHANGE_ENABLE_DPB_COUNT (1<<19)

#define SEQ_CHANGE_INTER_RES_CHANGE (1<<17)         /* VP9 */
#define SEQ_CHANGE_ENABLE_ALL_VP9      (SEQ_CHANGE_ENABLE_PROFILE    | \
										SEQ_CHANGE_ENABLE_SIZE       | \
										SEQ_CHANGE_INTER_RES_CHANGE | \
										SEQ_CHANGE_ENABLE_BITDEPTH   | \
										SEQ_CHANGE_ENABLE_DPB_COUNT)

#define SEQ_CHANGE_ENABLE_ALL_HEVC     (SEQ_CHANGE_ENABLE_PROFILE    | \
										SEQ_CHANGE_ENABLE_SIZE       | \
										SEQ_CHANGE_ENABLE_BITDEPTH   | \
										SEQ_CHANGE_ENABLE_DPB_COUNT)


#define DISPLAY_IDX_FLAG_SEQ_END     -1
#define DISPLAY_IDX_FLAG_NO_OUTPUT   -3
#define DECODED_IDX_FLAG_NO_FB       -1
#define DECODED_IDX_FLAG_SKIP_OR_EOS -2
#define RECON_IDX_FLAG_ENC_END       -1
#define RECON_IDX_FLAG_ENC_DELAY     -2
#define RECON_IDX_FLAG_HEADER_ONLY   -3
#define RECON_IDX_FLAG_CHANGE_PARAM  -4

#ifdef SUPPORT_ROI_50
#define MAX_ROI_NUMBER  50
#else
#define MAX_ROI_NUMBER  10
#endif

/**
* @brief    This is a special enumeration type for some configuration commands
* which can be issued to VPU by HOST application. Most of these commands can be called occasionally,
* not periodically for changing the configuration of decoder or encoder operation running on VPU.
* Details of these commands are presented in <<vpuapi_h_VPU_DecGiveCommand>>.
 */
typedef enum {
	ENABLE_ROTATION,
	DISABLE_ROTATION,
	ENABLE_MIRRORING,
	DISABLE_MIRRORING,
	SET_MIRROR_DIRECTION,
	SET_ROTATION_ANGLE,
	SET_ROTATOR_OUTPUT,
	SET_ROTATOR_STRIDE,
	DEC_GET_SEQ_INFO,
	DEC_SET_SPS_RBSP,
	DEC_SET_PPS_RBSP,
	DEC_SET_SEQ_CHANGE_MASK,
	ENABLE_DERING,
	DISABLE_DERING,
	SET_SEC_AXI,
	SET_DRAM_CONFIG,    //coda960 only
	GET_DRAM_CONFIG,    //coda960 only
	ENABLE_AFBCE,
	DISABLE_AFBCE,
	ENABLE_REP_USERDATA,
	DISABLE_REP_USERDATA,
	SET_ADDR_REP_USERDATA,
	SET_VIRT_ADDR_REP_USERDATA,
	SET_SIZE_REP_USERDATA,
	SET_USERDATA_REPORT_MODE,
	SET_CACHE_CONFIG,
	GET_TILEDMAP_CONFIG,
	SET_LOW_DELAY_CONFIG,
	GET_LOW_DELAY_OUTPUT,
	SET_DECODE_FLUSH,
	DEC_SET_FRAME_DELAY,
	DEC_SET_WTL_FRAME_FORMAT,
	DEC_GET_FIELD_PIC_TYPE,
	DEC_GET_DISPLAY_OUTPUT_INFO,
	DEC_ENABLE_REORDER,
	DEC_DISABLE_REORDER,
	DEC_SET_AVC_ERROR_CONCEAL_MODE,
	DEC_FREE_FRAME_BUFFER,
	DEC_GET_FRAMEBUF_INFO,
	DEC_RESET_FRAMEBUF_INFO,
	ENABLE_DEC_THUMBNAIL_MODE,
	DEC_SET_DISPLAY_FLAG,
	DEC_SET_DISPLAY_FLAG_EX,
	DEC_GET_SCALER_INFO,
	DEC_SET_SCALER_INFO,
	DEC_SET_TARGET_TEMPORAL_ID,             //!<< H.265 temporal scalability
	DEC_SET_BWB_CUR_FRAME_IDX,
	DEC_SET_FBC_CUR_FRAME_IDX,
	DEC_SET_INTER_RES_INFO_ON,
	DEC_SET_INTER_RES_INFO_OFF,
	DEC_FREE_FBC_TABLE_BUFFER,
	DEC_FREE_MV_BUFFER,
	DEC_ALLOC_FBC_Y_TABLE_BUFFER,
	DEC_ALLOC_FBC_C_TABLE_BUFFER,
	DEC_ALLOC_MV_BUFFER,
	DEC_GET_SUPERFRAME_INFO,
	ENABLE_LOGGING,
	DISABLE_LOGGING,
	DEC_GET_QUEUE_STATUS,
	ENC_GET_BW_REPORT,  // wave520 only
	CMD_END,
	CMD_NULL = 0x7FFFFFFF
} CodecCommand;

/**
 * @brief   This is an enumeration type for representing error conceal modes. (H.264/AVC decoder only)
 */
typedef enum {
	AVC_ERROR_CONCEAL_MODE_DEFAULT                                      = 0, /**< basic error concealment and error concealment for missing reference frame, wrong frame_num syntax (default)  */
	AVC_ERROR_CONCEAL_MODE_ENABLE_SELECTIVE_CONCEAL_MISSING_REFERENCE   = 1, /**< error concealment - selective error concealment for missing reference frame */
	AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_MISSING_REFERENCE            = 2, /**< error concealment - disable error concealment for missing reference frame */
	AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_WRONG_FRAME_NUM              = 4, /**< error concealment - disable error concealment for wrong frame_num syntax */
	AVC_ERROR_CONCEAL_MODE_NULL = 0x7FFFFFFF
} AVCErrorConcealMode;

/**
 * @brief   This is an enumeration type for representing the way of writing chroma data in planar format of frame buffer.
 */
typedef enum {
	CBCR_ORDER_NORMAL,  /**< Cb data are written in Cb buffer, and Cr data are written in Cr buffer. */
	CBCR_ORDER_REVERSED,/**< Cr data are written in Cb buffer, and Cb data are written in Cr buffer. */
	CBCR_NULL                   = 0x7FFFFFFF
} CbCrOrder;

/**
 * @brief    This is an enumeration type for representing the mirroring direction.
 */
typedef enum {
	MIRDIR_NONE,
	MIRDIR_VER,
	MIRDIR_HOR,
	MIRDIR_HOR_VER,
	MIRDIR_NULL                 = 0x7FFFFFFF
} MirrorDirection;

/**
 * @brief   This is an enumeration type for representing chroma formats of the frame buffer and pixel formats in packed mode.
 */
typedef enum {
	FORMAT_420                  = 0,  /* 8bit */
	FORMAT_420_ARM              = 0,
	FORMAT_422                     ,  /* 8bit */
	FORMAT_224                     ,  /* 8bit */
	FORMAT_444                     ,  /* 8bit */
	FORMAT_400                     ,  /* 8bit */

										/* Little Endian Perspective     */
										/*     | addr 0  | addr 1  |     */
	FORMAT_420_P10_16BIT_LSB_ARM= 5,
	FORMAT_420_P10_16BIT_MSB    = 5,  /* lsb | 00xxxxx |xxxxxxxx | msb */
	FORMAT_420_P10_16BIT_LSB       ,  /* lsb | xxxxxxx |xxxxxx00 | msb */
	FORMAT_420_P10_32BIT_MSB       ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
	FORMAT_420_P10_32BIT_LSB       ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

										/* 4:2:2 packed format */
										/* Little Endian Perspective     */
										/*     | addr 0  | addr 1  |     */
	FORMAT_422_P10_16BIT_MSB       ,  /* lsb | 00xxxxx |xxxxxxxx | msb */
	FORMAT_422_P10_16BIT_LSB       ,  /* lsb | xxxxxxx |xxxxxx00 | msb */
	FORMAT_422_P10_32BIT_MSB       ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
	FORMAT_422_P10_32BIT_LSB       ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

	FORMAT_YUYV                    ,  /**< 8bit packed format : Y0U0Y1V0 Y2U1Y3V1 ... */
	FORMAT_YUYV_P10_16BIT_MSB      ,  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
	FORMAT_YUYV_P10_16BIT_LSB      ,  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
	FORMAT_YUYV_P10_32BIT_MSB      ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
	FORMAT_YUYV_P10_32BIT_LSB      ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */

	FORMAT_YVYU                    ,  /**< 8bit packed format : Y0V0Y1U0 Y2V1Y3U1 ... */
	FORMAT_YVYU_P10_16BIT_MSB      ,  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
	FORMAT_YVYU_P10_16BIT_LSB      ,  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
	FORMAT_YVYU_P10_32BIT_MSB      ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
	FORMAT_YVYU_P10_32BIT_LSB      ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */

	FORMAT_UYVY                    ,  /**< 8bit packed format : U0Y0V0Y1 U1Y2V1Y3 ... */
	FORMAT_UYVY_P10_16BIT_MSB      ,  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
	FORMAT_UYVY_P10_16BIT_LSB      ,  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
	FORMAT_UYVY_P10_32BIT_MSB      ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
	FORMAT_UYVY_P10_32BIT_LSB      ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */

	FORMAT_VYUY                    ,  /**< 8bit packed format : V0Y0U0Y1 V1Y2U1Y3 ... */
	FORMAT_VYUY_P10_16BIT_MSB      ,  /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
	FORMAT_VYUY_P10_16BIT_LSB      ,  /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
	FORMAT_VYUY_P10_32BIT_MSB      ,  /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */
	FORMAT_VYUY_P10_32BIT_LSB      ,  /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */

	FORMAT_MAX,
	FORMAT_NULL                 = 0x7FFFFFFF
} FrameBufferFormat;

/**
 * @brief   This is an enumeration type for representing output image formats of down scaler.
 */
typedef enum{
	YUV_FORMAT_I420,    /**< This format is a 420 planar format, which is described as forcc I420. */
	YUV_FORMAT_NV12,    /**< This format is a 420 semi-planar format with U and V interleaved, which is described as fourcc NV12. */
	YUV_FORMAT_NV21,    /**< This format is a 420 semi-planar format with V and U interleaved, which is described as fourcc NV21. */
	YUV_FORMAT_I422,    /**< This format is a 422 planar format, which is described as forcc I422. */
	YUV_FORMAT_NV16,    /**< This format is a 422 semi-planar format with U and V interleaved, which is described as fourcc NV16. */
	YUV_FORMAT_NV61,    /**< This format is a 422 semi-planar format with V and U interleaved, which is described as fourcc NV61. */
	YUV_FORMAT_UYVY,    /**< This format is a 422 packed mode with UYVY, which is described as fourcc UYVY. */
	YUV_FORMAT_YUYV,    /**< This format is a 422 packed mode with YUYV, which is described as fourcc YUYV. */
	YUV_FORMAT_NULL	= 0x7FFFFFFF
} ScalerImageFormat;

/**
 * @brief   This is an enumeration type for representing YUV packed format.
 */
typedef enum {
	NOT_PACKED                  = 0,
	PACKED_YUYV,
	PACKED_YVYU,
	PACKED_UYVY,
	PACKED_VYUY,
	PACKED_NULL                 = 0x7FFFFFFF
} PackedFormatNum;


/**
 * @brief    This is an enumeration type for representing interrupt bit positions.

NOTE: This is only for WAVE5 IP.
 */
typedef enum {
	INT_WAVE5_INIT_VPU          = 0,
	INT_WAVE5_WAKEUP_VPU        = 1,
	INT_WAVE5_SLEEP_VPU         = 2,
	INT_WAVE5_CREATE_INSTANCE   = 3,
	INT_WAVE5_FLUSH_INSTANCE    = 4,
	INT_WAVE5_DESTORY_INSTANCE  = 5,
	INT_WAVE5_INIT_SEQ          = 6,
	INT_WAVE5_SET_FRAMEBUF      = 7,
	INT_WAVE5_DEC_PIC           = 8,
	INT_WAVE5_ENC_PIC           = 8,
	INT_WAVE5_DEC_QUERY         = 14,
	INT_WAVE5_BSBUF_EMPTY       = 15,
	INT_WAVE5_BSBUF_FULL        = 15,
	INT_WAVE5_BIT_NULL          = 0x7FFFFFFF
} Wave5InterruptBit;

/**
 * @brief    This is an enumeration type for representing picture types.
 */
typedef enum {
	PIC_TYPE_I                  = 0, /**< I picture */
	PIC_TYPE_P                  = 1, /**< P picture */
	PIC_TYPE_B                  = 2, /**< B picture (except VC1) */
	PIC_TYPE_REPEAT             = 2, /**< Repeat frame (VP9 only) */
	PIC_TYPE_VC1_BI             = 2, /**< VC1 BI picture (VC1 only) */
	PIC_TYPE_VC1_B              = 3, /**< VC1 B picture (VC1 only) */
	PIC_TYPE_D                  = 3, /**< D picture in MPEG2 that is only composed of DC coefficients (MPEG2 only) */
	PIC_TYPE_S                  = 3, /**< S picture in MPEG4 that is an acronym of Sprite and used for GMC (MPEG4 only)*/
	PIC_TYPE_VC1_P_SKIP         = 4, /**< VC1 P skip picture (VC1 only) */
	PIC_TYPE_MP4_P_SKIP_NOT_CODED= 4, /**< Not Coded P Picture in mpeg4 packed mode */
	PIC_TYPE_IDR                = 5, /**< H.264/H.265 IDR picture */
	PIC_TYPE_MAX,                    /**< No Meaning */
	PIC_TYPE_NULL               = 0x7FFFFFFF
}PicType;

/**
 * @brief    This is an enumeration type for specifying frame buffer types when tiled2linear or wtlEnable is used.
 */
typedef enum {
	FF_NONE                     = 0, /**< Frame buffer type when tiled2linear or wtlEnable is disabled */
	FF_FRAME                    = 1, /**< Frame buffer type to store one frame */
	FF_FIELD                    = 2, /**< Frame buffer type to store top field or bottom field separately */
	FF_NULL                     = 0x7FFFFFFF
}FrameFlag;

/**
 * @brief   This is an enumeration type for representing bitstream handling modes in decoder.
 */
typedef enum {
	BS_MODE_INTERRUPT,  /**< VPU returns an interrupt when bitstream buffer is empty while decoding. VPU waits for more bitstream to be filled. */
	BS_MODE_RESERVED,   /**< Reserved for the future */
	BS_MODE_PIC_END,    /**< VPU tries to decode with very small amount of bitstream (not a complete 512-byte chunk). If it is not successful, VPU performs error concealment for the rest of the frame. */
	BS_MODE_NULL                = 0x7FFFFFFF
}BitStreamMode;

/**
 * @brief  This is an enumeration type for representing software reset options.
 */
typedef enum {
	SW_RESET_SAFETY,    /**< It resets VPU in safe way. It waits until pending bus transaction is completed and then perform reset. */
	SW_RESET_FORCE,     /**< It forces to reset VPU without waiting pending bus transaction to be completed. It is used for immediate termination such as system off. */
	SW_RESET_ON_BOOT,   /**< This is the default reset mode that is executed since system booting.  This mode is actually executed in VPU_Init(), so does not have to be used independently. */
	SW_RESET_NULL               = 0x7FFFFFFF
}SWResetMode;

/**
 * @brief  This is an enumeration type for representing product IDs.
 */
typedef enum {
	PRODUCT_ID_980,
	PRODUCT_ID_960              = 1,
	PRODUCT_ID_950              = 1,
	PRODUCT_ID_512,
	PRODUCT_ID_520,
	PRODUCT_ID_NONE,
	PRODUCT_ID_NULL             = 0x7FFFFFFF
}ProductId;

#define PRODUCT_ID_W_SERIES(x)      (x == PRODUCT_ID_512 || x == PRODUCT_ID_520)
#define PRODUCT_ID_NOT_W_SERIES(x)  !PRODUCT_ID_W_SERIES(x)

/**
 * @brief This is an enumeration type for representing map types for frame buffer.

NOTE: Tiled maps are only for CODA9. Please find them in the CODA9 datasheet for detailed information.
 */
typedef enum {
/**
  @verbatim
  Linear frame map type

NOTE: Products earlier than CODA9 can only set this linear map type.
@endverbatim
 */
	LINEAR_FRAME_MAP            = 0,  /**< Linear frame map type */
	TILED_FRAME_V_MAP           = 1,  /**< Tiled frame vertical map type (CODA9 only) */
	TILED_FRAME_H_MAP           = 2,  /**< Tiled frame horizontal map type (CODA9 only) */
	TILED_FIELD_V_MAP           = 3,  /**< Tiled field vertical map type (CODA9 only) */
	TILED_MIXED_V_MAP           = 4,  /**< Tiled mixed vertical map type (CODA9 only) */
	TILED_FRAME_MB_RASTER_MAP   = 5,  /**< Tiled frame MB raster map type (CODA9 only) */
	TILED_FIELD_MB_RASTER_MAP   = 6,  /**< Tiled field MB raster map type (CODA9 only) */
	TILED_FRAME_NO_BANK_MAP     = 7,  /**< Tiled frame no bank map. (CODA9 only) */ // coda980 only
	TILED_FIELD_NO_BANK_MAP     = 8,  /**< Tiled field no bank map. (CODA9 only) */ // coda980 only
	LINEAR_FIELD_MAP            = 9,  /**< Linear field map type. (CODA9 only) */ // coda980 only
	CODA_TILED_MAP_TYPE_MAX     = 10,
	COMPRESSED_FRAME_MAP        = 10, /**< Compressed frame map type (WAVE only) */ // WAVE4 only
	ARM_COMPRESSED_FRAME_MAP    = 11, /**< AFBC(ARM Frame Buffer Compression) compressed frame map type */ // AFBC enabled WAVE decoder
	TILED_MAP_TYPE_MAX,
	TILED_MAP_NULL = 0x7FFFFFFF
} TiledMapType;

typedef enum {
	FB_TYPE_CODEC,  /**< A framebuffer type used for decoding or encoding  */
	FB_TYPE_PPU,    /**< A framebuffer type used for additional allocation of framebuffer for postprocessing(rotation/mirror) or display (tiled2linear) purpose */
	FB_TYPE_NULL = 0x7FFFFFFF
} FramebufferAllocType;

/**
 * @brief    This is data structure of product information. (WAVE only)
 */
typedef struct {
	Uint32 productId;  /**< The product id */
	Uint32 fwVersion;  /**< The firmware version */
	Uint32 productName;  /**< VPU hardware product name  */
	Uint32 productVersion;  /**< VPU hardware product version */
	Uint32 customerId;  /**< The customer id */
	Uint32 stdDef0;  /**< The system configuration information  */
	Uint32 stdDef1;  /**< The hardware configuration information  */
	Uint32 confFeature;  /**< The supported codec standard */
	Uint32 configDate;  /**< The date that the hardware has been configured in YYYYmmdd in digit */
	Uint32 configRevision;  /**< The revision number when the hardware has been configured */
	Uint32 configType;  /**< The define value used in hardware configuration */
	Uint32 configVcore[4];  /**< VCORE Configuration Information */
}ProductInfo;

/**
 * @brief
 @verbatim
 This is a data structure of tiled map information.

NOTE: WAVE does not support tiledmap type so this structure is not used in the product.
@endverbatim
 */
typedef struct {
	// gdi2.0
	int xy2axiLumMap[32];
	int xy2axiChrMap[32];
	int xy2axiConfig;

	// gdi1.0
	int xy2caMap[16];
	int xy2baMap[16];
	int xy2raMap[16];
	int rbc2axiMap[32];
	int xy2rbcConfig;
	unsigned long long tiledBaseAddr;

	// common
	int mapType;
	int productId;
	int tbSeparateMap;
	int topBotSplit;
	int tiledMap;
	int convLinear;
} TiledMapConfig;

/**
 * @brief    This is a data structure of DRAM information. (CODA960 and BODA950 only).
 VPUAPI sets default values for this structure.
 However, HOST application can configure if the default values are not associated with their DRAM
 or desirable to change.
 */
typedef struct {
	int rasBit;     /**< This value is used for width of RAS bit. (13 on the CNM FPGA platform) */
	int casBit;     /**< This value is used for width of CAS bit. (9 on the CNM FPGA platform) */
	int bankBit;    /**< This value is used for width of BANK bit. (2 on the CNM FPGA platform) */
	int busBit;     /**< This value is used for width of system BUS bit. (3 on CNM FPGA platform) */
} DRAMConfig;

/**
 * @brief
 @verbatim
 This is a data structure for representing frame buffer information such as pointer of each YUV
 component, endian, map type, etc.

 All of the 3 component addresses must be aligned to AXI bus width.
 HOST application must allocate external SDRAM spaces for those components by using this data
 structure. For example, YCbCr 4:2:0, one pixel value
 of a component occupies one byte, so the frame data sizes of Cb and Cr buffer are 1/4 of Y buffer size.

 In case of CbCr interleave mode, Cb and Cr frame data are written to memory area started from bufCb address.
 Also, in case that the map type of frame buffer is a field type, the base addresses of frame buffer for bottom fields -
 bufYBot, bufCbBot and bufCrBot should be set separately.
 @endverbatim
 */
typedef struct {
	PhysicalAddress bufY;       /**< It indicates the base address for Y component in the physical address space when linear map is used. It is the RAS base address for Y component when tiled map is used (CODA9). It is also compressed Y buffer or ARM compressed framebuffer (WAVE). */
	PhysicalAddress bufCb;      /**< It indicates the base address for Cb component in the physical address space when linear map is used. It is the RAS base address for Cb component when tiled map is used (CODA9). It is also compressed CbCr buffer (WAVE) */
	PhysicalAddress bufCr;      /**< It indicates the base address for Cr component in the physical address space when linear map is used. It is the RAS base address for Cr component when tiled map is used (CODA9). */
	PhysicalAddress bufYBot;    /**< It indicates the base address for Y bottom field component in the physical address space when linear map is used. It is the RAS base address for Y bottom field component when tiled map is used (CODA980 only). */ // coda980 only
	PhysicalAddress bufCbBot;   /**< It indicates the base address for Cb bottom field component in the physical address space when linear map is used. It is the RAS base address for Cb bottom field component when tiled map is used (CODA980 only). */ // coda980 only
	PhysicalAddress bufCrBot;   /**< It indicates the base address for Cr bottom field component in the physical address space when linear map is used. It is the RAS base address for Cr bottom field component when tiled map is used (CODA980 only). */ // coda980 only
	/**
	  @verbatim
	  It specifies a chroma interleave mode of frame buffer.

	  @* 0 : Cb data are written in Cb frame memory and Cr data are written in Cr frame memory. (chroma separate mode)
	  @* 1 : Cb and Cr data are written in the same chroma memory. (chroma interleave mode)
	  @endverbatim
	 */
	int cbcrInterleave;
	/**
	  @verbatim
	  It specifies the way chroma data is interleaved in the frame buffer, bufCb or bufCbBot.

	  @* 0 : CbCr data is interleaved in chroma memory (NV12).
	  @* 1 : CrCb data is interleaved in chroma memory (NV21).
	  @endverbatim
	 */
	int nv21;
	/**
	  @verbatim
	  It specifies endianess of frame buffer.

	  @* 0 : little endian format
	  @* 1 : big endian format
	  @* 2 : 32 bit little endian format
	  @* 3 : 32 bit big endian format
	  @* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE Datasheet'.
@endverbatim
	 */
	int endian;
	int myIndex;        /**< A frame buffer index to identify each frame buffer that is processed by VPU. */
	int mapType;        /**< A map type for GDI inferface or FBC (Frame Buffer Compression). NOTE: For detailed map types, please refer to <<vpuapi_h_TiledMapType>>. */
	int stride;         /**< A horizontal stride for given frame buffer */
	int width;          /**< A width for given frame buffer */
	int height;         /**< A height for given frame buffer */
	int size;           /**< A size for given frame buffer */
	int lumaBitDepth;   /**< Bit depth for luma component */
	int chromaBitDepth; /**< Bit depth for chroma component  */
	FrameBufferFormat   format;     /**< A YUV format of frame buffer */
	int orgLumaBitDepth;   /**< Bit depth of luma component to encode in AFBC format */
	int orgChromaBitDepth; /**< Bit depth of chroma component to encode in AFBC format */
	int lfEnable;       /**< This variable reports whether a decoded frame has been loopfiltered or not. This information is used to derive a size of AFBC(ARM Frame Buffer Compression) stream. */
	/**
	  @verbatim
	  It enables source frame data with long burst length to be loaded for reducing DMA latency (CODA9 encoder only).

	  @* 0 : disable the long-burst mode.
	  @* 1 : enable the long-burst mode.
	  @endverbatim
	 */
	int sourceLBurstEn;
	int sequenceNo;     /**< A sequence number that the frame belongs to. It increases by 1 every time a sequence changes in decoder.  */

	PhysicalAddress bufFBCY;
	PhysicalAddress bufFBCC;
	PhysicalAddress bufMVCOL;

	BOOL updateFbInfo; /**< If this is TRUE, VPU updates API-internal framebuffer information when any of the information is changed. */
} FrameBuffer;

typedef struct {
	int mapType;                /**< <<vpuapi_h_TiledMapType>> */
	/**
	  @verbatim
	  @* 0 : Cb data are written in Cb frame memory and Cr data are written in Cr frame memory. (chroma separate mode)
	  @* 1 : Cb and Cr data are written in the same chroma memory. (chroma interleave mode)
	  @endverbatim
	 */
	int cbcrInterleave;
	int nv21;                   /**< 1 : CrCb (NV21), 0 : CbCr (NV12).  This is valid when cbcrInterleave is 1. */
	FrameBufferFormat format;   /**< <<vpuapi_h_FrameBufferFormat>>  */
	int stride;                 /**< A stride value of frame buffer  */
	int height;                 /**< A height of frame buffer  */
	int size;                   /**< A size of frame buffer  */
	int lumaBitDepth;           /**< A bit-depth of luma sample */
	int chromaBitDepth;         /**< A bit-depth of chroma sample */
	int endian;                 /**< An endianess of frame buffer  */
	int num;                    /**< The number of frame buffer to allocate  */
	int type;                   /**< <<vpuapi_h_FramebufferAllocType>>  */
} FrameBufferAllocInfo;

/**
 * @brief
 @verbatim
 This is a data structure for representing rectangular window information in a
 frame.

 In order to specify a display window (or display window after cropping), this structure is
 provided to HOST application. Each value means an offset from the start point of
 a frame and therefore, all variables have positive values.
 @endverbatim
 */
typedef struct {
	Uint32 left;    /**< A horizontal pixel offset of top-left corner of rectangle from (0, 0) */
	Uint32 top;     /**< A vertical pixel offset of top-left corner of rectangle from (0, 0) */
	Uint32 right;   /**< A horizontal pixel offset of bottom-right corner of rectangle from (0, 0) */
	Uint32 bottom;  /**< A vertical pixel offset of bottom-right corner of rectangle from (0, 0) */
} VpuRect;


typedef struct {
	BOOL        enScaler;
	Uint32      scaleWidth;
	Uint32      scaleHeight;
} ScalerInfo;

/**
 * @brief    The data structure to enable low delay decoding.
 */
typedef struct {
	/**
	  @verbatim
	  This enables low delay decoding. (CODA980 H.264/AVC decoder only)

	  If this flag is 1, VPU sends an interrupt to HOST application when numRows decoding is done.

	 * 0 : disable
	 * 1 : enable

	 When this field is enabled, reorderEnable, tiled2LinearEnable, and the post-rotator should be disabled.
	 @endverbatim
	 */
	int lowDelayEn;
	/**
	  @verbatim
	  This field indicates the number of mb rows (macroblock unit).

	  The value is from 1 to height/16 - 1.
	  If the value of this field is 0 or picture height/16, low delay decoding is disabled even though lowDelayEn is 1.
	  @endverbatim
	 */
	int numRows;
} LowDelayInfo;

/**
 * @brief    This is a data structure for representing use of secondary AXI for each hardware block.
 */
typedef struct {
	union {
		struct {
			int useBitEnable;   /**<  This enables AXI secondary channel for prediction data of the BIT-processor. */
			int useIpEnable;    /**<  This enables AXI secondary channel for row pixel data of IP.  */
			int useDbkYEnable;  /**<  This enables AXI secondary channel for temporal luminance data of the de-blocking filter. */
			int useDbkCEnable;  /**<  This enables AXI secondary channel for temporal chrominance data of the de-blocking filter.  */
			int useOvlEnable;   /**<   This enables AXI secondary channel for temporal data of the the overlap filter (VC1 only). */
			int useBtpEnable;   /**<  This enables AXI secondary channel for bit-plane data of the BIT-processor (VC1 only).  */
		} coda9;
		struct {
			// for Decoder
			int useSclEnable; /**< This enables AXI secondary channel for Scaler line buffer. */
			int useSclPackedModeEnable; /**< This enables AXI secondary channel for Scaler packed mode line buffer.  */
			int useBitEnable;   /**<  This enables AXI secondary channel for prediction data of the BIT-processor. */
			int useIpEnable;    /**<  This enables AXI secondary channel for row pixel data of IP. */
			int useLfRowEnable; /**<  This enables AXI secondary channel for loopfilter.   */

			// for Encoder
			int useEncImdEnable; /**< This enables AXI secondary channel for intra mode decision. */
			int useEncLfEnable;  /**< This enables AXI secondary channel for loopfilter. */
			int useEncRdoEnable; /**< This enables AXI secondary channel for RDO. */
		} wave;
	} u;
} SecAxiUse;

// For MaverickCache1
/**
 * @brief    This is a data structure for representing cache rectangle area for each component of MC reference frame. (CODA9 only)
 */
typedef struct {
	unsigned BufferSize     : 8; /**< This is the cache buffer size for each component and can be set with 0 to 255. The unit of this value is fixed with 256byte. */
	unsigned PageSizeX      : 4; /**< This is the cache page size and can be set as 0 to 4. With this value(n), 8*(2^n) byte is requested as the width of a page. */
	unsigned PageSizeY      : 4; /**< This is the cache page size and can be set as 0 to 7. With this value(m), a page width*(2^m) byte is requested as the rectangle of a page.*/
	unsigned CacheSizeX     : 4; /**< This is the component data cache size, and it can be set as 0 to 7 in a page unit. Then there can be 2^n pages in x(y)-direction. Make sure that for luma component the CacheSizeX + CacheSizeY must be less than 8. For chroma components, CacheSizeX + CacheSizeY must be less than 7. */
	unsigned CacheSizeY     : 4; /**< This is the component data cache size, and it can be set as 0 to 7 in a page unit. Then there can be 2^n pages in x(y)-direction. Make sure that for luma component the CacheSizeX + CacheSizeY must be less than 8. For chroma components, CacheSizeX + CacheSizeY must be less than 7. */
	unsigned Reserved       : 8;
} CacheSizeCfg;

/**
 * @brief    This is a data structure for cache configuration. (CODA9 only)
 */
typedef struct {
	struct {
		union {
			Uint32 word;
			CacheSizeCfg cfg; /**< <<vpuapi_h_CacheSizeCfg>> */
		} luma;
		union {
			Uint32 word;
			CacheSizeCfg cfg; /**< <<vpuapi_h_CacheSizeCfg>> */
		} chroma;
		/**
		  @verbatim
		  It disables cache function.

		  @* 1 : cache off
		  @* 0 : cache on
		  @endverbatim
		 */
		unsigned Bypass         : 1;
		/**
		  @verbatim
		  It enables two frame caching mode.

		  @* 1 : dual mode (caching for FrameIndex0 and FrameIndex1)
		  @* 0 : single mode (caching for FrameIndex0)
		  @endverbatim
		 */
		unsigned DualConf       : 1;
		/**
		  @verbatim
		  Mode for page merging

		  @* 0 : disable
		  @* 1 : horizontal
		  @* 2 : vertical

		  We recommend you to set 1 (horizontal) in tiled map or to set 2 (vertical) in linear map.
		  @endverbatim
		 */
		unsigned PageMerge      : 2;
	} type1;
	struct {
		/**
		  @verbatim
		  CacheMode represents cache configuration.

		  @* [10:9] : cache line processing direction and merge mode
		  @* [8:5] : CacheWayShape
		  @** [8:7] : CacheWayLuma
		  @** [6:5] : CacheWayChroma
		  @* [4] reserved
		  @* [3] CacheBurstMode
		  @** 0: burst 4
		  @** 1: bust 8
		  @* [2] CacheMapType
		  @** 0: linear
		  @** 1: tiled
		  @* [1] CacheBypassME
		  @** 0: cache enable
		  @** 1: cache disable (bypass)
		  @* [0] CacheBypassMC
		  @** 0: cache enable
		  @** 1: cache disable (bypass)
		  @endverbatim
		 */
		unsigned int CacheMode;
	} type2;
} MaverickCacheConfig;

/**
 * @brief    This structure is used when HOST application additionally wants to send SPS data
 or PPS data from external way. The resulting SPS data or PPS data can be used in
 real applications as a kind of out-of-band information.
 */
typedef struct {
	Uint32 * paraSet;   /**< The SPS/PPS rbsp data */
	int     size;       /**< The size of stream in byte */
} DecParamSet;

// The subblock order lookup table entry
typedef struct afbc_block4x4_order_struct {
	char plane;
	char x;
	char y;
} afbc_block4x4_order;

// AFBC frame level info
// This struct is essentially an expanded version of the AFBC file header
// It controls the operation of the encoder/decoder with regards to
// format details and as well as geometrical information such as width
// and height.
typedef struct AfbcFrameInfo_struct{
	unsigned long long   frame_size;        // total frame size (to check subblocks offset ranges)
	int subsampling;                        // subsampling modes AFBC_SUBSAMPLING_*
	int yuv_transform;                      // apply integer yuv transform before compressing
	int nsubblocks;                         // number of subblocks
	int nplanes;                            // number of planes, 1 or 2. 2 is used for YUV formats
	int ncomponents[2];                     // number of components per plane
	int first_component[2];                 // the index of the first component for the given plane
	int uncompressed_size[2];               // number of byte for an uncompressed block for a given plane
	int body_base_ptr_bits;                 // number of bits for body_base_ptr field
	int subblock_size_bits;                 // number of bits for subblock_size field
	int sbs_multiplier[2];                  // The value to multipie the subblock size with in the header
	int inputbits[4];                       // uncompressed bit depth per component
	int compbits[4];                        // compressed bit depth per component
	int defaultcolor[4];                    // default color per component
	afbc_block4x4_order *subblock_order;	// the subblock order lookup table
	int version;                            // AFBC version
	int width;                              // width in pixels
	int height;                             // height in pixels
	int mbw;                                // width in block16 units
	int mbh;                                // height in block16 units
	int left_crop;                          // in pixels, <16
	int top_crop;                           // in pixels, <16
	int crop_ignore;                        // flag to ignore the crop and decode whole image

	int actual_bytes;                       // Keep track of how many bits we have actually read

	int file_message;                       // Used to indicate non standard properties of this file
	int maximum_allowed_bctree;             // The maximum bctree we allow to decode. 0 means this is limited
	// by  the c implemntation. This can be used to mimic rtl behaviour
	int block_split;                        // the block split mode is enabled for this buffer
	int format;								// afbc format (8bit or 10bit)
} AfbcFrameInfo;


struct CodecInst;

//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------
typedef struct CodecInstHeader {
	Int32   inUse;
	Int32   coreIdx;
	Int32   productId;
	Int32   instIndex;
	Int32   codecMode;
	Int32   codecModeAux;
	Int32   loggingEnable;
	Uint32  isDecoder;
} CodecInstHeader;

#define VPU_HANDLE_INSTANCE_NO(_handle)         (_handle->instIndex)
#define VPU_HANDLE_CORE_INDEX(_handle)          (((CodecInst*)_handle)->coreIdx)
#define VPU_HANDLE_PRODUCT_ID(_handle)          (((CodecInst*)_handle)->productId)
#define VPU_CONVERT_WTL_INDEX(_handle, _index)  ((((CodecInst*)_handle)->CodecInfo->decInfo).numFbsForDecoding+_index)
//#define VPU_HANDLE_TO_DECINFO(_handle)          (&(((CodecInst*)_handle)->CodecInfo->decInfo))
#define VPU_HANDLE_TO_DECINFO(_handle)          (&(((CodecInst*)_handle)->CodecInfo.decInfo))
/**
 * @brief
 @verbatim
 This is a dedicated type for handle returned when a decoder instance or a encoder instance is
 opened.

 @endverbatim
 */
typedef struct CodecInst* VpuHandle;

/**
 * @brief
 @verbatim
 This is a dedicated type for decoder handle returned when a decoder instance is
 opened. A decoder instance can be referred to by the corresponding handle. CodecInst
 is a type managed internally by API. Application does not need to care about it.

NOTE: This type is vaild for decoder only.
@endverbatim
 */
typedef struct CodecInst* DecHandle;


#define FBC_MODE_BEST_PREDICTION    0x00        //!<< Best for bandwidth, some performance overhead
#define FBC_MODE_NORMAL_PREDICTION  0x0c        //!<< Good for badnwidth and performance
#define FBC_MODE_BASIC_PREDICTION   0x3c        //!<< Best for performance

/**
 * @brief    This data structure is a group of common decoder parameters to run VPU with a new decoding instance.
 This is used when HOST application calls VPU_Decopen().
 */
typedef struct {
	//	CodStd          bitstreamFormat;    /**< A standard type of bitstream in decoder operation. It is one of codec standards defined in CodStd. */
	Uint32          bitstreamFormat;    /**< A standard type of bitstream in decoder operation. It is one of codec standards defined in CodStd. */
	PhysicalAddress bitstreamBuffer;    /**< The start address of bitstream buffer from which the decoder can get the next bitstream. This address must be aligned to AXI bus width. */
	int             bitstreamBufferSize;/**< The size of the buffer pointed by bitstreamBuffer in byte. This value must be a multiple of 1024.  */

	int             tiled2LinearEnable; /**< It enables a tiled to linear map conversion feature for display. */
	/**
	  @verbatim
	  It specifies which picture type is converted to. (CODA980 only)

	  @* 1 : conversion to linear frame map (when FrameFlag enum is FF_FRAME)
	  @* 2 : conversion to linear field map (when FrameFlag enum is FF_FIELD)

	  @endverbatim
	 */
	int             tiled2LinearMode;
	/**
	  @verbatim
	  It enables WTL (Write Linear) function. If this field is enabled,
	  VPU writes a decoded frame to the frame buffer twice - first in linear map and second in tiled or compressed map.
	  Therefore, HOST application should allocate one more frame buffer for saving both formats of frame buffers.

	  @endverbatim
	 */
	int             wtlEnable;
	/**
	  @verbatim
	  It specifies whether VPU writes in frame linear map or in field linear map when WTL is enabled. (CODA980 only)

	  @* 1 : write decoded frames in frame linear map (when FrameFlag enum is FF_FRAME)
	  @* 2 : write decoded frames in field linear map (when FrameFlag enum is FF_FIELD)

	  @endverbatim
	 */
	int             wtlMode;
	/**
	  @verbatim
	  @* 0 : Cb data are written in Cb frame memory and Cr data are written in Cr frame memory. (chroma separate mode)
	  @* 1 : Cb and Cr data are written in the same chroma memory. (chroma interleave mode)
	  @endverbatim
	 */
	int             cbcrInterleave;
	/**
	  @verbatim
	  CrCb interleaved mode (NV21).

	  @* 0 : decoded chroma data is written in CbCr (NV12) format.
	  @* 1 : decoded chroma data is written in CrCb (NV21) format.

	  This is only valid if cbcrInterleave is 1.
	  @endverbatim
	 */
	int             nv21;
	/**
	  @verbatim
	  CbCr order in planar mode (YV12 format)

	  @* 0 : Cb data are written first and then Cr written in their separate plane.
	  @* 1 : Cr data are written first and then Cb written in their separate plane.
	  @endverbatim
	 */
	int             cbcrOrder;
	/**
	  @verbatim
	  It writes output with 8 burst in linear map mode. (CODA9 only)

	  @* 0 : burst write back is disabled
	  @* 1 : burst write back is enabled.

	  @endverbatim
	 */
	int             bwbEnable;
	/**
	  @verbatim
	  Frame buffer endianness

	  @* 0 : little endian format
	  @* 1 : big endian format
	  @* 2 : 32 bit little endian format
	  @* 3 : 32 bit big endian format
	  @* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE Datasheet'.
@endverbatim
	 */
	EndianMode      frameEndian;
	/**
	  @verbatim
	  Bitstream buffer endianess

	  @* 0 : little endian format
	  @* 1 : big endian format
	  @* 2 : 32 bits little endian format
	  @* 3 : 32 bits big endian format
	  @* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE Datasheet'.
@endverbatim
	 */
	EndianMode      streamEndian;
	/**
	  @verbatim
	  When read pointer reaches write pointer in the middle of decoding one picture,

	  @* 0 : VPU sends an interrupt to HOST application and waits for more bitstream to decode. (interrupt mode)
	  @* 1 : reserved
	  @* 2 : VPU decodes bitstream from read pointer to write pointer. (PicEnd mode)
	  @endverbatim
	 */
	int             bitstreamMode;
	Uint32          coreIdx;    /**< VPU core index number (0 ~ [number of VPU core] - 1) */
	/**
	  BIT processor work buffer SDRAM address/size information. In parallel decoding operation, work buffer is shared between VPU cores.
	  The work buffer address is set to this member variable when VPU_Decopen() is called.
	  Unless HOST application sets the address and size of work buffer, VPU allocates automatically work buffer
	  when VPU_DecOpen() is executed.
	 */
	vpu_buffer_t    vbWork;
	/**
	  @verbatim
	  It determines prediction mode of frame buffer compression.

	  @* 0x00 : best Predection (best for bandwidth, but some performance overhead might exist)
	  @* 0x0C : normal Prediction (good for bandwidth and performance)
	  @* 0x3C : basic Predcition (best for performance)
	  @endverbatim
	 */
	int             fbc_mode;
	Uint32          virtAxiID; /**< AXI_ID to distinguish guest OS. For virtualization only. Set this value in highest bit order.*/
	BOOL            bwOptimization; /**< Bandwidth optimization feature which allows WTL(Write to Linear)-enabled VPU to skip writing compressed format of non-reference pictures or linear format of non-display pictures to the frame buffer for BW saving reason. */
	int             afbceEnable;  /**< This field enables VPU to generate AFBC(Arm Frame Buffer Compression) format of decoded frames. */
	Int32           afbceFormat;  /**< This field specifies FrameBufferFormat to be encoded in AFBC format.  */
	vpu_buffer_t    vbTemp;	// TCC
	int				TenBitDisable;	// 10 to 8 enable
	int				FullSizeFB;
	int				maxwidth;
	int				maxheight;
	int				maxbit;
	int				CQ_Depth;	// command queue depth;
	int				bufferRegMode;
	int				buffer2Enable;
	int				seq_init_bsbuff_change;
} DecOpenParam;

#if TCC_VP9____DEC_SUPPORT == 1
typedef struct Vp9PicInfo {
	/*
	 * 0	UNKNOWN
	 * 1	BT.601 (SDTV)
	 * 2	BT.709 (HDTV)
	 * 3	SMPTE 170 (
	 * 4	SMPTE 240
	 * 5	BT.2020 (UHDTV, HDR)
	 * 6	Reserved
	 * 7	sRGB
	 */
	unsigned int color_space;
	/*
	 * this syntax is meaningful if color_spage is one of 0, 1, 2, 3, 4.
	 * 0	Limited Range (TV Range, MPEG Range)
	 * 1	Full Range (PC Range, JPEG Range)
	 */
	unsigned int color_range;
} Vp9PicInfo;
#endif

/**
 * @brief Data structure to get information necessary to start decoding from the decoder.
 */

typedef struct {
	/**
	  @verbatim
	  Horizontal picture size in pixel

	  This width value is used while allocating decoder frame buffers. In some
	  cases, this returned value, the display picture width declared on stream header,
	  should be aligned to a specific value depending on product and video standard before allocating frame buffers.
	  @endverbatim
	 */
	Int32           picWidth;
	/**
	  @verbatim
	  Vertical picture size in pixel

	  This height value is used while allocating decoder frame buffers.
	  In some cases, this returned value, the display picture height declared on stream header,
	  should be aligned to a specific value depending on product and video standard before allocating frame buffers.
	  @endverbatim
	 */
	Int32           picHeight;

	/**
	  @verbatim
	  The numerator part of frame rate fraction

NOTE: The meaning of this flag can vary by codec standards.
For details about this,
please refer to 'Appendix: FRAME RATE NUMERATORS in programmer\'s guide'.
@endverbatim
	 */
	Int32           fRateNumerator;
	/**
	  @verbatim
	  The denominator part of frame rate fraction

NOTE: The meaning of this flag can vary by codec standards.
For details about this,
please refer to 'Appendix: FRAME RATE DENOMINATORS in programmer\'s guide'.
@endverbatim
	 */
	Int32           fRateDenominator;
	/**
	  @verbatim
	  Picture cropping rectangle information (H.264/H.265/AVS decoder only)

	  This structure specifies the cropping rectangle information.
	  The size and position of cropping window in full frame buffer is presented
	  by using this structure.
	  @endverbatim
	 */
	VpuRect         picCropRect;

	/**< This is the minimum number of frame buffers required for decoding. Applications must allocate at least as many as this number of frame buffers and register the number of buffers to VPU using VPU_DecRegisterFrameBuffer() before decoding pictures. */
	// TELPIC-99
	// 1) minimum frame buffer count
	//    1-1) HEVC
	//         I would like to inquire whether there may be cases where the minimum frame buffer count exceeds 7 (whether the maximum is 7).
	//         => It may vary depending on the value of max_dec_pic_buffering and may exceed 7. However, 7 is typical in the spec.
	//         => 4K,level5.1 => : 7 = max_dec_pic_buffering(6) +1(current)
	//         => 2K(1920x1080) 256fps => 16
	//    1-2) VP9 ? (TELPIC-118)
	// 2) What is the minimum value that "minimum frame buffer count" can be output?
	//     2-1) HEVC : If it is an ALL Intra structure, it will be 1, and if there is no reorder in the IP stream, it will be 2.
	//     2-2) VP9 : Fixed to 10 images regardless of resolution (the number is not defined in the spec like hevc)
	Int32           minFrameBufferCount;

	Int32           frameBufDelay;          /**< This is the maximum display frame buffer delay for buffering decoded picture reorder. VPU may delay decoded picture display for display reordering when H.264/H.265, pic_order_cnt_type 0 or 1 case and for B-frame handling in VC1 decoder. */
	Int32           normalSliceSize;        /**< This is the recommended size of buffer used to save slice in normal case. This value is determined by quarter of the memory size for one raw YUV image in KB unit. This is only for H.264. */
	Int32           worstSliceSize;         /**< This is the recommended size of buffer used to save slice in worst case. This value is determined by half of the memory size for one raw YUV image in KB unit. This is only for H.264. */

	// Report Information
	Int32           maxSubLayers;           /**< Number of sub-layer for H.265/HEVC */
	/**
	  @verbatim
	  @* H.265/H.264 : profile_idc
	  @endverbatim
	 */
	Int32           profile;
	/**
	  @verbatim
	  @* H.265/H.264 : level_idc
	  @endverbatim
	 */
	Int32           level;
	/**
	  @verbatim
	  A tier indicator

	  @* 0 : Main
	  @* 1 : High
	  @endverbatim
	 */
	Int32           tier;
	Int32           interlace;              /**< When this value is 1, decoded stream may be decoded into progressive or interlace frame. Otherwise, decoded stream is progressive frame. */
	Int32           constraint_set_flag[4]; /**< constraint_set0_flag ~ constraint_set3_flag in H.264/AVC SPS */
	Int32           isExtSAR;
	/**
	  @verbatim
	  This is one of the SPS syntax elements in H.264.

	  @* 0 : max_num_ref_frames is 0.
	  @* 1 : max_num_ref_frames is not 0.
	  @endverbatim
	 */
	Int32           maxNumRefFrmFlag;
	Int32           maxNumRefFrm;
	/**
	  @verbatim
	  @* H.264/AVC : When avcIsExtSAR is 0, this indicates aspect_ratio_idc[7:0]. When avcIsExtSAR is 1, this indicates sar_width[31:16] and sar_height[15:0].
	  If aspect_ratio_info_present_flag = 0, the register returns -1 (0xffffffff).
	  @endverbatim
	 */
	Int32           aspectRateInfo;
	Int32           bitRate;        /**< The bitrate value written in bitstream syntax. If there is no bitRate, this reports -1. */
	Uint32          userDataHeader; /**< Refer to userDataHeader in <<vpuapi_h_DecOutputExtData>>. */
	Int32           userDataNum;    /**< Refer to userDataNum in <<vpuapi_h_DecOutputExtData>>. */
	Int32           userDataSize;   /**< Refer to userDataSize in <<vpuapi_h_DecOutputExtData>>. */
	Int32           userDataBufFull;/**< Refer to userDataBufFull in <<vpuapi_h_DecOutputExtData>>. */
	//VUI information
	Int32           chromaFormatIDC;/**< A chroma format indicator */
	Int32           lumaBitdepth;   /**< A bit-depth of luma sample */
	Int32           chromaBitdepth; /**< A bit-depth of chroma sample */
	/**
	  @verbatim
	  This is an error reason of sequence header decoding.
	  For detailed meaning of returned value,
	  please refer to the 'Appendix: ERROR DEFINITION in programmer\'s guide'.
	  @endverbatim
	 */
	Int32           seqInitErrReason;
	Int32           warnInfo;
	Int32           framebufferformat;
	PhysicalAddress rdPtr;         /**< A read pointer of bitstream buffer */
	PhysicalAddress wrPtr;         /**< A write pointer of bitstream buffer */
	Uint32          sequenceNo;    /**< This is the number of sequence information. This variable is increased by 1 when VPU detects change of sequence. */

#if TCC_VP9____DEC_SUPPORT == 1
	Vp9PicInfo      vp9PicInfo;    /**< VP9 color information, including color space and color range. */
#endif
} DecInitialInfo;


// Report Information

/**
 * @brief    The data structure for options of picture decoding.
 */
#define WAVE_SKIPMODE_WAVE_NONE 0
#define WAVE_SKIPMODE_NON_IRAP  1
#define WAVE_SKIPMODE_NON_REF   2
typedef struct {
	/**
	  @verbatim
	  @* 0 : disable
	  @* 1 : enable
	  @* 2 : I frame search enable (H.264/AVC only)

	  If this option is enabled, then decoder performs skipping frame decoding until
	  decoder meets an I (IDR) frame. If there is no I frame in given stream, decoder
	  waits for I (IDR) frame.
	  Especially in H.264/AVC stream decoding, they might have I-frame and IDR frame.
	  Therefore HOST application should set iframeSearchEnable value according to frame type.
	  If HOST application wants to search IDR frame, this flag should be set to 1 like other standards.
	  Otherwise if HOST application wants to search I frame, this flag should be set to 2.

	  Note that when decoder meets EOS (End Of Sequence) code during I-Search, decoder
	  returns -1 (0xFFFF). And if this option is enabled,
	  skipframeMode options are ignored.

NOTE: CODA9 only supports it.
@endverbatim
	 */
	Int32 iframeSearchEnable;
	/**
	  @verbatim
	  Skip frame function enable and operation mode

	  In case of CODA9,

	  @* 0 : skip frame disable
	  @* 1 : skip frame enabled (skip frames but I (IDR) frame)
	  @* 2 : skip frame enabled (skip any frames),

	  If this option is enabled, decoder skips decoding as many as skipframeNum.

	  If skipframeMode is 1, decoder skips frames but I (IDR) frames and decodes I (IDR)
	  frames. If skipframeMode is 2, decoder skips any frames. After decoder skips
	  frames, decoder returns the decoded index -2 (0xFFFE) when decoder does not have any
	  frames displayed.

	  In case of WAVE4,

	  @* 0x0 : skip frame off
	  @* 0x1 : skip non-RAP pictures. That is, all pictures that are not IDR, CRA, or BLA are skipped.
	  @* 0x2 : skip non-reference pictures.
	  @* 0x3 : reserved
	  @* 0x4~0xf : reserved

NOTE: When decoder meets EOS (End Of Sequence) code during frame skip,
decoder returns -1(0xFFFF).
@endverbatim
	 */
	Int32 skipframeMode;
#ifdef SUPPORT_NON_IRAP_DELAY_ENABLE
	Int32 skipframeDelayEnable;	// [TELPIC-112] Immediate or delayed output option for decIdx when operating in I-frame search mode
	//  decParam.skipframeDelayEnable = 1 : Display index is output after delay, m_iSkipFrameMode & (1<<4)
	//  decParam.skipframeDelayEnable = 0 : The display index is output immediately without delay.
#endif
	union {
		/**
		  @verbatim
		  Forces to flush a display index of the frame buffer that
		  delayed without decoding of the current picture.

		  @* 0 : disable
		  @* 1 : enable flushing
		  @endverbatim
		 */
		Int32 mp2PicFlush;
		/**
		  @verbatim
		  FSets a de-blocking filter mode for streams.

		  @* 0 : enable de-blocking filter for all pictures.
		  @* 1 : disable de-blocking filter for all pictures.
		  @* 2 : disable de-blocking filter for P and B pictures.
		  @* 3 : disable de-blocking filter only for B pictures.
		  @endverbatim
		 */
		Int32 vDbkMode;
	} DecStdParam;

	BOOL    craAsBlaFlag;   /**< It handles CRA picture as BLA picture not to use reference from the previous decoded pictures (H.265/HEVC only) */
	BOOL    vp9SuperFrameParseEnable;
} DecParam;

// Report Information
/**
 * @brief    The data structure to get result information from decoding a frame.
 */

typedef struct {
	/**
	  @verbatim
	  This variable indicates which userdata is reported by VPU. (WAVE only)
	  When this variable is not zero, each bit corresponds to the `H265_USERDATA_FLAG_XXX`.

	// H265 USER_DATA(SPS & SEI) ENABLE FLAG
#define H265_USERDATA_FLAG_RESERVED_0           (0)
#define H265_USERDATA_FLAG_RESERVED_1           (1)
#define H265_USERDATA_FLAG_VUI                  (2)
#define H265_USERDATA_FLAG_ALTERNATIVE_TRANSFER_CHARACTERISTICS (3)
#define H265_USERDATA_FLAG_PIC_TIMING           (4)
#define H265_USERDATA_FLAG_ITU_T_T35_PRE        (5)
#define H265_USERDATA_FLAG_UNREGISTERED_PRE     (6)
#define H265_USERDATA_FLAG_ITU_T_T35_SUF        (7)
#define H265_USERDATA_FLAG_UNREGISTERED_SUF     (8)
#define H265_USERDATA_FLAG_RESERVED_9           (9)
#define H265_USERDATA_FLAG_MASTERING_COLOR_VOL  (10)
#define H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT  (11)
#define H265_USERDATA_FLAG_KNEE_FUNCTION_INFO   (12)

Userdata are written from the memory address specified to SET_ADDR_REP_USERDATA,
and userdata consists of two parts, header (offset and size) and userdata as shown below.

-------------------------------------
| offset_00(32bit) | size_00(32bit) |
| offset_01(32bit) | size_01(32bit) |
|                  ...              | header
|                  ...              |
| offset_31(32bit) | size_31(32bit) |
-------------------------------------
|                                   | data
|                                   |

@endverbatim
	 */
	Uint32 userDataHeader;
	Uint32 userDataNum;        /**< This is the number of user data. */
	Uint32 userDataSize;       /**< This is the size of user data. */
	Uint32 userDataBufFull;    /**< When userDataEnable is enabled, decoder reports frame buffer status into the userDataBufAddr and userDataSize in byte size. When user data report mode is 1 and the user data size is bigger than the user data buffer size, VPU reports user data as much as buffer size, skips the remainings and sets userDataBufFull. */
} DecOutputExtData;

// HEVC specific Recovery Point information
/**
 * @brief    This is a data structure for H.265/HEVC specific picture information. (H.265/HEVC decoder only)
 VPU returns this structure after decoding a frame.
 */
typedef struct {
	/**
	  @verbatim
	  This is a flag to indicate whether H.265/HEVC Recovery Point SEI exists or not.

	  @* 0 : H.265/HEVC RP SEI does not exist.
	  @* 1 : H.265/HEVC RP SEI exists.
	  @endverbatim
	 */
	unsigned exist;
	int recoveryPocCnt;         /**< recovery_poc_cnt */
	int exactMatchFlag;         /**< exact_match_flag */
	int brokenLinkFlag;         /**< broken_link_flag */

} H265RpSei;

/**
 * @brief    This is a data structure that H.265/HEVC decoder returns for reporting POC (Picture Order Count).
 */
typedef struct {
	int decodedPOC; /**< A POC value of picture that has currently been decoded and with decoded index. When indexFrameDecoded is -1, it returns -1. */
	int displayPOC; /**< A POC value of picture with display index. When indexFrameDisplay is -1, it returns -1. */
	int temporalId; /**< A temporal ID of the picture */
} H265Info;

/**
 * @brief    The data structure to get result information from decoding a frame.
 */
typedef struct {
	/**
	  @verbatim
	  This is a frame buffer index for the picture to be displayed at the moment among
	  frame buffers which are registered using VPU_DecRegisterFrameBuffer(). Frame
	  data to be displayed are stored into the frame buffer with this index.
	  When there is no display delay, this index is always
	  the same with indexFrameDecoded. However, if display delay does exist for display reordering in AVC
	  or B-frames in VC1), this index might be different with indexFrameDecoded.
	  By checking this index, HOST application can easily know whether sequence decoding has been finished or not.

	  @* -3(0xFFFD) or -2(0xFFFE) : it is when a display output cannot be given due to picture reordering or skip option.
	  @* -1(0xFFFF) : it is when there is no more output for display at the end of sequence decoding.

	  @endverbatim
	 */
	int indexFrameDisplay;
	int indexFrameDisplayForTiled;  /**< In case of WTL mode, this index indicates a display index of tiled or compressed framebuffer. */
	/**
	  @verbatim
	  This is a frame buffer index of decoded picture among frame buffers which were
	  registered using VPU_DecRegisterFrameBuffer(). The currently decoded frame is stored into the frame buffer specified by
	  this index.

	 * -2 : it indicates that no decoded output is generated because decoder meets EOS (End Of Sequence) or skip.
	 * -1 : it indicates that decoder fails to decode a picture because there is no available frame buffer.
	 @endverbatim
	 */
	int indexFrameDecoded;
	int indexInterFrameDecoded;     /**< In case of VP9 codec, this indicates an index of the frame buffer to reallocate for the next frame's decoding. VPU returns this information when detecting change of the inter-frame resolution. */
	int indexFrameDecodedForTiled;  /**< In case of WTL mode, this indicates a decoded index of tiled or compressed framebuffer. */
	int nalType;                    /**< This is nal Type of decoded picture. Please refer to nal_unit_type in Table 7-1 - NAL unit type codes and NAL unit type classes in H.265/HEVC specification. (WAVE only) */
	int picType;                    /**< This is the picture type of decoded picture. It reports the picture type of bottom field for interlaced stream. <<vpuapi_h_PicType>>. */
	int picTypeFirst;               /**< This is only valid in interlaced mode and indicates the picture type of the top field. */
	int numOfErrMBs;                /**< This is the number of error coded unit in a decoded picture. */
	int numOfTotMBs;                /**< This is the number of coded unit in a decoded picture. */
	int numOfErrMBsInDisplay;       /**< This is the number of error coded unit in a picture mapped to indexFrameDisplay. */
	int numOfTotMBsInDisplay;       /**< This is the number of coded unit in a picture mapped to indexFrameDisplay. */
	BOOL refMissingFrameFlag;       /**< This indicates that the current frame's references are missing in decoding. (WAVE only)  */
	int notSufficientSliceBuffer;   /**< This is a flag which represents whether slice save buffer is not sufficient to decode the current picture. VPU might not get the last part of the current picture stream due to buffer overflow, which leads to macroblock errors. HOST application can continue decoding the remaining pictures of the current bitstream without closing the current instance, even though several pictures could be error-corrupted. (H.264/AVC BP only) */
	int notSufficientPsBuffer;      /**< This is a flag which represents whether PS (SPS/PPS) save buffer is not sufficient to decode the current picture. VPU might not get the last part of the current picture stream due to buffer overflow. HOST application must close the current instance, since the following picture streams cannot be decoded properly for loss of SPS/PPS data. (H.264/AVC only) */
	/**
	  @verbatim
	  This variable indicates whether decoding process was finished completely or not. If stream
	  has error in the picture header syntax or has the first slice header syntax of H.264/AVC
	  stream, VPU returns 0 without proceeding MB decode routine.

	  @* 0 : it indicates incomplete finish of decoding process.
	  @* 1 : it indicates complete finish of decoding process.

	  @endverbatim
	 */
	int decodingSuccess;
	/**
	  @verbatim
	  @* 0 : progressive frame which consists of one picture
	  @* 1 : interlaced frame which consists of two fields
	  @endverbatim
	 */
	int interlacedFrame;
	/**
	  @verbatim
	  This is a flag which represents whether chunk in bitstream buffer should be reused or not, even after VPU_DecStartOneFrame() is executed.
	  This flag is meaningful when bitstream buffer operates in PicEnd mode. In that mode, VPU consumes all the bitstream in bitstream buffer for the current VPU_DecStartOneFrame()
	  in assumption that one chunk is one frame.
	  However, there might be a few cases that chunk needs to be reused such as the following:

	 * One chunk can contain P frame and B frame to reduce display delay.
	 In that case after decoding P frame, this flag is set to 1. HOST application should try decoding with the rest of chunk data to get B frame.
	 * H.264/AVC NPF stream : After the first field has been decoded, this flag is set to 1. HOST application should check if the next field is NPF or not.
	 * No DPB available: VPU is not able to consume chunk with no frame buffers available at the moment. Thus, the whole chunk should be provided again.
	 @endverbatim
	 */
	int chunkReuseRequired;
	VpuRect rcDisplay;              /**< This field reports the rectangular region in pixel unit after decoding one frame - the region of `indexFrameDisplay` frame buffer. */
	int dispPicWidth;               /**< This field reports the width of a picture to be displayed in pixel unit after decoding one frame - width of `indexFrameDisplay` frame bufffer.  */
	int dispPicHeight;              /**< This field reports the height of a picture to be displayed in pixel unit after decoding one frame - height of `indexFrameDisplay` frame bufffer.  */
	VpuRect rcDecoded;              /**< This field reports the rectangular region in pixel unit after decoding one frame - the region of `indexFrameDecoded` frame buffer. */
	int decPicWidth;                /**< This field reports the width of a decoded picture in pixel unit after decoding one frame - width of `indexFrameDecoded` frame bufffer. */
	int decPicHeight;               /**< This field reports the height of a decoded picture in pixel unit after decoding one frame - height of `indexFrameDecoded` frame bufffer.  */
	int aspectRateInfo;             /**< This is aspect ratio information for each standard. Refer to aspectRateInfo of <<vpuapi_h_DecInitialInfo>>. */
	int fRateNumerator;             /**< The numerator part of frame rate fraction. Note that the meaning of this flag can vary by codec standards. For details about this, please refer to 'Appendix: FRAME RATE NUMERATORS in programmer\'s guide'.  */
	int fRateDenominator;           /**< The denominator part of frame rate fraction. Note that the meaning of this flag can vary by codec standards. For details about this, please refer to 'Appendix: FRAME RATE DENOMINATORS in programmer\'s guide'.  */
	H265Info h265Info;              /**< This is H.265/HEVC picture information. Refer to <<vpuapi_h_H265Info>>. */
	H265RpSei h265RpSei;            /**< This is H.265/HEVC recovery point SEI information. Refer to <<vpuapi_h_H265RpSei>>. */

	DecOutputExtData decOutputExtData;  /**< The data structure to get additional information about a decoded frame. Refer to <<vpuapi_h_DecOutputExtData>>. */
	int consumedByte;       /**< The number of bytes that are consumed by VPU. */
	int rdPtr;              /**< A stream buffer read pointer for the current decoder instance */
	int wrPtr;              /**< A stream buffer write pointer for the current decoder instance */
	/**
	  @verbatim
	  The start byte position of the current frame after decoding the frame for audio-to-video synchronization

	  H.265/HEVC or H.264/AVC decoder seeks only 3-byte start code
	  (0x000001) while other decoders seek 4-byte start code(0x00000001).
	  @endverbatim
	 */
	PhysicalAddress bytePosFrameStart;
	PhysicalAddress bytePosFrameEnd;    /**< It indicates the end byte position of the current frame after decoding. This information helps audio-to-video synchronization. */
	FrameBuffer dispFrame;              /**< It indicates the display frame buffer address and information. Refer to <<vpuapi_h_FrameBuffer>>. */
	int frameDisplayFlag;               /**< It reports a frame buffer flag to be displayed.  */
	/**
	  @verbatim
	  This variable reports that sequence has been changed while H.264/AVC stream decoding.
	  If it is 1, HOST application can get the new sequence information by calling VPU_DecGetInitialInfo() or VPU_DecIssueSeqInit().

	  For H.265/HEVC decoder, each bit has a different meaning as follows.

	  @* sequenceChanged[5] : it indicates that the profile_idc has been changed.
	  @* sequenceChanged[16] : it indicates that the resolution has been changed.
	  @* sequenceChanged[19] : it indicates that the required number of frame buffer has been changed.
	  @endverbatim
	 */
	int sequenceChanged;
	// CODA9: [0]   1 - sequence changed
	// WAVEX: [5]   1 - H.265 profile changed
	//        [16]  1 - resolution changed
	//        [19]  1 - number of DPB changed

	int streamEndFlag;  /**< This variable reports the status of `end of stream` flag. This information can be used for low delay decoding (CODA980 only). */
	int frameCycle;     /**< This variable reports the cycle number of decoding one frame. */
	int errorReason;    /**< This variable reports the error reason that occurs while decoding. For error description, please find the 'Appendix: Error Definition' in the Programmer's Guide. */
	int errorReasonExt; /**< This variable reports the specific reason of error. For error description, please find the 'Appendix: Error Definition' in the Programmer's Guide. (WAVE only) */
	int warnInfo;
	Uint32 sequenceNo;  /**< This variable increases by 1 whenever sequence changes. If it happens, HOST should call VPU_DecFrameBufferFlush() to get the decoded result that remains in the buffer in the form of DecOutputInfo array. HOST can recognize with this variable whether this frame is in the current sequence or in the previous sequence when it is displayed. (WAVE only) */

	/**
	  @verbatim
	  This variable reports the result of pre-scan which is the start of decoding routine for DEC_PIC command. (WAVE only)
	  In the prescan phase, VPU parses bitstream and pre-allocates frame buffers.

	  @* -2 : it is when VPU prescanned bitstream(bitstream consumed), but a decode buffer was not allocated for the bitstream during pre-scan, since there was only header information.
	  @* -1 : it is when VPU detected full of framebuffer while pre-scannig (bitstream not consumed).
	  @* >= 0 : it indicates that prescan has been successfully done. This index is returned to a decoded index for the next decoding.
	  @endverbatim
	 */
	int indexFramePrescan;
#ifdef SUPPORT_REF_FLAG_REPORT
	int frameReferenceFlag[31];
#endif
	Int32   lfEnable;           /**< If this is 1, AFBCE starts encoding the first CTU row with padding zeros into the first 4 lines. */
#ifdef PROFILE_DECODING_TIME_INCLUDE_TICK
	Int32   decHostCmdTick;
	Int32   decSeekStartTick;
	Int32   decSeekEndTick;
	Int32   decParseStartTick;
	Int32   decParseEndTick;
	Int32   decDecodeStartTick;
	Int32   decDecodeEndTick;
#endif
	Int32   seekCycle;          /**< This variable reports the number of cycles in seeking phase on the command queue. (WAVE5 only) */
	Int32   parseCycle;         /**< This variable reports the number of cycles in prescan phase on the command queue. (WAVE5 only) */
	Int32   decodeCycle;        /**< This variable reports the number of cycles in decoding phase on the command queue. (WAVE5 only) */

	/**
	  @verbatim
	  A CTU size (only for WAVE series)

	  @* 16 : CTU16x16
	  @* 32 : CTU32x32
	  @* 64 : CTU64x64
	  @endverbatim
	 */
	Int32 ctuSize;
	Int32 outputFlag;          /**< This variable reports whether the current frame is bumped out or not. (WAVE5 only) */

	Int32 lumaBitdepth;
	Int32 chromaBitdepth;
	Int32 chromaFormatIDC;
	Int32 isExtSAR;

#if TCC_VP9____DEC_SUPPORT == 1
	Vp9PicInfo vp9PicInfo;     /**< VP9 color information, including color space and color range. */
#endif
} DecOutputInfo;

/**
 * @brief   This is a data structure of frame buffer information. It is used for parameter when host issues DEC_GET_FRAMEBUF_INFO of <<vpuapi_h_VPU_DecGiveCommand>>.
 */
typedef struct {
	vpu_buffer_t  vbFrame;          /**< The information of frame buffer where compressed frame is saved  */
	vpu_buffer_t  vbWTL;            /**< The information of frame buffer where decoded, uncompressed frame is saved with linear format if WTL is on   */
	vpu_buffer_t  vbFbcYTbl[MAX_REG_FRAME];        /**< The information of frame buffer to save luma offset table of compressed frame  */
	vpu_buffer_t  vbFbcCTbl[MAX_REG_FRAME];        /**< The information of frame buffer to save chroma offset table of compressed frame */
	vpu_buffer_t  vbMvCol[MAX_REG_FRAME];          /**< The information of frame buffer to save motion vector collocated buffer */
	FrameBuffer   framebufPool[64]; /**< This is an array of <<vpuapi_h_FrameBuffer>> which contains the information of each frame buffer. When WTL is enabled, the number of framebufPool would be [number of compressed frame buffer] x 2, and the starting index of frame buffer for WTL is framebufPool[number of compressed frame buffer].  */
} DecGetFramebufInfo;

/**
 * @brief   This is a data structure of queue command information. It is used for parameter when host issues DEC_GET_QUEUE_STATUS of <<vpuapi_h_VPU_DecGiveCommand>>. (WAVE5 only)
 */
typedef struct {
	Uint32  instanceQueueCount; /**< This variable indicates the number of queued commands of the instance.  */
	Uint32  totalQueueCount;    /**< This variable indicates the number of queued commands of all instances.  */
} DecQueueStatusInfo;

//------------------------------------------------------------------------------
// encode struct and definition
//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief This function returns whether processing a frame by VPU is completed or not.
	 * @return
	 @verbatim
	 @* 0 : VPU hardware is idle.
	 @* Non-zero value : VPU hardware is busy with processing a frame.
	 @endverbatim
	 */
	Int32  WAVE5_IsBusy(
						Uint32 coreIdx  /**< [Input] An index of VPU core */
					   );

	/**
	 * @brief This function returns whether VPU is currently running or not.
	 * @param coreIdx [Input] An index of VPU core
	 * @return
	 @verbatim
	 @* 0 : VPU is not running.
	 @* 1 or more : VPU is running.
	 @endverbatim
	 */
	Int32  WAVE5_IsInit(
						Uint32 coreIdx
					   );

	/**
	 * @brief
	 @verbatim
	 This function initializes VPU hardware and its data
	 structures/resources. HOST application must call this function only once before calling
	 VPU_DeInit().

NOTE: Before use, HOST application needs to define the header file path of BIT firmware to BIT_CODE_FILE_PATH.
@endverbatim
	 */
	RetCode WAVE5_Init(
					   Uint32 coreIdx,  /**<[Input] An index of VPU core. This value can be from 0 to (number of VPU core - 1).*/
					   PhysicalAddress workBuf,
					   PhysicalAddress codeBuf,
					   Int32 cq_count
					  );


	/**
	 * @brief This function returns the product information of VPU which is currently running on the system.
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means version information is acquired
	 successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means the current firmware does not contain any version
	 information.
	 *RETCODE_NOT_INITIALIZED* ::
	 VPU was not initialized at all before calling this function. Application should
	 initialize VPU by calling VPU_Init() before calling this function.
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame-decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST application should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 *RETCODE_VPU_RESPONSE_TIMEOUT* ::
	 Operation has not received any response from VPU and has timed out.
	 @endverbatim
	 */
	RetCode WAVE5_GetVersionInfo(
								 Uint32 coreIdx,     /**< [Input] An index of VPU core */
								 /**
								   @verbatim
								   [Output]

								   @* Version_number[15:12] - Major revision
								   @* Version_number[11:8] - Hardware minor revision
								   @* Version_number[7:0] - Software minor revision
								   @endverbatim
								  */
								 Uint32* versionInfo,
								 Uint32* revision,   /**< [Output] Revision information  */
								 Uint32* productId   /**< [Output] Product information. Refer to the <<vpuapi_h_ProductId>> enumeration */
								);

	/**
	 * @brief This function clears VPU interrupts that are pending.
	 * @return      none
	 */
	void WAVE5_ClearInterrupt(
							  Uint32 coreIdx  /**< [Input] An index of VPU core */
							 );

	void VPU_ClearInterruptEx(
							  VpuHandle handle,
							  Int32 intrFlag  /**< An interrupt flag to be cleared */
							 );

	/**
	 * @brief
	 @verbatim

	 This function stops operation of the current frame and initializes VPU hardware by sending reset signals.
	 It can be used when VPU is having a longer delay or seems hang-up.
	 After VPU has completed initialization, the context is rolled back to the state
	 before calling the previous VPU_DecStartOneFrame() or VPU_EncStartOneFrame().
	 HOST can resume decoding from the next picture, instead of decoding from the sequence header.
	 It works only for the current instance, so this function does not affect other instance\'s running in multi-instance operation.

	 This is some applicable scenario of using `VPU_SWReset()` when a series of hang-up happens.
	 For example, when VPU is hung up with frame 1, HOST application calls `VPU_SWReset()` to initialize VPU and then calls `VPU_DecStartOneFrame()` for frame 2
	 with specifying the start address, read pointer.
	 If there is still problem with frame 2, we recommend calling `VPU_SWReset()` and `seq_init()` or calling `VPU_SWReset()` and enabling iframe search.
	 @endverbatim
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information of the stream data to be
	 decoded was received successfully.
	 @endverbatim
	 */
	RetCode WAVE5_SWReset(
						  Uint32      coreIdx,    /**<[Input] An index of VPU core */
						  /**
							@verbatim
							[Input] Way of reset

							@* SW_RESET_SAFETY : It waits until AXI bus completes ongoing tasks. If remaining bus transactions are done, VPU enters the reset process. (recommended mode)
							@* SW_RESET_FORCE : It forces to reset VPU no matter whether bus transactions are completed or not. It might affect what other blocks do with bus, so we do not recommend using this mode.
							@* SW_RESET_ON_BOOT : This is the default reset mode that is executed once system boots up. This mode is actually executed in VPU_Init(), so does not have to be used independently.
							@endverbatim
						   */
						  SWResetMode resetMode,
						  void*       pendingInst,
						  Int32		cq_count
						 );

	/**
	 *  @brief   This function returns the size of frame buffer that is required for VPU to decode or encode one frame.
	 *
	 *  @return  The size of frame buffer to be allocated
	 */
	int WAVE5_GetFrameBufSize(
							  int         coreIdx,    /**< [Input] VPU core index number */
							  int         stride,     /**< [Input] The stride of image  */
							  int         height,     /**< [Input] The height of image */
							  int         mapType,    /**< [Input] The map type of framebuffer */
							  int         format,     /**< [Input] The color format of framebuffer */
							  int         interleave, /**< [Input] Whether to use CBCR interleave mode or not */
							  DRAMConfig* pDramCfg    /**< [Input] Attributes of DRAM. It is only valid for CODA960. Set NULL for this variable in case of other products. */
							 );

	/**
	 *  @brief  This function returns the product ID of VPU which is currently running.
	 *  @return Product information. Please refer to the <<vpuapi_h_ProductId>> enumeration.
	 */

	// function for decode
	/**
	 *  @brief In order to decode, HOST application must open the decoder. By calling this function, HOST application can get a handle by which they can refer to a decoder instance. Because the VPU is multiple instance codec, HOST application needs this kind of handle. Once HOST application gets a handle, the HOST application must pass this handle to all subsequent decoder-related functions.
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means a new decoder instance was created
	 successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means getting a new decoder instance was not done
	 successfully. If there is no free instance anymore, this value is returned
	 in this function call.
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pOpenParam, was invalid, which means it has a null
	 pointer, or given values for some member variables are improper values.
	 *RETCODE_NOT_INITIALIZED* ::
	 This means VPU was not initialized yet before calling this function.
	 Applications should initialize VPU by calling VPU_Init() before calling this
	 function.
	 @endverbatim
	 */
	RetCode WAVE5_DecOpen(
						  DecHandle *pHandle,     /**< [Output] A pointer to a DecHandle type variable which specifies each instance for HOST application. */
						  DecOpenParam *pop       /**< [Input] A pointer to a DecOpenParam type structure which describes required parameters for creating a new decoder instance. */
						 );

	/**
	 *  @brief      When HOST application finished decoding a sequence and wanted to release this
	 instance for other processing, the applicaton should close this instance. After
	 completion of this function call, the instance referred to by handle gets
	 free. Once HOST application closes an instance, the HOST application cannot call any further
	 decoder-specific function with the current handle before re-opening a new
	 decoder instance with the same handle.
	 *  @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means the current decoder instance was closed
	 successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, handle, was invalid.
	 This return code might be caused if

	 * `handle` is not the handle which has been obtained by VPU_DecOpen()
	 * `handle` is the handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_VPU_RESPONSE_TIMEOUT* ::
	 Operation has not received any response from VPU and has timed out.
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST application should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 function.
	 @endverbatim
	 */
	RetCode WAVE5_DecClose(
						   DecHandle handle    /**< [Input] A decoder handle obtained from VPU_DecOpen() */
						  );

	/**
	 * @brief
	 @verbatim
	 Applications must pass the address of <<vpuapi_h_DecInitialInfo>> structure, where the
	 decoder stores information such as picture size, number of necessary frame
	 buffers, etc. For the details, see definition of <<vpuapi_h_DecInitialInfo>> data structure.
	 This function should be called once after creating a decoder instance and before starting frame decoding.

	 It is HOST application\'s responsibility to provide sufficient amount of bitstream to
	 the decoder by calling VPU_DecUpdateBitstreamBuffer() so that bitstream buffer
	 does not get empty before this function returns.
	 If HOST application cannot ensure to feed stream enough, they can use the Forced
	 Escape option by using VPU_DecSetEscSeqInit().
	 @endverbatim
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information of the stream
	 data to be decoded was received successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means there was an error in getting information for
	 configuring the decoder.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pInfo, was invalid, which means it has a null
	 pointer, or given values for some member variables are improper values.
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling  VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means the current API function call was invalid considering the allowed
	 sequences between API functions. In this case, HOST might call this
	 function before successfully putting bitstream data by calling
	 VPU_DecUpdateBitstreamBuffer(). In order to perform this functions call,
	 bitstream data including sequence level header should be transferred into
	 bitstream buffer before calling VPU_DecGetInitialInfo().
	 *RETCODE_CALLED_BEFORE* ::
	 This function call might be invalid, which means multiple calls of the current API
	 function for a given instance are not allowed. In this case, decoder initial
	 information has been already received, so that this function call is meaningless
	 and not allowed anymore.
	 *RETCODE_VPU_RESPONSE_TIMEOUT* ::
	 +
	 --
	 Operation has not received any response from VPU and has timed out.

NOTE: When this function returns RETCODE_SUCCESS, HOST should call VPU_ClearInterrupt() function to clear out the interrupt pending status.
--
@endverbatim
	 */

	/**
	 * @brief
	 @verbatim
	 This function starts decoding sequence header. Returning from this function does not mean
	 the completion of decoding sequence header, and it is just that decoding sequence header was initiated.
	 Every call of this function should be matched with VPU_DecCompleteSeqInit() with the same handle.
	 Without calling a pair of these funtions, HOST can not call any other API functions
	 except for VPU_IsBusy(), VPU_DecGetBitstreamBuffer(), and VPU_DecUpdateBitstreamBuffer().

	 A pair of this function and VPU_DecCompleteSeqInit() or VPU_DecGetInitialInfo() should be called
	 at least once after creating a decoder instance and before starting frame decoding.
	 @endverbatim
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information of the stream data to be
	 decoded was received successfully
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means there was an error in getting information for configuring
	 the decoder.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid. This return
	 code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API function call
	 cannot be performed this time. A frame decoding operation should be completed by calling
	 VPU_DecIssueSeqInit (). Even though the result of the current frame operation is not necessary,
	 HOST should call VPU_DecIssueSeqInit () to proceed this function call.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means the current API function call was invalid considering the allowed sequences between
	 API functions. In this case, HOST application might call this function before successfully putting
	 bitstream data by calling VPU_DecUpdateBitstreamBuffer(). In order to perform this functions
	 call, bitstream data including sequence level header should be transferred into bitstream buffer
	 before calling VPU_ DecIssueSeqInit ().
	 @endverbatim
	 */
	RetCode WAVE5_DecIssueSeqInit(
								  DecHandle handle        /**<  [Input] A decoder handle obtained from VPU_DecOpen() */
								 );

	/**
	 * @brief Application can get the information about sequence header.
	 Applications must pass the address of DecInitialInfo structure, where the decoder stores
	 information such as picture size, number of necessary frame buffers, etc.
	 For more details, see definition of the section called the DecInitialInfo data structure.
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information of the stream data to be
	 decoded was received successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means there was an error in getting information for configuring
	 the decoder.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid. This return
	 code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pInfo, was invalid, which means it has a null pointer, or given
	 values for some member variables are improper values.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means the current API function call was invalid considering the allowed sequences between
	 API functions. It might happen because VPU_DecIssueSeqInit () with the same handle was
	 not called before calling this function
	 *RETCODE_CALLED_BEFORE* ::
	 This function call might be invalid, which means multiple calls of the current API function for a given
	 instance are not allowed. In this case, decoder initial information has been already received,
	 so that this function call is meaningless and not allowed anymore.
	 @endverbatim
	 */
	RetCode WAVE5_DecCompleteSeqInit(
									 DecHandle       handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen() */
									 DecInitialInfo* info        /**< [Output] A pointer to DecInitialInfo data structure */
									);

	/**
	 *  @brief  This is a special function to provide a way of escaping VPU hanging during DEC_SEQ_INIT.
	 When this flag is set to 1 and stream buffer empty happens, VPU terminates automatically
	 DEC_SEQ_INIT operation. If target applications ensure that high layer header syntax is
	 periodically sent through the channel, they do not need to use this option. But if target applications
	 cannot ensure that such as file-play, it might be very useful to avoid VPU hanging without HOST timeout caused
	 by crucial errors in header syntax.
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means Force escape flag is successfully provided to
	 BIT Processor.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid. This return
	 code might be caused if

	 * `handle` is not the handle which has been obtained by VPU_DecOpen().
	 * `handle` is the handle of an instance which has been closed already, etc.
	 * bitstreamMode of DecOpenParam structure is not BS_MODE_INTERRUPT.
	 --
	 @endverbatim
	 */
	RetCode WAVE5_DecSetEscSeqInit(
								   DecHandle   handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen() */
								   int         escape      /**< [Input] A flag to enable or disable forced escape from SEQ_INIT */
								  );

	/**
	 * @brief This function is used for registering frame buffers with the acquired
	 information from VPU_DecGetInitialInfo(). The frame buffers pointed to by
	 bufArray are managed internally within VPU. These include reference
	 frames, reconstructed frame, etc. Applications must not change the contents of
	 the array of frame buffers during the life time of the instance, and `num` must
	 not be less than `minFrameBufferCount` obtained by VPU_DecGetInitialInfo().
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means registering frame buffer
	 information was done successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means the current API function call was invalid considering the allowed
	 sequences between API functions. HOST might call this
	 function before calling VPU_DecGetInitialInfo() successfully. This function
	 should be called after successful calling VPU_DecGetInitialInfo().
	 *RETCODE_INVALID_FRAME_BUFFER* ::
	 This happens when pBuffer was invalid, which means pBuffer was not initialized
	 yet or not valid anymore.
	 *RETCODE_INSUFFICIENT_FRAME_BUFFERS* ::
	 This means the given number of frame buffers, num, was not enough for the
	 decoder operations of the given handle. It should be greater than or equal to
	 the value   requested by VPU_DecGetInitialInfo().
	 *RETCODE_INVALID_STRIDE* ::
	 The given argument stride was invalid, which means it is smaller than the
	 decoded picture width, or is not a multiple of 8 in this case.
	 *RETCODE_CALLED_BEFORE* ::
	 This function call is invalid which means multiple calls of the current API function
	 for a given instance are not allowed. In this case, registering decoder frame
	 buffers has been already done, so that this function call is meaningless and not
	 allowed anymore.
	 *RETCODE_VPU_RESPONSE_TIMEOUT* ::
	 Operation has not recieved any response from VPU and has timed out.
	 @endverbatim
	 */
	RetCode DecRegisterFrameBuffer(
								   DecHandle    handle,	/**< [Input] A decoder handle obtained from VPU_DecOpen() */
								   FrameBuffer* bufArray,	/**< [Input] Allocated frame buffer address and information. If this parameter is set to 0, VPU allocates frame buffers. */
								   PhysicalAddress addr,
								   int          numFbsForDecoding,
								   int          numFbsForWTL,
								   int          stride,
								   int          height,
								   int          mapType
								  );

	RetCode WAVE5_DecRegisterFrameBuffer(
										 DecHandle    handle,	/**< [Input] A decoder handle obtained from VPU_DecOpen() */
										 FrameBuffer* bufArray,	/**< [Input] Allocated frame buffer address and information. If this parameter is set to 0, VPU allocates frame buffers. */
										 PhysicalAddress addr,
										 int          numFbsForDecoding,
										 int          numFbsForWTL,
										 int          stride,
										 int          height,
										 int          mapType
										);

	RetCode WAVE5_DecRegisterFrameBuffer2(
										  DecHandle    handle,	/**< [Input] A decoder handle obtained from VPU_DecOpen() */
										  FrameBuffer* bufArray,	/**< [Input] Allocated frame buffer address and information. If this parameter is set to 0, VPU allocates frame buffers. */
										  PhysicalAddress addr[32],
										  int          numFbsForDecoding,
										  int          numFbsForWTL,
										  int          stride,
										  int          height,
										  int          mapType
										 );

	RetCode WAVE5_DecRegisterFrameBuffer3(
										  DecHandle handle,
										  codec_addr_t framebuffer_addrs[64][9],
										  int numFbsForWTL,
										  int numFbsForDecoding,
										  int stride,
										  int height,
										  int mapType);

	/**
	 * @brief
	 @verbatim
	 This function returns the frame buffer information that was allocated by VPU_DecRegisterFrameBuffer() function.

	 It does not affect actual decoding and simply does obtain the information of frame buffer.
	 This function is more helpful especially when frame buffers are automatically assigned by
	 setting 0 to `bufArray` of VPU_DecRegisterFrameBuffer() and HOST wants to know about the allocated frame buffer.
	 @endverbatim
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means registering frame buffer
	 information was done successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, frameIdx, was invalid, which means frameIdx is larger than allocated framebuffer.
	 @endverbatim
	 */
	RetCode WAVE5_DecGetFrameBuffer(
									DecHandle   handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen() */
									int         frameIdx,	/**< [Input] An index of frame buffer */
									FrameBuffer* frameBuf	/**< [output] Allocated frame buffer address and information.  */
								   );

	/**
	 * @brief
	 @verbatim
	 Before decoding bitstream, HOST application must feed the decoder with bitstream. To
	 do that, HOST application must know where to put bitstream and the maximum size.
	 Applications can get the information by calling this function. This way is more
	 efficient than providing arbitrary bitstream buffer to the decoder as far as
	 VPU is concerned.

	 The given size is the total sum of free space in ring buffer. So when
	 a HOST application downloads this given size of bitstream, Wrptr could meet the end of
	 stream buffer. In this case, the HOST application should wrap-around the Wrptr back to the
	 beginning of stream buffer, and download the remaining bits. If not, decoder
	 operation could be crashed.
	 @endverbatim
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information for decoder
	 stream buffer was received successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not the handle which has been obtained by VPU_DecOpen().
	 * `handle` is the handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pRdptr, pWrptr or size, was invalid, which means
	 it has a null pointer, or given values for some member variables are improper
	 values.
	 @endverbatim
	 */
	RetCode WAVE5_DecGetBitstreamBuffer(
										DecHandle       handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen()   */
										PhysicalAddress *prdPrt,	/**< [Output] A stream buffer read pointer for the current decoder instance  */
										PhysicalAddress *pwrPtr,	/**< [Output] A stream buffer write pointer for the current decoder instance   */
										Uint32          *size		/**< [Output] A variable specifying the available space in bitstream buffer for the current decoder instance   */
									   );

	/**
	 * @brief
	 @verbatim
	 Before decoding bitstream, HOST application must feed the decoder with bitstream. To
	 do that, HOST application must know where to put bitstream and the maximum size.
	 Applications can get the information by calling this function. This way is more
	 efficient than providing arbitrary bitstream buffer to the decoder as far as
	 VPU is concerned.

	 The given size is the total sum of free space in ring buffer. So when
	 a HOST application downloads this given size of bitstream, Wrptr could meet the end of
	 stream buffer. In this case, the HOST application should wrap-around the Wrptr back to the
	 beginning of stream buffer, and download the remaining bits. If not, decoder
	 operation could be crashed.
	 @endverbatim
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information for decoder
	 stream buffer was received successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not the handle which has been obtained by VPU_DecOpen().
	 * `handle` is the handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pRdptr, pWrptr or size, was invalid, which means
	 it has a null pointer, or given values for some member variables are improper
	 values.
	 @endverbatim
	 */

	/**
	 * @brief   Applications must let decoder know how much bitstream has been transferred to the
	 address obtained from VPU_DecGetBitstreamBuffer(). By just giving the size as
	 an argument, API automatically handles pointer wrap-around and updates the write
	 pointer.
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means putting new stream data was done successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not the handle which has been obtained by VPU_DecOpen().
	 * `handle` is the handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, size, was invalid, which means size is larger than
	 the value obtained from VPU_DecGetBitstreamBuffer(), or than the available space
	 in the bitstream buffer.
	 @endverbatim
	 */
	RetCode WAVE5_DecUpdateBitstreamBuffer(
										   DecHandle   handle,		/**< [Input] A decoder handle obtained from VPU_DecOpen() */
										   /**
											 @verbatim
											 [Input]
											 A variable specifying the amount of bits transferred into bitstream buffer for the current decoder instance.

											 The size 0 means that no more bitstream exists to feed (end of stream).
											 After setting 0 to this variable, VPU decodes remaing bitstream and returns -1 to `indexFrameDisplay`.
											 If it is -1, decoder can continue to decode without VPU_DecClose()
											 after remaining stream with VPU_DecUpdateBitstreamBuffer(handle, 0) has completely been decoded,
											 which is especially useful when sequence changes in the middle of decoding.
											 @endverbatim
											*/
										   int         size
										  );

	/**
	 * @brief   This function starts decoding one frame. Returning from this function does not
	 mean the completion of decoding one frame, and it is just that decoding one
	 frame was initiated. Every call of this function should be matched with VPU_DecGetOutputInfo()
	 with the same handle. Without calling a pair of these funtions, HOST cannot call any other
	 API functions except for VPU_IsBusy(), VPU_DecGetBitstreamBuffer(), and
	 VPU_DecUpdateBitstreamBuffer().
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 +
	 --
	 Operation was done successfully, which means decoding a new frame was started
	 successfully.

NOTE: This return value does not mean that decoding a frame was completed
successfully.
--
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means current API function call was invalid considering the allowed
	 sequences between API functions. A HOST might call this
	 function before successfully calling VPU_DecRegisterFrameBuffer(). This function
	 should be called after calling VPU_ DecRegisterFrameBuffer() successfully.
	 @endverbatim
	 */
	RetCode WAVE5_DecStartOneFrame(
								   DecHandle handle,   /**< [Input] A decoder handle obtained from VPU_DecOpen() */
								   DecParam *param		/**< [Input] A pointer to a DecParam type structure which describes picture decoding parameters for the given decoder instance */
								  );

	/**
	 * @brief   Application can get the information of output of decoding. The information
	 includes the information of a frame buffer which contains the reconstructed image.
	 HOST should call this function after frame decoding is finished and
	 before starting the further processing.
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means receiving the output information of
	 current frame was done successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means there was an error in getting information for
	 configuring the decoder.
	 *RETCODE_INVALID_HANDLE* ::
	 This means argument handle is invalid. This includes cases where `handle` is not
	 a handle which has been obtained by VPU_DecOpen(), `handle` is a handle to an
	 instance already closed, or `handle` is a handle to a decoder instance. Also,
	 this value is returned when VPU_DecStartOneFrame() is matched with
	 VPU_DecGetOutputInfo() with different handles.
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, pInfo, was invalid, which means it has a null
	 pointer, or given values for some member variables are improper values.
	 @endverbatim
	 */
	RetCode WAVE5_DecGetOutputInfo(
								   DecHandle       handle,		/**< [Input] A decoder handle obtained from VPU_DecOpen() */
								   DecOutputInfo*  info		/**< [Output] A pointer to a DecOutputInfo type structure which describes picture decoding results for the current decoder instance. */
								  );

	/**
	 * @brief
	 @verbatim
	 This function flushes all of the decoded framebuffer contexts that remain in decoder firmware.
	 It is used to start seamless decoding operation without randome access to the buffer or calling VPU_DecClose().

NOTE: In WAVE410, this function returns all of the decoded framebuffer contexts that remain.
pRetNum always has 0 in CODA960, CODA980, and WAVE320.
@endverbatim
	 *
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means receiving the output information of
	 current frame was done successfully.
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 *RETCODE_INVALID_HANDLE* ::
	 This means argument handle is invalid. This includes cases where `handle` is not
	 a handle which has been obtained by VPU_DecOpen(), `handle` is a handle to an
	 instance already closed, or `handle` is a handle to an decoder instance.
	 Also,this value is returned when VPU_DecStartOneFrame() is matched with
	 VPU_DecGetOutputInfo() with different handles.
	 *RETCODE_VPU_RESPONSE_TIMEOUT* ::
	 Operation has not recieved any response from VPU and has timed out.
	 @endverbatim
	 */
	RetCode WAVE5_DecFrameBufferFlush(
									  DecHandle       handle,         /**< [Input] A decoder handle obtained from VPU_DecOpen() */
									  DecOutputInfo*  pRemainingInfo,	/**<  [Output] All of the decoded framebuffer contexts are stored in display order as array of DecOutputInfo.
																		  If this is NULL, the remaining information is not returned.  */
									  Uint32*         pRetNum			/**<  [Output] The number of the decoded frame buffer contexts. It this is null, the information is not returned.  */
									 );

	/**
	 * @brief
	 This function specifies the location of read pointer in bitstream buffer.
	 It can also set a write pointer with same value of read pointer (addr) when updateWrPtr is not a zero value,
	 which allows to flush up the bitstream buffer at once.
	 This function is used to operate bitstream buffer in PicEnd mode.
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means required information of the stream data to be
	 decoded was received successfully.
	 *RETCODE_FAILURE* ::
	 Operation was failed, which means there was an error in getting information for configuring
	 the decoder.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for current API function call, `handle`, was invalid. This return
	 code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API function call
	 cannot be performed this time. A frame decoding operation should be completed by calling
	 VPU_ DecSetRdPtr ().
	 @endverbatim
	 */
	RetCode WAVE5_DecSetRdPtr(
							  DecHandle       handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen() */
							  PhysicalAddress addr,       /**< [Input] Updated read or write pointer */
							  int             updateWrPtr /**< [Input] A flag whether to update a write pointer or not as well as a read pointer. */
							 );

	/**
	 * @brief
	 Applications can clear display flag of each frame buffer by calling this function
	 after creating decoder instance. If display flag of frame buffer is cleared, the
	 frame buffer can be used in the decoding process. So, HOST can control
	 displaying a buffer with clearing the display flag which set by VPU at every
	 display index output process.
	 * @return
	 @verbatim
	 *RETCODE_SUCCESS* ::
	 Operation was done successfully, which means receiving the output information of
	 current frame was done successfully.
	 *RETCODE_INVALID_HANDLE* ::
	 This means argument handle is invalid. This includes cases where `handle` is not
	 a handle which has been obtained by VPU_DecOpen(), `handle` is a handle to an
	 instance already closed, or `handle` is a handle to an decoder instance.
	 Also,this value is returned when VPU_DecStartOneFrame() is matched with
	 VPU_DecGetOutputInfo() with different handles.
	 *RETCODE_WRONG_CALL_SEQUENCE* ::
	 This means current API function call was invalid considering the allowed
	 sequences between API functions.
	 It might happen because VPU_DecRegisterFrameBuffer() with the
	 same handle was not called before calling this function.
	 *RETCODE_INVALID_PARAM* ::
	 The given argument parameter, index, was invalid, which means it has improper
	 values.
	 @endverbatim
	 */
	RetCode WAVE5_DecClrDispFlag(
								 DecHandle   handle, /**< [Input] A decoder handle obtained from VPU_DecOpen() */
								 int         index   /**< [Input] A frame buffer index to be cleared */
								);

	RetCode WAVE5_DecClrDispFlagEx(
								   DecHandle   	handle, 	/**< [Input] A decoder handle obtained from VPU_DecOpen() */
								   unsigned int 	dispFlag   	/**< [Input] Frame buffers (bit flags) to be cleared*/
								  );
	/**
	 * @brief   This function is provided to let HOST have a certain level of freedom for
	 re-configuring decoder operation after creating a decoder instance. Some options
	 which can be changed dynamically during decoding as the video sequence has been
	 included. Some command-specific return codes are also presented.
	 * @return
	 @verbatim
	 *RETCODE_INVALID_COMMAND* ::
	 The given argument, cmd, was invalid, which means the given cmd was undefined,
	 or not allowed in the current instance.
	 *RETCODE_INVALID_HANDLE* ::
	 +
	 --
	 This means the given handle for the current API function call, `handle`, was invalid.
	 This return code might be caused if

	 * `handle` is not a handle which has been obtained by VPU_DecOpen().
	 * `handle` is a handle of an instance which has been closed already, etc.
	 --
	 *RETCODE_FRAME_NOT_COMPLETE* ::
	 This means frame decoding operation was not completed yet, so the given API
	 function call cannot be performed this time. A frame decoding operation should
	 be completed by calling VPU_DecGetOutputInfo(). Even though the result of the
	 current frame operation is not necessary, HOST application should call
	 VPU_DecGetOutputInfo() to proceed this function call.
	 values.
	 @endverbatim
	 * @note
	 @verbatim
	 The list of commands can be summarized as follows:

	 @* ENABLE_ROTATION
	 @* ENABLE_MIRRORING
	 @* DISABLE_MIRRORING
	 @* ENABLE_DERING
	 @* DISABLE_DERING
	 @* SET_MIRROR_DIRECTION
	 @* SET_ROTATION_ANGLE
	 @* SET_ROTATOR_OUTPUT
	 @* SET_ROTATOR_STRIDE
	 @* ENABLE_DEC_THUMBNAIL_MODE,
	 @* DEC_SET_SPS_RBSP
	 @* DEC_SET_PPS_RBSP
	 @* DEC_SET_SEQ_CHANGE_MASK
	 @* ENABLE_REP_USERDATA
	 @* DISABLE_REP_USERDATA
	 @* SET_ADDR_REP_USERDATA
	 @* SET_VIRT_ADDR_REP_USERDATA
	 @* SET_SIZE_REP_USERDATA
	 @* SET_USERDATA_REPORT_MODE
	 @* SET_SEC_AXI
	 @* SET_DRAM_CONFIG
	 @* GET_DRAM_CONFIG
	 @* SET_CACHE_CONFIG
	 @* GET_TILEDMAP_CONFIG
	 @* SET_LOW_DELAY_CONFIG
	 @* DEC_GET_DISPLAY_OUTPUT_INFO
	 @* SET_DECODE_FLUSH
	 @* DEC_SET_FRAME_DELAY
	 @* DEC_FREE_FRAME_BUFFER
	 @* DEC_GET_FIELD_PIC_TYPE
	 @* DEC_ENABLE_REORDER
	 @* DEC_DISABLE_REORDER
	 @* DEC_GET_FRAMEBUF_INFO
	 @* DEC_RESET_FRAMEBUF_INFO
	 @* DEC_SET_DISPLAY_FLAG
	 @* DEC_GET_SEQ_INFO
	 @* ENABLE_LOGGING
	 @* DISABLE_LOGGING

	 ====== ENABLE_ROTATION
	This command enables rotation of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== DISABLE_ROTATION
		This command disables rotation of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== ENABLE_MIRRORING
		This command enables mirroring of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== DISABLE_MIRRORING
		This command disables mirroring of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== ENABLE_DERING
		This command enables deringring filter of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== DISABLE_DERING
		This command disables deringing filter of the post-rotator.
		In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

		====== SET_MIRROR_DIRECTION
		This command sets mirror direction of the post-rotator, and `parameter` is
		interpreted as a pointer to MirrorDirection. The `parameter` should be one of
		MIRDIR_NONE, MIRDIR_VER, MIRDIR_HOR, and MIRDIR_HOR_VER.

		@* MIRDIR_NONE: No mirroring
		@* MIRDIR_VER: Vertical mirroring
		@* MIRDIR_HOR: Horizontal mirroring
		@* MIRDIR_HOR_VER: Both directions

		This command has one of the following return codes.::

		* RETCODE_SUCCESS:
		Operation was done successfully, which means given mirroring direction is valid.
		* RETCODE_INVALID_PARAM:
		The given argument parameter, `parameter`, was invalid, which means given mirroring
		direction is invalid.

		====== SET_ROTATION_ANGLE
		This command sets counter-clockwise angle for post-rotation, and `parameter` is
		interpreted as a pointer to the integer which represents rotation angle in
		degrees. Rotation angle should be one of 0, 90, 180, and 270.

		This command has one of the following return codes.::

		* RETCODE_SUCCESS:
		Operation was done successfully, which means given rotation angle is valid.
		* RETCODE_INVALID_PARAM:
		The given argument parameter, `parameter`, was invalid, which means given rotation
		angle is invalid.

		====== SET_ROTATOR_OUTPUT
		This command sets rotator output buffer address, and `parameter` is interpreted as
		the pointer of a structure representing physical addresses of YCbCr components
		of output frame. For storing the rotated output for display, at least one more
		frame buffer should be allocated. When multiple display buffers are required,
			  HOST application could change the buffer pointer of rotated output at every frame by
				  issuing this command.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given rotation angle is valid.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means given frame
				  buffer pointer is invalid.

				  ====== SET_ROTATOR_STRIDE
				  This command sets the stride size of the frame buffer containing rotated output,
			  and `parameter` is interpreted as the value of stride of the rotated output.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given rotation angle is valid.
				  * RETCODE_INVALID_STRIDE:
				  The given argument parameter, `parameter`, was invalid, which means given value of
				  stride is invalid. The value of stride must be greater than 0 and a multiple of 8.

				  ====== ENABLE_DEC_THUMBNAIL_MODE
				  This command decodes only an I-frame of picture from bitstream for using it as a thumbnail.
				  It requires as little as size of frame buffer since I-picture does not need any reference picture.
				  If HOST issues this command and sets one frame buffer address to FrameBuffer array in VPU_DecRegisterFrameBuffer(),
			  only the frame buffer is used.
				  And please make sure that the number of frame buffer `num` should be registered as minFrameBufferCount.

				  ====== DEC_SET_SPS_RBSP
				  This command applies SPS stream received from a certain
				  out-of-band reception scheme to the decoder.
				  The stream should be in RBSP format and in big Endian.
				  The argument `parameter` is interpreted as a pointer to DecParamSet structure.
				  In this case, `paraSet` is an array of 32 bits which contains SPS RBSP, and `size`
				  is the size of the stream in bytes.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means transferring an SPS RBSP to decoder
				  was done successfully.
				  * RETCODE_INVALID_COMMAND:
				  The given argument `cmd` was invalid, which means the given `cmd` was undefined,
			  or not allowed in the current instance. In this case, current instance might not
				  be an H.264/AVC decoder instance.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means it has a null
				  pointer, or given values for some member variables are improper values.

				  ====== DEC_SET_PPS_RBSP
				  This command applies PPS stream received from a certain
				  out-of-band reception scheme to the decoder. The stream should be in RBSP format and in
				  big Endian. The argument `parameter` is interpreted as a pointer to a DecParamSet structure.
				  In this case, paraSet is an array of 32 bits which contains PPS RBSP, and `size`
				  is the size of the stream in bytes.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means transferring a PPS RBSP to decoder
				  was done successfully.
				  * RETCODE_INVALID_COMMAND:
				  The given argument `cmd` was invalid, which means the given cmd was undefined,
			  or not allowed in the current instance. In this case, current instance might not
				  be an H.264/AVC decoder instance.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means it has a null
				  pointer, or given values for some member variables are improper values.


				  ====== DEC_SET_SEQ_CHANGE_MASK

				  This command sets DEC_SET_SEQ_CHANGE_MASK which allows VPU to notify change of sequence information
				  such as picture size, DPB count, profile, and bit-depth.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means transferring a PPS RBSP to decoder
				  was done successfully.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means it has a null
				  pointer, or given values for some member variables are improper values.

				  ====== ENABLE_REP_USERDATA
				  This command enables user data report. This command ignores `parameter`.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means enabling user data report is done
				  successfully.
				  * RETCODE_USERDATA_BUF_NOT_SET:
				  This means user data buffer address and size have not set yet.

				  ====== DISABLE_REP_USERDATA
				  This command disables user data report. This command ignores `parameter` and returns
				  RETCODE_SUCCESS.

				  ====== SET_ADDR_REP_USERDATA
				  This command sets user data buffer address. `parameter` is interpreted as a pointer
				  to address. This command returns as follows.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given value of address is valid and
				  setting is done successfully.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter `parameter` was invalid, which means given value of
				  address is invalid. The value of address must be greater than 0 and a multiple
				  of 8.

				  ====== SET_VIRT_ADDR_REP_USERDATA
				  This command sets user data buffer address (virtual address) as well as physical address by using SET_ADDR_REP_USERDATA
				  `parameter` is interpreted as a pointer to address. This command returns as follows.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given value of address is valid and setting is done successfully.
				  * RETCODE_USERDATA_BUF_NOT_SET:
				  SET_ADDR_REP_USERDATA command was not been executed
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter `parameter` was invalid, which means given value of address is invalid.
				  The value of address must be greater than 0 and a multiple of 8.

				  ====== SET_SIZE_REP_USERDATA
				  This command sets the size of user data buffer which is set with
				  SET_ADDR_REP_USERDATA command. `parameter` is interpreted as a pointer to the value
				  of size. This command returns RETCODE_SUCCESS.

				  According to codec standards, user data type means as below.

				  @* H.264/AVC
				  @** 4 : user_data_registered_itu_t_t35
				  @** 5 : user_data_unregistered

				  More details are in Annex D of H.264 specifications.

				  The user data size 0 - 15 is used to make offset from userDataBuf Base + 8x17.
				  It specifies byte size of user data 0 to 15 excluding 0 padding byte,
			  which exists between user data. So HOST reads 1 user data from
				  userDataBuf Base + 8x17 + 0 User Data Size + 0 Padding.
				  Size of 0 padding is (8 - (User Data Size % 8))%8.

				  ====== SET_USERDATA_REPORT_MODE
				  TBD

				  ====== SET_SEC_AXI
				  This command sets the secondary channel of AXI for saving memory bandwidth to
				  dedicated memory. The argument `parameter` is interpreted as a pointer to SecAxiUse which
				  represents an enable flag and physical address which is related with the secondary
				  channel for BIT processor, IP/AC-DC predictor, de-blocking filter, overlap
				  filter respectively.

				  This command has one of the following return codes::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given value for setting secondary
				  AXI is valid.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means given value is
				  invalid.

				  ====== SET_DRAM_CONFIG
				  TBD

				  ====== GET_DRAM_CONFIG
				  TBD

				  ====== ENABLE_REP_MV
				  This command enables MV report. This command ignores `parameter` and returns
				  RETCODE_SUCCESS.

				  If this option is enabled, decoder reports 1 motion vector for P picture and 2
				  motion vectors for B picture.

				  ====== DISABLE_REP_MV
				  This command disables MV report. This command ignores `parameter` and returns
				  RETCODE_SUCCESS.

				  ====== SET_CACHE_CONFIG

				  This command sets the configuration of cache. The `parameter` is interpreted as a pointer to MaverickCacheConfig.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given value is valid and setting is done successfully.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid. The value of address must be not zero.

				  ====== GET_TILEDMAP_CONFIG
				  This command gets tiled map configuration according to `TiledMapConfig` structure.

				  This command has one of the following return codes.::

				  * RETCODE_SUCCESS:
				  Operation was done successfully, which means given value is valid and setting is done successfully.
				  * RETCODE_INVALID_PARAM:
				  The given argument parameter, `parameter`, was invalid, which means it has a null pointer,
			  or given values for some member variables are improper values.

				  ====== SET_LOW_DELAY_CONFIG
				  This command sets the low delay decoding options which enable low delay decoding and indicate the number of MB row.
				  The argument `parameter` is interpreted as a pointer to LowDelayInfo which represents an enable flag and the number of MB row.
				  If low delay decoding is enabled, VPU sends an interrupt and indexFrameDisplay to HOST when the number of MB row decoding is done.
				  If the interrupt is issued, HOST should clear the interrupt and read indexFrameDisplay from the RET_DEC_PIC_FRAME_IDX register in order to display.

				  ====== DEC_GET_DISPLAY_OUTPUT_INFO
				  HOST can get decoder output information according to display index in DecOutputInfo structure.
				  HOST can set display index using member variable `indexFrameDisplay`.
				  This command returns RETCODE_SUCCESS.

				  * Example code
				  .........................................................................
				  DecOutputInfo decOutputInfo;
	decOutputInfo. indexFrameDisplay = disp_index;
	VPU_DecGiveCommand(handle, DEC_GET_DISPLAY_OUTPUT_INFO, & decOutputInfo);
	.........................................................................

		====== SET_DECODE_FLUSH
		This command is used to change bitstream buffer mode from Interrupt mode to Picture End mode.
		When HOST receives an interrupt about bistream buffer empty in Interrupt Mode, but there is no more bitstream for the current frame in bitstream buffer,
			 this command completes decoding only with remaining bitstream.

				 This command returns RETCODE_SUCCESS.

				 ====== DEC_SET_FRAME_DELAY
				 HOST can set the number of frame to be delayed before display (H.264/AVC only) by using this command.
				 This command is useful when display frame buffer delay is supposed to happen for buffering decoded picture reorder and
				 HOST is sure of that.
				 Unless this command is executed, VPU has display frame buffer delay as frameBufDelay value of DecInitialInfo structure.

				 ====== DEC_FREE_FRAME_BUFFER
				 HOST can free all the frame buffers allocated by VPUAPI.
				 This command is useful when VPU detects sequence change.
				 For example, if HOST knows resolution change while decoding through `sequenceChanged` variable of DecOutputInfo structure,
			 HOST should change the size of frame buffer accordingly.
				 This command is used to release the frame buffers allocated for the previous sequence.
				 Then VPU_DecGetInitialInfo() and VPU_DecIsseuSeqInit() are called before frame buffer allocation for a new sequence.

				 ====== DEC_SET_DISPLAY_FLAG
				 Applications can set a display flag for each frame buffer by calling this function after creating
				 decoder instance. If a certain display flag of frame buffer is set, the frame buffer cannot be used in the
				 decoding process. Applications can control displaying a buffer with this command
				 to prevent VPU from using buffer in every decoding process.

				 This command is the opposite of what VPU_DecClrDispFlag() does.
				 All of buffer flags are initialized with 0x00000000 which means being able to decode.
				 Unless it is called, VPU starts decoding with available frame buffer that has been cleared
				 in round robin order.

				 ====== DEC_GET_SEQ_INFO
				 This command returns the information of the current sequence in the form of <<vpuapi_h_DecInitialInfo>> structure.
				 HOST can get this information with more accuracy after VPU_DecIssueSeqInit() or VPU_DecGetOutputInfo() is called.

				 ====== ENABLE_LOGGING
				 HOST can activate message logging once VPU_DecOpen() or VPU_EncOpen() is called.

				 ====== DISABLE_LOGGING
				 HOST can deactivate message logging which is off as default.

				 ====== DEC_GET_FIELD_PIC_TYPE
				 This command gets a field picture type of decoded picture after INT_BIT_DEC_FIELD interrupt is issued.

				 ====== DEC_ENABLE_REORDER
				 HOST can enable display buffer reordering when decoding H.264 streams. In H.264 case
				 output decoded picture may be re-ordered if pic_order_cnt_type is 0 or 1. In that case, decoder
				 must delay output display for re-ordering but some applications (ex. video telephony) do not
				 want such display delay.

				 ====== DEC_DISABLE_REORDER
				 HOST can disable output display buffer reorder-ing. Then BIT processor does not re-order output buffer when pic_order_cnt_type is 0 or 1. If
				 In H.264/AVC case. pic_order_cnt_type is 2 or the other standard case, this flag is ignored because output display
				 buffer reordering is not allowed.

				 ====== DEC_GET_FRAMEBUF_INFO
				 This command gives HOST the information of framebuffer in the form of DecGetFramebufInfo structure.

				 ====== DEC_RESET_FRAMEBUF_INFO
				 This command resets the information of framebuffer.
				 Unlike DEC_FREE_FRAME_BUFFER, it does not release the assigned memory itself.
				 This command is used for sequence change along with DEC_GET_FRAMEBUF_INFO.

				 ====== ENABLE_REP_CUDATA
				 This command enables to report CU data.

				 ====== DISABLE_REP_CUDATA
				 This command disables to report CU data.

				 ====== SET_ADDR_REP_CUDATA
				 This command sets the address of buffer where CU data is written.

				 ====== SET_SIZE_REP_CUDATA
				 This command sets the size of buffer where CU data is written.

				 ====== DEC_SET_WTL_FRAME_FORMAT
				 This command sets FrameBufferFormat for WTL.

				 ====== DEC_SET_AVC_ERROR_CONCEAL_MODE
				 This command sets error conceal mode for H.264 decoder.
				 This command must be issued through VPU_DecGiveCommand() before calling VPU_DecGetInitialInfo() or VPU_DecIssueSeqInit().
				 In other words, error conceal mode cannot be applied once a sequence initialized.

				 @* AVC_ERROR_CONCEAL_MODE_DEFAULT - VPU performs error concealment in default mode.
				 @* AVC_ERROR_CONCEAL_MODE_ENABLE_SELECTIVE_CONCEAL_MISSING_REFERENCE -
				 VPU performs error concealment using another framebuffer if the error comes from missing reference frame.
				 @* AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_MISSING_REFERENCE -
				 VPU does not perform error concealment if the error comes from missing reference frame.
				 @* AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_WRONG_FRAME_NUM -
				 VPU does not perform error concealment if the error comes from wrong frame_num syntax.

				 ====== DEC_GET_SCALER_INFO
				 This command returns setting information to downscale an image such as enable, width, and height.

				 ====== DEC_SET_SCALER_INFO
				 This command sets information to downscale an image such as enable, width, and height.

				 ====== DEC_SET_TARGET_TEMPORAL_ID
				 This command decodes only a frame whose temporal id is equal to or less than the given target temporal id.

				 NOTE: It is only for H.265/HEVC decoder.

				 ====== DEC_SET_BWB_CUR_FRAME_IDX
				 This command specifies the index of linear frame buffer which needs to be changed to due to change of inter-frame resolution while decoding.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_SET_FBC_CUR_FRAME_IDX
				 This command specifies the index of FBC frame buffer which needs to be changed to due to change of inter-frame resolution while decoding.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_SET_INTER_RES_INFO_ON
				 This command informs inter-frame resolution has been changed while decoding. After this command issued,
			 VPU reallocates one frame buffer for the change.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_SET_INTER_RES_INFO_OFF
				 This command releases the flag informing inter-frame resolution change.
				 It should be issued after reallocation of one frame buffer is completed.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_FREE_FBC_TABLE_BUFFER
				 This command frees one FBC table to deal with inter-frame resolution change.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_FREE_MV_BUFFER
				 This command frees one MV buffer to deal with inter-frame resolution change.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_ALLOC_FBC_Y_TABLE_BUFFER
				 This command allocates one FBC luma table to deal with inter-frame resolution change.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_ALLOC_FBC_C_TABLE_BUFFER
				 This command allocates one FBC chroma table to deal with inter-frame resolution change.

				 NOTE: It is only for VP9 decoder.

				 ====== DEC_ALLOC_MV_BUFFER
				 This command allocates one MV buffer to deal with inter-frame resolution change.

				 NOTE: It is only for VP9 decoder.

				 @endverbatim
				 */
				 RetCode WAVE5_DecGiveCommand(
											  DecHandle       handle,     /**< [Input] A decoder handle obtained from VPU_DecOpen() */
											  CodecCommand    cmd,        /**< [Input] A variable specifying the given command of CodecComand type */
											  void*           parameter   /**< [Input/Output] A pointer to command-specific data structure which describes picture I/O parameters for the current decoder instance */
											 );

	int checkWave5CodecInstanceAddr(void *ptr);
	int checkWave5CodecInstanceUse(void *ptr);
	int checkWave5CodecInstancePended(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
