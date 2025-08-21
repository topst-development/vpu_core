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
// File         : vpuapi.h
// Description  :
//-----------------------------------------------------------------------------

#ifndef VPUAPI_H_INCLUDED
#define VPUAPI_H_INCLUDED

#include "vpuconfig.h"

#ifndef NULL
#	define NULL 0
#endif

#ifdef ARM_ADS
#	define VpuWriteReg(ADDR, DATA)     *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
#	define VpuReadReg(ADDR)            *(volatile unsigned int *)(ADDR)
#	define VpuWriteMem(ADDR, DATA )    *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
#	define VpuReadMem(ADDR)            *(volatile unsigned int *)(ADDR)
#	define VirtualWriteReg(ADDR, DATA) *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
#elif defined(_WIN32_WCE) || defined(__LINUX__) || defined(__ANDROID__)
extern void VpuWriteReg(unsigned int ADDR, unsigned int DATA);
extern unsigned int VpuReadReg(unsigned int ADDR);
#	define VpuWriteMem(a,b,c,d)
#	define VpuReadMem(a,b,c,d)
#	define VirtualWriteReg(ADDR, DATA) *((volatile unsigned int *)(ADDR)) = (unsigned int)(DATA)
#endif

#define MAX_ROI_NUMBER	10

//------------------------------------------------------------------------------
// common struct and definition
//------------------------------------------------------------------------------

typedef enum {
	STD_AVC                         = 0,
	STD_VC1                         = 1,
	STD_MPEG2                       = 2,
	STD_MPEG4                       = 3,
	STD_H263                        = 4,
	STD_AVS                         = 7,
	STD_MJPG                        = 10,
	STD_VP8                         = 11,
	STD_MVC                         = 14,
	STD_NULL                        = 0x7FFFFFFF
} CodStd;

typedef enum {
	RETCODE_SUCCESS,
	RETCODE_FAILURE,
	RETCODE_INVALID_HANDLE,
	RETCODE_INVALID_PARAM,
	RETCODE_INVALID_COMMAND,
	RETCODE_ROTATOR_OUTPUT_NOT_SET,
	RETCODE_ROTATOR_STRIDE_NOT_SET,
	RETCODE_FRAME_NOT_COMPLETE,
	RETCODE_INVALID_FRAME_BUFFER,
	RETCODE_INSUFFICIENT_FRAME_BUFFERS,
	RETCODE_INVALID_STRIDE,
	RETCODE_WRONG_CALL_SEQUENCE,
	RETCODE_CALLED_BEFORE,
	RETCODE_NOT_INITIALIZED,
	RETCODE_USERDATA_BUF_NOT_SET,
	RETCODE_MEMORY_ACCESS_VIOLATION,
	RETCODE_JPEG_EOS,
	RETCODE_JPEG_BIT_EMPTY,
	RETCODE_NULL                    = 0x7FFFFFFF
} RetCode;

typedef enum {
	ENABLE_ROTATION,
	DISABLE_ROTATION,
	ENABLE_MIRRORING,
	DISABLE_MIRRORING,
	SET_MIRROR_DIRECTION,
	SET_ROTATION_ANGLE,
	SET_ROTATOR_OUTPUT,
	SET_ROTATOR_STRIDE,
	DEC_SET_SPS_RBSP,
	DEC_SET_PPS_RBSP,
	ENABLE_DERING,
	DISABLE_DERING,
	SET_SEC_AXI,
	ENABLE_REP_USERDATA,
	DISABLE_REP_USERDATA,
	SET_ADDR_REP_USERDATA,
	SET_SIZE_REP_USERDATA,
	SET_USERDATA_REPORT_MODE,
	SET_CACHE_CONFIG,
	ENABLE_MC_CACHE,
	DISABLE_MC_CACHE,
	SET_DECODE_FLUSH,
	DEC_SET_FRAME_DELAY,
	DEC_SET_AVC_ERROR_CONCEAL_MODE,	// REFSW_313
	DEC_GET_FIELD_PIC_TYPE,
	DEC_ENABLE_REORDER,
	DEC_DISABLE_REORDER,
	DEC_SET_DISPLAY_FLAG,
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	ENC_PUT_VIDEO_HEADER,
	ENC_PUT_MP4_HEADER,
	ENC_PUT_AVC_HEADER,
	ENC_GET_JPEG_HEADER,
	ENC_SET_INTRA_MB_REFRESH_NUMBER,
	ENC_ENABLE_HEC,
	ENC_DISABLE_HEC,
	ENC_SET_SLICE_INFO,
	ENC_SET_GOP_NUMBER,
	ENC_SET_INTRA_QP,
	ENC_SET_BITRATE,
	ENC_SET_FRAME_RATE,
	ENC_SET_REPORT_MBINFO,
	ENC_SET_PIC_PARA_ADDR,
	ENC_SET_SUB_FRAME_SYNC,
	ENC_ENABLE_SUB_FRAME_SYNC,
	ENC_DISABLE_SUB_FRAME_SYNC,
#endif
	CMD_END,
	CMD_NULL                        = 0x7FFFFFFF
} CodecCommand;

