/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/*!
 ***********************************************************************
 *
 * \file
 *		wave5_pre_define.h : This file is part of libvpu
 * \date
 *		2015/05/13
 * \brief
 *		vpu pre-defines
 *
 ***********************************************************************
 */
#ifndef VPU_PRE_DEFINE_H
#define VPU_PRE_DEFINE_H

#include "wave5error.h"

/************************************************************************

	Check Compiler Preprocessor
		- defines on preprocessor of arm compiler for generating lib.
		- TCC_ONBOARD

************************************************************************/
#if defined(__arm__) || defined(__aarch64__) || defined(__arm) // Linux or ADS or WinCE
	#ifndef ARM
		#define ARM
	#endif
#endif
//#define ARM
#ifdef ARM
	#if	!defined( TCC_ONBOARD )
//	#	Error in preprocessor, cannot find TCC_ONBOARD definition
//	#	It must be defined TCC_ONBOARD in arm preprocessor
	#endif//TCC_ONBOARD
#endif//ARM


/************************************************************************

	PREFIX-DEFINES

*************************************************************************/

#define STREAM_ENDIAN                       0	// 0 (64 bit little endian), 1 (64 bit big endian), 2 (32 bit swaped little endian), 3 (32 bit swaped big endian)


#ifdef ARM
#	if defined(__GNUC__) || defined(__LINUX__)
#		define ARM_LINUX
#	elif defined(_WIN32_WCE)
#		define ARM_WINCE
//#	else
//#		define ARM_ADS
#	else
#		define ARM_RVDS
#	endif
#endif

//------------------------------------------------------
// architecture and ASM
//------------------------------------------------------
#ifdef ARM
//#	define ARM_ASM
#endif

//------------------------------------------------------
// pre-define align
//------------------------------------------------------
#ifdef ARM
#	define PRE_ALIGN_BYTES              4
#else//WIN32
#	define PRE_ALIGN_BYTES              8 // or 16 (SSE)
#endif

//------------------------------------------------------
// Telechips chip only
//------------------------------------------------------
#ifndef TCC_ONBOARD
#define TCC_ONBOARD
#endif

#ifdef TCC_ONBOARD

	#define TCC_HEVC___DEC_SUPPORT      1	// [DEC] HEVC   0: Not support, 1: support
	#define TCC_VP9____DEC_SUPPORT      1	// [DEC] VP9    0: Not support, 1: support

	#define PROFILE_DECODING_TIME_INCLUDE_TICK

	#define VPU_4K_D2_BUILD_RELEASE

	#define FIX_BUG__IGNORE_DECODE_CODEC_FINISH	//In case of codec_finish in the decode step, it does not return as an error.

	#define ENABLE_FORCE_ESCAPE
	#ifdef ENABLE_FORCE_ESCAPE
		#define ENABLE_FORCE_ESCAPE_2  //2019.09.27
	#endif

	#define ENABLE_DISABLE_AXI_TRANSACTION_TELPIC_147

#endif

#ifdef ENABLE_FORCE_ESCAPE_2
	#define MACRO_WAVE5_GDI_BUS_WAITING_EXIT(CoreIdx, X)	{ int loop=X; for(;loop>0;loop--){ if(Wave5ReadReg(CoreIdx, W5_GDI_BUS_STATUS) == 0x738) break; if( gWave5InitFirst_exit > 0 ){ loop = -1; break; } } if( loop <= 0 ){ WAVE5_dec_reset(NULL, (0 | (1<<16))); return RETCODE_CODEC_EXIT;} }
#else
	#define MACRO_WAVE5_GDI_BUS_WAITING_EXIT(CoreIdx, X)	{ int loop=X; for(;loop>0;loop--){if(Wave5ReadReg(CoreIdx, W5_GDI_BUS_STATUS) == 0x738) break;} if( loop <= 0 ){unsigned int ctrl; unsigned int data = 0x00;	unsigned int addr = W5_GDI_BUS_CTRL; Wave5WriteReg(CoreIdx, W5_VPU_FIO_DATA, data); ctrl  = (addr&0xffff);	ctrl |= (1<<16); Wave5WriteReg(CoreIdx, W5_VPU_FIO_CTRL_ADDR, ctrl); return RETCODE_CODEC_EXIT;}}
