/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

/*!
 ***********************************************************************
 *
 * \file
 *		vpu_pre_define.h : This file is part of libvpu
 * \date
 *		2009/03/23
 * \brief
 *		vpu pre-defines
 *
 ***********************************************************************
 */
#ifndef VPU_PRE_DEFINE_HEADER
#define VPU_PRE_DEFINE_HEADER

#include "vpu_err.h"

/************************************************************************

	Check Compiler Preprocessor
	  - defines on preprocessor of arm compiler for generating lib.
	  - TCC_ONBOARD

************************************************************************/
#if defined(__arm__) || defined(__arm) || defined(__aarch64__) // Linux or ADS or WinCE
#	ifndef ARM
#		define ARM
#	endif
#endif

#ifdef ARM
#	if	!defined( TCC_ONBOARD )
//#		Error in preprocessor, cannot find TCC_ONBOARD definition
//#		It must be defined TCC_ONBOARD in arm preprocessor
#	endif//TCC_ONBOARD
#endif//ARM


/************************************************************************

		PREFIX-DEFINES

*************************************************************************/

#define STREAM_ENDIAN                             0
		// 0 (64 bit little endian), 1 (64 bit big endian),
		// 2 (32 bit swaped little endian), 3 (32 bit swaped big endian)

#ifndef NULL
#	define NULL                               0
#endif

#ifdef ARM
#	if defined(__GNUC__) || defined(__LINUX__)	// ARM Linux
#		define ARM_LINUX
#	elif defined(_WIN32_WCE)	// ARM WinCE
#		define ARM_WINCE
#	else
#		define ARM_RVDS	// ARM RVDS
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
#	define PRE_ALIGN_BYTES                    4
#else//WIN32
#	define PRE_ALIGN_BYTES                    8 // or 16 (SSE)
#endif

//------------------------------------------------------
// Telechips chip only
//------------------------------------------------------
#ifndef TCC_ONBOARD
#	define TCC_ONBOARD
#endif

#define ENABLE_FORCE_ESCAPE	// VPU_DecBitBufferFlush, SetParaSet


/*****************************************************************************************/
//   The simple polling method checks every 0.5 msec through usleep and generates a timeout of 200 msec
//   to speed up recovery in the event of a Video IP hangup.
//   In Linux, a process that does not respond for more than 20 seconds is considered halt.
//#define ENABLE_WAIT_POLLING_USLEEP //2019.08.27
#if defined(ENABLE_WAIT_POLLING_USLEEP)
#	define VPU_BUSY_CHECK_USLEEP_DELAY_MAX        550	//0.55 msec
#	define VPU_BUSY_CHECK_USLEEP_DELAY_MIN        500	//0.50 msec
#	define VPU_BUSY_CHECK_USLEEP_CNT              400	//200~220 msec
#	define VPU_BUSY_CHECK_USLEEP_CNT_1MSEC          2	//1.00 ~1.10 msec
#	define VPU_BUSY_CHECK_USLEEP_CNT_5MSEC         10	//5.00 ~5.50 msec
#	define VPU_BUSY_CHECK_USLEEP_DELAY_1MSEC_MAX 1100	//0.55 msec
#	define VPU_BUSY_CHECK_USLEEP_DELAY_1MSEC_MIN 1000	//0.50 msec
#endif


/* Macros */

#define MACRO_VPU_BIT_INT_ENABLE_RESET1 { if( VpuReadReg(BIT_INT_STS) != 0 ) { VpuWriteReg(BIT_INT_REASON, 0); VpuWriteReg(BIT_INT_CLEAR, 1); } else { VpuWriteReg(BIT_INT_ENABLE, 0); } }
#define MACRO_VPU_BIT_INT_ENABLE_RESET2 { if( VpuReadReg(BIT_INT_STS) != 0 ){ VpuWriteReg(BIT_INT_REASON, 0); VpuWriteReg(BIT_INT_CLEAR, 1); } }
#define MACRO_VPU_SWRESET               { RetCode iRet = 0; iRet = VPU_SWReset( SW_RESET_SAFETY ); while(VPU_IsBusy() && (iRet==0)) { if( VpuReadReg(BIT_CUR_PC) == 0 ) break; } }
#define MACRO_VPU_SWRESET_EXIT          { RetCode iRet = 0; iRet = VPU_SWReset( SW_RESET_SAFETY ); if(iRet != RETCODE_SUCCESS) return RETCODE_CODEC_EXIT; while(VPU_IsBusy()) { if( VpuReadReg(BIT_CUR_PC) == 0 ) break; } }

#define VPU_DELAY_US_1_LOOPCNT          0x20 //0x7FF