typedef enum {
	MIRDIR_NONE,
	MIRDIR_VER,
	MIRDIR_HOR,
	MIRDIR_HOR_VER,
	MIRDIR_NULL                     = 0x7FFFFFFF
} MirrorDirection;

typedef enum {
	FORMAT_420                      = 0,
	FORMAT_422                      = 1,
	FORMAT_224                      = 2,
	FORMAT_444                      = 3,
	FORMAT_400                      = 4,
	FORMAT_NULL                     = 0x7FFFFFFF
} FrameBufferFormat;

typedef enum {
	AVC_ERROR_CONCEAL_MODE_DEFAULT  = 0,
	AVC_ERROR_CONCEAL_MODE_ENABLE_SELECTIVE_CONCEAL_MISSING_REFERENCE = 1,
	AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_MISSING_REFERENCE = 2,
	AVC_ERROR_CONCEAL_MODE_DISABLE_CONCEAL_WRONG_FRAME_NUM = 4,
	AVC_ERROR_CONCEAL_MODE_NULL     = 0x7FFFFFFF
} AVCErrorConcealMode;

typedef enum {
	VPUCNM_PRODUCT_ID_980           = 0,
	VPUCNM_PRODUCT_ID_960           = 1,
	VPUCNM_PRODUCT_ID_950           = 1,    // same with CODA960
	VPUCNM_PRODUCT_ID_512,
	VPUCNM_PRODUCT_ID_520,
	VPUCNM_PRODUCT_ID_NONE          = 0x7FFFFFFF,
} ProductId;

typedef enum {
	INT_BIT_INIT                    = 0,
	INT_BIT_SEQ_INIT                = 1,
	INT_BIT_SEQ_END                 = 2,
	INT_BIT_PIC_RUN                 = 3,
	INT_BIT_FRAMEBUF_SET            = 4,
	INT_BIT_ENC_HEADER              = 5,
	INT_BIT_ENC_PARA_SET            = 6,
	INT_BIT_DEC_PARA_SET            = 7,
	INT_BIT_DEC_BUF_FLUSH           = 8,
	INT_BIT_USERDATA                = 9,
	INT_BIT_DEC_FIELD               = 10,
	INT_BIT_DEC_MB_ROWS             = 13,
	INT_BIT_BIT_BUF_EMPTY           = 14,
	INT_BIT_BIT_BUF_FULL            = 15,
	INT_BIT_NULL                    = 0x7FFFFFFF
} InterruptBit;

typedef enum {
	PIC_TYPE_I                      = 0,
	PIC_TYPE_P                      = 1,
	PIC_TYPE_B                      = 2,
	PIC_TYPE_VC1_BI                 = 2,
	PIC_TYPE_VC1_B                  = 3,
	PIC_TYPE_D                      = 3, // D picture in mpeg2, and is only composed of DC codfficients
	PIC_TYPE_S                      = 3, // S picture in mpeg4, and is an acronym of Sprite. and used for GMC
	PIC_TYPE_VC1_P_SKIP             = 4,
	PIC_TYPE_MP4_P_SKIP_NOT_CODED   = 4, // Not Coded P Picture at mpeg4 packed mode
	PIC_TYPE_IDR                    = 5, // H.264 IDR frame
	PIC_TYPE_MAX                    = 6, // no meaning
	PIC_TYPE_NULL                   = 0x7FFFFFFF
} PicType;

