/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef TCC_VPU_HEVC_ENC_H
#define TCC_VPU_HEVC_ENC_H

#include "TCCxxxx_VPU_CODEC_COMMON.h"

#define HEVC_E3_API_VERSION "2.0"

#define VPU_HEVC_ENC_CTRL_LOG_STATUS    0x1002 /**< Command to control the log status using the vpu_hevc_enc_ctrl_log_status_t structure. This command can be issued at any time, even before initialization. */
#define VPU_HEVC_ENC_SET_FW_ADDRESS     0x1003 /**< Command to set firmware base address of the VPU 4K D2 decoder. */
#define VPU_ENC_GET_VERSION             0x3001

#if !defined(RETCODE_INSUFFICIENT_WORKBUF)
#define RETCODE_INSUFFICIENT_WORKBUF	32
#endif

// HEVC/H.265 Main Profile @ L5.0 High-tier
#define VPU_HEVC_ENC_PROFILE  1      //!< Main Profile
#define VPU_HEVC_ENC_MAX_LEVEL 50 //!< level 5.0


#define VPU_HEVC_ENC_MAX_WIDTH   3840  //!< Max resolution: widthxheight = 3840x2160
#define VPU_HEVC_ENC_MAX_HEIGHT  2160

#define VPU_HEVC_ENC_MIM_WIDTH   256  //!<Min resolution: widthxheight = 256x128
#define VPU_HEVC_ENC_MIM_HEIGHT  128

#define VPU_HEVC_ENC_MAX_NUM_INSTANCE	8

// VPU_HEVC_ENC Memory map
// Total Size = (1) + (2) + (3)
// (1) VPU_HEVC_ENC_WORK_CODE_BUF_SIZE  (video_sw)
// (2) VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE
// (3) VPU_HEVC_ENC_STREAM_BUF_SIZE

#define VPU_HEVC_ENC_MAX_CODE_BUF_SIZE		(1024*1024)
#define VPU_HEVC_ENC_TEMPBUF_SIZE		(2*1024*1024)
#define VPU_HEVC_ENC_SEC_AXI_BUF_SIZE		(256*1024)
#define VPU_HEVC_ENC_STACK_SIZE			(8*1024)
#define VPU_HEVC_ENC_WORKBUF_SIZE		(3*1024*1024)
#define VPU_HEVC_ENC_HEADER_BUF_SIZE		(16*1024)

#define VPU_HEVC_ENC_SIZE_BIT_WORK			(VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + VPU_HEVC_ENC_TEMPBUF_SIZE + VPU_HEVC_ENC_SEC_AXI_BUF_SIZE + VPU_HEVC_ENC_STACK_SIZE)
#define VPU_HEVC_ENC_WORK_CODE_BUF_SIZE	(VPU_HEVC_ENC_SIZE_BIT_WORK + ((VPU_HEVC_ENC_WORKBUF_SIZE+VPU_HEVC_ENC_HEADER_BUF_SIZE)*VPU_HEVC_ENC_MAX_NUM_INSTANCE))


// Calculation formula = FrameCount * Align1M( ( Align256(width) * Align64(height) * 69 / 32 + 20*1024 ) )
// The size of frame buffers is calculated by VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE macro.
// The size of one frame buffer is calculated by VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE macro.
// In the currnet version, the count of frame buffer, FrameCount is 2.
// example) 3840x2160:36MB  2560x1920:22MB  2560x1440:16MB  1920x1088:10MB  1920x720:8MB  1280x720:6MB  1024x768:4MB

#if !defined(ALIGNED_SIZE_VPU_HEVC_ENC)
#define ALIGNED_SIZE_VPU_HEVC_ENC(buffer_size, align) (((unsigned int)(buffer_size) + ((align)-1)) & ~((align)-1))
#endif
#define VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE(width, height) ALIGNED_SIZE_VPU_HEVC_ENC((ALIGNED_SIZE_VPU_HEVC_ENC((width), 256)*ALIGNED_SIZE_VPU_HEVC_ENC((height), 64)*69/32 + (20*1024)), (1024*1024))
#define VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE(width, height, frame_count)   (VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE(width, height) * (frame_count))