#ifdef ENABLE_WAIT_POLLING_USLEEP
#	define MACRO_VPU_DELAY_1MS_FOR_INIT           { if(vpu_usleep != NULL){ vpu_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_1MSEC_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_1MSEC_MAX); }else{ int loop=0; for(;loop<VPU_DELAY_US_1_LOOPCNT;loop++) { if(VpuReadReg(BIT_CODE_RUN+0x0FFC) == 0) break; }	} 	} //clk (activate: 0x5E4D3C2B, not activate: 0xA1B2C3D4)
#else
#	define MACRO_VPU_DELAY_1MS_FOR_INIT           { int loop=0; for(;loop<VPU_DELAY_US_1_LOOPCNT;loop++) { if(VpuReadReg(BIT_CODE_RUN+0x0FFC) == 0) break; } } //clk (activate: 0x5E4D3C2B, not activate: 0xA1B2C3D4)
#endif

#define MACRO_VPU_BUSY_WAITING_DEC_BUF_FLUSH(X)       { int loop=0; for(;loop<X;loop++){ if( VPU_IsBusy() == 0 )break; if(VpuReadReg(BIT_CUR_PC) == 0) return RETCODE_CODEC_EXIT; } if( X <= loop ) return RETCODE_MEMORY_ACCESS_VIOLATION; }
#ifdef ENABLE_WAIT_POLLING_USLEEP
#	define MACRO_VPU_BUSY_WAITING_EXIT(X)         { int cnt=0; int loop=X; for(;loop>0;loop--){ if(VPU_IsBusy() == 0) break; if(VpuReadReg(BIT_CUR_PC) == 0) return RETCODE_CODEC_EXIT; if( gInitFirst_exit > 0 ) return RETCODE_CODEC_EXIT; if(vpu_usleep != NULL){ vpu_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX); if( cnt > VPU_BUSY_CHECK_USLEEP_CNT ){ return RETCODE_CODEC_EXIT; } cnt++; } } if( loop <= 0 ) return RETCODE_CODEC_EXIT;  }
#	define MACRO_VPU_GDI_BUS_WAITING_EXIT(X)      { int cnt=0; int loop=X; for(;loop>0;loop--){ if(VpuReadReg(GDI_BUS_STATUS) == 0x77) break; if( gInitFirst_exit > 0 ) return RETCODE_CODEC_EXIT; if(vpu_usleep != NULL){ vpu_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX); if( cnt > VPU_BUSY_CHECK_USLEEP_CNT_5MSEC ){ return RETCODE_CODEC_EXIT; } cnt++; } } if( loop <= 0 ) return RETCODE_CODEC_EXIT; }  //5msec
#	define MACRO_VPU_BIT_SW_RESET_WAITING_EXIT(X) { int cnt=0; int loop=X; for(;loop>0;loop--){ if(VpuReadReg(BIT_SW_RESET_STATUS) == 0) break; if(VpuReadReg(BIT_CUR_PC) == 0x0) return RETCODE_CODEC_EXIT; if( gInitFirst_exit > 0 ) return RETCODE_CODEC_EXIT; if(vpu_usleep != NULL) { 	vpu_usleep(VPU_BUSY_CHECK_USLEEP_DELAY_MIN, VPU_BUSY_CHECK_USLEEP_DELAY_MAX); if( cnt > VPU_BUSY_CHECK_USLEEP_CNT_5MSEC ){ return RETCODE_CODEC_EXIT; }	cnt++; } } 	if( loop <= 0 ) return RETCODE_CODEC_EXIT; } //5msec
#else
#	define MACRO_VPU_BUSY_WAITING_EXIT(X)         { int loop=X; for(;loop>0;loop--){ if( VPU_IsBusy() == 0) break; if(VpuReadReg(BIT_CUR_PC) == 0) return RETCODE_CODEC_EXIT; } if( loop <= 0 ) return RETCODE_CODEC_EXIT; }
#	define MACRO_VPU_GDI_BUS_WAITING_EXIT(X)      { int loop=X; for(;loop>0;loop--){ if(VpuReadReg(GDI_BUS_STATUS) == 0x77) break; } if( loop <= 0 ) return RETCODE_CODEC_EXIT; }
#	define MACRO_VPU_BIT_SW_RESET_WAITING_EXIT(X) { int loop=X; for(;loop>0;loop--){ if(VpuReadReg(BIT_SW_RESET_STATUS) == 0) break; if(VpuReadReg(BIT_CUR_PC) == 0x0) return RETCODE_CODEC_EXIT; } if( loop <= 0 ) return RETCODE_CODEC_EXIT; }
#endif


/* Controls */

//#define ADD_VPU_INIT_SWRESET
// SWReset processing during VPU init.
//   (To temporarily correct the problem that the bit I/O H/W accelerator
//    between enc/dec in multi-instance is used differently)