typedef enum {
	PAIRED_FIELD                    = 0,
	TOP_FIELD_MISSING               = 1,
	BOT_FIELD_MISSING               = 2,
	TOP_BOT_FIELD_MISSING           = 3,
	FIELD_NULL                      = 0x7FFFFFFF
} AvcNpfFieldInfo;

typedef enum {
	FF_NONE                         = 0,
	FF_FRAME                        = 1,
	FF_FIELD                        = 2,
	FF_NULL                         = 0x7FFFFFFF
} FrameFlag;

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
typedef enum {
	INT_JPU_DONE                    = 0,
	INT_JPU_ERROR                   = 1,
	INT_JPU_BIT_BUF_EMPTY           = 2,
	INT_JPU_BIT_BUF_FULL            = 2,
	INT_JPU_PARIAL_OVERFLOW         = 3,
	INT_JPU_NULL                    = 0x7FFFFFFF
} InterruptVput;
#endif

typedef enum {
	BS_MODE_INTERRUPT,
	BS_MODE_ROLLBACK,
	BS_MODE_PIC_END,
	BS_MODE_NULL                    = 0x7FFFFFFF
} BitStreamMode;

typedef enum {
	SW_RESET_SAFETY,
	SW_RESET_FORCE,
	SW_RESET_ON_BOOT,
	SW_RESET_NULL                   = 0x7FFFFFFF
} SWResetMode;

typedef struct {
	PhysicalAddress bufY;
	PhysicalAddress bufCb;
	PhysicalAddress bufCr;
	PhysicalAddress bufMvCol;
	int myIndex;
	int mapType;
	int stride;
#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	int height;
#endif
} FrameBuffer;

typedef struct {
	Uint32 left;
	Uint32 top;
	Uint32 right;
	Uint32 bottom;
} Rect;

// VP8 specific display information
typedef struct {
	unsigned hScaleFactor : 2;
	unsigned vScaleFactor : 2;
	unsigned picWidth     : 14;
	unsigned picHeight    : 14;
} Vp8ScaleInfo;

typedef struct {
	PhysicalAddress bufferBase;
	int bufferSize;
} ExtBufCfg;

typedef struct {
	ExtBufCfg avcSliceBufInfo;
	ExtBufCfg vp8MbDataBufInfo;
} DecBufInfo;

typedef struct {
	int lowDelayEn;
	int numRows;
} LowDelayInfo;

typedef	struct {
	int useBitEnable;
	int useIpEnable;
	int useDbkYEnable;
	int useDbkCEnable;
	int useOvlEnable;
	int useBtpEnable;
	PhysicalAddress bufBitUse;
	PhysicalAddress bufIpAcDcUse;
	PhysicalAddress bufDbkYUse;
	PhysicalAddress bufDbkCUse;
	PhysicalAddress bufOvlUse;
	PhysicalAddress bufBtpUse;
	int bufSize;
} SecAxiUse;

typedef struct {
	unsigned int CacheMode;
} MaverickCacheConfig;

typedef struct {
	Uint32 *paraSet;
	int size;
} DecParamSet;

struct CodecInst;

//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------

typedef struct CodecInst DecInst;
typedef DecInst *DecHandle;

