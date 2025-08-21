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
// Description  : This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------

#ifndef VPUAPI_H_INCLUDED
#define VPUAPI_H_INCLUDED

#include "vpuconfig.h"
#include "vputypes.h"
#include "vpuerror.h"

typedef struct vpu_buffer_t {
    int size;
    unsigned long phys_addr;
    unsigned long base;
    unsigned long virt_addr;
} vpu_buffer_t;

typedef enum {
    VDI_LITTLE_ENDIAN = 0,      /* 64bit LE */
    VDI_BIG_ENDIAN,             /* 64bit BE */
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
    VDI_128BIT_BIG_ENDIAN        = 31,
    VDI_ENDIAN_MAX
} EndianMode;

#define VDI_128BIT_ENDIAN_MASK      0xf

#define MAX_GDI_IDX                             31

#ifdef SUPPORT_SRC_BUF_CONTROL
    #define MAX_REG_FRAME                           2000
#else
    #define MAX_REG_FRAME                           MAX_GDI_IDX*2 // 2 for WTL
#endif


void wave420l_VpuWriteReg(int coreIdx, unsigned int ADDR, unsigned int DATA);
unsigned int wave420l_VpuReadReg(int coreIdx, unsigned int ADDR);

#define VpuWriteReg(CORE, ADDR, DATA) wave420l_VpuWriteReg(CORE, ADDR, DATA)	// system register write
#define VpuReadReg(CORE, ADDR)        wave420l_VpuReadReg(CORE, ADDR)   	// system register read
#if !defined(VpuWriteMem)
	#define VpuWriteMem(CORE, ADDR, DATA, LEN, ENDIAN)	// system memory write
	#define VpuReadMem(CORE, ADDR, DATA, LEN, ENDIAN)	// system memory read
#endif

#define WAVE4_FBC_LUMA_TABLE_SIZE(_w, _h)       (((_h+15)/16)*((_w+255)/256)*128)
#define WAVE4_FBC_CHROMA_TABLE_SIZE(_w, _h)     (((_h+15)/16)*((_w/2+255)/256)*128)
#define WAVE4_DEC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (((_w+63)/64)*((_h+63)/64)*256+ 64)
#define WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (((((_w+127)/128)*128)/64)*((_h+63)/64)*64) //HEVCENC_REPORT_MVINFO
//#define WAVE4_ENC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (((_w+63)/64)*((_h+63)/64)*128)          // encoder case = (((_w+63)/64)*((_h+63)/64)128).

#define WAVE5_ENC_HEVC_MVCOL_BUF_SIZE(_w, _h)   (VPU_ALIGN64(_w)/64*VPU_ALIGN64(_h)/64*128)
#define WAVE5_FBC_LUMA_TABLE_SIZE(_w, _h)       (VPU_ALIGN16(_h)*VPU_ALIGN256(_w)/32)
#define WAVE5_FBC_CHROMA_TABLE_SIZE(_w, _h)     (VPU_ALIGN16(_h)*VPU_ALIGN256(_w/2)/32)

//------------------------------------------------------------------------------
// common struct and definition
//------------------------------------------------------------------------------
/**
* @brief
@verbatim
This is an enumeration for declaring codec standard type variables. Currently,
VPU supports H.265/HEVC video standards.

@endverbatim
*/
typedef enum {
    WAVE420L_STD_AVC    = 0,
    WAVE420L_STD_VC1    = 1,
    WAVE420L_STD_MPEG2  = 2,
    WAVE420L_STD_MPEG4  = 3,
    WAVE420L_STD_H263   = 4,
    WAVE420L_STD_AVS    = 7,
    WAVE420L_STD_VP8    = 11,
    WAVE420L_STD_HEVC   = 12,
    WAVE420L_STD_VP9    = 13,
    WAVE420L_STD_AVS2,
    WAVE420L_STD_MAX
} CodStd;

/**
* @brief
@verbatim
This is an enumeration for declaring SET_PARAM command options.
Depending on this, SET_PARAM command parameter registers have different settings.

NOTE: This is only for WAVE encoder IP.

@endverbatim
*/
typedef enum {
    OPT_COMMON          = 0, /**< SET_PARAM command option for encoding sequence */
    OPT_CUSTOM_GOP      = 1, /**< SET_PARAM command option for setting custom GOP */
    OPT_CUSTOM_HEADER   = 2, /**< SET_PARAM command option for setting custom VPS/SPS/PPS */
    OPT_VUI             = 3, /**< SET_PARAM command option for encoding VUI  */
    OPT_CHANGE_PARAM    = 16 /**< SET_PARAM command option for parameters change (WAVE520 only)  */
} SET_PARAM_OPTION;

/**
* @brief
@verbatim
This is an enumeration for declaring SET_PARAM command options. (CODA7Q encoder only)
Depending on this, SET_PARAM command parameter registers have different settings.

@endverbatim
*/
typedef enum {
    C7_OPT_COMMON       = 16,
    C7_OPT_SET_PARAM    = 17,
    C7_OPT_PARAM_CHANGE = 18,
} C7_SET_PARAM_OPTION;

/**
* @brief
@verbatim
This is an enumeration for declaring the operation mode of DEC_PIC_HDR command. (WAVE decoder only)

@endverbatim
*/
typedef enum {
    INIT_SEQ_NORMAL      = 0x01,  /**< It initializes some parameters (i.e. buffer mode) required for decoding sequence, performs sequence header, and returns information on the sequence. */
    INIT_SEQ_W_THUMBNAIL = 0x11, /**< It decodes only the first I picture of sequence to get thumbnail. */
} DEC_PIC_HDR_OPTION;

/**
* @brief
@verbatim
This is an enumeration for declaring the running option of DEC_PIC command. (WAVE decoder only)

@endverbatim
*/
typedef enum {
    DEC_PIC_NORMAL      = 0x00, /**< It is normal mode of DEC_PIC command. */
    DEC_PIC_W_THUMBNAIL = 0x10, /**< It handles CRA picture as BLA picture not to use reference from the previously decoded pictures. */
    SKIP_NON_IRAP       = 0x11, /**< It is thumbnail mode (skip non-IRAP without reference reg.) */
    SKIP_NON_RECOVERY   = 0x12, /**< It skips to decode non-IRAP pictures. */
    SKIP_NON_REF_PIC    = 0x13, /**< It skips to decode non-reference pictures which correspond to sub-layer non-reference picture with MAX_DEC_TEMP_ID. (The sub-layer non-reference picture is the one whose nal_unit_type equal to TRAIL_N, TSA_N, STSA_N, RADL_N, RASL_N, RSV_VCL_N10, RSV_VCL_N12, or RSV_VCL_N14. )*/
    SKIP_TEMPORAL_LAYER = 0x14, /**< It decodes only frames whose temporal id is equal to or less than MAX_DEC_TEMP_ID. */
} DEC_PIC_OPTION;

/************************************************************************/
/* Limitations                                                          */
/************************************************************************/
#define MAX_FRAMEBUFFER_COUNT               31  /* The 32nd framebuffer is reserved for WTL */

/************************************************************************/
/* PROFILE & LEVEL                                                      */
/************************************************************************/
/* HEVC */
#define HEVC_PROFILE_MAIN                   1
#define HEVC_PROFILE_MAIN10                 2
#define HEVC_PROFILE_STILLPICTURE           3

/* Tier */
#define HEVC_TIER_MAIN                      0
#define HEVC_TIER_HIGH                      1

/* Level */
#define HEVC_LEVEL(_Major, _Minor)          (_Major*10+_Minor)

/* H265 USER_DATA(SPS & SEI) ENABLE FLAG */
#define H265_USERDATA_FLAG_RESERVED_0           (0)
#define H265_USERDATA_FLAG_RESERVED_1           (1)
#define H265_USERDATA_FLAG_VUI                  (2)
#define H265_USERDATA_FLAG_RESERVED_3           (3)
#define H265_USERDATA_FLAG_PIC_TIMING           (4)
#define H265_USERDATA_FLAG_ITU_T_T35_PRE        (5)  /* SEI Prefix: user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_UNREGISTERED_PRE     (6)  /* SEI Prefix: user_data_unregistered */
#define H265_USERDATA_FLAG_ITU_T_T35_SUF        (7)  /* SEI Suffix: user_data_registered_itu_t_t35 */
#define H265_USERDATA_FLAG_UNREGISTERED_SUF     (8)  /* SEI Suffix: user_data_unregistered */
#define H265_USERDATA_FLAG_RESERVED_9           (9)  /* SEI RESERVED */
#define H265_USERDATA_FLAG_MASTERING_COLOR_VOL  (10) /* SEI Prefix: mastering_display_color_volume */
#define H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT  (11) /* SEI Prefix: chroma_resampling_filter_hint */
#define H265_USERDATA_FLAG_KNEE_FUNCTION_INFO   (12) /* SEI Prefix: knee_function_info */

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
    WAVE420L_RETCODE_SUCCESS,                    /**< This means that operation was done successfully.  */  /* 0  */
    WAVE420L_RETCODE_FAILURE,                    /**< This means that operation was not done successfully. When un-recoverable decoder error happens such as header parsing errors, this value is returned from VPU API.  */
    WAVE420L_RETCODE_INVALID_HANDLE,             /**< This means that the given handle for the current API function call was invalid (for example, not initialized yet, improper function call for the given handle, etc.).  */
    WAVE420L_RETCODE_INVALID_PARAM,              /**< This means that the given argument parameters (for example, input data structure) was invalid (not initialized yet or not valid anymore). */
    WAVE420L_RETCODE_INVALID_COMMAND,            /**< This means that the given command was invalid (for example, undefined, or not allowed in the given instances).  */
    WAVE420L_RETCODE_ROTATOR_OUTPUT_NOT_SET,     /**< This means that rotator output buffer was not allocated even though postprocessor (rotation, mirroring, or deringing) is enabled. */  /* 5  */
    WAVE420L_RETCODE_ROTATOR_STRIDE_NOT_SET,     /**< This means that rotator stride was not provided even though postprocessor (rotation, mirroring, or deringing) is enabled.  */
    WAVE420L_RETCODE_FRAME_NOT_COMPLETE,         /**< This means that frame decoding operation was not completed yet, so the given API function call cannot be allowed.  */
    WAVE420L_RETCODE_INVALID_FRAME_BUFFER,       /**< This means that the given source frame buffer pointers were invalid in encoder (not initialized yet or not valid anymore).  */
    WAVE420L_RETCODE_INSUFFICIENT_FRAME_BUFFERS, /**< This means that the given numbers of frame buffers were not enough for the operations of the given handle. This return code is only received when calling wave420l_VPU_EncRegisterFrameBuffer() or wave420l_VPU_EncRegisterFrameBuffer() function. */
    WAVE420L_RETCODE_INVALID_STRIDE,             /**< This means that the given stride was invalid (for example, 0, not a multiple of 8 or smaller than picture size). This return code is only allowed in API functions which set stride.  */   /* 10 */
    WAVE420L_RETCODE_WRONG_CALL_SEQUENCE,        /**< This means that the current API function call was invalid considering the allowed sequences between API functions (for example, missing one crucial function call before this function call).  */
    WAVE420L_RETCODE_CALLED_BEFORE,              /**< This means that multiple calls of the current API function for a given instance are invalid. */
    WAVE420L_RETCODE_NOT_INITIALIZED,            /**< This means that VPU was not initialized yet. Before calling any API functions, the initialization API function, VPU_Init(), should be called at the beginning.  */
    WAVE420L_RETCODE_USERDATA_BUF_NOT_SET,       /**< This means that there is no memory allocation for reporting userdata. Before setting user data enable, user data buffer address and size should be set with valid value. */
    RETCODE_MEMORY_ACCESS_VIOLATION = 115,    /**< This means that access violation to the protected memory has been occurred. */   /* 15 */
    RETCODE_VPU_RESPONSE_TIMEOUT,       /**< This means that VPU response time is too long, time out. */
    RETCODE_INSUFFICIENT_RESOURCE,      /**< This means that VPU cannot allocate memory due to lack of memory. */
    RETCODE_NOT_FOUND_BITCODE_PATH,     /**< This means that BIT_CODE_FILE_PATH has a wrong firmware path or firmware size is 0 when calling VPU_InitWithBitcode() function.  */
    RETCODE_NOT_SUPPORTED_FEATURE,      /**< This means that HOST application uses an API option that is not supported in current hardware.  */
    RETCODE_NOT_FOUND_VPU_DEVICE,       /**< This means that HOST application uses the undefined product ID. */   /* 20 */
    RETCODE_CP0_EXCEPTION,              /**< This means that coprocessor exception has occurred. (WAVE only) */
    RETCODE_STREAM_BUF_FULL,            /**< This means that stream buffer is full in encoder. */
#if 0
    RETCODE_ACCESS_VIOLATION_HW,        /**< This means that GDI access error has occurred. It might come from violation of write protection region or spec-out GDI read/write request. (WAVE only) */
    RETCODE_QUERY_FAILURE,              /**< This means that query command was not successful (WAVE5 only) */
    RETCODE_QUEUEING_FAILURE,           /**< This means that commands cannot be queued (WAVE5 only) */
    RETCODE_VPU_STILL_RUNNING,          /**< This means that VPU cannot be flushed or closed now. because VPU is running (WAVE5 only) */
    RETCODE_REPORT_NOT_READY,           /**< This means that report is not ready for Query(GET_RESULT) command (WAVE5 only) */
#endif
} RetCode;

/************************************************************************/
/* Utility macros                                                       */
/************************************************************************/
#define VPU_ALIGN4(_x)              (((_x)+0x03)&~0x03)
#define VPU_ALIGN8(_x)              (((_x)+0x07)&~0x07)
#define VPU_ALIGN16(_x)             (((_x)+0x0f)&~0x0f)
#define VPU_ALIGN32(_x)             (((_x)+0x1f)&~0x1f)
#define VPU_ALIGN64(_x)             (((_x)+0x3f)&~0x3f)
#define VPU_ALIGN128(_x)            (((_x)+0x7f)&~0x7f)
#define VPU_ALIGN256(_x)            (((_x)+0xff)&~0xff)
#define VPU_ALIGN512(_x)            (((_x)+0x1ff)&~0x1ff)
#define VPU_ALIGN4096(_x)           (((_x)+0xfff)&~0xfff)
#define VPU_ALIGN16384(_x)          (((_x)+0x3fff)&~0x3fff)
#define VPU_ALIGN(val, align_size)	(((val) + ((align_size) - 1)) & (~((align_size) - 1)))

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