#define VPU_HEVC_ENC_STREAM_BUF_SIZE			0x00BDEC00 //!< A maximum bitstream buffer size is the same size of uncompressed frame size.
																				 //!< HD - 1080p @ L4.1 Main Profile : 2.97 Mbytes  (0x002F8800)
																				 //!< 4K - 3840x2160 @ L5.0 Main Profile : 11.87 Mbytes (0x00BDEC00)

#define VPU_CAL_HEVC_ENC_PROCBUFFER(width, height, frame_count)  (ALIGNED_SIZE_VPU_HEVC_ENC(VPU_HEVC_ENC_STREAM_BUF_SIZE, (1024*1024)) + VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE(width, height, frame_count)) //frame_count = 2;

#define VPU_HEVC_ENC_USERDATA_BUF_SIZE		(512*1024)

#define VPU_HEVC_ENC_MIN_BUF_START_ADDR_ALIGN	(4*1024)	//VPU_MIM_BUF_SIZE_ALIGN
#define VPU_HEVC_ENC_MIN_BUF_SIZE_ALIGN		(4*1024)	//VPU_MIM_BUF_START_ADD_ALIGN


#ifndef STD_HEVC_ENC
	#define STD_HEVC_ENC  17
#endif

#define RETCODE_HEVCENCERR_HW_PRODUCTID		 2000

#ifndef INC_DEVICE_TREE_PMAP

typedef enum {
	HEVC_ENC_SRC_FORMAT_YUV_NOT_PACKED = 0,	// YUV420 format
	HEVC_ENC_SRC_FORMAT_YUYV = 1,		// Packed YUYV format : Y0U0Y1V0 Y2U1Y3V1
	HEVC_ENC_SRC_FORMAT_YVYU = 2,		// Packed YVYU format : Y0V0Y1U0 Y2V1Y3U1
	HEVC_ENC_SRC_FORMAT_UYVY = 3,		// Packed UYVY format : U0Y0V0Y1 U1Y2V1Y3
	HEVC_ENC_SRC_FORMAT_VYUY = 4,		// Packed VYUY format : V0Y0U0Y1 V1Y2U1Y3
	HEVC_ENC_SRC_FORMAT_NULL = 0x7FFFFFFF
} HevcEncSrcFormat;

//This is a special enumeration type for explicit encoding headers such as VPS, SPS, PPS.
typedef enum {
	HEVC_ENC_CODEOPT_ENC_VPS = (1 << 2), // A flag to encode VPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_SPS = (1 << 3), // A flag to encode SPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_PPS = (1 << 4), // A flag to encode PPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_NULL = 0x7FFFFFFF
} HevcEncHeaderType;


typedef unsigned int hevc_physical_addr_t; //!< Physical address (VPU uses only 32bits physical address)

/**
 * @struct vpu_hevc_enc_ctrl_log_status_t
 * @brief Structure for controlling logging status within the VPU HEVC encoder library.
 *
 * This structure is used to set the logging levels and conditions for the VPU HEVC encoder.
 * It allows fine-grained control over the types of logs that are printed, as well as specific
 * conditions under which logs should be generated.
 */