#define ADD_VPU_CLOSE_SWRESET
#define ADD_SWRESET_VPU_HANGUP

#define ADD_BIT_INT_ENABLE_RESET
#ifndef ADD_BIT_INT_ENABLE_RESET
#	undef MACRO_VPU_BIT_INT_ENABLE_RESET1
#	define MACRO_VPU_BIT_INT_ENABLE_RESET1
#	undef MACRO_VPU_BIT_INT_ENABLE_RESET2
#	define MACRO_VPU_BIT_INT_ENABLE_RESET2
#endif

#define USE_VPU_SWRESET_CHECK_PC_ZERO
#define USE_VPU_SWRESET_BPU_CORE_ONLY	//cmd =  VPU_SW_RESET_BPU_CORE; Hangup during only interlace seek, unable to enter system Cause of problem


#define USER_OVERRIDE_MPEG4_PROFILE_LEVEL
#define USER_OVERRIDE_H264_PROFILE_LEVEL
#define USE_HOST_PROFILE_LEVEL

//#define TEST_INTERLACE_BOTTOM_TEST


/* bitstream mode test */
#define USE_VPU_DEC_RING_MODE_RDPTR_WRAPAROUND

//#define TEST_MVC_DEC	// Used only during MVC testing. Test using two files (00140_P0001.4113, 00140_P0001.4114)

#define SEQ_INIT_USERDATA_DISABLE

#define USERDATA_SIZE_CHECK

#define USE_EXTERNAL_FW_WRITER

#define TEST_WRPTR_UPDATE_TIMEOUT	// TMMT-273


//------------------------------------------------------
// Encoder
//------------------------------------------------------

#define USE_VPU_ENC_PUTHEADER_INTERRUPT

#define MJPEG_ENC_INCLUDE
//#define SUPPORT_MJPEG_ERROR_CONCEAL
#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
#	define MJPEG_ERROR_CONCEAL_DEBUG
#	define MJPEG_ERROR_CONCEAL_WORKAROUND
#endif

// SD805XL-1444  formal_CODA960_v3.1.3_TELBB-505_r237525_20210614.h
// Option to encode as IDR in CMD_ENC_SEQ_OPTION: bit[3](0: non-IDR, 1: IDR)
#define H264_ENC_IDR_OPTION

// Fix H.264 encoding noise when using CbCr interleaved mode by disabling raster tiled maps - SD805XA2-1721, TELBB-509
//#define H264_ENC_CBCRINTERL_TIELD_OFF


//------------------------------------------------------
// USE constraints : min. size, 16-byte align.
//------------------------------------------------------
#define VPU_ENC_MIM_HORIZONTAL                    96
#define VPU_ENC_MIM_VERTICAL                      16


/************************************************************************

		VPU CODEC (Decoder/Encoder) Options

*************************************************************************/

//------------------------------------------------------
// for GPL license
//------------------------------------------------------
#define MEM_FN_REPLACE_INTERNAL_FN	// If memset or memcpy does not come in as a callback, it is replaced with its own function.


//------------------------------------------------------
// RTL(HW) and FW version
//------------------------------------------------------
#define USE_HW_AND_FW_VERSION


//------------------------------------------------------
// use secondary AXI(SRAM) (F/W 20100527)
//  register : BIT_AXI_SRAM_USE , BIT_AXI_SRAM_BIT_ADDR , SET_SEC_AXI
//  SRAM  info
//   TCC892X :  64 Kbytes 0x10000000 - 0x1000FFFF (4K for DVFS)
//------------------------------------------------------

#define USE_SEC_AXI_SRAM
#ifdef USE_SEC_AXI_SRAM
#	define USE_SEC_AXI_SRAM_BASE_ADDR         0x10001000	//for 89xx/92xx/93xx