#endif

#define DISABLE_WAVE5_SWReset_AT_CODECEXIT
#define ENABLE_LIBSWRESET_CLOSE		// Force vpu lib. initialization command(close step option : param2 = 0x10, 2019.08.12)

#define USE_SEC_AXI
#ifdef USE_SEC_AXI
	//#define TEST_USE_SECAXI_SRAM
#endif

#define USE_SEC_AXI_SRAM_BASE_ADDR	    0xF0000000	// SRAM0 Base         : 0xF0000000 ~ 0xF000FFFF
							// SRAM1 ~ SRAM4 Base : 0xF1000000 ~ 0xF103FFFF

//#define USE_INTERRUPT_CALLBACK // [20191203] Go to polling besides DEC_PIC and W5_INT_BSBUF_EMPTY

#define VPU_BUSY_CHECK_COUNT	            0x07FFFFFF

#define LION_DOLPHINPLUS_FW_ADDRESS         0x2DA00000

#define USER_DATA_SKIP

// If the next step command is executed while the seq. header init. step has not been executed yet, an error is returned.(RETCODE_WRONG_CALL_SEQUENCE)
#define CHECK_SEQ_INIT_SUCCESS

// Prevent dec and disp status from being output as success when RETCODE_CODEC_FINISH occurs during flush command execution
// Since flush command is executed, decoding is not performed, so dec status should be FAIL.
// If it is RETCODE_CODEC_FINISH, all internal buffered disp indexes have been removed and there is nothing more to output, so the disp status should be FAIL.
#define FLUSH_CODEC_FINISH_RET_STATUS_FAIL

// Option for immediate or delayed output of decIdx when I-frame search mode is in operation [TELPIC-112]
//  decParam.skipframeDelayEnable = 1 : After delay, display index is printed (m_iSkipFrameMode & (1<<4))
//  decParam.skipframeDelayEnable = 0 : The display index is output immediately without delay
#define SUPPORT_NON_IRAP_DELAY_ENABLE

#define DISABLE_WAIT_POLLING_USLEEP_SENDQUERY
#define ENABLE_WAIT_POLLING_USLEEP_AFTER        500	// Add sleep after polling 500 times only in Wave5VpuDecode() function.
#define ENABLE_WAIT_POLLING_USLEEP			//Checks in 0.5 msec units, but returns with a timeout if there is no response for a total of 200 msec.

#if defined(ENABLE_WAIT_POLLING_USLEEP) || defined(ENABLE_WAIT_POLLING_USLEEP_AFTER)
	#define VPU_BUSY_CHECK_USLEEP_DELAY_MAX 550  //0.55 msec
	#define VPU_BUSY_CHECK_USLEEP_DELAY_MIN 500  //0.50 msec
	#define VPU_BUSY_CHECK_USLEEP_CNT       400  //200~220 msec
	#define VPU_BUSY_CHECK_USLEEP_CNT_1MSEC 3    // 1msec
#endif

#define DISABLE_RC_DPB_FLUSH

//#define ADD_FBREG_2
#define ADD_FBREG_3

//#define TEST_OUTPUTINFO

//#define TEST_FULLFB_SEQCHANGE_REMAINING

#define TEST_COMPRESSED_FBCTBL_INDEX_SET	// compressed[0], fbcY[0], fbcC[0], compressed[1], fbcY[1], fbcC[1].........

#define TEST_INTERRUPT_REASON_REGISTER

#define TEST_FLUSH_POLLING

#define TEST_BUFFERFULL_POLLTIMEOUT

#define TEST_SUPERFRAME_DECIDX_OUT

#define TEST_HEVC_RC_CROPINFO_REGISTER_READ
#define TEST_HEVC_RC_FBREG_SKIP

#define TEST_SEQCHANGEMASK_ZERO

#define TEST_FLUSH_TELPIC_110

#define TEST_BITBUFF_FLUSH_INTERRUPT
#define TEST_CQ2_SF_SKIPMODE	// Modify CQ2 SuperFrame Decoding Routine