/**
* @brief    This is a data structure for AVC specific picture information. Only AVC decoder
returns this structure after decoding a frame. For details about all these
flags, please find them in H.264/AVC VUI syntax.
*/
typedef struct {
/**
@verbatim
@* 1 : indicates that the temporal distance between the decoder output times of any
two consecutive pictures in output order is constrained as fixed_frame_rate_flag
in H.264/AVC VUI syntax.
@* 0 : indicates that no such constraints apply to the temporal distance between
the decoder output times of any two consecutive pictures in output order
@endverbatim
*/
	int fixedFrameRateFlag;
/**
@verbatim
timing_info_present_flag in H.264/AVC VUI syntax

@* 1 : FixedFrameRateFlag is valid.
@* 0 : FixedFrameRateFlag is not valid.
@endverbatim
*/
	int timingInfoPresent;
	int chromaLocBotField;      /**< chroma_sample_loc_type_bottom_field in H.264/AVC VUI syntax. It specifies the location of chroma samples for thebottom field. */
	int chromaLocTopField;      /**< chroma_sample_loc_type_top_field in H.264/AVC VUI syntax. It specifies the location of chroma samples for the top field. */
	int chromaLocInfoPresent;   /**< chroma_loc_info_present_flag in H.264/AVC VUI syntax. */
/**
@verbatim
chroma_loc_info_present_flag in H.264/AVC VUI syntax

@* 1 : ChromaSampleLocTypeTopField and ChromaSampleLoc TypeTopField are valid.
@* 0 : ChromaSampleLocTypeTopField and ChromaSampleLoc TypeTopField are not valid.
@endverbatim
*/
	int colorPrimaries;         /**< colour_primaries syntax in VUI parameter in H.264/AVC */
	int colorDescPresent;       /**< colour_description_present_flag in VUI parameter in H.264/AVC */
	int isExtSAR;               /**< This flag This flag indicates whether aspectRateInfo represents 8bit aspect_ratio_idc or 32bit extended_SAR. If the aspect_ratio_idc == extended_SAR mode, this flag returns 1. */
	int vidFullRange;           /**< video_full_range in VUI parameter in H.264/AVC */
	int vidFormat;              /**< video_format in VUI parameter in H.264/AVC */
	int vidSigTypePresent;      /**< video_signal_type_present_flag in VUI parameter in H.264/AVC */
	int vuiParamPresent;        /**< vui_parameters_present_flag in VUI parameter in H.264/AVC */
	int vuiPicStructPresent;    /**< pic_struct_present_flag of VUI in H.264/AVC. This field is valid only for H.264/AVC decoding. */
	int vuiPicStruct;           /**< pic_struct in H.264/AVC VUI reporting (Table D-1 in H.264/AVC specification) */
	int vuiTransferCharacteristics; /**< transfer_characteristics in VUI parameter in H.264/AVC */
	int vuiMatrixCoefficients;  /**< matrix_coefficients in VUI parameter in H.264/AVC */
} AvcVuiInfo;

typedef struct {
	short barLeft;
	short barRight;
	short barTop;
	short barBottom;
} MP2BarDataInfo;

typedef struct {
	Uint32 offsetNum;

	Int16 horizontalOffset1;
	Int16 horizontalOffset2;
	Int16 horizontalOffset3;

	Int16 verticalOffset1;
	Int16 verticalOffset2;
	Int16 verticalOffset3;

} MP2PicDispExtInfo;

typedef struct {
	CodStd bitstreamFormat;
	PhysicalAddress bitstreamBuffer;
	int bitstreamBufferSize;
	int mp4DeblkEnable;
	int avcExtension;
	int mp4Class;
	int mapType;
	int tiled2LinearEnable;
	int x264Enable;
	PhysicalAddress workBuffer;
	int workBufferSize;
	int cbcrInterleave;
	int bwbEnable;
	int frameEndian;
	int streamEndian;
	int bitstreamMode;
	int m_bM4vGmcDisable;
	int avcnpfenable;
	int ReorderEnable;
	int SWRESETEnable;
	int seq_init_bsbuff_change;
	int H263_SKIPMB_ON;
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	int picWidth;
	int picHeight;
	BYTE *pBitStream;
	int jpgLineBufferMode;
	int thumbNailEn;
#endif
} DecOpenParam;

typedef struct {
	int picWidth;
	int picHeight;
	int fRateNumerator;
	int fRateDenominator;
	int frameRateRes;
	int frameRateDiv;
	Rect picCropRect;
	int mp4DataPartitionEnable;
	int mp4ReversibleVlcEnable;
	int mp4ShortVideoHeader;
	int mp4_packMode;
	int h263AnnexJEnable;
	int minFrameBufferCount;
	int frameBufDelay;
	int normalSliceSize;
	int worstSliceSize;

	// Report Information
	int profile;
	int level;
	int interlace;
	int constraint_set_flag[4];
	int direct8x8Flag;
	int vc1Psf;
	int avcCtType;
	//int avcIsExtSAR;
	//int maxNumRefFrm;
	//int maxNumRefFrmFlag;
	int aspectRateInfo;
	int bitRate;
	Vp8ScaleInfo vp8ScaleInfo;

	//int mp2LowDelay;
	//int mp2DispVerSize;
	//int mp2DispHorSize;

	int userDataNum;	// User Data
	int userDataSize;
	int userDataBufFull;

	int mp2ColorPrimaries;
	int mp2TransferChar;
	int mp2MatrixCoeff;
	MP2BarDataInfo mp2BardataInfo;	// RefSW 2.1.13 20140228

	int chromaFormat;
	//int chromaFormatIDC;/**< A chroma format indicator */	//VUI information

	AvcVuiInfo avcVuiInfo;	// REFSW_313

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	int mjpg_sourceFormat;
	int mjpg_ecsPtr;
#endif

	PhysicalAddress rdPtr;
	PhysicalAddress wrPtr;

	int seqInitErrReason;	// please refer to [Appendix D : ERROR REASONS IN DECODING SEQUENCE HEADERS chapter in programmers guide] to know the meaning of the value for detail.
	//int sequenceNo;
} DecInitialInfo;