#	define USE_SEC_AXI_BIT
#	define USE_SEC_AXI_IPACDC
#	define USE_SEC_AXI_DBKY
#	define USE_SEC_AXI_OVL
#	define USE_SEC_AXI_BTP
#	define USE_SEC_AXI_DBKC
#	define BIT_INTERNAL_USE_BUF_SIZE_FULLHD    0x04400	// BIT_SIZE  = (MAX_WIDTH/16)*144						17408 Bytes (0x05000 4k align)
#	define IPACDC_INTERNAL_USE_BUF_SIZE_FULLHD 0x03C00	// IP_SIZE   = (MAX_WIDTH/16)*128						15360 Bytes (0x04000 4k align)
#	define DBKY_INTERNAL_USE_BUF_SIZE_FULLHD   0x07800	// DBKY_SIZE = (MAX_WIDTH/16)*256						30720 Bytes (0x08000 4k align)
#	define DBKC_INTERNAL_USE_BUF_SIZE_FULLHD   0x07800	// DBKC_SIZE = (MAX_WIDTH/16)*256						30720 Bytes (0x08000 4k align)
//#	define OVL_INTERNAL_USE_BUF_SIZE_FULLHD    0x02600	// OVL_SIZE  = (MAX_WIDTH/16)*80						 9728 Bytes
#	define OVL_INTERNAL_USE_BUF_SIZE_FULLHD    0x05000	// OVL_SIZE  = (MAX_WIDTH/16)*80						20480 Bytes (0x05000 4k align)
//#	define BTP_INTERNAL_USE_BUF_SIZE_FULLHD    0x01700	// BTP_SIZE  = {(MAX_WIDTH/256)*(MAX_HEIGHT/16) + 1}*6	 5888 Bytes(256bytes align, address)
#	define BTP_INTERNAL_USE_BUF_SIZE_FULLHD    0x01800	// BTP_SIZE  = {(MAX_WIDTH/256)*(MAX_HEIGHT/16) + 1}*6	 6144 Bytes(256bytes align, address) (0x02000 4k align)
//#	define USE_SEC_AXI_SIZE                    0x1AD00	//109824 bytes 108KB
#	define USE_SEC_AXI_SIZE                    0x1D800	//109824 bytes 108KB (0x20000 4k align)
#endif


//------------------------------------------------------
// Use PIC_RUN interrupt in VPU_DecStartOneFrame
//------------------------------------------------------
#define USE_CODEC_PIC_RUN_INTERRUPT_MOD //20100706
#define USE_CODEC_PIC_RUN_INTERRUPT
#ifdef USE_CODEC_PIC_RUN_INTERRUPT
#	define CODEC_INT_ENABLE_SEQ_INIT                (1<< 1)
#	define CODEC_INT_ENABLE_SEQ_END                 (1<< 2)
#	define CODEC_INT_ENABLE_PIC_RUN                 (1<< 3)
#	define CODEC_INT_ENABLE_SET_FRAME_BUF           (1<< 4)
#	define CODEC_INT_ENABLE_DEC_PARA_SET            (1<< 7)
#	define CODEC_INT_ENABLE_DEC_BUF_FLUSH           (1<< 8)
#	define CODEC_INT_ENABLE_EXT_BITSTREAM_BUF_EMPTY (1<<14)
#	define CODEC_INT_ENABLE_EXT_BITSTREAM_BUF_FULL  (1<<15)

#	define USE_CODEC_INT_ENABLE_BITS_SEQ_INIT (   CODEC_INT_ENABLE_SEQ_INIT )

#	define USE_CODEC_INT_ENABLE_BITS_RUN      (   CODEC_INT_ENABLE_PIC_RUN\
	                                            | CODEC_INT_ENABLE_EXT_BITSTREAM_BUF_EMPTY\
	                                            | CODEC_INT_ENABLE_EXT_BITSTREAM_BUF_FULL\
	                                          )

#	define USE_CODEC_INT_ENABLE_BITS_CLOSE    (   CODEC_INT_ENABLE_SEQ_END\
	                                            | CODEC_INT_ENABLE_PIC_RUN\
	                                          )

//#	define USE_CODEC_PIC_RUN_INTERRUPT_CLOSE
#endif

//------------------------------------------------------
//  Enc/Decoding Options
//	unsigned int m_uiEncOptFlags in enc_init_t structure
//	unsigned int m_uiDecOptFlags in dec_init_t structure
//	USE_MAX_INSTANCE_NUM_CTRL       (15<<11)	bit[11:14]
//	SEC_AXI_BUS_ENABLE_SRAM         ( 1<<21)	bit[21   ]	Use SRAM for sec. AXI bus in encoder
//	SEC_AXI_ENABLE_CTRL             ( 3<<21)	bit[21:22]	Use SRAM for sec. AXI bus in decoder
//------------------------------------------------------
#define USE_MAX_INSTANCE_NUM_CTRL                 (15<<11 )
#ifdef USE_MAX_INSTANCE_NUM_CTRL
#	define USE_MAX_INSTANCE_NUM_CTRL_SHIFT    11
#endif