//------------------------------------------------------
// CQ2 define ------------------------------------------
//------------------------------------------------------
#define TEST_CQ2_DEC_SKIPFRAME
//#define TEST_CQ2_DEC_SKIPFRAME_CQCOUNT
#define TEST_CQ2_CLEAR_INTO_DECODE
#define ENABLE_TCC_WAVE410_FLOW             0	// 1 : for performance measurement

#define TEST_ADAPTIVE_CQ_NUM
//#define CQ_BUFFER_RESILIENCE //need to test for VP9
#define CQ_BUFFER_RESILIENCE_HEVC_QUEUEING_FAILURE
//------------------------------------------------------

//------------------------------------------------------
// Debugginf info --------------------------------------
//------------------------------------------------------
#define DEBUG_DEC_SEQUENCE	            (1<<0)
#define DEBUG_DEC_ERRORMB	            (1<<1)
#define DEBUG_DEC_INTERRUPT	            (1<<2)
#define DEBUG_DEC_RINGBUFMODE	            (1<<4)
#define DEBUG_DEC_NAL_PARSING	            (1<<5)
//------------------------------------------------------

//------------------------------------------------------
// TEST LOG ENABLE -------------------------------------
//------------------------------------------------------
#ifdef VPU_4K_D2_BUILD_RELEASE
#else
	//#define ENABLE_ERRORMB_LOG
	//#define ENABLE_VUI_SEI_LOG
	//#define ENABLE_FW_VERSION_LOG
	//#define ENABLE_FBADDR_LOG
	//#define ENABLE_DEC_CYCLE_LOG
	//#define ENABLE_VDI_LOG
	#if defined(ENABLE_VDI_LOG)
		#define ENABLE_VDI_LOG_HALF //2019.08.03
		#ifdef ENABLE_VDI_LOG_HALF
			#define ENABLE_VDI_LOG_ONE_LINE
		#endif
		#define ENABLE_SENDQUERY_CHECK0809_TELPIC_134   //2019.08.09 Wave5WriteReg(instance->coreIdx, W5_RET_SUCCESS, 0xffffffff); log  on SendQuery function)
	#endif
	//#define ENABLE_VDI_STATUS_LOG //Debugging information for hangups
	//#define ENABLE_CQ_NUM_LOG
#endif
//------------------------------------------------------

//------------------------------------------------------
// for FPGA Test
//------------------------------------------------------
#if !defined(TCC_ONBOARD) && defined(_WIN32)
	#define PLATFORM_FPGA
#endif

#define EVB_BUILD
#define HPI_DEVICE_TYPE PCI2HPI

#ifdef TCC_ONBOARD
	#undef  EVB_BUILD
	#undef  HPI_DEVICE_TYPE
	#define HPI_DEVICE_TYPE 0xff
#endif

#define MAX_SLEEP_COUNT 100000

//------------------------------------------------------
// Decoding Options
//	unsigned int m_uiDecOptFlags in vpu_4K_D2_dec_init_t structure
//		WAVE5_WTL_DISABLE                      0        // (default)
//		WAVE5_WTL_ENABLE                       1        // WTL Enable
//		SEC_AXI_BUS_DISABLE                    ( 1<<1)  // SecAxi Disable
//		WAVE5_HEVC_REORDER_DISABLE             ( 1<<2)  // reorder disable only for HEVC, (default) reorder enable
//		WAVE5_10BITS_DISABLE                   ( 1<<3)  // 10 to 8 bits Output Enable
//		WAVE5_nv21_ENABLE                      ( 1<<4)  // NV21 Enable
//		WAVE5_AFBC_ENABLE                      ( 1<<5)  // AFBC Enable
//		WAVE5_AFBC_FORMAT                      ( 1<<6)  // AFBC Format
//		FW_WRITING_DISABLE                     ( 1<<7)  // FW writing disable
//		SEC_AXI_BUS_ENABLE_SRAM                ( 1<<8)  // SecAxi Disable
//		USE_MAX_INSTANCE_NUM_CTRL              (15<<11) // instance num.(4bits)
//		WAVE5_RESOLUTION_CHANGE                ( 1<<16) // alloc. max. resolution for changing resolution
//		WAVE5_CQ_DEPTH                         (15<<17) // Command Queue Depth
//		WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE ( 1<<26) // Sequence Init buffer change enable
//------------------------------------------------------
#define WAVE5_WTL_ENABLE_SHIFT              0 // WTL Enable
#define SEC_AXI_BUS_DISABLE_SHIFT           1 // Disable sec. AXI bus
#define WAVE5_HEVC_REORDER_DISABLE_SHIFT    2	// reorder disable only for HEVC, (default) reorder enable
#define WAVE5_10BITS_DISABLE_SHIFT          3 // 10 bits Output Enable
#define WAVE5_NV21_ENABLE_SHIFT             4 // NV21 Enable
#define WAVE5_AFBC_ENABLE_SHIFT             5 // AFBC Enable
#define WAVE5_AFBC_FORMAT_SHIFT             6 // AFBC Format
#define FW_WRITING_DISABLE_SHIFT            7 // FW writing disable
#define SEC_AXI_BUS_ENABLE_SRAM_SHIFT       8 // Enable SRAM for sec. AXI bus
#define USE_MAX_INSTANCE_NUM_CTRL           (15<<11 )
#ifdef USE_MAX_INSTANCE_NUM_CTRL
	#define USE_MAX_INSTANCE_NUM_CTRL_SHIFT  11