#define DISPLAY_IDX_FLAG_SEQ_END    -1
#define DISPLAY_IDX_FLAG_NO_FB      -3
#define DECODED_IDX_FLAG_NO_FB      -1
#define DECODED_IDX_FLAG_SKIP       -2

#define MAX_ROI_NUMBER  64

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
    SET_MIRROR_DIRECTION,           //HEVC enc doesn't used this Command
    SET_ROTATION_ANGLE,             //HEVC enc doesn't used this Command
    SET_ROTATOR_OUTPUT,             //HEVC enc doesn't used this Command
    SET_ROTATOR_STRIDE,             //HEVC enc doesn't used this Command
    DEC_GET_SEQ_INFO,               //HEVC enc doesn't used this Command
    DEC_SET_SPS_RBSP,               //HEVC enc doesn't used this Command
    DEC_SET_PPS_RBSP,               //HEVC enc doesn't used this Command
    DEC_SET_SEQ_CHANGE_MASK,        //HEVC enc doesn't used this Command
    ENABLE_DERING,                  //HEVC enc doesn't used this Command
    DISABLE_DERING,

    SET_SEC_AXI,
    SET_DRAM_CONFIG,    //coda960 only
    GET_DRAM_CONFIG,    //coda960 only

    ENABLE_REP_USERDATA,
    DISABLE_REP_USERDATA,
    SET_ADDR_REP_USERDATA,
    SET_VIRT_ADDR_REP_USERDATA,
    SET_SIZE_REP_USERDATA,
    SET_USERDATA_REPORT_MODE,

    // WAVE4 CU REPORT INFTERFACE
    ENABLE_REP_CUDATA,
    DISABLE_REP_CUDATA,
    SET_ADDR_REP_CUDATA,
    SET_SIZE_REP_CUDATA,
    SET_CACHE_CONFIG,
    GET_TILEDMAP_CONFIG,
    SET_LOW_DELAY_CONFIG,
    GET_LOW_DELAY_OUTPUT,

    SET_DECODE_FLUSH,                   //HEVC enc doesn't used this Command
    DEC_SET_FRAME_DELAY,                //HEVC enc doesn't used this Command
    DEC_SET_WTL_FRAME_FORMAT,           //HEVC enc doesn't used this Command
    DEC_GET_FIELD_PIC_TYPE,             //HEVC enc doesn't used this Command
    DEC_GET_DISPLAY_OUTPUT_INFO,        //HEVC enc doesn't used this Command
    DEC_ENABLE_REORDER,                 //HEVC enc doesn't used this Command
    DEC_DISABLE_REORDER,                //HEVC enc doesn't used this Command
    DEC_SET_AVC_ERROR_CONCEAL_MODE,     //HEVC enc doesn't used this Command
    DEC_FREE_FRAME_BUFFER,              //HEVC enc doesn't used this Command
    DEC_GET_FRAMEBUF_INFO,                 //HEVC enc doesn't used this Command
    DEC_RESET_FRAMEBUF_INFO,
    ENABLE_DEC_THUMBNAIL_MODE,
    DEC_SET_DISPLAY_FLAG,
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

    //vpu put header stream to bitstream buffer
    ENC_PUT_VIDEO_HEADER,
    ENC_PUT_MP4_HEADER,             //HEVC enc doesn't used this Command
    ENC_PUT_AVC_HEADER,             //HEVC enc doesn't used this Command

    //host generate header bitstream
    ENC_GET_VIDEO_HEADER,           //HEVC enc doesn't used this Comman
    ENC_SET_INTRA_MB_REFRESH_NUMBER,
    ENC_ENABLE_HEC,                 //HEVC enc doesn't used this Command
    ENC_DISABLE_HEC,                //HEVC enc doesn't used this Command
    ENC_SET_SLICE_INFO,
    ENC_SET_GOP_NUMBER,             //HEVC enc doesn't used this Command
    ENC_SET_INTRA_QP,               //HEVC enc doesn't used this Command
    ENC_SET_BITRATE,                //HEVC enc doesn't used this Command
    ENC_SET_FRAME_RATE,             //HEVC enc doesn't used this Command

    ENC_SET_PARA_CHANGE,
    ENABLE_LOGGING,

    DISABLE_LOGGING,
    ENC_SET_SEI_NAL_DATA,
    DEC_GET_QUEUE_STATUS,           //HEVC enc doesn't used this Command
    ENC_GET_BW_REPORT,      // wave520 only
    CMD_END
} CodecCommand;

/**
 * @brief   This is an enumeration type for representing the way of writing chroma data in planar format of frame buffer.
 */
 typedef enum {
    CBCR_ORDER_NORMAL,  /**< Cb data are written in Cb buffer, and Cr data are written in Cr buffer. */
    CBCR_ORDER_REVERSED /**< Cr data are written in Cb buffer, and Cb data are written in Cr buffer. */
} CbCrOrder;

/**
* @brief    This is an enumeration type for representing the mirroring direction.
*/
 typedef enum {
    MIRDIR_NONE,
    MIRDIR_VER,
    MIRDIR_HOR,
    MIRDIR_HOR_VER
} MirrorDirection;

/**
 * @brief   This is an enumeration type for representing chroma formats of the frame buffer and pixel formats in packed mode.
 */
 typedef enum {
    FORMAT_420             = 0  ,      /* 8bit */
    FORMAT_422               ,         /* 8bit */
    FORMAT_224               ,         /* 8bit */
    FORMAT_444               ,         /* 8bit */
    FORMAT_400               ,         /* 8bit */

                                       /* Little Endian Perspective     */
                                       /*     | addr 0  | addr 1  |     */
    FORMAT_420_P10_16BIT_MSB = 5 ,     /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_420_P10_16BIT_LSB ,         /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_420_P10_32BIT_MSB ,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_420_P10_32BIT_LSB ,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

                                       /* 4:2:2 packed format */
                                       /* Little Endian Perspective     */
                                       /*     | addr 0  | addr 1  |     */
    FORMAT_422_P10_16BIT_MSB ,         /* lsb | 00xxxxx |xxxxxxxx | msb */
    FORMAT_422_P10_16BIT_LSB ,         /* lsb | xxxxxxx |xxxxxx00 | msb */
    FORMAT_422_P10_32BIT_MSB ,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */
    FORMAT_422_P10_32BIT_LSB ,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */

    FORMAT_YUYV              ,         /**< 8bit packed format : Y0U0Y1V0 Y2U1Y3V1 ... */
    FORMAT_YUYV_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YUYV) format(1Pixel=2Byte) */
    FORMAT_YUYV_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */
    FORMAT_YUYV_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YUYV) format(3Pixel=4Byte) */

    FORMAT_YVYU              ,         /**< 8bit packed format : Y0V0Y1U0 Y2V1Y3U1 ... */
    FORMAT_YVYU_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(YVYU) format(1Pixel=2Byte) */
    FORMAT_YVYU_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */
    FORMAT_YVYU_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(YVYU) format(3Pixel=4Byte) */

    FORMAT_UYVY              ,         /**< 8bit packed format : U0Y0V0Y1 U1Y2V1Y3 ... */
    FORMAT_UYVY_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_16BIT_LSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(UYVY) format(1Pixel=2Byte) */
    FORMAT_UYVY_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */
    FORMAT_UYVY_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(UYVY) format(3Pixel=4Byte) */

    FORMAT_VYUY              ,         /**< 8bit packed format : V0Y0U0Y1 V1Y2U1Y3 ... */
    FORMAT_VYUY_P10_16BIT_MSB,         /* lsb | 000000xxxxxxxxxx | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_16BIT_LSB,         /* lsb | xxxxxxxxxx000000 | msb */ /**< 10bit packed(VYUY) format(1Pixel=2Byte) */
    FORMAT_VYUY_P10_32BIT_MSB,         /* lsb |00xxxxxxxxxxxxxxxxxxxxxxxxxxx| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */
    FORMAT_VYUY_P10_32BIT_LSB,         /* lsb |xxxxxxxxxxxxxxxxxxxxxxxxxxx00| msb */ /**< 10bit packed(VYUY) format(3Pixel=4Byte) */

    FORMAT_MAX,
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
} ScalerImageFormat;

typedef enum {
    NOT_PACKED = 0,
    PACKED_YUYV,
    PACKED_YVYU,
    PACKED_UYVY,
    PACKED_VYUY,
} PackedFormatNum;

/**
* @brief    This is an enumeration type for representing interrupt bit positions for CODA series.
*/
typedef enum {
    INT_BIT_INIT            = 0,
    INT_BIT_SEQ_INIT        = 1,
    INT_BIT_SEQ_END         = 2,
    INT_BIT_PIC_RUN         = 3,
    INT_BIT_FRAMEBUF_SET    = 4,
    INT_BIT_ENC_HEADER      = 5,
    INT_BIT_DEC_PARA_SET    = 7,
    INT_BIT_DEC_BUF_FLUSH   = 8,
    INT_BIT_USERDATA        = 9,
    INT_BIT_DEC_FIELD       = 10,
    INT_BIT_DEC_MB_ROWS     = 13,
    INT_BIT_BIT_BUF_EMPTY   = 14,
    INT_BIT_BIT_BUF_FULL    = 15
} InterruptBit;

/* For backward compatibility */
typedef InterruptBit Coda9InterruptBit;

/**
* @brief    This is an enumeration type for representing interrupt bit positions.

NOTE: This is only for WAVE4 IP.
*/
typedef enum {
    INT_WAVE_INIT           = 0,
    INT_WAVE_DEC_PIC_HDR    = 1,
    INT_WAVE_ENC_SETPARAM   = 2,
    INT_WAVE_FINI_SEQ       = 2,
    INT_WAVE_DEC_PIC        = 3,
    INT_WAVE_ENC_PIC        = 3,
    INT_WAVE_SET_FRAMEBUF   = 4,
    INT_WAVE_FLUSH_DECODER  = 5,
    INT_WAVE_GET_FW_VERSION = 8,
    INT_WAVE_QUERY_DECODER  = 9,
    INT_WAVE_SLEEP_VPU      = 10,
    INT_WAVE_WAKEUP_VPU     = 11,
    INT_WAVE_CHANGE_INST    = 12,
    INT_WAVE_CREATE_INSTANCE= 14,
    INT_WAVE_BIT_BUF_EMPTY  = 15,   /* Decoder */
    INT_WAVE_BIT_BUF_FULL   = 15,   /* Encoder */
} Wave4InterruptBit;

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
    INT_WAVE5_ENC_SET_PARAM     = 9,
    INT_WAVE5_DEC_QUERY         = 14,
    INT_WAVE5_BSBUF_EMPTY       = 15,
    INT_WAVE5_BSBUF_FULL        = 15,
} Wave5InterruptBit;

/**
* @brief    This is an enumeration type for representing picture types.
*/
typedef enum {
    WAVE420L_PIC_TYPE_I            = 0, /**< I picture */
    WAVE420L_PIC_TYPE_P            = 1, /**< P picture */
    WAVE420L_PIC_TYPE_B            = 2, /**< B picture (except VC1) */
    PIC_TYPE_REPEAT       = 2, /**< Repeat frame (VP9 only) */
    PIC_TYPE_VC1_BI       = 2, /**< VC1 BI picture (VC1 only) */
    PIC_TYPE_VC1_B        = 3, /**< VC1 B picture (VC1 only) */
    PIC_TYPE_D            = 3, /**< D picture in MPEG2 that is only composed of DC coefficients (MPEG2 only) */
    PIC_TYPE_S            = 3, /**< S picture in MPEG4 that is an acronym of Sprite and used for GMC (MPEG4 only)*/
    PIC_TYPE_VC1_P_SKIP   = 4, /**< VC1 P skip picture (VC1 only) */
    PIC_TYPE_MP4_P_SKIP_NOT_CODED = 4, /**< Not Coded P Picture in mpeg4 packed mode */
    WAVE420L_PIC_TYPE_IDR          = 5, /**< H.264/H.265 IDR picture */
    PIC_TYPE_MAX               /**< No Meaning */
} PicType;

/**
* @brief    This is an enumeration type for specifying frame buffer types when tiled2linear or wtlEnable is used.
*/
typedef enum {
    FF_NONE                 = 0, /**< Frame buffer type when tiled2linear or wtlEnable is disabled */
    FF_FRAME                = 1, /**< Frame buffer type to store one frame */
    FF_FIELD                = 2, /**< Frame buffer type to store top field or bottom field separately */
}FrameFlag;

/**
 * @brief   This is an enumeration type for representing bitstream handling modes in decoder.
 */
typedef enum {
    BS_MODE_INTERRUPT,  /**< VPU returns an interrupt when bitstream buffer is empty while decoding. VPU waits for more bitstream to be filled. */
    BS_MODE_RESERVED,   /**< Reserved for the future */
    BS_MODE_PIC_END,    /**< VPU tries to decode with very small amount of bitstream (not a complete 512-byte chunk). If it is not successful, VPU performs error concealment for the rest of the frame. */
}BitStreamMode;

/**
 * @brief  This is an enumeration type for representing software reset options.
 */
typedef enum {
    SW_RESET_SAFETY,    /**< It resets VPU in safe way. It waits until pending bus transaction is completed and then perform reset. */
    SW_RESET_FORCE,     /**< It forces to reset VPU without waiting pending bus transaction to be completed. It is used for immediate termination such as system off. */
    SW_RESET_ON_BOOT    /**< This is the default reset mode that is executed since system booting.  This mode is actually executed in VPU_Init(), so does not have to be used independently. */
}SWResetMode;

/**
 * @brief  This is an enumeration type for representing product IDs.
 */