//------------------------------------------------------
//  Encoding Options
//      unsigned int m_uiEncOptFlags in enc_init_t structure
//                                      ( 1<<0 )  bit[ 0   ]
//      H264_ENC_IDR_OPTION             ( 1<<3 )  bit[ 3   ]  Option to encode as IDR in CMD_ENC_SEQ_OPTION: bit[3](0: non-IDR, 1: IDR)
//      USE_AVC_ENC_VUI_ADDITION        ( 7<<4 )  bit[ 4: 7]  MSB[2] : fixed_frame_rate_flag, MSB[1:0] : 1- (1:1) mode, 2- (1:2) mode
//                                                            ex1) fixed_frame_rate_flag off / 1:2 mode :  2 (2b'010)
//                                                            ex2) fixed_frame_rate_flag on  / 1:2 mode :  6 (2b'110)
//      VPU_ENHANCED_RC_MODEL_ENABLE    ( 1<<10)  bit[10   ]
//      USE_MAX_INSTANCE_NUM_CTRL       (15<<11)  bit[11:14]
//      SEC_AXI_BUS_ENABLE_SRAM         ( 1<<21)  bit[21   ]  Use SRAM for sec. AXI bus in encoder
//      SEC_AXI_ENABLE_CTRL             ( 3<<21)  bit[21:22]  Use SRAM for sec. AXI bus in decoder
//------------------------------------------------------
#define USE_AVC_ENC_VUI_ADDITION                  (7<<4)
#ifdef  USE_AVC_ENC_VUI_ADDITION
#	define USE_AVC_ENC_VUI_ADDITION_RSHIFT    (4)
#endif
#ifdef H264_ENC_IDR_OPTION
#	define H264_ENC_IDR_OPTION_RSHIFT         (3)
#endif


//------------------------------------------------------
//	Decoding Options
//	unsigned int m_uiDecOptFlags in dec_init_t structure
//	M4V_DEBLK_DISABLE                ( 0    )		// (default)
//	M4V_DEBLK_ENABLE                 ( 1    )		// mpeg-4 deblocking
//	M4V_GMC_FILE_SKIP                ( 0<<1 )	bit[ 0   ]	(default) seq.init failure
//	M4V_GMC_FRAME_SKIP               ( 1<<1 )	bit[ 1   ]	frame skip without decoding
//	AVC_VC1_REORDER_DISABLE          ( 1<<2 )	bit[ 2   ]	reorder disable only for AVC and VC1 - (default) reorder enable
//	AVC_FIELD_DISPLAY                ( 1<<3 )	bit[ 3   ]	if only field is fed, display it
//	SWRESET_ENABLE                   ( 1<<4 )	bit[ 4   ]	SW RESET Enable
//	AVC_ERROR_CONCEAL_MODE           ( 1<<5 )	bit[ 5   ]	AVC error conceal mode selection (iPod issue(ITS2592))
//	FW_WRITING_DISABLE               ( 1<<7 )	bit[ 7   ]	FW writing disable
//	H263_SKIPMB_ENABLE               ( 1<<10)	bit[10   ]	H263 SKIPMB Enable (IS004A-3624, TELBB-456)
//	USE_MAX_INSTANCE_NUM_CTRL        (15<<11)	bit[11:14]
//	DISABLE_DETECT_RESOLUTION_CHANGE ( 1<<15)	bit[15   ]	disable resolution change detection option - (Default) if resolution change is detected, VPU returns "VPU_DEC_DETECT_RESOLUTION_CHANGE" without decoding process.
//	RESOLUTION_CHANGE_SUPPORT        ( 1<<16)	bit[16   ]	alloc. max. resolution for changing resolution
//	SEC_AXI_BUS_ENABLE_SRAM          ( 1<<21)	bit[21   ]	Use SRAM for sec. AXI bus in encoder
//	SEC_AXI_ENABLE_CTRL              ( 3<<21)	bit[21:22]	use secAXI (0:N/A TCC89xx, 1:SRAM 114K for TCC93xx, 2:SRAM 80K for TCC88xx, 3: test)
//	SEQ_INIT_BS_BUFFER_CHANGE_ENABLE ( 1<<26)	bit[26   ]	SEQ_INIT bitstream buffer change enable
//------------------------------------------------------
#define DISABLE_DETECT_RESOLUTION_CHANGE          (1<<15)

#define RESOLUTION_CHANGE_SUPPORT                 (1<<16)	// alloc. max. resolution for changing resolution
						// use maximum stride, if the resolution is less than  frame buffer size(use max. stride), the output shall be cropped.
#define SEC_AXI_ENABLE_CTRL                       (3<<21) // use secAXI (0:N/A TCC89xx, 1:SRAM 114K for TCC93xx, 2:SRAM 80K for TCC88xx, 3: test)
#ifdef SEC_AXI_ENABLE_CTRL
#	define SEC_AXI_ENABLE_CTRL_SHIFT          21
#endif

#define USE_AVC_ErrorConcealment_CTRL_SHIFT       5

#define SEQ_INIT_BS_BUFFER_CHANGE_ENABLE          (1<<26)	// SEQ_INIT bitstream buffer change enable
#ifdef SEQ_INIT_BS_BUFFER_CHANGE_ENABLE
#	define SEQ_INIT_BS_BUFFER_CHANGE_ENABLE_SHIFT 26
#endif

