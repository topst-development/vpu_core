/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

 /*!
 ***********************************************************************
 *
 * \file
 *		wave420l_pre_define.h : This file is part of libvpu
 * \date
 *		2020/04/24
 * \brief
 *		hevc encoder pre-defines
 *
 ***********************************************************************
 */

#ifndef _VPU_HEVC_ENC_PRE_DEFINE_H_
#define _VPU_HEVC_ENC_PRE_DEFINE_H_

#include "TCC_VPU_HEVC_ENC.h"
#include "wave420l_error.h"


/*
 ***********************************************************************
 *
 *	Check Compiler Preprocessor
 *		- defines on preprocessor of arm compiler for generating lib.
 *		- TCC_ONBOARD
 *
 ***********************************************************************
 */

#if defined(__arm__) || defined(__arm)
	#if defined(WAVE420L_ARM_ARCH)
		#if defined(__aarch64__)
			#if WAVE420L_ARM_ARCH == 64
				Error(ARM_AARCH64!!!)
			#endif
		#else
			#if WAVE420L_ARM_ARCH == 32
				Error(ARM_AARCH32!!!)
			#endif
		#endif
	#else
		#if defined(__aarch64__)
			#define WAVE420L_ARM_ARCH 64
		#else
			#define WAVE420L_ARM_ARCH 32
		#endif
	#endif
#else
	#define WAVE420L_ARM_ARCH 64
#endif

#if defined(WAVE420L_ARM_ARCH)
	#if defined(WIN32)
		#define TCC_SW_SIM       // Software simulation environment
	#else
	#if !defined(TCC_ONBOARD)
		#define TCC_ONBOARD      // Telechips board environment
	#endif
	#endif

	#if defined(TCC_ONBOARD)
		#if defined(__GNUC__) || defined(__LINUX__)	// AR M Linux
			#define ARM_LINUX
		#else
			Error(ARM_LINUX);
		#endif
	#else
		#define WAVE420L_SW_SIM
	#endif
#endif

/*
 ***********************************************************************
 *
 *	pre-define align
 *
 ***********************************************************************
 */

#ifdef WAVE420L_ARM_ARCH
#	define PRE_ALIGN_BYTES 4
#else//WIN32
#	define PRE_ALIGN_BYTES 8 // or 16 (SSE)
#endif


/*
 ***********************************************************************
 *
 *	Config.
 *
 ***********************************************************************
 */
#define VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE 8

/*
 ***********************************************************************
 *
 *	Stream Endian control
 *
 ***********************************************************************
 */
#define STREAM_ENDIAN           0       // 0 (64 bit little endian), 1 (64 bit big endian), 2 (32 bit swaped little endian), 3 (32 bit swaped big endian)

/*
 ***********************************************************************
 *
 *	Wave420L HW/FW version control
 *
 ***********************************************************************
 */

/*
 ***********************************************************************
 *
 *	WAVE420L FIRMWARE Download control
 *  TCC803x 0x20000000 ~ 0xBFFFFFFF R.3 DRAM (2.5 GB)
 *  TCC805x 0x20000000 ~ 0xBFFFFFFF R.7 DRAM (Up to 2.5 GB)
 *
 ***********************************************************************
 */
#define LION_DOLPHINPLUS_FW_ADDRESS 0x2DA00000

#define USE_EXTERNAL_FW_WRITER
//	#define TEST_CNM_FW_DOWN	// CNM FW In Test

/*
 ***********************************************************************
 *
 *	Secondary AXI control
 *
 ***********************************************************************
 */
#define USE_SEC_AXI
#ifdef USE_SEC_AXI
    //refer to Encoding Options Control  : SEC_AXI_BUS_DISABLE_SHIFT
#endif


