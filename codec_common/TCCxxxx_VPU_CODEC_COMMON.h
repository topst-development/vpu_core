/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef TCCXXXX_VPU_CODEC_COMMON__H
#define TCCXXXX_VPU_CODEC_COMMON__H

#define PA 0 // physical address
#define VA 1 // virtual  address

#define PIC_TYPE_I                                0x000
#define PIC_TYPE_P                                0x001
#define PIC_TYPE_B                                0x002
#define PIC_TYPE_IDR                              0x005
#define PIC_TYPE_B_PB                             0x102	// only for MPEG-4 Packed PB-frame


#define STD_AVC                                   0	// DEC / ENC : AVC / H.264 / MPEG-4 Part 10
#define STD_VC1                                   1	// DEC
#define STD_MPEG2                                 2	// DEC
#define STD_MPEG4                                 3	// DEC / ENC
#define STD_H263                                  4	// DEC / ENC
#define STD_AVS                                   7	// DEC
#define STD_MJPG                                  10	// DEC
#define STD_VP8                                   11	// DEC
#define STD_MVC                                   14	// DEC
#define STD_HEVC                                  15	// DEC
#define STD_VP9                                   16	// DEC
#define STD_HEVC_ENC                              17	// ENC  : HEVC
#define STD_HEVC_ENC2                             18    // ENC2 : HEVC

#define RETCODE_SUCCESS                           0
#define RETCODE_FAILURE                           1
#define RETCODE_INVALID_HANDLE                    2
#define RETCODE_INVALID_PARAM                     3
#define RETCODE_INVALID_COMMAND                   4
#define RETCODE_ROTATOR_OUTPUT_NOT_SET            5
#define RETCODE_ROTATOR_STRIDE_NOT_SET            6
#define RETCODE_FRAME_NOT_COMPLETE                7
#define RETCODE_INVALID_FRAME_BUFFER              8
#define RETCODE_INSUFFICIENT_FRAME_BUFFERS        9
#define RETCODE_INVALID_STRIDE                    10
#define RETCODE_WRONG_CALL_SEQUENCE               11
#define RETCODE_CALLED_BEFORE                     12
#define RETCODE_NOT_INITIALIZED                   13
#define RETCODE_USERDATA_BUF_NOT_SET              14
#define RETCODE_CODEC_FINISH                      15	//the end of decoding
#define RETCODE_CODEC_EXIT                        16
#define RETCODE_CODEC_SPECOUT                     17

#define RETCODE_MEM_ACCESS_VIOLATION              18

#define RETCODE_INSUFFICIENT_BITSTREAM            20
#define RETCODE_INSUFFICIENT_BITSTREAM_BUF        21
#define RETCODE_INSUFFICIENT_PS_BUF               22

#define RETCODE_ACCESS_VIOLATION_HW               23
#define RETCODE_INSUFFICIENT_SECAXI_BUF           24
#define RETCODE_QUEUEING_FAILURE                  25
#define RETCODE_VPU_STILL_RUNNING                 26
#define RETCODE_REPORT_NOT_READY                  27

#define RETCODE_API_VERSION_MISMATCH              28
#define RETCODE_INVALID_MAP_TYPE                  29

#define RETCODE_INSTANCE_MISMATCH_ON_PENDING_STATUS   30
#define RETCODE_INSUFFICIENT_WORKBUF              32

#ifndef RETCODE_WRAP_AROUND
#define RETCODE_WRAP_AROUND                       (-10)
#endif


//------------------------------------------------------------------------------
// VPU deivce driver's notification on the library side with video IP HW
//
// Contents: defines the integer variable, which
// is set by system calls and some library functions in the event of a state or
// an error to indicate what went wrong.
//
//------------------------------------------------------------------------------

/*!
 * Interrupt Status
 */

#define RETCODE_INTR_DETECTION_NOT_ENABLED    512

#ifndef INC_DEVICE_TREE_PMAP

//To prevent overflow from occurring when storing data, use the data type with the largest number of bits.
//  When comparing ILP32 and LP64,
//    the number of bits of the compiler data type changes to 32 bits and 64 bits for the "long/size_t/pointer" data type,
//    but does not change to 64 bits for the "long long" data type.
//    (ARM white paper : Porting to 64-bit ARM)

#	ifndef VPU_CODEC_HANDLE_T_DEF
#		define VPU_CODEC_HANDLE_T_DEF
#		if defined(CONFIG_ARM64)
typedef long long codec_handle_t;	// handle - 64bit
#		else
typedef long codec_handle_t;	// handle - 32bit
#		endif
typedef long vcodec_handle_t;	// handle
#	endif

#	ifndef VPU_CODEC_RESULT_T_DEF
#		define VPU_CODEC_RESULT_T_DEF
typedef int codec_result_t;	// return value
#	endif