typedef struct vpu_hevc_enc_ctrl_log_status_t {
	/**
	 * @brief A pointer to the callback function for logging prints to be used internally within the library.
	 *
	 * This callback function is used to print log messages generated by the library. The function
	 * should follow the printf-style format.
	 *
	 * @param format The format string for the log message.
	 * @param ... Additional arguments for the format string.
	 */
	void (*pfLogPrintCb)(const char *, ...);

	/**
	 * @brief Structure for specifying the log levels.
	 *
	 * Each member of this structure controls whether a specific type of log message is enabled or disabled.
	 * A value of 1 enables the log type, while a value of 0 disables it.
	 */
	struct {
		int bVerbose; /**< Enable verbose logging (on=1, off=0) */
		int bDebug;   /**< Enable debug logging (on=1, off=0) */
		int bInfo;    /**< Enable info logging (on=1, off=0) */
		int bWarn;    /**< Enable warning logging (on=1, off=0) */
		int bError;   /**< Enable error logging (on=1, off=0) */
		int bAssert;  /**< Enable assert logging (on=1, off=0) */
		int bFunc;    /**< Enable function begin/end logging (on=1, off=0) */
		int bTrace;   /**< Enable trace logging (on=1, off=0) */
	} stLogLevel;

	/**
	 * @brief Structure for specifying log conditions.
	 *
	 * Each member of this structure represents a condition that can be checked before generating a log message.
	 * These conditions can be used to control logging more precisely, based on specific states or events in the
	 * decoder.
	 */
	struct {
		int bEncodeSuccess; /**< Log when encoding is successful (on=1, off=0) */
	} stLogCondition;
} vpu_hevc_enc_ctrl_log_status_t;

typedef struct vpu_hevc_enc_set_fw_addr_t {
	codec_addr_t m_FWBaseAddr; /**< Address of VPU4K D2 firmware. */
	int iReserved[6];
} vpu_hevc_enc_set_fw_addr_t;

typedef struct hevc_enc_rc_init_t {
	//////////// QP Related ////////////
	unsigned int m_iUseQpOption;	// 0 : Use default setting, Positive value : Use QP parameters
					// bit[0] 0 : disable(if this is 0, the following variables are ignored)
					//        1 : enable
					// bit[1-3] Reserved
					// bit[4] m_iIntraQpMin (0 : disable, 1 : enable}
					// bit[5] m_iIntraQpMax (0 : disable, 1 : enable}
					// bit[6] m_iInterQpMin (0 : disable, 1 : enable}
					// bit[7] m_iInterQpMax (0 : disable, 1 : enable}
					// bit[8] m_iInitialIntraRcQp(0 : disable, 1 : enable}
					// bit[9] m_iInitialInterRcQp(0 : disable, 1 : enable}
					// bit[10-31] Reserved
	int m_iIntraQpMin;		// It specifies a minimum QP for intra picture (0 ~ 51).
					// It is only valid when RateControl is 1(bitrate > 0).
	int m_iIntraQpMax;		// It specifies a maximum QP for intra picture (1 ~ 51).
					// It is only valid when RateControl is 1(bitrate > 0).
	int m_iInterQpMin;		// It specifies a minimum QP for inter picture (0 ~ 51).
					// It is only valid when RateControl is 1(bitrate > 0).
	int m_iInterQpMax;		// It specifies a maximum QP for inter picture (1 ~ 51).
					// It is only valid when RateControl is 1(bitrate > 0).
	int m_iInitialIntraRcQp;	// It specifies a initial QP for intra picture (0 ~ 51).
					// It specified a initial QP when RateControl is 1(bitrate > 0).
					// It specified a intra QP when RateControl is 0.
	int m_iInitialInterRcQp;	// It specifies a initial QP for inter picture (0 ~ 51).
					// It is only valid when RateControl is 0.

	//////////// Bitrate Related ////////////
	unsigned int m_iUseBitrateOption;	// 0 : Use default setting, Positive value : Use Bitrate parameters
						// It is only valid when RateControl is 1(bitrate > 0).
						// bit[0] 0 : disable(if this is 0, the following variables are ignored)
						//        1 : enable
						// bit[1] Reserved
						// bit[2] m_iTransBitrate (0 : disable, >= bitrate : enable}
						// bit[3-31] Reserved
	int m_iTransKbps;			// It specifies a peak transmission bitrate in Kbps.
						// VBR : m_iTransBitrate > m_iTargetKbps
						// CBR : m_iTransBitrate = m_iTargetKbps
						// ABR : 0 <= m_iTransBitrate < m_iTargetKbps(the input variable is ignored)

	unsigned int m_Reserved[119];
} hevc_enc_rc_init_t;