#define H263_SKIPMB_ENABLE                        (1<<10)	// H263 SKIPMB Enable (IS004A-3624, TELBB-456)
#ifdef H263_SKIPMB_ENABLE
#	define H263_SKIPMB_ENABLE_SHIFT           10
#endif

#define FW_WRITING_DISABLE_SHIFT                  7 // FW writing disable


//------------------------------------------------------
// returns pic_struct of PicTiming SEI in H.264 (F/W 20100622)
// RET_DEC_PIC_TYPE
// bit [23] : pic_structure_present_flag (val>>23)&0x01
// bit [27:24] : pic_struct
// 0: frame
// 1: top field
// 2: bottom field
// 3: top field, bottom field, in that order
// 4: bottom field, top field, in that order
// 5: top field, bottom field, top field repeated, in that order
// 6: bottom filed, top field, bottom field repeated, in that order
// 7: frame doubling
// 8: frame tripling
//------------------------------------------------------
#define USE_AVC_SEI_PICTIMING_INFO

//------------------------------------------------------
// change the API of frame rate info. from 16bits to 32bits (FW 20100503)
// H.264		: 32 bits
// another std  : 16 bits
// from FrameRateInfo to FrameRateRes and FrameRateDivMinux1
//------------------------------------------------------
#define USE_FRAME_RATE_INFO_32BITS

//------------------------------------------------------
// MPEG-4 GMC control (FW 20091110)
//  CMD_DEC_SEQ_MP4_ASP_CALSS (BIT_BASE + 0x19C) [4:3] bit
//	0	: default (failure in seq. init.)
//	1	: decodingSuccess is failed without decoding
//  2	: copy the previous frame (Video Quality issue ?)
//  3	: ?
//------------------------------------------------------
#define USE_MPEG4_GMC_CTRL

//------------------------------------------------------
// MPEG-4 Scalability control (FW 20091208)
//  CMD_DEC_SEQ_MP4_ASP_CALSS (BIT_BASE + 0x19C) [5] bit
//	0 - Normal mode: Return SEQ_INIT fail if scalability is enabled
//	1 - Forced mode: Ignore scalability syntax in VOL header
//------------------------------------------------------
#define USE_MPEG4_SCALABILITY_CTRL

//------------------------------------------------------
//	- buffer control for mpeg-4 deblocking
//------------------------------------------------------
#define USE_DEC_M4V_DEBLK_BUFCTRL
#ifdef USE_DEC_M4V_DEBLK_BUFCTRL
	// increase to two times for mpeg-4 deblocking
#	define INCREASE_2TIMES_FRAME_BUFFER
#endif

//------------------------------------------------------
// MPEG-2 Max. resolution allocation option
//	for changing the resolution in decoding stream
//------------------------------------------------------
#define USE_MAX_RESOLUTION


//************** decoder additional info************************
//------------------------------------------------------
// decoder additional info. summary
//
// unsigned int m_Reserved[36] of dec_init_t
//	        m_Reserved[ 0]: CNM FW Size
//	        m_Reserved[ 1]: CNM FW Memory
//
// unsigned int m_Reserved[32] of dec_initial_info_t
//	        m_Reserved[ 5]: H.264 ct_type of pic timing SEI
//
// unsigned int m_Reserved[22] of dec_input_t
//	        m_Reserved[0]: avcDpbResetOnIframeSearch options, [24]:control flag(1), [0]: disable(0)/enable(1)
//
// unsigned int m_Reserved[29] of dec_output_info_t
//	        m_Reserved[ 0]: [USE] USE_VC1_SKIPFRAME_KEEPING
//	        m_Reserved[ 1]: time_code (25bits) in MPEG-2 GoP : USE_EXTRACT_MP2_TIME_CODE
//	        m_Reserved[ 2]: VPU status info : USE_VPU_STATUS_RESERVED_VAR
//	        m_Reserved[ 3]: VPU Pending handle : USE_VPU_STATUS_RESERVED_VAR
//	        m_Reserved[ 4]: user data buffer full
//	        m_Reserved[ 5]: [USE] H.264 ct_type of SEI
//	        m_Reserved[ 6]:
//	        m_Reserved[ 7]:
//	        m_Reserved[ 8]:
//	        m_Reserved[ 9]:
//	        m_Reserved[10]:
//	        m_Reserved[11]: [USE] AVC POC info : avcPocPic
//	        m_Reserved[12]: [USE] internal VPU clock count for profiling (frame cycle)
//	        m_Reserved[13]:
//	        m_Reserved[14]:
//	        m_Reserved[15]: MPEG-2 Active format : USE_MPEG2_ATSC_USER_DATA_INFO_ACTIVE_FORMAT
//------------------------------------------------------