/*
 ***********************************************************************
 *
 * Work Memory map and control
 *
 *   ADDR_TEMP_BASE   W4_ADDR_TEMP_BASE    pEncInfo->vbTemp.phys_addr     1M : 128KB*8
 *   ADDR_WORK_BASE   W4_ADDR_WORK_BASE    pEncInfo->vbWork.phys_addr      128KB
 *   ADDR_SEC_AXI     W4_ADDR_SEC_AXI      pEncInfo->secAxiInfo.bufBase    256KB
 *   BS_START_ADDR    W4_BS_START_ADDR     pEncInfo->streamBufStartAddr
 *   ADDR_CODE_BASE   W4_ADDR_CODE_BASE
 *   ------------------------------------------------------  VPU_HEVC_ENC_SIZE_BIT_WORK
 *       + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE
 *       + VPU_HEVC_ENC_STACK_SIZE
 *       + VPU_HEVC_ENC_TEMPBUF_SIZE          = m_BitWorkAddr[PA] + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + WAVE4_STACK_SIZE;
 *       + VPU_HEVC_ENC_SEC_AXI_BUF_SIZE      = m_BitWorkAddr[PA] + VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + WAVE4_TEMPBUF_SIZE + WAVE4_STACK_SIZE;
 *   ------------------------------------------------------  VPU_HEVC_ENC_WORKBUF_SIZE [0...n]
 *       VPU_HEVC_ENC_WORKBUF_SIZE [0]        = m_BitWorkAddr[PA] + VPU_HEVC_ENC_SIZE_BIT_WORK
 *        ...
 *       VPU_HEVC_ENC_WORKBUF_SIZE [n]
 *   ------------------------------------------------------
 *
 ***********************************************************************
 */


/*
 ***********************************************************************
 *
 * Interrupt or polling 	control
 *
 ***********************************************************************
 */
#define USE_HEVC_INTERRUPT
#if defined(USE_HEVC_INTERRUPT)
#  define INT_ENABLE_FLAG_CMD_SET_PARAM	1  //TELMON-56  wave420l_Wave4BitIssueCommand(instance, SET_PARAM); W4_INT_SET_PARAM = 1  SETPARAM bit position is [2] in user guide Table 1.49
#  define INT_ENABLE_FLAG_CMD_ENC_PIC	1
#else  // If you only use polling (Do not use interrupts)
#  define INT_ENABLE_FLAG_CMD_SET_PARAM	0
#  define INT_ENABLE_FLAG_CMD_ENC_PIC	0
#endif

//#define USE_WAVE420L_SWReset_AT_CODECEXIT


/*
 ***********************************************************************
 *
 *	VUI
 *
 ***********************************************************************
 */
// CMD_ENC_VUI_PARAM_FLAGS (0x015C) : VUI parameter flag
// Bit     Name                                Type Function                                                                           Reset Value
// [31:10] RSVD                                R/W  Reserved                                                                           0x0
// [ 9   ] BITSTREAM_RESTRICTION_FLAG          R/W  A flag whether to insert bitstream_restriction_flag syntax of VUI parameters       0x0
// [ 8   ] DEFAULT_DISPLAY_WINDOW_FLAG         R/W  A flag whether to insert default_display_window_flag syntax of VUI parameters      0x0
// [ 7   ] CHROMA_LOC_INFO_PRESENT_FLAG        R/W  A flag whether to insert chroma_loc_info_present_flag syntax of VUI parameters     0x0
// [ 6   ] COLOUR_DESCRIPTION_PRESENT_FLAG     R/W  A flag whether to insert colour_description_present_flag syntax of VUI parameters  0x0
// [ 5   ] VIDEO_SIGNAL_TYPE_PRESENT_FLAG      R/W  A flag whether to insert video_signal_type_present_flag syntax of VUI parameters   0x0
// [ 4   ] OVERSCAN_INFO_PRESENT_FLAG          R/W  A flag whether to insert overscan_info_present_flag syntax of VUI parameters       0x0
// [ 3   ] ASPECT_RATIO_INFO_PRESENT_FLAG      R/W  A flag whether to insert aspect_ratio_info_present_flag syntax of VUI parameters   0x0
// [ 2: 1] RSVD                                R/W  Reserved                                                                           0x0
// [ 0   ] NEUTRAL_CHROMA_INDICATION_FLAG      R/W  A flag whether to insert neutral_chroma_indication_flag syntax of VUI parameters   0x0
#define HEVC_VUI_BITSTREAM_RESTRICTION_FLAG_POS       9	// N/A
#define HEVC_VUI_DEFAULT_DISPLAY_WINDOW_FLAG_POS      8
#define HEVC_VUI_CHROMA_LOC_INFO_PRESENT_FLAG_POS     7
#define HEVC_VUI_COLOUR_DESCRIPTION_PRESENT_FLAG_POS  6
#define HEVC_VUI_VIDEO_SIGNAL_TYPE_PRESENT_FLAGP_POS  5
#define HEVC_VUI_OVERSCAN_INFO_PRESENT_FLAG_POS       4
#define HEVC_VUI_ASPECT_RATIO_INFO_PRESENT_FLAG_POS   3
#define HEVC_VUI_NEUTRAL_CHROMA_INDICATION_FLAG_POS   0