typedef enum {
    PRODUCT_ID_420 = 6,
    PRODUCT_ID_420L = 9,
    PRODUCT_ID_NONE,
}ProductId;

#define PRODUCT_ID_W_SERIES(x)      (x == PRODUCT_ID_420L)
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
    COMPRESSED_FRAME_MAP        = 10, /**< Compressed frame map type (WAVE only) */ // WAVE4 only
    TILED_MAP_TYPE_MAX
} TiledMapType;

/**
* @brief    This is an enumeration for declaring a type of framebuffer that is allocated when VPU_DecAllocateFrameBuffer()
and VPU_EncAllocateFrameBuffer() function call.
*/
typedef enum {
    FB_TYPE_CODEC,  /**< A framebuffer type used for decoding or encoding  */
    FB_TYPE_PPU,    /**< A framebuffer type used for additional allocation of framebuffer for postprocessing(rotation/mirror) or display (tiled2linear) purpose */
} FramebufferAllocType;

/**
* @brief    This is data structure of product information. (WAVE only)
*/
typedef struct {
    Uint32 productId;  /**< The product id */    //W4_RET_PRODUCT_ID
    Uint32 fwVersion;  /**< The firmware version */    //W4_RET_FW_VERSION
    Uint32 productName;  /**< VPU hardware product name  */    //W4_RET_PRODUCT_NAME
    Uint32 productVersion;  /**< VPU hardware product version */    //W4_RET_PRODUCT_VERSION
    Uint32 customerId;  /**< The customer id */    //W4_RET_CUSTOMER_ID
    Uint32 stdDef0;  /**< The system configuration information  */    //W4_RET_STD_DEF0
    Uint32 stdDef1;  /**< The hardware configuration information  */    //W4_RET_STD_DEF1
    Uint32 confFeature;  /**< The supported codec standard */    //W4_RET_CONF_FEATURE
    Uint32 configDate;  /**< The date that the hardware has been configured in YYYYmmdd in digit */    //W4_RET_CONFIG_DATE
    Uint32 configRevision;  /**< The revision number when the hardware has been configured */    //W4_RET_CONFIG_REVISION
    Uint32 configType;  /**< The define value used in hardware configuration */    //W4_RET_CONFIG_TYPE
    Uint32 configVcore[4];  /**< VCORE Configuration Information */    //W4_RET_CONF_VCORE0
}ProductInfo;

/**
* @brief
@verbatim
This is a data structure of tiled map information.

NOTE: WAVE4 does not support tiledmap type so this structure is not used in the product.
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
    unsigned long tiledBaseAddr;

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

* 0 : CbCr data is written in their separate frame memory (chroma separate mode).
* 1 : CbCr data is interleaved in chroma memory (chroma interleave mode).
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
    int srcBufState;    /**< It indicates a status of each source buffer, whether the source buffer is used for encoding or not. */
    int sequenceNo;     /**< A sequence number that the frame belongs to. It increases by 1 every time a sequence changes in decoder.  */
    int packedEnable;   /**< It enables frames to be saved in YUV422 packed mode. */
/**
@* 00 : YUYV
@* 01 : UYVY
@* 10 : YVYU
@* 11 : VYUY
*/
    int packedMode;
    BOOL updateFbInfo; /**< If this is TRUE, VPU updates API-internal framebuffer information when any of the information is changed. */
} FrameBuffer;