typedef struct {
	unsigned short aspect_ratio_info_present_flag;      //  u( 1)
	unsigned char  overscan_info_present_flag;          //  u( 1)
	unsigned char  video_signal_type_present_flag;      //  u( 1)

	unsigned char  chroma_loc_info_present_flag;        //  u( 1)
	unsigned char  neutral_chroma_indication_flag;      //  u( 1), 0(default)
	unsigned char  default_display_window_flag;         //  u( 1)
	unsigned char  vui_timing_info_present_flag;        //  u( 1)

	// aspect_ratio_inf : Encoded only when aspect_ratio_info_present_flag is 1.
	unsigned int   aspect_ratio_idc;                    //  u( 8)
	unsigned short sar_width;                           //  u(16)
	unsigned short sar_height;                          //  u(16)

	// overscan_info : Encoded only when overscan_info_present_flag is 1.
	unsigned int   overscan_appropriate_flag;           //  u( 1)

	// video_signal_type & colour_description : Encoded only when video_signal_type_present_flag is 1.
	unsigned short video_format;                        //  u( 3)
	unsigned short video_full_range_flag;               //  u( 1)
	unsigned char  colour_description_present_flag;     //  u( 1)
	unsigned char  colour_primaries;                    //  u( 8), Encoded only when colour_description_present_flag is 1.
	unsigned char  transfer_characteristics;            //  u( 8), Encoded only when colour_description_present_flag is 1.
	unsigned char  matrix_coeffs;                       //  u( 8), Encoded only when colour_description_present_flag is 1.

	// chroma_loc_info : Encoded only when chroma_loc_info_present_flag is 1.
	unsigned short chroma_sample_loc_type_top_field;    // ue( v) : 0 ~ 5
	unsigned short chroma_sample_loc_type_bottom_field; // ue( v) : 0 ~ 5

	// default_display_window : Encoded only when default_display_window_flag is 1.
	unsigned short def_disp_win_left_offset;            // ue( v)
	unsigned short def_disp_win_right_offset;           // ue( v)
	unsigned short def_disp_win_top_offset;             // ue( v)
	unsigned short def_disp_win_bottom_offset;          // ue( v)

	// timing_info : Encoded only when vui_timing_info_present_flag is 1.
	unsigned int   vui_num_units_in_tick;               //  u(32)
	unsigned int   vui_time_scale;                      //  u(32)
	unsigned int   vui_poc_proportional_to_timing_flag; //  u( 1)
	unsigned int   vui_num_ticks_poc_diff_one_minus1;   // ue( v) : 0 ~ (23^2 - 2), Encoded only when vui_poc_proportional_to_timing_flag is 1.
} hevc_enc_vui_t;

