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

#ifndef ERROR_CODE_HEVC_ENC_INCLUDED
#define ERROR_CODE_HEVC_ENC_INCLUDED


// This is an enumeration for declaring return codes from API function calls. 
// The meaning of each return code is the same for all of the API functions, but the reasons of 
// non-successful return might be different. Some details of those reasons are
// briefly described in the API definition chapter. In this chapter, the basic meaning
// of each return code is presented.

#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_SUCCESS                              0       /**< This means that operation was done successfully.  */  /* 0  */ 
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_FAILURE                               1       /**< This means that operation was not done successfully. When un-recoverable decoder error happens such as header parsing errors, this value is returned from VPU API.  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INVALID_HANDLE                   2       /**< This means that the given handle for the current API function call was invalid (for example, not initialized yet, improper function call for the given handle, etc.).  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INVALID_PARAM                    3       /**< This means that the given argument parameters (for example, input data structure) was invalid (not initialized yet or not valid anymore). */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INVALID_COMMAND               4      /**< This means that the given command was invalid (for example, undefined, or not allowed in the given instances).  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_ROTATOR_OUTPUT_NOT_SET     5      /**< This means that rotator output buffer was not allocated even though postprocessor (rotation, mirroring, or deringing) is enabled. */  /* 5  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_ROTATOR_STRIDE_NOT_SET       6      /**< This means that rotator stride was not provided even though postprocessor (rotation, mirroring, or deringing) is enabled.  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_FRAME_NOT_COMPLETE           7      /**< This means that frame decoding operation was not completed yet, so the given API function call cannot be allowed.  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INVALID_FRAME_BUFFER          8       /**< This means that the given source frame buffer pointers were invalid in encoder (not initialized yet or not valid anymore).  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INSUFFICIENT_FRAME_BUFFERS  9      /**< This means that the given numbers of frame buffers were not enough for the operations of the given handle. This return code is only received when calling wave420l_VPU_EncRegisterFrameBuffer() or wave420l_VPU_EncRegisterFrameBuffer() function. */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_INVALID_STRIDE                     10     /**< This means that the given stride was invalid (for example, 0, not a multiple of 8 or smaller than picture size). This return code is only allowed in API functions which set stride.  */   /* 10 */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_WRONG_CALL_SEQUENCE        11     /**< This means that the current API function call was invalid considering the allowed sequences between API functions (for example, missing one crucial function call before this function call).  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_CALLED_BEFORE                     12     /**< This means that multiple calls of the current API function for a given instance are invalid. */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_NOT_INITIALIZED                    13     /**< This means that VPU was not initialized yet. Before calling any API functions, the initialization API function, VPU_Init(), should be called at the beginning.  */
#define ERROR_VPU_HEVC_ENC_WAVE420L_RETCODE_USERDATA_BUF_NOT_SET         14     /**< This means that there is no memory allocation for reporting userdata. Before setting user data enable, user data buffer address and size should be set with valid value. */