// CMD_ENC_VUI_SAR_SIZE (0x0164)
// Bit     Name                                Type Function                                                 Reset Value
// [31:16] SAR_HEIGHT                          R/W  sar_height (valid when aspect_ratio_idc is equal to 255) 0x0
// [15: 0] SAR_WIDTH                           R/W  sar_width (valid when aspect_ratio_idc is equal to 255)  0x0
#define HEVC_VUI_SAR_HEIGHT_POS                       16
#define HEVC_VUI_SAR_WIDTH_POS                        0

// CMD_ENC_VUI_VIDEO_SIGNAL (0x016C)	// VUI video signal
// Bit     Name                                Type Function                Reset Value
// [31:28] RSVD                                R/W Reserved                 0x0
// [27:20] MATRIX_COEFFS                       R/W matrix_coeffs            0x0
// [19:12] TRANSFER_CHARACTERISTICS            R/W transfer_characteristics 0x0
// [11: 4] COLOUR_PRIMARIES                    R/W colour_primaries         0x0
// [ 3   ] VIDEO_FULL_RANGE_FLAG               R/W video_full_range_flag    0x0
// [ 2: 0] VIDEO_FORMAT                        R/W video_format             0x0
#define HEVC_VUI_MATRIX_COEFFS_POS                    20 // 8bits : [27:20] matrix_coeffs
#define HEVC_VUI_TRANSFER_CHARACTERISTICS_POS         12 // 8bits : [19:12] transfer_characteristics
#define HEVC_VUI_COLOUR_PRIMARIES_POS                 4  // 8bits : [11: 4] colour_primaries
#define HEVC_VUI_VIDEO_FULL_RANGE_FLAG_POS            3  // 1bit  : [ 3   ] video_full_range_flag
#define HEVC_VUI_VIDEO_FORMAT_POS                     0  // 3bits : [ 2: 0] video_format

// CMD_ENC_VUI_CHROMA_SAMPLE_LOC      (0x0170)
// Bit     Name                                Type Function                             Reset Value
// [31:16] CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD R/W chroma_sample_loc_type_bottom_fxield  0x0
// [15: 0] CHROMA_SAMPLE_LOC_TYPE_TOP_FIELD    R/W chroma_sample_loc_type_top_field      0x0
#define HEVC_VUI_CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD_POS 16
#define HEVC_VUI_CHROMA_SAMPLE_LOC_TYPE_TOP_FIELD_POS    0


// CMD_ENC_VUI_DISP_WIN_LEFT_RIGHT (0x0174)
// Bit     Name                                Type Function                  Reset Value
// [31:16] DEF_DISP_WIN_RIGHT_OFFSET           R/W def_disp_win_right_offset  0x0
// [15: 0] DEF_DISP_WIN_LEFT_OFFSET            R/W def_disp_win_left_offset   0x0
#define HEVC_VUI_DEF_DISP_WIN_RIGHT_OFFSET_POS        16
#define HEVC_VUI_DEF_DISP_WIN_LEFT_OFFSET_POS         0

// CMD_ENC_VUI_DISP_WIN_TOP_BOT (0x0178)
// Bit     Name                                Type Function                   Reset Value
// [31:16] DEF_DISP_WIN_BOTTOM_OFFSET          R/W def_disp_win_bottom_offset  0x0
// [15: 0] DEF_DISP_WIN_TOP_OFFSET             R/W def_disp_win_top_offset     0x0
#define HEVC_VUI_DEF_DISP_WIN_BOTTOM_OFFSET_POS       16
#define HEVC_VUI_DEF_DISP_WIN_TOP_OFFSET_POS          0


/*
 ***********************************************************************
 *
 *	Definitions for Testing
 *
 ***********************************************************************
 */

#define HEVCENC_REPORT_MVINFO
// hevc_enc_input_t and hevc_enc_output_t structures
//    void *mvInfoBufAddr[2];  // physical[0] and virtual[1] address for reporting of MV info.
//    int mvInfoBufSize;       // Buffer size
//    int mvInfoFormat;        // MV info. format
//                             //   0 : (default)nothing,
//                             //   1 : convert (little endian, 16bits per X or Y),
//                             //   2 : copy (big endian, 12bits per X or Y)


/*
 ***********************************************************************
 *
 * Control bus transaction completion waiting function
 *
 ***********************************************************************
 */