typedef struct hevc_enc_init_t {
	//////////// Base Address ////////////
	codec_addr_t m_RegBaseAddr[2];	// physical[0] and virtual[1] address of a hevc encoder registers (refer to datasheet)
	codec_addr_t m_BitWorkAddr[2];	// physical[0] and virtual[1] address of a working space of the encoder. This working buffer space consists of work buffer, code buffer, and parameter buffer.
	int m_iBitWorkSize;		// The size of the buffer in bytes pointed by VPU Work Buffer. This value must be a multiple of 1024.
	int m_iCodeSize;		// TBD : 0 (The size of the buffer in bytes pointed by Code Buffer. This value must be a multiple of 1024.)
	codec_addr_t m_CodeAddr[2];	// TBD : 0 (physical[0] and virtual[1] address of a code buffer of the encoder.)

	//////////// Callback Func ////////////

	// Copies the values of num bytes from the location pointed to by source directly to the memory block pointed to by destination.
	//  [input] Pointer to the destination buffer where the content is to be copied
	//  [input] srcPointer to the source of data to be copied
	//  [input] num : Number of bytes to copy
	//  [input] type : option (default 0)
	//  [output] Returns a pointer to the destination area str.
	void *(*m_Memcpy) (void *, const void *, unsigned int, unsigned int);

	// Sets the first num bytes of the block of memory pointed by ptr to the specified value
	//  [input] ptr : Pointer to the block of memory to fill
	//  [input] value : Value to be set.
	//  [input] num : Number of bytes to be set to the value.
	//  [input] type : option (default 0)
	//  [output] none
	void (*m_Memset) (void *, int, unsigned int, unsigned int);
	int (*m_Interrupt) (void);	// hw interrupt (return value is always 0)
	void (*m_Usleep)(unsigned int, unsigned int);

	// [input] hevc_physical_addr_t physical_address
	// [input] unsigned int size
	// [output] void* virtual_address
	void *(*m_Ioremap) (unsigned int, unsigned int);
	void  (*m_Iounmap) (void *);	// [input] void* virtual_address

	// [input]  void * vritual_base_reg_addr
	// [input]  unsigned int offset
	// [output] reading register 32-bits data
	// example:  *((volatile codec_addr_t *)(vritual_base_video_reg_addr+offset))
	unsigned int (*m_reg_read)(void *, unsigned int);

	// [input] void *vritual_base_video_reg_addr
	// [input] unsigned int offset
	// [input] unsigned int data
	// [output] none
	// example: *((volatile codec_addr_t *)(vritual_base_video_reg_addr+offset)) = (unsigned int)(data);
	void (*m_reg_write)(void *, unsigned int, unsigned int);

	void *m_CallbackReserved[4];


	//////////// Encoding Info ////////////

	int m_iBitstreamFormat;		//!< STD_HEVC_ENC (17) only

	int m_iPicWidth;		//!< The width of a picture to be encoded in pixels (multiple of 8)
	int m_iPicHeight;		//!< The height of a picture to be encoded in pixels (multiple of 8)

	int m_iFrameRate;		//!< frames per second

	int m_iTargetKbps;		//!< Target bit rate in Kbps. if 0, there will be no rate control,
					//!< and pictures will be encoded with a quantization parameter equal to quantParam

	int m_iKeyInterval;		//!< max 32767

	// The start physical[0] and virtual[1] address of output bitstream buffer into which encoder puts bitstream. (multiple of 1024)
	codec_addr_t m_BitstreamBufferAddr[2];

	// The size of the buffer in bytes pointed by output bitstreamBuffer. This value must be a multiple of 1024. The maximum size is 16383 x 1024 bytes.
	// A maximum output bitstream buffer size is the same size of uncompressed frame size.
	// HD - 1080p @ L4.1 Main Profile : 2.97 Mbytes
	// 4K - 3840x2160 @ L5.0 Main Profile : 11.87 Mbytes
	int m_iBitstreamBufferSize;

	int m_iSrcFormat;	// Reserved : 0(YUV420)

	// It specifies a chroma interleave mode of input frame buffer.
	//  0 : chroma separate mode (CbCr data is written in separate frame memories)
	//  1 : chroma interleave mode (CbCr data is interleaved in chroma memory)
	unsigned int m_bCbCrInterleaveMode;

	unsigned int m_bCbCrOrder;	//!< Reserved : 0

	unsigned int m_uiEncOptFlags;	// Reserved : 0

	int m_iUseSpecificRcOption;	// 0 : Use default setting, 1 : Use parameters in m_stRcInit
	hevc_enc_rc_init_t m_stRcInit;

	unsigned int m_uiChipsetInfo;	//example : TCC8059 m_uiChipsetInfo = (1<<28) | (0<<24) | (8<<20) | (0<<16) | (5<<12) | (9<<8) | 0

	unsigned int   m_uiVuiParamOption;
	hevc_enc_vui_t m_stVuiParam;	// VPU encodes VUI only if m_uiVuiParamOption == 1.

	unsigned int m_Reserved[31];

} hevc_enc_init_t;