// Report Information

typedef struct {
	int iframeSearchEnable;
	int skipframeMode;
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	// for mjpeg partial decoding
	int mjpegScaleDownRatioWidth;
	int mjpegScaleDownRatioHeight;
	PhysicalAddress jpgChunkBase;
	BYTE *pJpgChunkBase;
	int jpgChunkSize;
#endif
	union {
		int mp2PicFlush;
		int vDbkMode;
	} DecStdParam;

	int avcDpbResetOnIframeSearch;	//0: None, 1:DPB reset //CNMFW_TEST_CONTROL_HIDDEN_API
} DecParam;

// Report Information
typedef struct {
	int userDataNum;	// User Data
	int userDataSize;
	int userDataBufFull;
	int mp2ActiveFormat;
} DecOutputExtData;

// VP8 specific header information
typedef struct {
	unsigned showFrame     : 1;
	unsigned versionNumber : 3;
	unsigned refIdxLast    : 8;
	unsigned refIdxAltr    : 8;
	unsigned refIdxGold    : 8;
} Vp8PicInfo;

// MVC specific picture information
typedef struct {
	int viewIdxDisplay;
	int viewIdxDecoded;
} MvcPicInfo;

// AVC specific SEI information (frame packing arrangement SEI)
typedef struct {
	unsigned int exist;
	unsigned int framePackingArrangementId;
	unsigned int framePackingArrangementCancelFlag;
	unsigned int quincunxSamplingFlag;
	unsigned int spatialFlippingFlag;
	unsigned int frame0FlippedFlag;
	unsigned int fieldViewsFlag;
	unsigned int currentFrameIsFrame0Flag;
	unsigned int frame0SelfContainedFlag;
	unsigned int frame1SelfContainedFlag;
	unsigned int framePackingArrangementExtensionFlag;
	unsigned int framePackingArrangementType;
	unsigned int contentInterpretationType;
	unsigned int frame0GridPositionX;
	unsigned int frame0GridPositionY;
	unsigned int frame1GridPositionX;
	unsigned int frame1GridPositionY;
	unsigned int framePackingArrangementRepetitionPeriod;
} AvcFpaSei;

// AVC specific HRD information
typedef struct {
	int cpbMinus1;
	int vclHrdParamFlag;
	int nalHrdParamFlag;
} AvcHrdInfo;