#define MACRO_WAVE4_GDI_BUS_WAITING_EXIT(CoreIdx, X)	{ int loop=X; for(;loop>0;loop--){if(VpuReadReg(CoreIdx, W4_GDI_BUS_STATUS) == 0x738) break;} if( loop <= 0 ){unsigned int ctrl; unsigned int data = 0x00;	unsigned int addr = W4_GDI_BUS_CTRL; VpuWriteReg(CoreIdx, W4_VCPU_FIO_DATA, data); ctrl  = (addr&0xffff);	ctrl |= (1<<16); VpuWriteReg(CoreIdx, W4_VCPU_FIO_CTRL, ctrl); return RETCODE_CODEC_EXIT;}}


/*
 ***********************************************************************
 *
 *	3DNR control
 *
 ***********************************************************************
 */
//#define CTRL_3DNR_FEATURE_TEST_DEFAULT  //for test with default values
#if defined(CTRL_3DNR_FEATURE_TEST_DEFAULT)	//TEST_3DNR_DEFAULT_ON
// encoder IP employs temporal noise reduction technology also called 3DNR (3D Noise Reduction) for better quality of image under low light.
// Video noise appears easily in a low light level. It is an important issue in surveillance camera, since images are captured often under
// dark condition or indoor and such can make interest of object or person hard to be identified. (TELMON-52)
//
// encoder IP includes noise reduction algorithm which is accomplished at the hardware level.
// Noise can be identified in the course of encoding blocks and filtered for each color component Y, Cb, and Cr of a frame.
// By eliminating noises while a picture is being encoded, it can achieve not only bitrate savings, not only enhancement of subjective quality.
// In addtion, noise reduction is performed in the encoding process, so additional read/write data bandwidth is not required.
// These are noise reduction related parameters with the default values in the followings:
//        EnNoiseReductionY : 0
//        EnNoiseReductionCb : 0
//        EnNoiseReductionCr : 0
//        EnNoiseEst : 1
//        NoiseSigmaY : 0
//        NoiseSigmaCb : 0
//        NoiseSigmaCr : 0
//        IntraNoiseWeightY : 7
//        IntraNoiseWeightCb : 7
//        IntraNoiseWeightCr : 7
//        InterNoiseWeightY : 4
//        InterNoiseWeightCb : 4
//        InterNoiseWeightCr : 4
//
// 3DNR basically operates in the transform domain. Transform coefficients are filtered by the noise sigma that is estimated and
// determined with the previous block by the embedded noise detection algorithm.
// If HOST(User) is capable of finding the best sigma value, it can work with the HOST-given noise sigma.
//
// If EnNoiseEst is 1, it automatically estimates noise over transformed coefficients of block.
// If EnNoiseEst is 0, it receives noise sigma of each component per picture from HOST - NoiseSigmaY, NoiseSigmaCb and NoiseSigmaCr.
// In case that HOST has its own appropriate noise estimation model, EnNoiseEst can be disabled.
// HOST can also give the filter strength (NoiseWeight) for each inter/intra prediction mode and individual color component.
//
//   Filtered DCT coefficient = DCT coefficient - sigma x weight
//
typedef struct wave420l_nrConfig_t {
        unsigned int nrYEnable;           //!< It enables noise reduction algorithm to Y component.
        unsigned int nrCbEnable;          //!< It enables noise reduction algorithm to Cb component.
        unsigned int nrCrEnable;          //!< It enables noise reduction algorithm to Cr component.
        unsigned int nrNoiseEstEnable;    //!< It enables noise estimation for reduction. When this is disabled, noise estimation is carried out ouside VPU.
        unsigned int nrNoiseSigmaY;       //!< It specifies Y noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). The value shall be in the range of 0 to 255.
        unsigned int nrNoiseSigmaCb;      //!< It specifies Cb noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). The value shall be in the range of 0 to 255.
        unsigned int nrNoiseSigmaCr;      //!<It specifies Cr noise standard deviation if no use of noise estimation (nrNoiseEstEnable is 0). The value shall be in the range of 0 to 255.

        unsigned int nrIntraWeightY;      //!< A weight to Y noise level for intra picture (0 ~ 31). nrIntraWeight/4 is multiplied to the noise level that has been estimated. This weight is put for intra frame to be filtered more strongly or more weakly than just with the estimated noise level.
        unsigned int nrIntraWeightCb;     //!< A weight to Cb noise level for intra picture (0 ~ 31)
        unsigned int nrIntraWeightCr;     //!< A weight to Cr noise level for intra picture (0 ~ 31)
        unsigned int nrInterWeightY;      //!< A weight to Y noise level for inter picture (0 ~ 31). nrInterWeight/4 is multiplied to the noise level that has been estimated. This weight is put for inter frame to be filtered more strongly or more weakly than just with the estimated noise level.
        unsigned int nrInterWeightCb;     //!< A weight to Cb noise level for inter picture (0 ~ 31)
        unsigned int nrInterWeightCr;     //!< A weight to Cr noise level for inter picture (0 ~ 31)
} wave420l_nrConfig_t;
#endif