#	ifndef VPU_CODEC_ADDR_T_DEF
#		define VPU_CODEC_ADDR_T_DEF
#		if defined(CONFIG_ARM64)
typedef unsigned long long codec_addr_t;	// address - 64 bit
#		else
typedef unsigned long codec_addr_t;	// address - 32 bit
#		endif
#	endif

enum seq_change_type {
	CHANGE_RESOLUTION = 0x00000001,
	CHANGE_BITDEPTH   = 0x00000002,
	CHANGE_DPB_COUNT  = 0x00000004,
	CHANGE_PROFILE    = 0x00000008,
	CHANGE_SEQUENCE   = 0x00000010,
	CHANGE_MAX        = 0x0000FFFF,
	CHANGE_BOUND      = 0x7FFFFFFF
};

#endif	//INC_DEVICE_TREE_PMAP

#define COMP_Y 0
#define COMP_U 1
#define COMP_V 2

#ifndef ALIGNED_BUFF
#	define ALIGNED_BUFF(buf, mul) (((unsigned int)buf + (mul-1U)) & ~(mul-1U))
#endif

#ifndef VALIGN
#	define VALIGN(val, align_size)            (((val) + ((align_size) - 1)) & (~((align_size) - 1)))
#	define VALIGN02(val)                      VALIGN((val),   2)
#	define VALIGN04(val)                      VALIGN((val),   4)
#	define VALIGN08(val)                      VALIGN((val),   8)
#	define VALIGN16(val)                      VALIGN((val),  16)
#	define VALIGN32(val)                      VALIGN((val),  32)
#	define VALIGN64(val)                      VALIGN((val),  64)
#	define VALIGN128(val)                     VALIGN((val), 128)
#	define VALIGN256(val)                     VALIGN((val), 256)
#	define VALIGN512(val)                     VALIGN((val), 512)
#	define VALIGNKB(val, align_size)          VALIGN((val), ((align_size)*1024))
#endif

//------------------------------------------------------------------------------
// Definition of decoding process
//------------------------------------------------------------------------------

/*!
 * Output Status
 */

#define VPU_DEC_OUTPUT_FAIL                       0
#define VPU_DEC_OUTPUT_SUCCESS                    1


/*!
 * Decoding Status
 */

#define VPU_DEC_SUCCESS                           (1)
#define VPU_DEC_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF  (2)
#define VPU_DEC_INFO_NOT_SUFFICIENT_SLICE_BUFF    (3)
#define VPU_DEC_BUF_FULL                          (4)
#define VPU_DEC_SUCCESS_FIELD_PICTURE             (5)
#define VPU_DEC_DETECT_RESOLUTION_CHANGE          (6)
#define VPU_DEC_INVALID_INSTANCE                  (7)
#define VPU_DEC_DETECT_DPB_CHANGE                 (8)
#define VPU_DEC_QUEUEING_FAIL                     (9)
#define VPU_DEC_VP9_SUPER_FRAME                   (10)
#define VPU_DEC_CQ_EMPTY                          (11)
#define VPU_DEC_REPORT_NOT_READY                  (12)
#define VPU_DEC_FW_PENDING                        (30)
#define VPU_DEC_DETECT_SEQUENCE_CHANGE            (VPU_DEC_DETECT_RESOLUTION_CHANGE)


/*!
 * Decoder Op Code
 */

#define VPU_BASE_OP_KERNEL                        0x10000
#define VPU_DEC_INIT                              0x00    // init
#define VPU_DEC_SEQ_HEADER                        0x01    // decode sequence header
#define VPU_DEC_GET_INFO                          0x02
#define VPU_DEC_REG_FRAME_BUFFER                  0x03    // register frame buffer
#define VPU_DEC_REG_FRAME_BUFFER2                 0x04    // register frame buffer 2
#define VPU_DEC_REG_FRAME_BUFFER3                 0x05    // register frame buffer 3
#define VPU_DEC_GET_OUTPUT_INFO                   0x06
#define VPU_DEC_DECODE                            0x10    // decode
#define VPU_DEC_BUF_FLAG_CLEAR                    0x11    // display buffer flag clear
#define VPU_DEC_FLUSH_OUTPUT                      0x12    // flush delayed output frame
#define VPU_GET_RING_BUFFER_STATUS                0x13
#define VPU_FILL_RING_BUFFER_AUTO                 0x14    // Fill the ring buffer
#define VPU_UPDATE_WRITE_BUFFER_PTR               0x16    // Fill the ring buffer
#define VPU_DEC_SWRESET                           0x19    // decoder sw reset
#define VPU_DEC_CLOSE                             0x20    // close

#define VPU_DEC_GET_INSTANCE_STATUS               0x100   // get vpu instance information (instance number, instance index, pending index)
#define VPU_CODEC_GET_VERSION                     0x3000