//------------------------------------------------------
// VC-1 Skipped frame related: 20090928
// When there is a delay in the timing of display buffer clear,
//  when the frame buffer is reused due to a previously entered clear
//  before being output to the LCD for skipped frames that
//  are used multiple times, for avoidance purpose
// unsigned int m_Reserved[0] of dec_output_info_t
//			[31:16] Vc1SkipFrameKeepEnable
//			[15: 8] Vc1SkipFrameKeepIdx
//			[ 7: 0] Vc1SkipFrameClearIdx
//------------------------------------------------------
//#define USE_VC1_SKIPFRAME_KEEPING
#ifdef USE_VC1_SKIPFRAME_KEEPING
#	define USE_VC1_SKIPFRAME_KEEPING_DBG
#endif

//------------------------------------------------------
// time_code (25bits) in MPEG-2 GoP : USE_EXTRACT_MP2_TIME_CODE
// unsigned int m_Reserved[1] of dec_output_info_t
//         time_code of ch. 6.2.2.6 Group of picture header in MPEG-2 video
//          (firmware_coda960_v3_x_TELBBA-41_r177989)
//         If there is no GOP header in the frame, the register value is 0.
//         Table 6-11 time_code          :25bits
//            [24   ] drop_frame_flag    : 1bit
//            [23:19] time_code_hours    : 5bits(range of value: 0-23)
//            [18:13] time_code_minutes  : 6bits(range of value: 0-59)
//            [12   ] marker_bit         : 1bit (range of value: 1   )
//            [11: 6] time_code_seconds  : 6bits(range of value: 0-59)
//            [ 5: 0] time_code_pictures : 6bits(range of value: 0-59)
// Used only for non-KT projects.
// It is necessary to check whether the function is supported not only by
//  specific F/W (firmware_coda960_v3_x_TELBBA-41_r177989.h 20190823 TELBBA-41)
//  but also by the latest version.
//------------------------------------------------------
//#define USE_EXTRACT_MP2_TIME_CODE


//------------------------------------------------------
// Get debugging information using reserved member variables
//   0 : N/A
//   1 : vpu instance number, current instance index, pending instance index and pending instance handle for debugging
//         unsigned int m_Reserved[2-3] of dec_output_info_t  ( Need more in the future: dec_init_t, dec_initial_info_t )
//         m_Reserved[2]: VPU status info    : [31:24] used instance number
//                                           : [23:16] current instance index + 1
//                                           : [15: 8] Pending instance index + 1
//                                           : [ 7: 0]
//         m_Reserved[3]: VPU Pending handle : [ 0:31] handle
//------------------------------------------------------
#define USE_VPU_STATUS_RESERVED_VAR               0	// 0: disable, 1: enable

//------------------------------------------------------
// H.264/AVC
// ct_type of PicTiming SEI info. in H.264 for CDI Spec (TMMT-187 TELBBA-23)
//  If ct_type does not exist, '-1' is returned.
// ct_type indicates the scan type (interlaced or progressive) of the source material as specified in Table D-2.
//   (0: progressive, 1: interlaced, 2: unknown, 3: reserved)
// Bit set of clock types for fields/frames in picture timing SEI message.
//  For each found ct_type, appropriate bit is set (e.g., bit 1 for interlaced).
// VIDEO_DECODER_ENCODED_FORMAT_MPEG4_AVC
//  The value of the ct_type syntax, part of the picture timing Supplemental Enhancement Information(SEI) message, shall be used.
// [CDI] Scan type determination (Android)
//  CDI : Contexts and Dependency Injection
// int m_iExtAvcCtType        of dec_output_info_t
// unsigned int m_Reserved[5] of dec_output_info_t
//     avcCtType of DecOutputInfo stucture
//------------------------------------------------------
// USE

//------------------------------------------------------
// H.264/AVC POC(Picture Order Count) info
// POC defines display order of AUs in the encoded bitstream.
// According to H264, standard POC is derived from slice headers proportional to AUs timings starting from the first IDR AU where POC is equal to 0.
//  POC is defined in the H264 standard by TopFieldOrderCount and BottomFieldOrderCount.
// int m_iExtAvcPocPic         of dec_output_info_t
// unsigned int m_Reserved[11] of dec_output_info_t
//     avcPocPic of DecOutputInfo structure
//------------------------------------------------------
// USE

//------------------------------------------------------
// MPEG-2 Active format
// ATSC A/53 Part4 and SMPTE 2016-1-2007
// MPEG-2 Video System Characteristics - ATSC Digital Television Standard
// int m_iExtMp2ActiveFormat   of dec_output_info_t
// unsigned int m_Reserved[15] of dec_output_info_t
//     mp2ActiveFormat of DecOutputExtData stucture
//------------------------------------------------------
#define USE_MPEG2_ATSC_USER_DATA_INFO_ACTIVE_FORMAT