typedef struct {
	int indexFrameDisplay;
	int indexFrameDecoded;
	int picType;
	int picTypeFirst;
	int numOfErrMBs;
	int hScaleFlag;
	int vScaleFlag;
	int notSufficientSliceBuffer;
	int notSufficientPsBuffer;
	int decodingSuccess;
	int interlacedFrame;
	int chunkReuseRequired;
	int sequenceChanged;

	int decPicHeight;
	int decPicWidth;
	Rect decPicCrop;

	//int dispPicHeight;
	//int dispPicWidth;

#if defined(USE_AVC_SEI_PICTIMING_INFO)
	int pic_struct_PicTimingSei;
#endif

	int aspectRateInfo;
	int frameRateRes;
	int frameRateDiv;
	int fRateNumerator;
	int fRateDenominator;

	MP2BarDataInfo mp2BardataInfo;	// RefSW 2.1.13 20140228
	int mp2ColorPrimaries;
	int mp2TransferChar;
	int mp2MatrixCoeff;
	//MP2PicDispExtInfo mp2PicDispExtInfo;
	int mp2TimeCode;

	Vp8ScaleInfo vp8ScaleInfo;
	Vp8PicInfo vp8PicInfo;

	MvcPicInfo mvcPicInfo;
	AvcFpaSei avcFpaSei;
	AvcHrdInfo avcHrdInfo;
	AvcVuiInfo avcVuiInfo;
	int avcNpfFieldInfo;
	int avcPocPic;
	int avcPocTop;
	int avcPocBot;
	int avcCtType;

	//int wprotErrReason;
	//PhysicalAddress wprotErrAddress;

	// Report Information
	int pictureStructure;
	int topFieldFirst;
	int repeatFirstField;
	int progressiveFrame;
	int fieldSequence;
	int frameDct;	// V2.1.8
	int decFrameInfo;	// V2.1.8	// AvcPicTypeExt
	int nalRefIdc;

	int picStrPresent;
	int picTimingStruct;
	int progressiveSequence; // V2.1.8
	int mp4TimeIncrement;    // V2.1.8
	int mp4ModuloTimeBase;   // V2.1.8
	int mp4NVOP;             // TELBB-234
	int mp4packMode;         // TELBB-383

	DecOutputExtData decOutputExtData;
	int consumedByte;
	int frameCycle; // Cycle Log

	int rdPtr;
	int wrPtr;
	int bytePosFrameStart;
	int bytePosFrameEnd;

	//int streamEndFlag;
	//int mp2DispVerSize;
	//int mp2DispHorSize;
	//int mp2NpfFieldInfo;
} DecOutputInfo;


#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

//------------------------------------------------------------------------------
// encode struct and definition
//------------------------------------------------------------------------------

typedef struct CodecInst EncInst;
typedef EncInst *EncHandle;

typedef struct {
	int mp4_dataPartitionEnable;
	int mp4_reversibleVlcEnable;
	int mp4_intraDcVlcThr;
	int mp4_hecEnable;
	int mp4_verid;
	int mp4_profileAndIndication;
} EncMp4Param;

typedef struct {
	int h263_annexIEnable;
	int h263_annexJEnable;
	int h263_annexKEnable;
	int h263_annexTEnable;
} EncH263Param;

/**
 * QUANT_MATRIX_PARAM
 * to support user quantization-matrix interface
 */
typedef struct QUANT_MATRIX_PARAM{
	int scaling_matrix_present;
	int scaling_list_present;
	int scaling_list_default;
} quant_matrix_param;

/**
 * SCALING_LIST
 * to support user quantization-matrix interface
 */
typedef struct SCALING_LIST {
	int scaling_list4x4[6][4][4];    // intra/inter 2 * Y/Cb/Cr 3, 4by4
	int scaling_list8x8[2][8][8];    // intra/inter 2, 8by8
} scaling_list;

/**
    \struct VUI_PARAM
    \brief  VUI parameters
 */
typedef struct {
	int video_signal_type_pres_flag;
	char video_format;
	char video_full_range_flag;
	int colour_descrip_pres_flag;
	char colour_primaries;
	char transfer_characteristics;
	char matrix_coeff;
} VUI_PARAM;

typedef struct {
	int avc_constrainedIntraPredFlag;
	int avc_disableDeblk;
	int avc_deblkFilterOffsetAlpha;
	int avc_deblkFilterOffsetBeta;
	int avc_chromaQpOffset;
	int avc_audEnable;
	int avc_frameCroppingFlag;
	int avc_frameCropLeft;
	int avc_frameCropRight;
	int avc_frameCropTop;
	int avc_frameCropBottom;

	int avc_vui_present_flag; //vui_parameters_present_flag
	VUI_PARAM avc_vui_param;
	int avc_level;
} EncAvcParam;

typedef struct {
	int mjpg_sourceFormat;
	int mjpg_restartInterval;
	BYTE huffVal[4][162];
	BYTE huffBits[4][256];
	BYTE qMatTab[4][64];
	BYTE cInfoTab[4][6];
} EncMjpgParam;

typedef struct{
	int sliceMode;
	int sliceSizeMode;
	int sliceSize;
} EncSliceMode;

typedef struct {
	unsigned subFrameSyncOn : 1;
	unsigned sourceBufNumber : 7;
	unsigned sourceBufIndexBase : 8;
} EncSubFrameSyncConfig;