/*!
 * Decoder Op Code for kernel
 */

#define VPU_DEC_INIT_KERNEL                       (VPU_BASE_OP_KERNEL + VPU_DEC_INIT)
#define VPU_DEC_SEQ_HEADER_KERNEL                 (VPU_BASE_OP_KERNEL + VPU_DEC_SEQ_HEADER)
#define VPU_DEC_GET_INFO_KERNEL                   (VPU_BASE_OP_KERNEL + VPU_DEC_GET_INFO)
#define VPU_DEC_REG_FRAME_BUFFER_KERNEL           (VPU_BASE_OP_KERNEL + VPU_DEC_REG_FRAME_BUFFER)
#define VPU_DEC_REG_FRAME_BUFFER2_KERNEL          (VPU_BASE_OP_KERNEL + VPU_DEC_REG_FRAME_BUFFER2)
#define VPU_DEC_REG_FRAME_BUFFER3_KERNEL          (VPU_BASE_OP_KERNEL + VPU_DEC_REG_FRAME_BUFFER3)
#define VPU_DEC_GET_OUTPUT_INFO_KERNEL            (VPU_BASE_OP_KERNEL + VPU_DEC_GET_OUTPUT_INFO)
#define VPU_DEC_DECODE_KERNEL                     (VPU_BASE_OP_KERNEL + VPU_DEC_DECODE)
#define VPU_DEC_BUF_FLAG_CLEAR_KERNEL             (VPU_BASE_OP_KERNEL + VPU_DEC_BUF_FLAG_CLEAR)
#define VPU_DEC_FLUSH_OUTPUT_KERNEL               (VPU_BASE_OP_KERNEL + VPU_DEC_FLUSH_OUTPUT)
#define VPU_GET_RING_BUFFER_STATUS_KERNEL         (VPU_BASE_OP_KERNEL + VPU_GET_RING_BUFFER_STATUS)
#define VPU_FILL_RING_BUFFER_AUTO_KERNEL          (VPU_BASE_OP_KERNEL + VPU_FILL_RING_BUFFER_AUTO)
#define VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL        (VPU_BASE_OP_KERNEL + VPU_UPDATE_WRITE_BUFFER_PTR)
#define VPU_DEC_SWRESET_KERNEL                    (VPU_BASE_OP_KERNEL + VPU_DEC_SWRESET)
#define VPU_DEC_CLOSE_KERNEL                      (VPU_BASE_OP_KERNEL + VPU_DEC_CLOSE)

#define VPU_CODEC_GET_VERSION_KERNEL              (VPU_BASE_OP_KERNEL + VPU_CODEC_GET_VERSION)

#define GET_RING_BUFFER_STATUS	VPU_GET_RING_BUFFER_STATUS
#define FILL_RING_BUFFER_AUTO	VPU_FILL_RING_BUFFER_AUTO

// Get initial Info for ring buffer use
#define GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY	0x15

#define GET_RING_BUFFER_STATUS_KERNEL             (VPU_BASE_OP_KERNEL + GET_RING_BUFFER_STATUS)
#define FILL_RING_BUFFER_AUTO_KERNEL              (VPU_BASE_OP_KERNEL + FILL_RING_BUFFER_AUTO)
#define GET_INITIAL_INFO_KERNEL_FOR_STREAMING_MODE_ONLY (VPU_BASE_OP_KERNEL + GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY)

//------------------------------------------------------------------------------
// Definition of encoding process
//------------------------------------------------------------------------------

#define MPEG4_VOL_HEADER                          0x00
#define MPEG4_VOS_HEADER                          0x01
#define MPEG4_VIS_HEADER                          0x02
#define AVC_SPS_RBSP                              0x10
#define AVC_PPS_RBSP                              0x11
#define HEVC_VPS_RBSP                             0x04
#define HEVC_SPS_RBSP                             0x08
#define HEVC_PPS_RBSP                             0x10
#define HEVC_VPU_SPS_PPS_RBSP                     0x1C

/*!
 * Encoder Op Code
 */

#define VPU_ENC_INIT                              0x00    // init
#define VPU_ENC_REG_FRAME_BUFFER                  0x01    // register frame buffer
#define VPU_ENC_PUT_HEADER                        0x10
#define VPU_ENC_ENCODE                            0x12    // encode
#define VPU_ENC_CLOSE                             0x20    // close
#define VPU_ENC_GET_VERSION                       0x3001  //VPU_CODEC_GET_VERSION 0x3000

#define VPU_RESET_SW                              0x40

#define VPU_RESET_SW_KERNEL                       (VPU_BASE_OP_KERNEL + VPU_RESET_SW)

#endif //TCCXXXX_VPU_CODEC_COMMON__H