/**
* @brief    This is a data structure for representing framebuffer parameters.
It is used when framebuffer allocation using VPU_DecAllocateFrameBuffer() or VPU_EncAllocateFrameBuffer().
*/
typedef struct {
    int mapType;                /**< <<vpuapi_h_TiledMapType>> */
    int cbcrInterleave;         /**< CbCr interleave mode of frame buffer  */
    int nv21;                   /**< 1 : CrCb (NV21) , 0 : CbCr (YV12NV12).  This is valid when cbcrInterleave is 1. */
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
This is a data structure for representing rectangular window information in a
frame.

In order to specify a display window (or display window after cropping), this structure is
provided to HOST application. Each value means an offset from the start point of
a frame and therefore, all variables have positive values.
*/
typedef struct {
    Uint32 left;    /**< A horizontal pixel offset of top-left corner of rectangle from (0, 0) */
    Uint32 top;     /**< A vertical pixel offset of top-left corner of rectangle from (0, 0) */
    Uint32 right;   /**< A horizontal pixel offset of bottom-right corner of rectangle from (0, 0) */
    Uint32 bottom;  /**< A vertical pixel offset of bottom-right corner of rectangle from (0, 0) */
} VpuRect;



/**
* @brief    This is a data structure for representing use of secondary AXI for each hardware block.
*/
typedef struct {
    union {
        struct {
            // for Encoder
            int useEncLfEnable;  /**< This enables AXI secondary channel for loopfilter. */
            int useEncRdoEnable; /**< This enables AXI secondary channel for RDO. */
        } wave4;
    } u;
} SecAxiUse;


struct CodecInst;

//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------

#define VPU_HANDLE_INSTANCE_NO(_handle)         (_handle->instIndex)
#define VPU_HANDLE_CORE_INDEX(_handle)          (((CodecInst*)_handle)->coreIdx)
#define VPU_HANDLE_PRODUCT_ID(_handle)          (((CodecInst*)_handle)->productId)

/**
* @brief
@verbatim
This is a dedicated type for handle returned when a decoder instance or a encoder instance is
opened.

@endverbatim
*/
typedef struct CodecInst* VpuHandle;   //EncHandle


#define FBC_MODE_BEST_PREDICTION    0x00        //!<< Best for bandwidth, some performance overhead
#define FBC_MODE_NORMAL_PREDICTION  0x0c        //!<< Good for badnwidth and performance
#define FBC_MODE_BASIC_PREDICTION   0x3c        //!<< Best for performance


// Report Information

/**
* @brief    The data structure for options of picture decoding.
*/
#define WAVE_SKIPMODE_WAVE_NONE 0
#define WAVE_SKIPMODE_NON_IRAP  1
#define WAVE_SKIPMODE_NON_REF   2

// HEVC specific Recovery Point information
/**
* @brief    This is a data structure for H.265/HEVC specific picture information. (H.265/HEVC decoder only)
VPU returns this structure after decoding a frame.
*/
typedef struct {
/**
@verbatim
This is a flag to indicate whether H.265/HEVC RP SEI exists or not.

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



//------------------------------------------------------------------------------
// encode struct and definition
//------------------------------------------------------------------------------



#define MAX_NUM_TEMPORAL_LAYER          7
#define MAX_GOP_NUM                     8

typedef struct CodecInst EncInst;

/**
* @brief
@verbatim
This is a dedicated type for encoder handle returned when an encoder instance is
opened. An encoder instance can be referred by the corresponding handle. EncInst
is a type managed internally by API. Application does not need to care about it.

NOTE: This type is vaild for encoder only.
@endverbatim
*/
typedef EncInst * EncHandle;

/**
* @brief    This is a data structure for custom GOP parameters of the given picture. (WAVE encoder only)
*/
typedef struct {
    int picType;                        /**< A picture type of Nth picture in the custom GOP */
    int pocOffset;                      /**< A POC of Nth picture in the custom GOP */
    int picQp;                          /**< A quantization parameter of Nth picture in the custom GOP */
    int numRefPicL0;
    int refPocL0;                       /**< A POC of reference L0 of Nth picture in the custom GOP */
    int refPocL1;                       /**< A POC of reference L1 of Nth picture in the custom GOP */
    int temporalId;                     /**< A temporal ID of Nth picture in the custom GOP */
} CustomGopPicParam;

/**
* @brief    This is a data structure for custom GOP parameters. (WAVE encoder only)
*/
typedef struct {
    int customGopSize;                  /**< The size of custom GOP (0~8) */
    int useDeriveLambdaWeight;           /**< It internally derives a lamda weight instead of using the given lamda weight. */
    CustomGopPicParam picParam[MAX_GOP_NUM];  /**< Picture parameters of Nth picture in custom GOP */
    int gopPicLambda[MAX_GOP_NUM];       /**< A lamda weight of Nth picture in custom GOP */
} CustomGopParam;


/**
* @brief    This is a data structure for setting CTU level options (ROI, CTU mode, CTU QP) in H.265/HEVC encoder (WAVE4 encoder only).
*/
typedef struct {
    int roiEnable;                      /**< It enables ROI map. NOTE: It is valid when rcEnable is on. */
    int roiDeltaQp;                     /**< It specifies a delta QP that is used to calculate ROI QPs internally. */
    int mapEndian;                      /**< It specifies endianness of ROI CTU map. For the specific modes, refer to the EndianMode of <<vpuapi_h_DecOpenParam>>. */
/**
@verbatim
It specifies the stride of CTU-level ROI/mode/QP map. It should be set with the value as below.

 (Width  + CTB_SIZE - 1) / CTB_SIZE
@endverbatim
*/
    int mapStride;
/**
@verbatim
The start buffer address of ROI map

ROI map holds importance levels for CTUs within a picture. The memory size is the number of CTUs of picture in bytes.
For example, if there are 64 CTUs within a picture, the size of ROI map is 64 bytes.
All CTUs have their ROI importance level (0 ~ 8 ; 1 byte)  in raster order.
A CTU with a high ROI importance level is encoded with a lower QP for higher quality.
It should be given when roiEnable is 1.
@endverbatim
*/
    PhysicalAddress addrRoiCtuMap;

} HevcCtuOptParam;

/**
* @brief    This is a data structure for setting VUI parameters in H.265/HEVC encoder. (WAVE only)
*/
typedef struct {
    Uint32 vuiParamFlags;               /**< It specifies vui_parameters_present_flag.  */
    Uint32 vuiAspectRatioIdc;           /**< It specifies aspect_ratio_idc. */
    Uint32 vuiSarSize;                  /**< It specifies sar_width and sar_height (only valid when aspect_ratio_idc is equal to 255). */
    Uint32 vuiOverScanAppropriate;      /**< It specifies overscan_appropriate_flag. */
    Uint32 videoSignal;                 /**< It specifies video_signal_type_present_flag. */
    Uint32 vuiChromaSampleLoc;          /**< It specifies chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field. */
    Uint32 vuiDispWinLeftRight;         /**< It specifies def_disp_win_left_offset and def_disp_win_right_offset. */
    Uint32 vuiDispWinTopBottom;         /**< It specifies def_disp_win_top_offset and def_disp_win_bottom_offset. */
} HevcVuiParam;

/**
* @brief    This is a data structure for setting SEI NALs in H.265/HEVC encoder.
*/
typedef struct {
    Uint32 prefixSeiNalEnable;  /**< It enables to encode the prefix SEI NAL which is given by HOST application. */
    Uint32 prefixSeiDataSize;   /**< The total byte size of the prefix SEI */
/**
@verbatim
A flag whether to encode PREFIX_SEI_DATA with a picture of this command or with a source picture of the buffer at the moment

@* 0 : encode PREFIX_SEI_DATA when the current input source picture is encoded.
@* 1 : encode PREFIX_SEI_DATA at this command.
@endverbatim
*/
    Uint32 prefixSeiDataEncOrder;
    PhysicalAddress prefixSeiNalAddr; /**< The start address of the total prefix SEI NALs to be encoded */

    Uint32 suffixSeiNalEnable;  /**< It enables to encode the suffix SEI NAL which is given by HOST application. */
    Uint32 suffixSeiDataSize;   /**< The total byte size of the suffix SEI */
/**
@verbatim
A flag whether to encode SUFFIX_SEI_DATA with a picture of this command or with a source picture of the buffer at the moment

@* 0 : encode SUFFIX_SEI_DATA when the current input source picture is encoded.
@* 1 : encode SUFFIX_SEI_DATA at this command.
@endverbatim
*/
    Uint32 suffixSeiDataEncOrder;
    PhysicalAddress suffixSeiNalAddr; /**< The start address of the total suffix SEI NALs to be encoded */
} HevcSEIDataEnc;

/**
* @brief    This is a data structure for H.265/HEVC encoder parameters.
*/
typedef struct {
/**
@verbatim
A profile indicator

@* 1 : main
@* 2 : main10
@endverbatim
*/
    int profile;
    int level;                          /**< A level indicator (level * 10) */
/**
@verbatim
A tier indicator

@* 0 : Main
@* 1 : High
@endverbatim
*/
    int tier;
/**
@verbatim
A bit-depth (8bit or 10bit) which VPU internally uses for encoding

VPU encodes with internalBitDepth instead of InputBitDepth.
For example, if InputBitDepth is 8 and InternalBitDepth is 10,
VPU converts the 8-bit source frames into 10-bit ones and then encodes them.

@endverbatim
*/
    int internalBitDepth;
    int chromaFormatIdc;                /**< A chroma format indicator (0 for 4:2:0) */
    int losslessEnable;                 /**< It enables lossless coding. */
    int constIntraPredFlag;             /**< It enables constrained intra prediction. */
/**
@verbatim
A GOP structure preset option

@* 0 : Custom GOP
@* Other values : <<vpuapi_h_GOP_PRESET_IDX>>
@endverbatim
*/
    int gopPresetIdx;
/**
@verbatim
The type of I picture to be inserted at every intraPeriod

@* 0 : Non-IRAP
@* 1 : CRA
@* 2 : IDR
@endverbatim
*/
    int decodingRefreshType;
    int intraQP;                        /**< A quantization parameter of intra picture */
    int intraPeriod;                    /**< A period of intra picture in GOP size */

    int confWinTop;                     /**< A top offset of conformance window */
    int confWinBot;                     /**< A bottom offset of conformance window */
    int confWinLeft;                    /**< A left offset of conformance window */
    int confWinRight;                   /**< A right offset of conformance window */

/**
@verbatim
A slice mode for independent slice

@* 0 : No multi-slice
@* 1 : Slice in CTU number
@endverbatim
*/
    int independSliceMode;
    int independSliceModeArg;           /**< The number of CTU for a slice when independSliceMode is set with 1  */

/**
@verbatim
A slice mode for dependent slice

@* 0 : No multi-slice
@* 1 : Slice in CTU number
@* 2 : Slice in number of byte
@endverbatim
*/
    int dependSliceMode;
    int dependSliceModeArg;             /**< The number of CTU or bytes for a slice when dependSliceMode is set with 1 or 2  */

/**
@verbatim
An intra refresh mode

@* 0 : No intra refresh
@* 1 : Row
@* 2 : Column
@* 3 : Step size in CTU
@endverbatim
*/
    int intraRefreshMode;
    int intraRefreshArg;                /**< The number of CTU (only valid when intraRefreshMode is 3.) */

/**
@verbatim
It uses one of the recommended encoder parameter presets.

@* 0 : Custom
@* 1 : Recommend encoder parameters (slow encoding speed, highest picture quality)
@* 2 : Boost mode (normal encoding speed, moderate picture quality)
@* 3 : Fast mode (fast encoding speed, low picture quality)
@endverbatim
*/
    int useRecommendEncParam;
    int scalingListEnable;              /**< It enables a scaling list. */
/**
@verbatim
It enables CU(Coding Unit) size to be used in encoding process. Host application can also select multiple CU sizes.

@* 3'b001 : 8x8
@* 3'b010 : 16x16
@* 3'b100 : 32x32
@endverbatim
*/
    int cuSizeMode;
    int tmvpEnable;                     /**< It enables temporal motion vector prediction. */
    int wppEnable;                      /**< It enables WPP (Wave-front Parallel Processing). WPP is unsupported in ring buffer mode of bitstream buffer. */
    int maxNumMerge;                    /**< Maximum number of merge candidates (0~2) */
    int dynamicMerge8x8Enable;          /**< It enables dynamic merge 8x8 candidates. */
    int dynamicMerge16x16Enable;        /**< It enables dynamic merge 16x16 candidates. */
    int dynamicMerge32x32Enable;        /**< It enables dynamic merge 32x32 candidates. */
    int disableDeblk;                   /**< It disables in-loop deblocking filtering. */
    int lfCrossSliceBoundaryEnable;     /**< It enables filtering across slice boundaries for in-loop deblocking. */
    int betaOffsetDiv2;                 /**< It enables BetaOffsetDiv2 for deblocking filter. */
    int tcOffsetDiv2;                   /**< It enables TcOffsetDiv2 for deblocking filter. */
    int skipIntraTrans;                 /**< It enables transform skip for an intra CU. */
    int saoEnable;                      /**< It enables SAO (Sample Adaptive Offset). */
    int intraInInterSliceEnable;        /**< It enables to make intra CUs in an inter slice. */
    int intraNxNEnable;                 /**< It enables intra NxN PUs. */

    int intraQpOffset;                  /**< It specifies an intra QP offset relative to an inter QP. It is only valid when RateControl is enabled. */
/**
@verbatim
It specifies encoder initial delay. It is only valid when RateControl is enabled.

 encoder initial delay = InitialDelay * InitBufLevelx8 / 8

@endverbatim
*/
    int initBufLevelx8;
/**
@verbatim
It specifies picture bits allocation mode.
It is only valid when RateControl is enabled and GOP size is larger than 1.

@* 0 : More referenced pictures have better quality than less referenced pictures.
@* 1 : All pictures in a GOP have similar image quality.
@* 2 : Each picture bits in a GOP is allocated according to FixedRatioN.
@endverbatim
*/
    int bitAllocMode;
/**
@verbatim
A fixed bit ratio (1 ~ 255) for each picture of GOP's bit
allocation

@* N = 0 ~ (MAX_GOP_SIZE - 1)
@* MAX_GOP_SIZE = 8

For instance when MAX_GOP_SIZE is 3, FixedBitRatio0, FixedBitRatio1, and FixedBitRatio2 can be set as 2, 1, and 1 repsectively for
the fixed bit ratio 2:1:1. This is only valid when BitAllocMode is 2.

@endverbatim
*/
    int fixedBitRatio[MAX_GOP_NUM];
    int cuLevelRCEnable;                /**< It enable CU level rate control. */

    int hvsQPEnable;                    /**< It enable CU QP adjustment for subjective quality enhancement. */

    int hvsQpScaleEnable;               /**< It enable QP scaling factor for CU QP adjustment when hvsQPEnable is 1. */
    int hvsQpScale;                     /**< A QP scaling factor for CU QP adjustment when hvcQpenable is 1 */

    int minQp;                          /**< A minimum QP for rate control */
    int maxQp;                          /**< A maximum QP for rate control */
    int maxDeltaQp;                     /**< A maximum delta QP for rate control */

    int transRate;                      /**< A peak transmission bitrate in bps */

    // CUSTOM_GOP
    CustomGopParam gopParam;            /**< <<vpuapi_h_CustomGopParam>> */
    HevcCtuOptParam ctuOptParam;        /**< <<vpuapi_h_HevcCtuOptParam>> */
    HevcVuiParam vuiParam;              /**< <<vpuapi_h_HevcVuiParam>> */

    Uint32 numUnitsInTick;              /**< It specifies the number of time units of a clock operating at the frequency time_scale Hz. */
    Uint32 timeScale;                   /**< It specifies the number of time units that pass in one second. */
    Uint32 numTicksPocDiffOne;          /**< It specifies the number of clock ticks corresponding to a difference of picture order count values equal to 1. */

    int chromaCbQpOffset;               /**< The value of chroma(Cb) QP offset (only for WAVE420L) */
    int chromaCrQpOffset;               /**< The value of chroma(Cr) QP offset  (only for WAVE420L) */

    int initialRcQp;                    /**< The value of initial QP by HOST application. This value is meaningless if INITIAL_RC_QP is 63. (only for WAVE420L) */

    Uint32  nrYEnable;                  /**< It enables noise reduction algorithm to Y component.  */
    Uint32  nrCbEnable;                 /**< It enables noise reduction algorithm to Cb component. */
    Uint32  nrCrEnable;                 /**< It enables noise reduction algorithm to Cr component. */
    Uint32  nrNoiseEstEnable;           /**< It enables noise estimation for reduction. When this is disabled, noise estimation is carried out ouside VPU. */
    Uint32  nrNoiseSigmaY;              /**< It specifies Y noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0).  */
    Uint32  nrNoiseSigmaCb;             /**< It specifies Cb noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). */
    Uint32  nrNoiseSigmaCr;             /**< It specifies Cr noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). */

    // ENC_NR_WEIGHT
    Uint32  nrIntraWeightY;             /**< A weight to Y noise level for intra picture (0 ~ 31). nrIntraWeight/4 is multiplied to the noise level that has been estimated. This weight is put for intra frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    Uint32  nrIntraWeightCb;            /**< A weight to Cb noise level for intra picture (0 ~ 31) */
    Uint32  nrIntraWeightCr;            /**< A weight to Cr noise level for intra picture (0 ~ 31) */
    Uint32  nrInterWeightY;             /**< A weight to Y noise level for inter picture (0 ~ 31). nrInterWeight/4 is multiplied to the noise level that has been estimated. This weight is put for inter frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    Uint32  nrInterWeightCb;            /**< A weight to Cb noise level for inter picture (0 ~ 31) */
    Uint32  nrInterWeightCr;            /**< A weight to Cr noise level for inter picture (0 ~ 31) */

    // ENC_INTRA_MIN_MAX_QP
    Uint32 intraMinQp;                  /**< It specifies a minimum QP for intra picture (0 ~ 51). It is only valid when RateControl is 1. */
    Uint32 intraMaxQp;                  /**< It specifies a maximum QP for intra picture (0 ~ 51). It is only valid when RateControl is 1. */
    int    forcedIdrHeaderEnable;       /**< It enables every IDR frame to include VPS/SPS/PPS. */

}EncHevcParam;

/**
* @brief This is an enumeration for encoder parameter change. (WAVE4 encoder only)
*/
typedef enum {
    // COMMON parameters
    ENC_PIC_PARAM_CHANGE                = (1<<1),
    ENC_INTRA_PARAM_CHANGE              = (1<<3),
    ENC_CONF_WIN_TOP_BOT_CHANGE         = (1<<4),
    ENC_CONF_WIN_LEFT_RIGHT_CHANGE      = (1<<5),
    ENC_FRAME_RATE_CHANGE               = (1<<6),
    ENC_INDEPENDENT_SLICE_CHANGE        = (1<<7),
    ENC_DEPENDENT_SLICE_CHANGE          = (1<<8),
    ENC_INTRA_REFRESH_CHANGE            = (1<<9),
    ENC_PARAM_CHANGE                    = (1<<10),
    ENC_RC_INIT_QP                      = (1<<11),
    ENC_RC_PARAM_CHANGE                 = (1<<12),
    ENC_RC_MIN_MAX_QP_CHANGE            = (1<<13),
    ENC_RC_TARGET_RATE_LAYER_0_3_CHANGE = (1<<14),
    ENC_RC_TARGET_RATE_LAYER_4_7_CHANGE = (1<<15),
    ENC_RC_INTRA_MIN_MAX_QP_CHANGE      = (1<<16),
    ENC_NUM_UNITS_IN_TICK_CHANGE        = (1<<18),
    ENC_TIME_SCALE_CHANGE               = (1<<19),
    ENC_RC_TRANS_RATE_CHANGE            = (1<<21),
    ENC_RC_TARGET_RATE_CHANGE           = (1<<22),
    ENC_ROT_PARAM_CHANGE                = (1<<23),
    ENC_NR_PARAM_CHANGE                 = (1<<24),
    ENC_NR_WEIGHT_CHANGE                = (1<<25),
    ENC_CHANGE_SET_PARAM_ALL            = (0xFFFFFFFF),
} ChangeCommonParam;

/**
* @brief This is an enumeration for encoder parameter change. (WAVE5 encoder only)
*/
typedef enum {
    // COMMON parameters which are changed frame by frame.
    ENC_SET_PPS_PARAM_CHANGE                = (1<<1),
    ENC_SET_RC_FRAMERATE_CHANGE             = (1<<4),
    ENC_SET_INDEPEND_SLICE_CHANGE           = (1<<5),
    ENC_SET_DEPEND_SLICE_CHANGE             = (1<<6),
    ENC_SET_RDO_PARAM_CHANGE                = (1<<8),
    ENC_SET_RC_TARGET_RATE_CHANGE           = (1<<9),
    ENC_SET_RC_PARAM_CHANGE                 = (1<<10),
    ENC_SET_RC_MIN_MAX_QP_CHANGE            = (1<<11),
    ENC_SET_RC_BIT_RATIO_LAYER_CHANGE       = (1<<12),
    ENC_SET_BG_PARAM_CHANGE                 = (1<<18),
    ENC_SET_CUSTOM_MD_CHANGE                = (1<<19),
} Wave5ChangeParam;

/**
* @brief This is a data structure for encoding parameters that have changed.
*/
typedef struct {
/**
@verbatim

@* 0 : changes the COMMON parameters.
@* 1 : Reserved
@endverbatim
*/
    int changeParaMode;
    int enable_option;                  /**< <<vpuapi_h_ChangeCommonParam>> */

    // ENC_PIC_PARAM_CHANGE
    int losslessEnable;                 /**< It enables lossless coding. */
    int constIntraPredFlag;             /**< It enables constrained intra prediction. */
    int chromaCbQpOffset;               /**< The value of chroma(Cb) QP offset (only for WAVE420L) */
    int chromaCrQpOffset;               /**< The value of chroma(Cr) QP offset (only for WAVE420L) */


    // ENC_INTRA_PARAM_CHANGE
/**
@verbatim
The type of I picture to be inserted at every intraPeriod

@* 0 : Non-IRAP
@* 1 : CRA
@* 2 : IDR
@endverbatim
*/
    int decodingRefreshType;
    int intraPeriod;                    /**< A period of intra picture in GOP size */
    int intraQP;                        /**< A quantization parameter of intra picture */

    // ENC_CONF_WIN_TOP_BOT_CHANGE
    int confWinTop;                     /**< A top offset of conformance window */
    int confWinBot;                     /**< A bottom offset of conformance window */

    // ENC_CONF_WIN_LEFT_RIGHT_CHANGE
    int confWinLeft;                    /**< A left offset of conformance window */
    int confWinRight;                   /**< A right offset of conformance window */

    // ENC_FRAME_RATE_CHANGE
    int frameRate;                      /**< A frame rate indicator ( x 1024) */

    // ENC_INDEPENDENT_SLICE_CHANGE
/**
@verbatim
A slice mode for independent slice

@* 0 : no multi-slice
@* 1 : Slice in CTU number
@endverbatim
*/
    int independSliceMode;
    int independSliceModeArg;           /**< The number of CTU for a slice when independSliceMode is set with 1  */

    // ENC_DEPENDENT_SLICE_CHANGE
/**
@verbatim
A slice mode for dependent slice

@* 0 : no multi-slice
@* 1 : Slice in CTU number
@* 2 : Slice in number of byte
@endverbatim
*/
    int dependSliceMode;
    int dependSliceModeArg;             /**< The number of CTU or bytes for a slice when dependSliceMode is set with 1 or 2  */

    // ENC_INTRA_REFRESH_CHANGE
/**
@verbatim
An intra refresh mode

@* 0 : No intra refresh
@* 1 : Row
@* 2 : Column
@* 3 : Step size in CTU
@endverbatim
*/
    int intraRefreshMode;
    int intraRefreshArg;                /**< The number of CTU (only valid when intraRefreshMode is 3.) */

    // ENC_PARAM_CHANGE
/**
@verbatim
It uses a recommended ENC_PARAM setting.

@* 0 : Custom
@* 1 : Recommended ENC_PARAM
@* 2 ~ 3  : Reserved
@endverbatim
*/
    int useRecommendEncParam;
    int scalingListEnable;              /**< It enables a scaling list. */
/**
@verbatim
It enables CU(Coding Unit) size to be used in encoding process. Host application can also select multiple CU sizes.

@* 0 : 8x8
@* 1 : 16x16
@* 2 : 32x32
@endverbatim
*/
    int cuSizeMode;
    int tmvpEnable;                     /**< It enables temporal motion vector prediction. */
    int wppEnable;                      /**< It enables WPP (Wave-front Parallel Processing). WPP is unsupported in ring buffer mode of bitstream buffer.  */
    int maxNumMerge;                    /**< Maximum number of merge candidates (0~2) */
    int dynamicMerge8x8Enable;          /**< It enables dynamic merge 8x8 candidates. */
    int dynamicMerge16x16Enable;        /**< It enables dynamic merge 16x16 candidates. */
    int dynamicMerge32x32Enable;        /**< It enables dynamic merge 32x32 candidates. */
    int disableDeblk;                   /**< It disables in-loop deblocking filtering. */
    int lfCrossSliceBoundaryEnable;     /**< It enables filtering across slice boundaries for in-loop deblocking. */
    int betaOffsetDiv2;                 /**< It enables BetaOffsetDiv2 for deblocking filter. */
    int tcOffsetDiv2;                   /**< It enables TcOffsetDiv2 for deblocking filter. */
    int skipIntraTrans;                 /**< It enables transform skip for an intra CU. */
    int saoEnable;                      /**< It enables SAO (Sample Adaptive Offset). */
    int intraInInterSliceEnable;        /**< It enables to make intra CUs in an inter slice. */
    int intraNxNEnable;                 /**< It enables intra NxN PUs. */

    // ENC_RC_PARAM_CHANGE
/**
@verbatim
@* WAVE420
@** 0 : Rate control is off.
@** 1 : Rate control is on.

@* CODA9
@** 0 : Constant QP (VBR, rate control off)
@** 1 : Constant Bit-Rate (CBR)
@** 2 : Average Bit-Rate (ABR)
@** 4 : Picture level rate control
@endverbatim
*/
    int rcEnable;
    int intraQpOffset;                  /**< It specifies an intra QP offset relative to an inter QP. It is only valid when RateControl is enabled. */
/**
@verbatim
It specifies encoder initial delay. It is only valid when RateControl is enabled.

 encoder initial delay = InitialDelay * InitBufLevelx8 / 8

@endverbatim
*/
	int initBufLevelx8;
/**
@verbatim
It specifies picture bits allocation mode.
It is only valid when RateControl is enabled and GOP size is larger than 1.

@* 0 : More referenced pictures have better quality than less referenced pictures
@* 1 : All pictures in a GOP have similar image quality
@* 2 : Each picture bits in a GOP is allocated according to FixedRatioN
@endverbatim
*/
    int bitAllocMode;
/**
@verbatim
A fixed bit ratio (1 ~ 255) for each picture of GOP's bit
allocation

@* N = 0 ~ (MAX_GOP_SIZE - 1)
@* MAX_GOP_SIZE = 8

For instance when MAX_GOP_SIZE is 3, FixedBitRatio0, FixedBitRatio1, and FixedBitRatio2 can be set as 2, 1, and 1 repsectively for
the fixed bit ratio 2:1:1. This is only valid when BitAllocMode is 2.
@endverbatim
*/
    int fixedBitRatio[MAX_GOP_NUM];

    int cuLevelRCEnable;                /**< It enables CU level rate control. */
    int hvsQPEnable;                    /**< It enables CU QP adjustment for subjective quality enhancement. */
    int hvsQpScaleEnable;               /**< It enables QP scaling factor for CU QP adjustment when hvsQPEnable is 1. */
    int hvsQpScale;                     /**< QP scaling factor for CU QP adjustment when hvcQpenable is 1. */
    int initialDelay;                   /**< An initial cpb delay in msec */
    Uint32 initialRcQp;                 /**< The value of initial QP by HOST application. This value is meaningless if INITIAL_RC_QP is 63. (only for WAVE420L) */

    // ENC_RC_MIN_MAX_QP_CHANGE
    int minQp;                          /**< Minimum QP for rate control */
    int maxQp;                          /**< Maximum QP for rate control */
    int maxDeltaQp;                     /**< Maximum delta QP for rate control */

    // ENC_TARGET_RATE_CHANGE
    int bitRate;                        /**< A target bitrate when separateBitrateEnable is 0 */

    // ENC_TRANS_RATE_CHANGE
    int transRate;                      /**< A peak transmission bitrate in bps */

    // ENC_RC_INTRA_MIN_MAX_CHANGE
    int intraMaxQp;                     /**< It specifies a maximum QP for intra picture (0 ~ 51). It is only valid when RateControl is 1. */
    int intraMinQp;                     /**< It specifies a minimum QP for intra picture (0 ~ 51). It is only valid when RateControl is 1. */

/**
@verbatim
@* [1:0] 90 degree left rotate
@** 1=90'
@** 2=180'
@** 3=270'
@* [2] vertical mirror
@* [3] horizontal mirror
@endverbatim
*/
    int rotMode;

    // ENC_NR_PARAM_CHANGE
    Uint32  nrYEnable;                  /**< It enables noise reduction algorithm to Y component.  */
    Uint32  nrCbEnable;                 /**< It enables noise reduction algorithm to Cb component. */
    Uint32  nrCrEnable;                 /**< It enables noise reduction algorithm to Cr component. */
    Uint32  nrNoiseEstEnable;           /**< It enables noise estimation for reduction. When this is disabled, noise estimation is carried out ouside VPU. */
    Uint32  nrNoiseSigmaY;              /**< It specifies Y noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). */
    Uint32  nrNoiseSigmaCb;             /**< It specifies Cb noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). */
    Uint32  nrNoiseSigmaCr;             /**< It specifies Cr noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). */

    // ENC_NR_WEIGHT_CHANGE
    Uint32  nrIntraWeightY;             /**< A weight to Y noise level for intra picture (0 ~ 31). nrIntraWeight/4 is multiplied to the noise level that has been estimated. This weight is put for intra frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    Uint32  nrIntraWeightCb;            /**< A weight to Cb noise level for intra picture (0 ~ 31).*/
    Uint32  nrIntraWeightCr;            /**< A weight to Cr noise level for intra picture (0 ~ 31).*/
    Uint32  nrInterWeightY;             /**< A weight to Y noise level for inter picture (0 ~ 31). nrInterWeight/4 is multiplied to the noise level that has been estimated. This weight is put for inter frame to be filtered more strongly or more weakly than just with the estimated noise level. */
    Uint32  nrInterWeightCb;            /**< A weight to Cb noise level for inter picture (0 ~ 31).*/
    Uint32  nrInterWeightCr;            /**< A weight to Cr noise level for inter picture (0 ~ 31).*/

    // ENC_NUM_UNITS_IN_TICK_CHANGE
    Uint32  numUnitsInTick;             /**< It specifies the number of time units of a clock operating at the frequency time_scale Hz. */

    // ENC_TIME_SCALE_CHANGE
    Uint32 timeScale;                   /**< It specifies the number of time units that pass in one second. */

    // belows are only for WAVE5 encoder
    Uint32 numTicksPocDiffOne;
    Uint32 monochromeEnable;
    Uint32 strongIntraSmoothEnable;
    Uint32 weightPredEnable;
    Uint32 bgDetectEnable;
    Uint32 bgThrDiff;
    Uint32 bgThrMeanDiff;
    Uint32 bgLambdaQp;
    int    bgDeltaQp;
    Uint32 customLambdaEnable;
    Uint32 customMDEnable;
    int    pu04DeltaRate;
    int    pu08DeltaRate;
    int    pu16DeltaRate;
    int    pu32DeltaRate;
    int    pu04IntraPlanarDeltaRate;
    int    pu04IntraDcDeltaRate;
    int    pu04IntraAngleDeltaRate;
    int    pu08IntraPlanarDeltaRate;
    int    pu08IntraDcDeltaRate;
    int    pu08IntraAngleDeltaRate;
    int    pu16IntraPlanarDeltaRate;
    int    pu16IntraDcDeltaRate;
    int    pu16IntraAngleDeltaRate;
    int    pu32IntraPlanarDeltaRate;
    int    pu32IntraDcDeltaRate;
    int    pu32IntraAngleDeltaRate;
    int    cu08IntraDeltaRate;
    int    cu08InterDeltaRate;
    int    cu08MergeDeltaRate;
    int    cu16IntraDeltaRate;
    int    cu16InterDeltaRate;
    int    cu16MergeDeltaRate;
    int    cu32IntraDeltaRate;
    int    cu32InterDeltaRate;
    int    cu32MergeDeltaRate;
    int    coefClearDisable;
    int    seqRoiEnable;
}EncChangeParam;

/**
* @brief    This structure is used for declaring an encoder slice mode and its options. It is newly added for more flexible usage of slice mode control in encoder.
*/
typedef struct{
/**
@verbatim
@* 0 : one slice per picture
@* 1 : multiple slices per picture
@* 2 : multiple slice encoding mode 2 for H.264 only.

In normal MPEG4 mode, resync-marker and packet header are inserted between
slice boundaries. In short video header with Annex K of 0, GOB headers are inserted
at every GOB layer start. In short video header with Annex K of 1, multiple
slices are generated. In AVC mode, multiple slice layer RBSP is generated.
@endverbatim
*/
    int sliceMode;
/**
@verbatim
This parameter means the size of generated slice when sliceMode of 1.

@* 0 : sliceSize is defined by the amount of bits
@* 1 : sliceSize is defined by the number of MBs in a slice
@* 2 : sliceSize is defined by MBs run-length table (only for H.264)

This parameter is ignored when sliceMode of 0 or
in short video header mode with Annex K of 0.
@endverbatim
*/
    int sliceSizeMode;
    int sliceSize;  /**< The size of a slice in bits or in MB numbers included in a slice, which is specified by the variable, sliceSizeMode. This parameter is ignored when sliceMode is 0 or in short video header mode with Annex K of 0. */
} EncSliceMode;



/**
* @brief    This data structure is used when HOST wants to open a new encoder instance.
*/
typedef struct {
    PhysicalAddress bitstreamBuffer;        /**< The start address of bitstream buffer into which encoder puts bitstream. This address must be aligned to AXI bus width. */
    Uint32          bitstreamBufferSize;    /**< The size of the buffer in bytes pointed by bitstreamBuffer. This value must be a multiple of 1024. The maximum size is 16383 x 1024 bytes. */
    CodStd          bitstreamFormat;        /**< The standard type of bitstream in encoder operation. STD_HEVC */

    int             picWidth;           /**< The width of a picture to be encoded in pixels. */
    int             picHeight;          /**< The height of a picture to be encoded in pixels. */

/**
@verbatim
The 16 LSB bits, [15:0], is a numerator and 16 MSB bits, [31:16], is a
denominator for calculating frame rate. The numerator means clock ticks per
second, and the denominator is clock ticks between frames minus 1.

So the frame rate can be defined by (numerator/(denominator + 1)),
which equals to (frameRateInfo & 0xffff) /((frameRateInfo >> 16) + 1).

For example, the value 30 of frameRateInfo represents 30 frames/sec, and the
value 0x3e87530 represents 29.97 frames/sec.
@endverbatim
*/
    int             frameRateInfo;


    int             bitRate;
/**
@verbatim
Time delay (in mili-seconds)

It takes for the bitstream to reach initial occupancy of the vbv buffer from zero level.

This value is ignored if rate
control is disabled. The value 0 means the encoder does not check for reference
decoder buffer delay constraints.
@endverbatim
*/
    int             initialDelay;
/**
@verbatim
@* WAVE420
@** 0 : Rate control is off.
@** 1 : Rate control is on.
@endverbatim
*/
    int             rcEnable;

    union {
        EncHevcParam    hevcParam;      /**< The parameters for ITU-T H.265/HEVC */
    } EncStdParam;

/**
@verbatim
@* 0 : CbCr data is written in separate frame memories (chroma separate mode)
@* 1 : CbCr data is interleaved in chroma memory. (chroma interleave mode)
@endverbatim
*/
    int             cbcrInterleave;
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
Frame buffer endianness

@* 0 : little endian format
@* 1 : big endian format
@* 2 : 32 bit little endian format
@* 3 : 32 bit big endian format
@* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE4 Datasheet'.
@endverbatim
*/
    int             frameEndian;
/**
@verbatim
Bistream buffer endianness

@* 0 : little endian format
@* 1 : big endian format
@* 2 : 32 bit little endian format
@* 3 : 32 bit big endian format
@* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE4 Datasheet'.
@endverbatim
*/
    int             streamEndian;
/**
@verbatim
Endianness of source YUV

@* 0 : Little endian format
@* 1 : Big endian format
@* 2 : 32 bit little endian format
@* 3 : 32 bit big endian format
@* 16 ~ 31 : 128 bit endian format

NOTE: For setting specific values of 128 bit endiness, please refer to the 'WAVE4 Datasheet'.
@endverbatim
*/
    int             sourceEndian;

/**
@verbatim
@* 0 : Disable
@* 1 : Enable

This flag is used for AvcEnc frame-based streaming with line buffer.
If coding standard is not AvcEnc or ringBufferEnable is 1, this flag is ignored.

If this field is set, VPU sends a buffer full interrupt when line buffer is full, and waits until the interrupt is cleared.
HOST should read the bitstream in line buffer and clear the interrupt.
If this field is not set, VPU does not send a buffer full interrupt when line buffer is full.
In both cases, VPU sets RET_ENC_PIC_FLAG register to 1 if line buffer is full.
@endverbatim
*/
    int             lineBufIntEn;
/**
@verbatim
This flag indicates whether source images are in packed format.

@* 0 : Not packed mode
@* 1 : Packed mode
@endverbatim
*/
    int             packedFormat;
/**
This flag indicates a color format of source image which is one among <<vpuapi_h_FrameBufferFormat>>.
*/
    int             srcFormat;
    int             srcBitDepth;

/**
@verbatim
VPU core index number

* 0 ~ number of VPU core - 1
@endverbatim
*/
    Uint32          coreIdx;
/**
@verbatim
@* 0 : CbCr data is interleaved in chroma source frame memory. (NV12)
@* 1 : CrCb data is interleaved in chroma source frame memory. (NV21)
@endverbatim
*/
    int             nv21;



    Uint32 encodeHrdRbspInVPS;        /**< It encodes the HRD syntax into VPS. */
    Uint32 encodeHrdRbspInVUI;        /**< It encodes the HRD syntax into VUI. */
    Uint32 hrdRbspDataSize;           /**< The bit size of the HRD rbsp data */
    PhysicalAddress hrdRbspDataAddr;  /**< The address of the HRD rbsp data */

    Uint32 encodeVuiRbsp;     /**< A flag to encode the VUI syntax in rbsp which is given by HOST application */
    Uint32 vuiRbspDataSize;   /**< The bit size of the VUI rbsp data */
    PhysicalAddress vuiRbspDataAddr;   /**< The address of the VUI rbsp data */

    Uint32          virtAxiID;
    BOOL            enablePTS; /**< An enable flag to report PTS(Presentation Timestamp. */

    vpu_buffer_t        vbWork;
    vpu_buffer_t        vbTemp;                 //!< Temp buffer (WAVE420)
    int				fw_writing_disable;
} EncOpenParam;

/**
* @brief    This is a data structure for parameters of VPU_EncGetInitialInfo() which
is required to get the minimum required buffer count in HOST application. This
returned value is used to allocate frame buffers in
wave420l_VPU_EncRegisterFrameBuffer().
*/
typedef struct {
    int             minFrameBufferCount; /**< Minimum number of frame buffer */
    int             minSrcFrameCount;    /**< Minimum number of source buffer */
} EncInitialInfo;

/**
* @brief    This is a data structure for setting ROI. (CODA9 encoder only).
 */
typedef struct {
/**
@verbatim
@* 0 : It disables ROI.
@* 1 : User can set QP value for both ROI and non-ROI region
@* 2 : User can set QP value for only ROI region (Currently, not support)
@endverbatim
*/
    int mode;
/**
@verbatim
Total number of ROI

The maximum value is MAX_ROI_NUMBER.
MAX_ROI_NUMBER can be either 10 or 50. In order to use MAX_ROI_NUMBER as 50,
SUPPORT_ROI_50 define needs to be enabled in the reference SW.
@endverbatim
*/
    int number;
    VpuRect region[MAX_ROI_NUMBER]; /**< Rectangle information for ROI */
    int qp[MAX_ROI_NUMBER];         /**< QP value for ROI region */
} AvcRoiParam;

/**
* @brief    This is a data structure for setting NAL unit coding options.
 */
typedef struct {
    int implicitHeaderEncode;       /**< Whether HOST application encodes a header implicitly or not. If this value is 1, below encode options are ignored. */
    int encodeVCL;                  /**< A flag to encode VCL nal unit explicitly */
    int encodeVPS;                  /**< A flag to encode VPS nal unit explicitly */
    int encodeSPS;                  /**< A flag to encode SPS nal unit explicitly */
    int encodePPS;                  /**< A flag to encode PPS nal unit explicitly */
    int encodeAUD;                  /**< A flag to encode AUD nal unit explicitly */
    int encodeEOS;                  /**< A flag to encode EOS nal unit explicitly. This should be set when to encode the last source picture of sequence. */
    int encodeEOB;                  /**< A flag to encode EOB nal unit explicitly. This should be set when to encode the last source picture of sequence. */
    int encodeVUI;                  /**< A flag to encode VUI nal unit explicitly */
    int encodeFiller;               /**< A flag to encode Filler nal unit explicitly (WAVE5 only) */
} EncCodeOpt;

/**
* @brief This is a data structure for configuring one frame encoding operation.
 */
 typedef struct {
    FrameBuffer*    sourceFrame;    /**< This member must represent the frame buffer containing source image to be encoded. */
/**
@verbatim
If this value is 0, the picture type is determined by VPU
according to the various parameters such as encoded frame number and GOP size.

If this value is 1, the frame is encoded as an I-picture regardless of the
frame number or GOP size, and I-picture period calculation is reset to
initial state. In H.264/AVC case, the picture is encoded as an IDR (Instantaneous
Decoding Refresh) picture.

This value is ignored if skipPicture is 1.
@endverbatim
*/
    int             forceIPicture;
/**
@verbatim
If this value is 0, the encoder encodes a picture as normal.

If this value is 1, the encoder ignores sourceFrame and generates a skipped
picture. In this case, the reconstructed image at decoder side is a duplication
of the previous picture. The skipped picture is encoded as P-type regardless of
the GOP size.
@endverbatim
*/
    int             skipPicture;
    int             quantParam;         /**< This value is used for all quantization parameters in case of VBR (no rate control).  */
/**
@verbatim
The start address of a picture stream buffer under line-buffer mode and dynamic
buffer allocation. (CODA encoder only)

This variable represents the start of a picture stream for encoded output. In
buffer-reset mode, HOST might use multiple picture stream buffers for the
best performance. By using this variable, applications could re-register the
start position of the picture stream while issuing a picture encoding operation.
This start address of this buffer must be 4-byte aligned, and its size is
specified the following variable, picStreamBufferSize. In packet-based streaming
with ring-buffer, this variable is ignored.

NOTE: This variable is only meaningful when both line-buffer mode and dynamic
buffer allocation are enabled.
@endverbatim
*/
    PhysicalAddress picStreamBufferAddr;
/**
@verbatim
The byte size of a picture stream chunk

This variable represents the byte size of a picture stream buffer. This variable is
so crucial in line-buffer mode. That is because encoder output could be
corrupted if this size is smaller than any picture encoded output. So this value
should be big enough for storing multiple picture streams with average size. In
packet-based streaming with ring-buffer, this variable is ignored.
@endverbatim
*/
   int             picStreamBufferSize;
/**
@verbatim
@* 0 : progressive (frame) encoding mode
@* 1 : interlaced (field) encoding mode
@endverbatim
*/
    int             fieldRun;
    AvcRoiParam     setROI;             /**< This value sets ROI. If setROI mode is 0, ROI does work and other member value of setROI is ignored. */

    HevcCtuOptParam ctuOptParam;        /**< <<vpuapi_h_HevcCtuOptParam>> for H.265/HEVC encoding */



    int             forcePicQpEnable;   /**< A flag to use a force picture quantization parameter */
    int             forcePicQpSrcOrderEnable; /**< A flag to use a force picture QP by source index order */
    int             forcePicQpI;        /**< A force picture quantization parameter for I picture */
    int             forcePicQpP;        /**< A force picture quantization parameter for P picture */
    int             forcePicQpB;        /**< A force picture quantization parameter for B picture */
    int             forcePicTypeEnable; /**< A flag to use a force picture type */
    int             forcePicTypeSrcOrderEnable; /**< A flag to use a force picture type by source index order */
    int             forcePicType;       /**< A force picture type (I, P, B, IDR, CRA) */
    int             srcIdx;             /**< A source frame buffer index */
    int             srcEndFlag;         /**< A flag indicating that there is no more source frame buffer to encode */

    EncCodeOpt      codeOption;             /**< <<vpuapi_h_EncCodeOpt>> */
#if !defined(TCC_ONBOARD)
    Uint32  pts;                    /**< The presentation Timestamp (PTS) of input source */
#endif

} EncParam;

/**
* @brief This structure is used for reporting encoder information.
*/
typedef struct {
/**
@verbatim
@* 0 : reporting disable
@* 1 : reporting enable
@endverbatim
*/
    int enable;
    int type;               /**< This value is used for picture type reporting in MVInfo. */
    int sz;                 /**< This value means size for each reporting data (MBinfo, MVinfo, Sliceinfo).    */
    PhysicalAddress addr;   /**< The start address of each reporting buffer into which encoder puts data.  */
} EncReportInfo;


/**
* @brief    This is a data structure for reporting the results of picture encoding operations.
*/
typedef struct {
/**
@verbatim
The Physical address of the starting point of newly encoded picture stream

If dynamic buffer allocation is enabled in line-buffer mode, this value is
identical with the specified picture stream buffer address by HOST.
@endverbatim
*/
    PhysicalAddress bitstreamBuffer;
    Uint32 bitstreamSize;   /**< The byte size of encoded bitstream */
    int bitstreamWrapAround;/**< This is a flag to indicate whether bitstream buffer operates in ring buffer mode in which read and write pointer are wrapped arounded. If this flag is 1, HOST application needs a larger buffer. */
/**
@verbatim
Coded picture type

@* H.264/AVC
@** 0 : IDR picture
@** 1 : Non-IDR picture
@endverbatim
*/
    int picType;
    int numOfSlices;        /**<  The number of slices of the currently being encoded Picture  */
    int reconFrameIndex;    /**<  A reconstructed frame index. The reconstructed frame is used for reference of future frame.  */
    FrameBuffer reconFrame; /**<  A reconstructed frame address and information. Please refer to <<vpuapi_h_FrameBuffer>>.  */
    int rdPtr;              /**<  A read pointer in bitstream buffer, which is where HOST has read encoded bitstream from the buffer */
    int wrPtr;              /**<  A write pointer in bitstream buffer, which is where VPU has written encoded bitstream into the buffer  */

    // for WAVE420
    int picSkipped;         /**< A flag which represents whether the current encoding has been skipped or not. */
    int numOfIntra;         /**< The number of intra coded block */
    int numOfMerge;         /**< The number of merge block in 8x8 */
    int numOfSkipBlock;     /**< The number of skip block in 8x8 */
    int avgCtuQp;           /**< The average value of CTU QPs */
    int encPicByte;         /**< The number of encoded picture bytes   */
    int encGopPicIdx;       /**< GOP index of the current picture */
    int encPicPoc;          /**< POC(picture order count) value of the current picture */
    int encSrcIdx;          /**< Source buffer index of the current encoded picture */
    int encVclNal;          /**< Encoded NAL unit type of VCL */
    int encPicCnt;          /**< Encoded picture number */

    // Report Information
    EncReportInfo mbInfo;   /**<  The parameter for reporting MB data . Please refer to <<vpuapi_h_EncReportInfo>> structure. */
    EncReportInfo mvInfo;   /**<  The parameter for reporting motion vector. Please refer to <<vpuapi_h_EncReportInfo>> structure. */
    EncReportInfo sliceInfo;/**<  The parameter for reporting slice information. Please refer to <<vpuapi_h_EncReportInfo>> structure. */
    int frameCycle;         /**<  The parameter for reporting the cycle number of decoding/encoding one frame.*/
#if !defined(TCC_ONBOARD)
    Uint64      pts;        /**< Presentation Timestamp of encoded picture. */
#endif
    Uint32      encInstIdx;     /**< An index of instance which have finished encoding with a picture at this command. This is only for Multi-Core encoder product. */
    Uint32  encPrepareCycle;
    Uint32  encProcessingCycle;
    Uint32  encEncodingCycle;
} EncOutputInfo;

/**
 * @brief   This structure is used when HOST processor additionally wants to get SPS data or
PPS data from encoder instance. The resulting SPS data or PPS data can be used
in real applications as a kind of out-of-band information.
*/
 typedef struct {
    PhysicalAddress paraSet;/**< An array of 32 bits which contains SPS RBSP */
    int size;               /**< The size of stream in byte */
} EncParamSet;

/**
 * @brief   This structure is used for adding a header syntax layer into the encoded bit
stream. The parameter headerType is the input parameter to VPU, and the
other two parameters are returned values from VPU after completing
requested operation.
*/
 typedef struct {
    PhysicalAddress buf;        /**< A physical address pointing the generated stream location  */
    BYTE *pBuf;                 /**< The virtual address according to buf. This address is needed when a HOST wants to access encoder header bitstream buffer. */
    Uint32  size;               /**< The size of the generated stream in bytes */
    Int32   headerType;         /**< This is a type of header that HOST wants to generate and have values as VOL_HEADER, VOS_HEADER, VO_HEADER, SPS_RBSP or PPS_RBSP. */
    BOOL    zeroPaddingEnable;  /**< It enables header to be padded at the end with zero for byte alignment. */
    Int32   failReasonCode;     /**< Not defined yet */
} EncHeaderParam;

/**
 * @brief   This is a special enumeration type for explicit encoding headers such as VPS, SPS, PPS. (WAVE encoder only)
*/
typedef enum {
    CODEOPT_ENC_VPS             = (1 << 2), /**< A flag to encode VPS nal unit explicitly */
    CODEOPT_ENC_SPS             = (1 << 3), /**< A flag to encode SPS nal unit explicitly */
    CODEOPT_ENC_PPS             = (1 << 4), /**< A flag to encode PPS nal unit explicitly */
} HevcHeaderType ;

/**
 * @brief   This is a special enumeration type for NAL unit coding options
*/
typedef enum {
    CODEOPT_ENC_HEADER_IMPLICIT = (1 << 0), /**< A flag to encode (a) headers (VPS, SPS, PPS) implicitly for generating bitstreams conforming to spec. */
    CODEOPT_ENC_VCL             = (1 << 1), /**< A flag to encode VCL nal unit explicitly */
} ENC_PIC_CODE_OPTION;

/**
 * @brief   This is a special enumeration type for defining GOP structure presets.
*/
typedef enum {
    PRESET_IDX_CUSTOM_GOP       = 0,    /**< User defined GOP structure */
    PRESET_IDX_ALL_I            = 1,    /**< All Intra, gopsize = 1 */
    PRESET_IDX_IPP              = 2,    /**< Consecutive P, cyclic gopsize = 1  */
    PRESET_IDX_IPPPP            = 6,    /**< Consecutive P, cyclic gopsize = 4 */
} GOP_PRESET_IDX;


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
Int32  wave420l_VPU_IsBusy(
    Uint32 coreIdx  /**< [input] An index of VPU core */
    );

/**
* @brief
@verbatim
This function makes HOST application wait until VPU finishes processing a frame,
or check a busy flag of VPU during the given timeout period.
The behavior of this function depends on VDI layer\'s implementation.

NOTE: Timeout may not work according to implementation of VDI layer.
@endverbatim
* @param coreIdx [input] An index of VPU core
* @param timeout [output] See return value.
* @return
@verbatim
* 1 : Wait time out
* Non -1 value : The value of InterruptBit
@endverbatim
*/
Int32 wave420l_VPU_WaitInterrupt(
    Uint32 coreIdx,
    int timeout
    );

Int32 wave420l_VPU_WaitInterruptEx(
    VpuHandle handle,
    int timeout
    );

/**
 * @brief This function returns whether VPU is currently running or not.
 * @param coreIdx [input] An index of VPU core
* @return
@verbatim
@* 0 : VPU is not running.
@* 1 or more : VPU is running.
@endverbatim
 */
Int32  wave420l_VPU_IsInit(
    Uint32 coreIdx
    );

/**
* @brief
@verbatim
This function initializes VPU hardware and its data structures/resources.
HOST application must call this function only once before calling VPU_DeInit().

VPU_InitWithBitcodec() is basically same as VPU_Init() except that it takes additional arguments, a buffer pointer where
BIT firmware binary is located and the size.
HOST application can use this function when they wish to load a binary format of BIT firmware,
instead of it including the header file of BIT firmware.
Particularly in multi core running environment with different VPU products,
this function must be used because each core needs to load different firmware.
@endverbatim
*
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means VPU has been initialized successfully.
*RETCODE_CALLED_BEFORE* ::
This function call is invalid which means multiple calls of the current API function
for a given instance are not allowed. In this case, VPU has been already
initialized, so that this function call is meaningless and not allowed anymore.
*RETCODE_NOT_FOUND_BITCODE_PATH* ::
The header file path of BIT firmware has not been defined.
*RETCODE_VPU_RESPONSE_TIMEOUT* ::
Operation has not received any response from VPU and has timed out.
*RETCODE_FAILURE* ::
Operation was failed.
@endverbatim
*/
RetCode wave420l_VPU_InitWithBitcode(
    Uint32 coreIdx,         /**< [Input] An index of VPU core */
    PhysicalAddress workBuf,
    PhysicalAddress codeBuf,  /**< [Input] Buffer where binary format of BIT firmware is located */
    Uint32 size       /**< [Input] Size of binary BIT firmware in bytes */
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
be completed by calling VPU_Dec Info(). Even though the result of the
current frame operation is not necessary, HOST application should call
VPU_DecGetOutputInfo() to proceed this function call.
*RETCODE_VPU_RESPONSE_TIMEOUT* ::
Operation has not received any response from VPU and has timed out.
@endverbatim
*/
RetCode wave420l_VPU_GetVersionInfo(
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
void VPU_ClearInterrupt(
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
before calling the previous VPU_DecStartOneFrame() or wave420l_VPU_EncStartOneFrame().
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
RetCode wave420l_VPU_SWReset(
    Uint32      coreIdx,    /**<[Input] An index of VPU core */
    SWResetMode resetMode,
    void*       pendingInst
    );

/**
* @brief
@verbatim
This function resets VPU as VPU_SWReset() does, but
it is done by the system reset signal and all the internal contexts are initialized.
Therefore after the `VPU_HWReset()`, HOST application needs to call `VPU_Init()`.

`VPU_HWReset()` requires vdi_hw_reset part of VDI module to be implemented before use.
@endverbatim
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means required information of the stream data to be decoded was received successfully.
@endverbatim
 */
RetCode wave420l_VPU_HWReset(
    Uint32  coreIdx     /**<  [Input] An index of VPU core */
    );

/**
* @brief    This function returns the size of motion vector co-located buffer that are needed to decode H.265/AVC stream.
*           The mvcol buffer should be allocated along with frame buffers and given to VPU_DecRegisterFramebuffer() as an argument.
* @return   It returns the size of required mvcol buffer in byte unit.
*/
int wave420l_VPU_GetMvColBufSize(
    CodStd codStd,      /**< [Input] Video coding standards */
    int width,          /**< [Input] Width of framebuffer */
    int height,         /**< [Input] Height of framebuffer */
    int num             /**< [Input] Number of framebuffers. */
    );

/**
* @brief    This function returns the size of FBC (Frame Buffer Compression) offset table for luma and chroma.
*           The offset tables are to look up where compressed data is located.
*           HOST should allocate the offset table buffers for luma and chroma as well as frame buffers
*           and give their base addresses to VPU_DecRegisterFramebuffer() as an argument.
*
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means given value is valid and setting is done successfully.
*RETCODE_INVALID_PARAM* ::
The given argument parameter, ysize, csize, or handle was NULL.
@endverbatim
*/
RetCode VPU_GetFBCOffsetTableSize(
    CodStd  codStd,     /**< [Input] Video coding standards */
    int     width,      /**< [Input] Width of framebuffer */
    int     height,     /**< [Input] Height of framebuffer  */
    int*     ysize,     /**< [Output] Size of offset table for Luma in bytes */
    int*     csize      /**< [Output] Size of offset table for Chroma in bytes */
    );

/**
*  @brief   This function returns the size of frame buffer that is required for VPU to decode or encode one frame.
*
*  @return  The size of frame buffer to be allocated
*/
int wave420l_VPU_GetFrameBufSize(
    int         coreIdx,    /**< [Input] VPU core index number */
    int         stride,     /**< [Input] The stride of image  */
    int         height,     /**< [Input] The height of image */
    int         mapType,    /**< [Input] The map type of framebuffer */
    int         format,     /**< [Input] The color format of framebuffer */
    int         interleave, /**< [Input] Whether to use CBCR interleave mode or not */
    DRAMConfig* pDramCfg    /**< [Input] Attributes of DRAM. It is only valid for CODA960. Set NULL for this variable in case of other products. */
    );

// function for encode
/**
* @brief    In order to start a new encoder operation, HOST application must open a new instance
for this encoder operation. By calling this function, HOST application can get a
handle specifying a new encoder instance. Because VPU supports multiple
instances of codec operations, HOST application needs this kind of handles for the
all codec instances now on running. Once a HOST application gets a handle, the HOST application
must use this handle to represent the target instances for all subsequent
encoder-related functions.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means a new encoder instance was opened
successfully.

*RETCODE_FAILURE* ::
Operation was failed, which means getting a new encoder instance was not done
successfully. If there is no free instance anymore, this value is returned
in this function call.

*RETCODE_INVALID_PARAM* ::
The given argument parameter, pOpenParam, was invalid, which means it has a null
pointer, or given values for some member variables are improper values.

*RETCODE_NOT_INITIALIZED* ::
VPU was not initialized at all before calling this function. Application should
initialize VPU by calling VPU_Init() before calling this function.
@endverbatim
*/

RetCode wave420l_VPU_EncOpen(
    EncHandle*      handle,     /**< [Output] A pointer to a EncHandle type variable which specifies each instance for HOST application. If no instance is available, null handle is returned. */
    EncOpenParam*   encOpParam  /**< [Input] A pointer to a EncOpenParam type structure which describes required parameters for creating a new encoder instance. */
    );

/**
* @brief    When HOST application finished encoding operations and wanted to release this
instance for other processing, the HOST application should close this instance by calling
this function. After completion of this function call, the instance referred to
by pHandle gets free. Once HOST application closes an instance, the HOST application
cannot call any further encoder-specific function with the current handle before
re-opening a new instance with the same handle.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully. That means the current encoder instance was closed
successfully.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if
+
--
* pHandle is not a handle which has been obtained by VPU_EncOpen().
* pHandle is a handle of an instance which has been closed already, etc.
--
*RETCODE_FRAME_NOT_COMPLETE* ::
This means frame decoding or encoding operation was not completed yet, so the
given API function call cannot be performed this time. A frame encoding or
decoding operation should be completed by calling VPU_EncGetOutputInfo() .
Even though the result of the current frame operation is
not necessary, HOST application should call VPU_EncGetOutputInfo()
to proceed this function call.

*RETCODE_VPU_RESPONSE_TIMEOUT* ::
Operation has not recieved any response from VPU and has timed out.
@endverbatim
*/
RetCode wave420l_VPU_EncClose(
    EncHandle handle    /**< [Input] An encoder handle obtained from VPU_EncOpen() */
    );

/**
* @brief    Before starting encoder operation, HOST application must allocate frame buffers
according to the information obtained from this function. This function returns
the required parameters for wave420l_VPU_EncRegisterFrameBuffer(), which follows right after
this function call.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means receiving the initial parameters
was done successfully.

*RETCODE_FAILURE* ::
Operation was failed, which means there was an error in getting information for
configuring the encoder.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if

* pHandle is not a handle which has been obtained by VPU_EncOpen().
* pHandle is a handle of an instance which has been closed already, etc.

*RETCODE_FRAME_NOT_COMPLETE* ::
This means frame decoding or encoding operation was not completed yet, so the
given API function call cannot be performed this time. A frame encoding or
decoding operation should be completed by calling VPU_EncGetOutputInfo() .
Even though the result of the current frame operation is
not necessary, HOST application should call VPU_EncGetOutputInfo()
 to proceed this function call.

*RETCODE_INVALID_PARAM* ::
The given argument parameter, pInitialInfo, was invalid, which means it has a
null pointer, or given values for some member variables are improper values.

*RETCODE_CALLED_BEFORE* ::
This function call is invalid which means multiple calls of the current API function
for a given instance are not allowed. In this case, encoder initial information
has been received already, so that this function call is meaningless and not
allowed anymore.

*RETCODE_VPU_RESPONSE_TIMEOUT* ::
Operation has not recieved any response from VPU and has timed out.
@endverbatim
*/
RetCode wave420l_VPU_EncGetInitialInfo(
    EncHandle       handle,     /**< [Input] An encoder handle obtained from VPU_EncOpen() */
    EncInitialInfo* encInitInfo /**< [Output] Minimum number of frame buffer for this encoder instance  */
    );

/**
* @brief
@verbatim
This function registers frame buffers requested by VPU_EncGetInitialInfo(). The
frame buffers pointed to by pBuffer are managed internally within VPU.
These include reference frames, reconstructed frames, etc. Applications must not
change the contents of the array of frame buffers during the life time of the
instance, and `num` must not be less than minFrameBufferCount obtained by
VPU_EncGetInitialInfo().

The distance between a pixel in a row and the corresponding pixel in the next
row is called a stride. The value of stride must be a multiple of 8. The
address of the first pixel in the second row does not necessarily coincide with
the value next to the last pixel in the first row. In other words, a stride can
be greater than the picture width in pixels.

Applications should not set a stride value smaller than the picture width.
So, for Y component, HOST application must allocate at least a space of size
(frame height \* stride), and Cb or Cr component,
(frame height/2 \* stride/2), respectively.
But make sure that in Cb/Cr non-interleave (separate Cb/Cr) map,
a stride for the luminance frame buffer should be multiple of 16 so that a stride
for the luminance frame buffer becomes a multiple of 8.
@endverbatim
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means registering frame buffers were done
successfully.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if

* handle is not a handle which has been obtained by VPU_EncOpen().
* handle is a handle of an instance which has been closed already, etc.

*RETCODE_FRAME_NOT_COMPLETE* ::
This means frame decoding or encoding operation was not completed yet, so the
given API function call cannot be performed this time. A frame encoding or
decoding operation should be completed by calling VPU_EncGetOutputInfo().
Even though the result of the current frame operation is
not necessary, HOST application should call VPU_EncGetOutputInfo() or
to proceed this function call.

*RETCODE_WRONG_CALL_SEQUENCE* ::
This means the current API function call was invalid considering the allowed
sequences between API functions. HOST application might call this
function before calling VPU_EncGetInitialInfo() successfully. This function
should be called after successful calling of VPU_EncGetInitialInfo().

*RETCODE_INVALID_FRAME_BUFFER* ::
This means argument pBuffer were invalid, which means it was not initialized
yet or not valid anymore.

*RETCODE_INSUFFICIENT_FRAME_BUFFERS* ::
This means the given number of frame buffers, num, was not enough for the
encoder operations of the given handle. It should be greater than or equal to
the value of  minFrameBufferCount obtained from VPU_EncGetInitialInfo().

*RETCODE_INVALID_STRIDE* ::
This means the given argument stride was invalid, which means it is 0, or is not
a multiple of 8 in this case.

*RETCODE_CALLED_BEFORE* ::
This function call is invalid which means multiple calls of the current API function
for a given instance are not allowed. It might happen when registering frame buffer for
this instance has been done already so that this function call is meaningless
and not allowed anymore.

*RETCODE_VPU_RESPONSE_TIMEOUT* ::
Operation has not recieved any response from VPU and has timed out.
@endverbatim
*/
RetCode wave420l_VPU_EncRegisterFrameBuffer(
    EncHandle       handle,     /**< [Input] An encoder handle obtained from VPU_EncOpen() */
    FrameBuffer*    bufArray,   /**< [Input] Allocated frame buffer address and information. If this parameter is set to -1, VPU allocates frame buffers. */
    int             num,        /**< [Input] A number of frame buffers. VPU can allocate frame buffers as many as this given value. */
    int             stride,     /**< [Input] A stride value of the given frame buffers */
    int             height,     /**< [Input] Frame height */
    int             mapType     /**< [Input] Map type of frame buffer */
    );

/**
* @brief    This function is provided to let HOST application have a certain level of freedom for
re-configuring encoder operation after creating an encoder instance. Some
options which can be changed dynamically during encoding as the video sequence has
been included. Some command-specific return codes are also presented.
* @return
@verbatim
*RETCODE_INVALID_COMMAND* ::
This means the given argument, cmd, was invalid which means the given cmd was
undefined, or not allowed in the current instance.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if
+
--
* pHandle is not a handle which has been obtained by VPU_EncOpen().
* pHandle is a handle of an instance which has been closed already, etc.
--
*RETCODE_FRAME_NOT_COMPLETE* ::
This means frame decoding or encoding operation was not completed yet, so the
given API function call cannot be performed this time. A frame encoding or
decoding operation should be completed by calling VPU_EncGetOutputInfo().
Even though the result of the current frame operation is
not necessary, HOST application should call VPU_EncGetOutputInfo() or
to proceed this function call.
@endverbatim
* @note
@verbatim
The list of commands can be summarized as follows:

@* ENABLE_ROTATION
@* ENABLE_MIRRORING
@* DISABLE_MIRRORING
@* SET_MIRROR_DIRECTION
@* SET_ROTATION_ANGLE
@* ENC_PUT_VIDEO_HEADER
@* ENC_PUT_MP4_HEADER
@* ENC_PUT_AVC_HEADER
@* ENC_GET_VIDEO_HEADER
@* ENC_SET_INTRA_MB_REFRESH_NUMBER
@* ENC_ENABLE_HEC
@* ENC_DISABLE_HEC
@* ENC_SET_SLICE_INFO
@* ENC_SET_GOP_NUMBER
@* ENC_SET_INTRA_QP
@* ENC_SET_BITRATE
@* SET_SEC_AXI
@* ENABLE_LOGGING
@* DISABLE_LOGGING

====== ENABLE_ROTATION
This command enables rotation of the pre-rotator.
In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

====== ENABLE_MIRRORING
This command enables mirroring of the pre-rotator.
In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

====== DISABLE_MIRRORING
This command disables mirroring of the pre-rotator.
In this case, `parameter` is ignored. This command returns RETCODE_SUCCESS.

====== SET_MIRROR_DIRECTION
This command sets mirror direction of the pre-rotator, and `parameter` is
interpreted as a pointer to MirrorDirection.
The `parameter` should be one of MIRDIR_NONE, MIRDIR_VER, MIRDIR_HOR, and MIRDIR_HOR_VER.

@* MIRDIR_NONE: No mirroring
@* MIRDIR_VER: Vertical mirroring
@* MIRDIR_HOR: Horizontal mirroring
@* MIRDIR_HOR_VER: Both directions

This command has one of the following return codes::

* RETCODE_SUCCESS:
Operation was done successfully, which means given mirroring direction is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given mirroring
direction is invalid.

====== SET_ROTATION_ANGLE
This command sets counter-clockwise angle for post-rotation, and `parameter` is
interpreted as a pointer to the integer which represents
rotation angle in degrees. Rotation angle should be one of 0, 90, 180, and 270.

This command has one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means given rotation angle is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given rotation
angle is invalid.

NOTE: Rotation angle could not be changed after sequence initialization, because
it might cause problems in handling frame buffers.

====== ENC_PUT_VIDEO_HEADER
This command inserts SPS or PPS to the AVC/H.264 bitstream to the bitstream
during encoding. The argument `parameter` is interpreted as a pointer to EncHeaderParam structure holding

* buf is a physical address pointing the generated stream location
* size is the size of generated stream in bytes
* headerType is a type of header that HOST application wants to generate and have values as
VOL_HEADER, VOS_HEADER,, VO_HEADER, SPS_RBSP or PPS_RBSP.

This command has one of the following return codes.::
+
--
* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header
syntax was successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument cmd was invalid
which means the given cmd was undefined, or not allowed in the current instance. In this
case, the current instance might not be an MPEG4 encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or headerType
was invalid, which means it has a null pointer, or given values for some member variables
are improper values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.
--

====== ENC_GET_VIDEO_HEADER
This command gets a SPS to the AVC/H.264 bitstream to the bitstream during encoding.
Because VPU does not generate bitstream but HOST application generate bitstream in this command,
HOST application has to set pBuf value to access bitstream buffer.
The argument `parameter` is interpreted as a pointer to EncHeaderParam structure holding

* buf is a physical address pointing the generated stream location
* pBuf is a virtual address pointing the generated stream location
* size is the size of generated stream in bytes
* headerType is a type of header that HOST application wants to generate and have values as SPS_RBSP.

This command has one of the following return codes.::
+
--
* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header
syntax was successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument cmd was invalid
which means the given cmd was undefined, or not allowed in the current instance. In this
case, the current instance might not be an AVC/H.264 encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or headerType
was invalid, which means it has a null pointer, or given values for some member variables
are improper values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.
--

====== ENC_SET_INTRA_MB_REFRESH_NUMBER
This command sets intra MB refresh number of header syntax.
The argument `parameter` is interpreted as a pointer to integer which
represents an intra refresh number. It should be between 0 and
macroblock number of encoded picture.

This command returns the following code.::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== ENC_ENABLE_HEC
This command enables HEC(Header Extension Code) syntax of MPEG4.

This command ignores the argument `parameter` and returns one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument, cmd, was invalid which means the given cmd was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an MPEG4 encoder instance.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== ENC_DISABLE_HEC
This command disables HEC(Header Extension Code) syntax of MPEG4.

This command ignores the argument `parameter` and returns one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument, cmd, was invalid which means the given cmd was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an MPEG4 encoder instance.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== ENC_SET_SLICE_INFO
This command sets slice inforamtion of header syntax.
The argument `parameter` is interpreted as a pointer to EncSliceMode
structure holding

* sliceModeuf is a mode which means enabling multi slice structure
* sliceSizeMode is the mode representing mode of calculating one slicesize
* sliceSize is the size of one slice.

This command has one of the following return codes.::
+
--
* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.
--

====== ENC_SET_GOP_NUMBER
This command sets GOP number of header syntax.
The argument `parameter` is interpreted as a pointer to the integer which
represents a GOP number.

This command has one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== ENC_SET_INTRA_QP
This command sets intra QP value of header syntax.
The argument `parameter` is interpreted as a pointer to the integer which
represents a Constant I frame QP. The Constant I frame QP should be between 0 and 51 for AVC/H.264.

This command has one of the following return codes::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument `cmd` was invalid which means the given cmd was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== ENC_SET_BITRATE
This command sets bitrate inforamtion of header syntax.
The argument `parameter` is interpreted as a pointer to the integer which
represents a bitrate. It should be between 0 and 32767.

This command has one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument `cmd` was invalid which means the given `cmd` was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.

====== SET_SEC_AXI
This command sets the secondary channel of AXI for saving memory bandwidth to
dedicated memory. The argument `parameter` is interpreted as a pointer to SecAxiUse which
represents an enable flag and physical address which is related with the secondary
channel for BIT processor, IP/AC-DC predictor, de-blocking filter, overlap
filter respectively.

This command has one of the following return codes.::

* RETCODE_SUCCESS:
Operation was done successfully, which means given value for setting secondary
AXI is valid.
* RETCODE_INVALID_PARAM:
The given argument parameter, `parameter`, was invalid, which means given value is
invalid.

====== ENABLE_LOGGING
HOST can activate message logging once VPU_DecOpen() or VPU_EncOpen() is called.

====== DISABLE_LOGGING
HOST can deactivate message logging which is off as default.

====== ENC_SET_PARA_CHANGE
This command changes encoding parameter(s) during the encoding operation in H.265/AVC encoder
The argument `parameter` is interpreted as a pointer to <<vpuapi_h_EncChangeParam>> structure holding

* changeParaMode : OPT_COMMON (only valid currently)
* enable_option : Set an enum value that is associated with parameters to change, <<vpuapi_h_ChangeCommonParam>> (multiple option allowed).

For instance, if bitrate and framerate need to be changed in the middle of encoding, that requires some setting like below.

 EncChangeParam changeParam;
 changeParam.changeParaMode    = OPT_COMMON;
 changeParam.enable_option     = ENC_RC_TARGET_RATE_CHANGE | ENC_FRAME_RATE_CHANGE;

 changeParam.bitrate           = 14213000;
 changeParam.frameRate         = 15;
 VPU_EncGiveCommand(handle, ENC_SET_PARA_CHANGE, &changeParam);

This command has one of the following return codes.::
+
--
* RETCODE_SUCCESS:
Operation was done successfully, which means the requested header syntax was
successfully inserted.
* RETCODE_INVALID_COMMAND:
This means the given argument `cmd` was invalid which means the given `cmd` was
undefined, or not allowed in the current instance. In this case, the current
instance might not be an AVC/H.264 encoder instance.
* RETCODE_INVALID_PARAM:
The given argument parameter `parameter` or `headerType` was invalid, which means it
has a null pointer, or given values for some member variables are improper
values.
* RETCODE_VPU_RESPONSE_TIMEOUT:
Operation has not recieved any response from VPU and has timed out.
--

====== ENC_SET_SEI_NAL_DATA
This command sets variables in HevcSEIDataEnc structure for SEI encoding.

@endverbatim
*/
RetCode wave420l_VPU_EncGiveCommand(
    EncHandle handle,   /**< [Input] An encoder handle obtained from VPU_EncOpen() */
    CodecCommand cmd,   /**< [Input] A variable specifying the given command of CodecComand type */
    void * parameter    /**< [Input/Output] A pointer to command-specific data structure which describes picture I/O parameters for the current encoder instance */
    );

/**
* @brief
@verbatim
This function starts encoding one frame. Returning from this function does not
mean the completion of encoding one frame, and it is just that encoding one
frame was initiated.

Every call of this function should be matched with VPU_EncGetOutputInfo()
with the same handle. Without calling a pair of these funtions,
HOST application cannot call any other API functions
except for VPU_IsBusy(), VPU_EncGetBitstreamBuffer(), and
VPU_EncUpdateBitstreamBuffer().
@endverbatim
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means encoding a new frame was started
successfully.
+
--
NOTE: This return value does not mean that encoding a frame was completed
successfully.
--
*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if
+
--
* handle is not a handle which has been obtained by VPU_EncOpen().
* handle is a handle of an instance which has been closed already, etc.
--
*RETCODE_FRAME_NOT_COMPLETE* ::
This means frame decoding or encoding operation was not completed yet, so the
given API function call cannot be performed this time. A frame encoding or
decoding operation should be completed by calling VPU_EncGetOutputInfo().
Even though the result of the current frame operation is
not necessary, HOST application should call VPU_EncGetOutputInfo() to proceed this function call.

*RETCODE_WRONG_CALL_SEQUENCE* ::
This means the current API function call was invalid considering the allowed
sequences between API functions. In this case, HOST application might call this
function before successfully calling wave420l_VPU_EncRegisterFrameBuffer(). This function
should be called after successfully calling wave420l_VPU_EncRegisterFrameBuffer().

*RETCODE_INVALID_PARAM* ::
The given argument parameter, `parameter`, was invalid, which means it has a null
pointer, or given values for some member variables are improper values.

*RETCODE_INVALID_FRAME_BUFFER* ::
This means sourceFrame in input structure EncParam was invalid, which means
sourceFrame was not valid even though picture-skip is disabled.
@endverbatim
*/
RetCode wave420l_VPU_EncStartOneFrame(
    EncHandle handle,   /**< [Input] An encoder handle obtained from VPU_EncOpen()  */
    EncParam * param    /**< [Input] A pointer to EncParam type structure which describes picture encoding parameters for the current encoder instance.  */
    );

/**
* @brief    This function gets information of the output of encoding. Application can know
about picture type, the address and size of the generated bitstream, the number
of generated slices, the end addresses of the slices, and macroblock bit
position information. HOST application should call this function after frame
encoding is finished, and before starting the further processing.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully, which means the output information of the current
frame encoding was received successfully.

*RETCODE_INVALID_HANDLE* ::
The given handle for the current API function call, pHandle, was invalid. This
return code might be caused if
+
--
* handle is not a handle which has been obtained by VPU_EncOpen(), for example
a decoder handle,
* handle is a handle of an instance which has been closed already,
* handle is not the same handle as the last wave420l_VPU_EncStartOneFrame() has, etc.
--
*RETCODE_WRONG_CALL_SEQUENCE* ::
This means the current API function call was invalid considering the allowed
sequences between API functions. HOST application might call this
function before calling wave420l_VPU_EncStartOneFrame() successfully. This function
should be called after successful calling of wave420l_VPU_EncStartOneFrame().

*RETCODE_INVALID_PARAM* ::
The given argument parameter, pInfo, was invalid, which means it has a null
pointer, or given values for some member variables are improper values.
@endverbatim
*/
RetCode wave420l_VPU_EncGetOutputInfo(
    EncHandle handle,       /**<  [Input] An encoder handle obtained from VPU_EncOpen().  */
    EncOutputInfo * info    /**<  [Output] A pointer to a EncOutputInfo type structure which describes picture encoding results for the current encoder instance. */
    );

/**
* @brief    After encoding frame, HOST application must get bitstream from the encoder. To do
that, they must know where to get bitstream and the maximum size.
Applications can get the information by calling this function.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully. That means the current encoder instance was closed
successfully.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if
+
--
* pHandle is not a handle which has been obtained by VPU_EncOpen().
* pHandle is a handle of an instance which has been closed already, etc.
--
*RETCODE_INVALID_PARAM* ::
The given argument parameter, pRdptr, pWrptr or size, was invalid, which means
it has a null pointer, or given values for some member variables are improper
values.
@endverbatim
*/
RetCode wave420l_VPU_EncGetBitstreamBuffer(
    EncHandle handle,           /**<  [Input] An encoder handle obtained from VPU_EncOpen()  */
    PhysicalAddress *prdPrt,    /**<  [Output] A stream buffer read pointer for the current encoder instance */
    PhysicalAddress *pwrPtr,    /**<  [Output] A stream buffer write pointer for the current encoder instance */
    int * size                  /**<  [Output] A variable specifying the available space in bitstream buffer for the current encoder instance */
    );

/**
* @brief    Applications must let encoder know how much bitstream has been transferred from
the address obtained from VPU_EncGetBitstreamBuffer(). By just giving the size
as an argument, API automatically handles pointer wrap-around and updates the read
pointer.
* @return
@verbatim
*RETCODE_SUCCESS* ::
Operation was done successfully. That means the current encoder instance was closed
successfully.

*RETCODE_INVALID_HANDLE* ::
This means the given handle for the current API function call, pHandle, was invalid.
This return code might be caused if
+
--
* pHandle is not a handle which has been obtained by VPU_EncOpen().
* pHandle is a handle of an instance which has been closed already, etc.
--
*RETCODE_INVALID_PARAM* ::
The given argument parameter, size, was invalid, which means size is larger than
the value obtained from VPU_EncGetBitstreamBuffer().
@endverbatim
*/
RetCode wave420l_VPU_EncUpdateBitstreamBuffer(
    EncHandle handle,   /**< [Input] An encoder handle obtained from VPU_EncOpen() */
    int size            /**< [Input] A variable specifying the amount of bits being filled from bitstream buffer for the current encoder instance */
    );

#ifdef __cplusplus
}
#endif

#endif