#define RETCODE_VPU_HEVC_ENC_MEMORY_ACCESS_VIOLATION  115  /*0x73*/   /**< This means that access violation to the protected memory has been occurred. */   /* 15 */
#define RETCODE_VPU_HEVC_ENC_VPU_RESPONSE_TIMEOUT        116  /*0x74*/  /**< This means that VPU response time is too long, time out. */
#define RETCODE_VPU_HEVC_ENC_INSUFFICIENT_RESOURCE         117  /*0x75*/  /**< This means that VPU cannot allocate memory due to lack of memory. */
#define RETCODE_VPU_HEVC_ENC_NOT_FOUND_BITCODE_PATH    118  /*0x76*/   /**< This means that BIT_CODE_FILE_PATH has a wrong firmware path or firmware size is 0 when calling VPU_InitWithBitcode() function.  */
#define RETCODE_VPU_HEVC_ENC_NOT_SUPPORTED_FEATURE      119  /*0x77*/   /**< This means that HOST application uses an API option that is not supported in current hardware.  */
#define RETCODE_VPU_HEVC_ENC_NOT_FOUND_VPU_DEVICE       120  /*0x78*/  /**< This means that HOST application uses the undefined product ID. */   /* 20 */
#define RETCODE_VPU_HEVC_ENC_CP0_EXCEPTION                    121  /*0x79*/  /**< This means that coprocessor exception has occurred. (WAVE only) */
#define RETCODE_VPU_HEVC_ENC_STREAM_BUF_FULL                 122  /*0x7A*/   /**< This means that stream buffer is full in encoder. */
#define RETCODE_VPU_HEVC_ENC_ACCESS_VIOLATION_HW          123  /*0x7B*/  /**< This means that GDI access error has occurred. It might come from violation of write protection region or spec-out GDI read/write request. (WAVE only) */
#define RETCODE_VPU_HEVC_ENC_QUERY_FAILURE                     124  /*0x7C*/ /**< This means that query command was not successful (WAVE5 only) */
#define RETCODE_VPU_HEVC_ENC_QUEUEING_FAILURE                125  /*0x7D*/   /**< This means that commands cannot be queued (WAVE5 only) */
#define RETCODE_VPU_HEVC_ENC_VPU_STILL_RUNNING              126  /*0x7E*/  /**< This means that VPU cannot be flushed or closed now. because VPU is running (WAVE5 only) */
#define RETCODE_VPU_HEVC_ENC_REPORT_NOT_READY              127  /*0x7F*/   /**< This means that report is not ready for Query(GET_RESULT) command (WAVE5 only) */


//RET_SUCCESS Field Description  : [1:0]  RUN_CMD_STATUS  00: FAIL  01: SUCCESS   10: SUCCESS_WITH_WARNIG
//1.2.3.2.1.4. RET_FAIL_REASON (0x00000114)
//Table C.1. System Errors                                                               //[Bit Pos] [Bit Position (error_reason)] [Description]
#define RETREASON_CODEC_FW_ERROR                             0x00000001 //0 0x0000_0001 Codec FW error
#define RETREASON_INSTRUCTION_ACCESS_VIOLATION        0x00000004 //2 0x0000_0004 Coprocessor0 exception (INSTRUCTION_ACCESS_VIOLATION)
#define RETREASON_PRIVILEDGE_VIOLATION                      0x00000008 //3 0x0000_0008 Coprocessor0 exception (PRIVILEDGE_VIOLATION)
#define RETREASON_DATA_ADDR_ALIGNMENT_ERR              0x00000010 //4 0x0000_0010 Coprocessor0 exception (DATA_ADDR_ALIGNMENT_ERR)
#define RETREASON_DATA_ACCESS_VIOLATION                   0x00000020 //5 0x0000_0020 Coprocessor0 exception (DATA_ACCESS_VIOLATION)
#define RETREASON_WRITE_PROTECTION_ERR                     0x00000040 //6 0x0000_0040 Coprocessor0 exception (WRITE_PROTECTION_ERR)
#define RETREASON_INSTRUCTION_ADDR_ALIGNMENT_ERR   0x00000080 //7 0x0000_0080 Coprocessor0 exception (INSTRUCTION_ADDR_ALIGNMENT_ERR)
#define RETREASON_UNKNOWN_ERROR                            0x00000100 //8 0x0000_0100 Unknown error
#define RETREASON_BUS_ERROR                                      0x00000200 //9 0x0000_0200 Bus error
#define RETREASON_DOUBLE_FAULT_ERROR                       0x00000400 //10 0x0000_0400 Double fault error
//#define RETREASON_ 0x00000800//11 0x0000_0800 Reserved
#define RETREASON_HW_ACCESS_VIOLATION_ERROR           0x00001000 //12 0x0000_1000 HW access violation error
//#define RETREASON_ 0x00002000//13 0x0000_2000 Reserved
//#define RETREASON_ 0x00004000//14 0x0000_4000 Reverved
#define RETREASON_WATCHDOG_TIMEOUT_ERROR              0x00008000 //15 0x0000_8000 Watchdog time-out error
//#define RETREASON_ 0x00010000 //16 0x0001_0000 Reserved
//#define RETREASON_ 0x00020000 //17 0x0002_0000 Reserved
//#define RETREASON_ 0x00040000 //18 0x0004_0000 Reserved

#endif /* ERROR_CODE_HEVC_ENC_INCLUDED */