/*
 ***********************************************************************
 *
 *	ulseep callback function control
 *
 ***********************************************************************
 */

//The polling method is checked every 0.5 msec through usleep, and a timeout of 200 msec is generated to quickly process recovery in case of Video IP hangup.(2019.08.20)
#define ENABLE_WAIT_POLLING_USLEEP

#define ENABLE_WAIT_POLLING_USLEEP_AFTER  500

#if	 defined(ENABLE_WAIT_POLLING_USLEEP) || defined(ENABLE_WAIT_POLLING_USLEEP_AFTER)
	#define VPU_BUSY_CHECK_USLEEP_DELAY_MAX 550	//0.55 msec
	#define VPU_BUSY_CHECK_USLEEP_DELAY_MIN 500	//0.50 msec
	#define VPU_BUSY_CHECK_USLEEP_CNT       400	//200~220 msec
	#define VPU_BUSY_CHECK_USLEEP_CNT_1MSEC 3	// 1msec
#endif

//#define MAX_SLEEP_COUNT 100000

/*
 ***********************************************************************
 * Encoding Options
 * unsigned int m_uiEncOptFlags in hevc_enc_init_t structure
 *	SEC_AXI_BUS_DISABLE			(1<<1)	// [    1] SecAxi Disable
 *	SEC_AXI_BUS_ENABLE_SRAM			(1<<2)	// [    2] SRAM SecAxi Disable
 *	FW_WRITING_DISABLE			(1<<7)	// [    7] FW writing disable
 *	VPU_HEVC_ENC_LIB_MAX_NUM_INSTANCE_CTRL	(1<<11)	// [14:11] Instance Number control
 *	TEST_3DNR_DEFAULT_ON			(1<<31)	// [31   ] 3DNR test with default values
 ***********************************************************************
 */
#define SEC_AXI_BUS_DISABLE_SHIFT               1 //!< Disable sec. AXI bus
#define SEC_AXI_BUS_ENABLE_SRAM_SHIFT	        2 //!< Enable SRAM for sec. AXI bus
//#define NV21_ENABLE_SHIFT                     4 //!< NV21 Enable
#define FW_WRITING_DISABLE_SHIFT                7 //!< FW writing disable
#define HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_SHIFT  11 //!< Instance Number control (default 4, up to 8
    #define HEVCENCOPT_MAX_INSTANCE_NUM_CTRL_BITS_MASK     0xF //!< 4bits
    //check MAX_NUM_INSTANCE 4 in vpuconfig.h
#define TEST_3DNR_DEFAULT_ON                   (1<<31)  //3DNR test with default values

/*
 ***********************************************************************
 *
 *  Encoder Additional info. (summary)
 *    unsigned int m_Reserved[ ] in hevc_enc_init_t
 *
 *    unsigned int m_Reserved[ ] in hevc_enc_initial_info_t
 *                 m_Reserved[2] Bit[30] : Secondary AXI used flag
 *
 ***********************************************************************
 */


/*
 ***********************************************************************
 *
 *	MACROS(DEBUGGING and ALIGNMENT)
 *
 ***********************************************************************
 */

	//------------------------------------------------------
	// macros for debugging and alignment
	//------------------------------------------------------
	#if defined(ARM_LINUX) || defined(__LINUX__)
	#	define DECLARE_ALIGNED( n ) __attribute__((aligned(n)))
	#	define ALIGNED __attribute__((aligned(PRE_ALIGN_BYTES)))
	#else//if defined(WIN32)
    #  if !defined(inline)
	#	    define inline __inline
    #  endif
	#	define DECLARE_ALIGNED( n ) __declspec(align(n))
	#	define ALIGNED __declspec(align(PRE_ALIGN_BYTES))
	#endif

    #define DEBUG_LEVEL_LOW 	(1<<0)
    #define DEBUG_LEVEL_MEDIUM	(1<<1)
    #define DEBUG_LEVEL_HIGH	(1<<2)

    #define VLOG_DISABLE
    #if defined(VLOG_DISABLE)
        #define VLOG(x,...) do { } while(0)
    #else
        #define VLOG LogMsg
    #endif

#endif//_VPU_HEVC_ENC_PRE_DEFINE_H_