typedef struct {
	int picWidth;
	int picHeight;
	int alignedWidth;
	int alignedHeight;
	int seqInited;
	int frameIdx;
	int format;

	int rstIntval;
	int busReqNum;
	int mcuBlockNum;
	int compNum;
	int compInfo[3];

	Uint32 huffCode[4][256];
	Uint32 huffSize[4][256];
	BYTE *pHuffVal[4];
	BYTE *pHuffBits[4];
	BYTE *pCInfoTab[5];
	BYTE *pQMatTab[4];

} JpgEncInfo;

typedef struct {
	PhysicalAddress bitstreamBuffer;
	Uint32 bitstreamBufferSize;
	CodStd bitstreamFormat;
	PhysicalAddress workBuffer;
	int workBufferSize;
	int ringBufferEnable;
	int picWidth;
	int picHeight;
	int linear2TiledEnable;
	int mapType;
	int picQpY;
	int frameRateInfo;
	int MESearchRange;
	int bitRate;
	int initialDelay;
	int vbvBufferSize;
	int enableAutoSkip;
	int gopSize;
	int idrInterval;
	int me_blk_mode;
	EncSliceMode slicemode;
	int intraRefresh;
	int ConscIntraRefreshEnable;	// REFSW_313
	int userQpMax;
	int MEUseZeroPmv;		// 0: PMV_ENABLE, 1: PMV_DISABLE
	int IntraCostWeight;	// Additional weight of Intra Cost for mode decision to reduce Intra MB density

	//mp4 only
	int rcIntraQp;
	int userGamma;
	int RcIntervalMode;		// 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level
	int MbInterval;			// use when RcintervalMode is 3

	union {
		EncMp4Param mp4Param;
		EncH263Param h263Param;
		EncAvcParam avcParam;
		EncMjpgParam mjpgParam;
	} EncStdParam;

	// Maverick-II Cache Configuration
	int FrameCacheBypass;
	int FrameCacheBurst;
	int FrameCacheMerge;
	int FrameCacheWayShape;

	int cbcrInterleave;
	int frameEndian;
	int streamEndian;
	int lineBufIntEn;
	int bwbEnable;
	int enhancedRCmodelEnable;
	int h264IdrEncOption;
} EncOpenParam;

typedef struct {
	int minFrameBufferCount;
} EncInitialInfo;

typedef struct {
	int  mode;
	int  number;
	Rect region[MAX_ROI_NUMBER];
	int  qp[MAX_ROI_NUMBER];
} EncSetROI;

typedef struct {
	FrameBuffer * sourceFrame;
	int forceIPicture;
	int skipPicture;
	int quantParam;
	PhysicalAddress picStreamBufferAddr;
	int	picStreamBufferSize;
	int enReportMBInfo;
	PhysicalAddress picParaBaseAddr;
	PhysicalAddress picMbInfoAddr;
} EncParam;

// Report Information
typedef	struct {
	int enable;
	int type;
	int sz;
	PhysicalAddress	addr;
} EncReportInfo;

typedef struct {
	PhysicalAddress bitstreamBuffer;
	Uint32 bitstreamSize;
	int bitstreamWrapAround;
	int picType;
	int numOfSlices;
	int reconFrameIndex;
	int AvgQp;
	int frameCycle; // Cycle Log
	EncReportInfo MbInfo;	// Report Information
} EncOutputInfo;

typedef struct {
	PhysicalAddress paraSet;
#ifdef JPEG_THUMBNAIL_EN
	int thumbEn;
#endif
	BYTE *pParaSet;
	int size;
} EncParamSet;

typedef struct {
	PhysicalAddress buf;
	BYTE *pBuf;
	int size;
	int headerType;
} EncHeaderParam;

typedef enum {
	VOL_HEADER,
	VOS_HEADER,
	VIS_HEADER,
	VIS_HEADER_NULL = 0x7FFFFFFF
} Mp4HeaderType;

typedef enum {
	SPS_RBSP,
	PPS_RBSP,
	SPS_RBSP_MVC,
	PPS_RBSP_MVC,
	PPS_RBSP_MVC_NULL = 0x7FFFFFFF
} AvcHeaderType;

typedef struct {
	Uint32 gopNumber;
	Uint32 intraQp;
	Uint32 bitrate;
	Uint32 framerate;
} stChangeRcPara;