typedef struct hevc_enc_initial_info_t {
	int m_iMinFrameBufferCount;	// [out] minimum frame buffer count
	int m_iMinFrameBufferSize;	// [out] minimum frame buffer size
	unsigned int m_Reserved[6];
} hevc_enc_initial_info_t;

typedef struct hevc_enc_input_t {
	hevc_physical_addr_t m_PicYAddr;	// It indicates the base address for Y component in the physical address space. (VPU uses only 32bits physical address)
	hevc_physical_addr_t m_PicCbAddr;	// It indicates the base address for Cb component in the physical address space. (VPU uses only 32bits physical address)
						// In case of CbCr interleave mode, Cb and Cr frame data are written to memory area started from m_PicCbAddr address.
	hevc_physical_addr_t m_PicCrAddr;	// It indicates the base address for Cr component in the physical address space. (VPU uses only 32bits physical address)

	int m_iBitstreamBufferSize;
	codec_addr_t m_BitstreamBufferAddr[2]; //!< physical[0] and virtual[1] address (VPU uses only 32bits physical address)

	// If this value is 0, the encoder encodes a picture as normal.
	// If this value is 1, the encoder ignores sourceFrame and generates a skipped picture.
	// In this case, the reconstructed image at decoder side is a duplication of the previous picture. The skipped picture is encoded as P-type regardless of the GOP size.
	int m_iSkipPicture;

	// If this value is 0, the picture type is determined by VPU according to the various parameters such as encoded frame number and GOP size.
	// If this value is 1, the frame is encoded as an I-picture regardless of the frame number or GOP size, and I-picture period calculation is reset to initial state.
	// A force picture type (I, P, B, IDR, CRA)  type
	//   0 :  disable
	//   1 : IDR(Instantaneous Decoder Refresh) picture
	//   2 : I picture
	//   3 : CRA(Clean Random Access) picture
	// This value is ignored if m_iSkipPicture is 1.
	int m_iForceIPicture;

	int m_iQuantParam;		// If this value is (1~51), the value is used for all quantization parameters in case of VBR (no rate control).
					//!< In other cases, the QP is determined by VPU.

	int m_iChangeRcParamFlag;	// RC(Rate control) parameter changing mode
					//	bit[0]: 0-disable, 1-enable
					//	bit[1]: 0-disable, 1-enable(change a bitrate)
					//	bit[2]: 0-disable, 1-enable(change a framerate)
					//	bit[3]: 0-disable, 1-enable(change a Keyframe interval)
	int m_iChangeTargetKbps;	// Target bit rate in Kbps
	int m_iChangeFrameRate;		// Target frame rate
	int m_iChangeKeyInterval;	// Keyframe interval(max 32767)

	void *mvInfoBufAddr[2];		// physical[0] and virtual[1] address for reporting of MV info.
	int mvInfoBufSize;		// Buffer size (= (VALIGN128(width)/64)*VALIGN64(height)
	int mvInfoFormat;		// MV info. format 0 : (default)nothing, 1 : convert (little endian, 16bits per X or Y for CU_16x16)

	unsigned int m_Reserved[28];
} hevc_enc_input_t;

typedef struct hevc_enc_buffer_t {
	codec_addr_t m_FrameBufferStartAddr[2];	// [in] The start physical[0] and virtual[1] address of encoding frames buffer
} hevc_enc_buffer_t;