#endif

#define WAVE5_RESOLUTION_CHANGE             (1<<16)	// alloc. max. resolution for changing resolution
							// use maximum stride, if the resolution is less than
							// frame buffer size(use max. stride), the output shall be cropped.
#define WAVE5_CQ_DEPTH_SHIFT                17	// Command Queue Depth

#define WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE	(1<<26)	// SEQ_INIT bitstream buffer change enable
#ifdef WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE
	#define WAVE5_SEQ_INIT_BS_BUFFER_CHANGE_ENABLE_SHIFT	26
#endif

//------------------------------------------------------
// Additional info. (summary)
//
// [decoder]
// unsigned int m_Reserved[15] in vpu_4K_D2_dec_init_t
//		m_Reserved[0]: CNM FW Size
//		m_Reserved[1]: CNM FW Memory
//
// unsigned int m_Reserved[15] in vpu_4K_D2_dec_initial_info_t
//		m_Reserved[0]: RTL date info
//		m_Reserved[1]: RTL Revision Info
//
// unsigned int m_Reserved[37] in vpu_4K_D2_dec_output_info_t
//		m_Reserved[3]: decoded YUV output bitdepth
//		m_Reserved[4]: display YUV output bitdepth
//		m_Reserved[5]: decodedPOC
//		m_Reserved[6]: displayPOC
//		m_Reserved[7]: ref missing frame
//		m_Reserved[19]: DECODE return value
//		m_Reserved[20]: GetOutputInfo return value
//
// unsigned int m_Reserved[19] in vpu_4K_D2_dec_MapConv_info_t
//		m_Reserved[0]: codec standard : HEVC(15), VP9(16)
//------------------------------------------------------



/************************************************************************

	MACROS(DEBUGGING and ALIGNMENT)

*************************************************************************/

//------------------------------------------------------
// macros for debugging and alignment
//------------------------------------------------------
#if defined(ARM_ADS) || defined(ARM_RVDS)		// ARM ADS & RVDS
#	define inline __inline
#	define ALIGNED __align(PRE_ALIGN_BYTES)
#elif defined(ARM_WINCE)				// ARM WinCE
#	define inline __inline
#	define ALIGNED //__alignof(PRE_ALIGN_BYTES) : //!< \todo: FIXME
#elif defined(ARM_LINUX) || defined(__LINUX__)	// Linux
#	define DECLARE_ALIGNED( n ) __attribute__((aligned(n)))
#	define ALIGNED __attribute__((aligned(PRE_ALIGN_BYTES)))
#else//if defined(WIN32) && !defined(ARM_WINCE)
#	define inline __inline
#	define DECLARE_ALIGNED( n ) __declspec(align(n))
#	define ALIGNED __declspec(align(PRE_ALIGN_BYTES))
#endif

/*
Visual C++ 6		: 1200
Visual C++ .NET		: 1300
Visual C++ .NET 2003: 1310
Visual C++ 2005		: 1400
*/
#if ( _MSC_VER >= 1400 ) //
	#pragma warning(disable:4996) //: _CRT_SECURE_NO_WARNINGS
#endif

#endif