#endif	//TCC_VPU_2K_IP != 0x0630

#ifdef __cplusplus
extern "C" {
#endif

	Uint32 VPU_IsBusy(void);
	Uint32 VPU_IsInit(void);

#ifndef USE_EXTERNAL_FW_WRITER
	RetCode VPU_Init(PhysicalAddress workBuf);
#else
	RetCode VPU_Init(PhysicalAddress workBuf, PhysicalAddress codeBuf, int fw_writing_disable);
#endif

	RetCode VPU_GetVersionInfo(Uint32 *versionInfo, Uint32 *revision, Uint32 *productId);
	void VPU_ClearInterrupt();
#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	int VPU_GetMvColBufSize(CodStd codStd, int width, int height, int num);
#endif
	// function for decode
	RetCode VPU_DecOpen(DecHandle *pHandle, DecOpenParam *pop);
	RetCode VPU_DecClose(DecHandle handle);
	RetCode VPU_DecSetEscSeqInit(DecHandle handle, int escape);
	RetCode VPU_DecGetInitialInfo(DecHandle handle, DecInitialInfo *info);
	RetCode VPU_DecRegisterFrameBuffer(DecHandle handle, FrameBuffer *bufArray, int num, int stride, int height, DecBufInfo * pBufInfo);
	RetCode VPU_DecGetBitstreamBuffer(DecHandle handle, PhysicalAddress *prdPrt, PhysicalAddress *pwrPtr, int *size);
	RetCode VPU_DecUpdateBitstreamBuffer(DecHandle handle, int size);
	RetCode VPU_DecUpdateBitstreamBuffer2(DecHandle handle, int size, int updateEnableFlag);
	RetCode VPU_SWReset(int forcedReset);
	RetCode VPU_SWReset2(void);

	//RetCode VPU_HWReset(void);
	//RetCode VPU_SleepWake(int RunIndex, int RunCodStd, int iSleepWake);

	RetCode VPU_DecStartOneFrame(DecHandle handle, DecParam *param);
	RetCode VPU_DecGetOutputInfo(DecHandle handle, DecOutputInfo *info);
	RetCode VPU_DecBitBufferFlush(DecHandle handle);
	RetCode VPU_DecFrameBufferFlush(DecHandle handle);
	RetCode VPU_DecSetRdPtr(DecHandle handle, PhysicalAddress addr, int updateWrPtr);
	RetCode VPU_DecClrDispFlag(DecHandle handle, int index);
	RetCode VPU_DecGiveCommand(DecHandle handle, CodecCommand cmd, void *parameter);

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	// function for encode
	RetCode VPU_EncOpen(EncHandle *, EncOpenParam *);
	RetCode VPU_EncClose(EncHandle);
	RetCode VPU_EncGetInitialInfo(EncHandle, EncInitialInfo *);
	RetCode VPU_EncRegisterFrameBuffer(EncHandle handle,
					FrameBuffer *bufArray, int num, int stride,
					PhysicalAddress subSampBaseA,
					PhysicalAddress SubSampBaseB,
					ExtBufCfg *scratchBuf);
	RetCode VPU_EncGetBitstreamBuffer(EncHandle handle, PhysicalAddress *prdPrt, PhysicalAddress *pwrPtr, int *size);
	RetCode VPU_EncUpdateBitstreamBuffer(EncHandle handle, int size);
	RetCode VPU_EncStartOneFrame(EncHandle handle, EncParam *param);
	RetCode VPU_EncGetOutputInfo(EncHandle handle, EncOutputInfo *info);
	RetCode VPU_EncGiveCommand(EncHandle handle, CodecCommand cmd, void *parameter);

	int JPU_IsBusy();
	Uint32 JPU_GetStatus();
	void JPU_ClrStatus();

	int resetAllCodecInstanceMember(unsigned option);
	int checkCodecInstanceAddr(void *pCodecInst);
	int checkCodecInstanceUse(void *pCodecInst);
	int checkCodecInstancePended(void *pCodecInst);
	unsigned int checkCodecType(int bitstreamformat, void *pCodecInst);
#endif //TCC_VPU_2K_IP != 0x0630

#ifdef __cplusplus
}
#endif

#endif