//------------------------------------------------------
//The following member variables of the DecOutputInfo structure are commented out:
//    dispPicHeight
//    dispPicWidth
//    wprotErrReason
//    wprotErrAddress
//    mp2DispVerSize
//    mp2DispHorSize
//    mp2PicDispExtInfo
//    mp2NpfFieldInfo
//------------------------------------------------------


//************** the end of decoder hidden info********************


//************** encoder hidden info************************
//------------------------------------------------------
// encoder hidden info. summary
//
// unsigned int m_Reserved[11] of enc_init_t
//		m_Reserved[0]: CNM FW Size
//		m_Reserved[1]: CNM FW Memory
//
// int	m_Reserved in enc_output_t
//		m_Reserved : internal VPU clock count for profiling
//
//************** the end of encoder hidden info********************


//------------------------------------------------------
// VC-1 Skipped frame control (FW 20091104)
//	disable	: default (frame buffer index is reused
//	enable	: new frame index alloc. and copy for skipped frame buffer index
//------------------------------------------------------
#define USE_VC1_SKIP_FRAME_COPY
#ifdef USE_VC1_SKIP_FRAME_COPY
#	ifdef USE_VC1_SKIPFRAME_KEEPING
#		undef USE_VC1_SKIPFRAME_KEEPING
#		ifdef USE_VC1_SKIPFRAME_KEEPING_DBG
#			undef USE_VC1_SKIPFRAME_KEEPING_DBG
#		endif
#	endif
#endif


/************************************************************************

		MACROS(DEBUGGING and ALIGNMENT)

*************************************************************************/

//------------------------------------------------------
// macros for debugging and alignment
//------------------------------------------------------
#if defined(ARM_ADS) || defined(ARM_RVDS)		// ARM ADS & RVDS
#	define inline __inline
#	define ALIGNED __align(PRE_ALIGN_BYTES)
#elif defined(ARM_WINCE)						// ARM WinCE
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
Visual C++ 4.x		: 1000
Visual C++ 5		: 1100
Visual C++ 6		: 1200
Visual C++ .NET		: 1300
Visual C++ .NET 2003: 1310
Visual C++ 2005		: 1400
Visual C++ 2008		: 1500
*/
#if ( _MSC_VER >= 1400 ) //
	#pragma warning(disable:4996) //: _CRT_SECURE_NO_WARNINGS
#endif

#ifndef DPRINTF
	#if defined(_DEBUG) || defined(DEBUG)//----------
	#	if defined(_WIN32)
	#		include <windows.h>
	#		if defined(_WIN32_WCE)
				static void dprintf( char *fmt, ...)
				{
					va_list args;
					char buf[1024];
					wchar_t temp[1024];
					va_start(args, fmt);
					vsprintf(buf, fmt, args);
					va_end(args);
					wsprintf( temp, L"%hs", buf );
					OutputDebugString(temp);
				}
	#		define DPRINTF dprintf
	#		else
	#			include <stdio.h>
				static void dprintf( char *fmt, ...)
				{
					va_list args;
					char buf[1024];
					va_start(args, fmt);
					vsprintf(buf, fmt, args);
					va_end(args);
					OutputDebugString(buf);
					//printf("%s", buf);
				}
	#		define DPRINTF dprintf
	#		ifdef DEBUG_OUT_LOG_FILE
				static void lprintf( char *fmt, ...)
				{
					va_list args;
					char buf[1024];
					FILE *fp_log = fopen( OUT_LOG_FILE, "a+" );
					va_start(args, fmt);
					vsprintf(buf, fmt, args);
					va_end(args);
					fprintf(fp_log, "%s", buf);
					fclose(fp_log);
				}
	#		undef DPRINTF
	#		define DPRINTF lprintf
	#		endif//DEBUG_OUT_LOG_FILE
	#		endif
	#	else//#if !defined(_WIN32)
	#		define DPRINTF printf // printf
	#	endif//#if !defined(_WIN32)
	#else//RELEASE MODE---------------------------------
		static void dprintf( char *fmt, ...) { ; }
	#	define DPRINTF dprintf
	#endif//end of if defined(_DEBUG) || defined(DEBUG)
#endif//DPRINTF


//------------------------------------------------------
// debugging options
//------------------------------------------------------
#if defined(_DEBUG) || defined(DEBUG)
//#	define DBG_COUNT_PIC_INDEX
#endif

//#define DBG_PRINTF_RUN_STATUS
#ifdef DBG_PRINTF_RUN_STATUS
	//#include <stdio.h>
#	define DSTATUS printf
#	undef DPRINTF
#	define DPRINTF printf
#else
	//#define DSTATUS dprintf
	static void DSTATUS( char *fmt, ...) { ; }
#endif


#endif//VPU_PRE_DEFINE_HEADER