typedef struct hevc_enc_output_t {
	codec_addr_t m_BitstreamOutAddr[2];	// [out] The start physical[0] and virtual[1] address of output bitstream buffer into which encoder puts bitstream.

	int m_iBitstreamOutSize;		// [out] The size of the buffer in bytes pointed by output bitstreamBuffer.

	int m_iPicType;				// [out] This is the picture type of encoded picture. (I- or P- picture)

	int m_iAvgQp;

	void *mvInfoBufAddr[2];		// [out] physical[0] and virtual[1] address for reporting of MV info.
	int mvInfoBufSize;		// [out] Buffer size
	int mvInfoFormat;		// [out] MV info. format 0 : (default)nothing, 1 : convert (little endian, 16bits per X or Y for CU_16x16)

	unsigned int m_Reserved[11];
} hevc_enc_output_t;

// This structure is used for adding a header syntax layer into the encoded bit stream.
// The parameter headerType is the input parameter to VPU, and
// the other two parameters are returned values from VPU after completing requested operation.
typedef struct hevc_enc_header_t {
	int m_iHeaderType;	// [in] This is a type of header that User wants to generate and have values as (HEVC_ENC_CODEOPT_ENC_VPS | HEVC_ENC_CODEOPT_ENC_SPS | HEVC_ENC_CODEOPT_ENC_PPS)

	int m_iHeaderSize;	// [in] The size of header stream buffer
				// [out] The size of the generated stream in bytes

	codec_addr_t m_HeaderAddr[2];	// [in/out] A physical[0] and virtual[1] address pointing the generated stream location.

	unsigned int m_Reserved[8];
} hevc_enc_header_t;

/*!
 ********************************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC	: main api function of libvpuhevcenc
 * \param
 *		[in]Op			: encoder operation
 * \param
 *		[in,out]pHandle		: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: init or input parameter
 * \param
 *		[in]pParam2		: output or info parameter
 * \return
 *		If successful, TCC_VPU_HEVC_ENC returns 0 or plus. Otherwise, it returns a minus value.
 * \example
 *     TCC_VPU_HEVC_ENC( VPU_ENC_INIT                     , codec_handle_t*     , hevc_enc_init_t*      , hevc_enc_initial_info_t*);
 *     TCC_VPU_HEVC_ENC( VPU_ENC_REG_FRAME_BUFFER, codec_handle_t*     , hevc_enc_buffer_t*   , (void*)NULL);
 *     TCC_VPU_HEVC_ENC( VPU_ENC_PUT_HEADER         , codec_handle_t*     , hevc_enc_header_t*  , (void*)NULL);
 *     TCC_VPU_HEVC_ENC( VPU_ENC_ENCODE               , codec_handle_t*     , hevc_enc_input_t*    , hevc_enc_output_t*);
 *     TCC_VPU_HEVC_ENC( VPU_ENC_CLOSE                  , codec_handle_t*     , (void*)NULL            , (void*)NULL);
 ********************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);

/*!
 ********************************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC_ESC	: exit api function of libvpuhevcenc
 * \param
 *		[in]Op			: forced exit operation, 1 (only)
 * \param
 *		[in,out]pHandle		: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: NULL
 * \param
 *		[in]pParam2		: NULL
 * \return
 *		TCC_VPU_HEVC_ENC_ESC returns 0.
 * \example
 *      TCC_VPU_HEVC_ENC_ESC( 1, codec_handle_t*, (void*)NULL, (void*)NULL );
 ********************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_ESC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);

/*!
 ********************************************************************************
 * \brief
 *		TCC_VPU_HEVC_ENC_EXT	: exit api function of libvpuhevcenc
 * \param
 *		[in]Op			: forced exit operation, 1 (only)
 * \param
 *		[in,out]pHandle		: libvpuhevcenc's handle
 * \param
 *		[in]pParam1		: NULL
 * \param
 *		[in]pParam2		: NULL
 * \return
 *		TCC_VPU_HEVC_ENC_EXT returns 0.
 * \example
 *      TCC_VPU_HEVC_ENC_EXT( 1, codec_handle_t*, (void*)NULL, (void*)NULL );
 ********************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_EXT(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);

#endif  //INC_DEVICE_TREE_PMAP

#endif//TCC_VPU_HEVC_ENC_H
