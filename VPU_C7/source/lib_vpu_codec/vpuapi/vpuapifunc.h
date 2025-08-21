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
// File         : vpuapifunc.h
// Description  :
//-----------------------------------------------------------------------------

#ifndef VPUAPI_UTIL_H_INCLUDED
#define VPUAPI_UTIL_H_INCLUDED

#include "vpuapi.h"
#include "regdefine.h"
#if TCC_VPU_2K_IP == 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
#include "TCC_VPU_D6.h"
#else
#include "TCC_VPU_C7.h"
#endif
#include "vpu_pre_define.h"


#ifndef NULL
#	define NULL 0
#endif

// COD_STD
enum {
	AVC_DEC                         = 0,
	VC1_DEC                         = 1,
	MP2_DEC                         = 2,
	MP4_DEC                         = 3,
	AVS_DEC                         = 5,
	MJPG_DEC                        = 6,
	VPX_DEC                         = 7,
	AVC_ENC                         = 8,
	MP4_ENC                         = 11,
	MJPG_ENC                        = 13,
	COD_STD_NULL                    = 0x7FFFFFFF	//MJPG_ENC_NULL
};

// AUX_COD_STD
enum {
	MP4_AUX_MPEG4                   = 0,
	MP4_AUX_NULL                    = 0x7FFFFFFF
};

enum {
	VPX_AUX_THO                     = 0,
	VPX_AUX_VP6                     = 1,
	VPX_AUX_VP8                     = 2,
	VPX_AUX_NULL                    = 0x7FFFFFFF
};

enum {
	AVC_AUX_AVC                     = 0,
	AVC_AUX_MVC                     = 1,
	AVC_AUX_NULL                    = 0x7FFFFFFF
};

// BIT_RUN command
enum {
	SEQ_INIT                        = 1,
	SEQ_END                         = 2,
	PIC_RUN                         = 3,
	SET_FRAME_BUF                   = 4,
	ENCODE_HEADER                   = 5,
	ENC_PARA_SET                    = 6,
	DEC_PARA_SET                    = 7,
	DEC_BUF_FLUSH                   = 8,
	RC_CHANGE_PARAMETER             = 9,
	VPU_SLEEP                       = 10,
	VPU_WAKE                        = 11,
	ENC_ROI_INIT                    = 12,
	FIRMWARE_GET                    = 0xf,
	FIRMWARE_GET_NULL               = 0x7FFFFFFF
};

typedef enum {
	LINEAR_FRAME_MAP                = 0,
	TILED_FRAME_MB_RASTER_MAP       = 5,
	TILED_MAP_TYPE_MAX,
	TILED_MAP__NULL                 = 0x7FFFFFFF
} GDI_TILED_MAP_TYPE;

#define MAX_GDI_IDX                     31
#define MAX_REG_FRAME                   (MAX_GDI_IDX*2)

// SW Reset command
#define VPU_SW_RESET_BPU_CORE           0x008
#define VPU_SW_RESET_BPU_BUS            0x010
#define VPU_SW_RESET_VCE_CORE           0x020
#define VPU_SW_RESET_VCE_BUS            0x040
#define VPU_SW_RESET_GDI_CORE           0x080
#define VPU_SW_RESET_GDI_BUS            0x100

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

#define DC_TABLE_INDEX0                 0
#define AC_TABLE_INDEX0                 1
#define DC_TABLE_INDEX1                 2
#define AC_TABLE_INDEX1                 3

#define Q_COMPONENT0                    0
#define Q_COMPONENT1                    0x40
#define Q_COMPONENT2                    0x80

static enum{ //Exif
	IMAGE_WIDTH                     = 0x0100,
	IMAGE_HEIGHT                    = 0x0101,
	BITS_PER_SAMPLE                 = 0x0102,
	COMPRESSION_SCHEME              = 0x0103,
	PIXEL_COMPOSITION               = 0x0106,
	SAMPLE_PER_PIXEL                = 0x0115,
	YCBCR_SUBSAMPLING               = 0x0212,
	JPEG_IC_FORMAT                  = 0x0201,
	PLANAR_CONFIG                   = 0x011c,
	EXIF_TAG_NULL                   = 0x7FFFFFFF
} EXIF_TAG;

typedef struct{
	Uint32 tag;
	Uint32 type;
	int    count;
	int    offset;
} TAG;

enum {
	JFIF                            = 0,
	JFXX_JPG                        = 1,
	JFXX_PAL                        = 2,
	JFXX_RAW                        = 3,
	EXIF_JPG                        = 4,
	EXIF_JPG_NULL                   = 0x7FFFFFFF
};

typedef struct {
	BYTE *buffer;
	int  index;
	int  size;
} vpu_getbit_context_t;

typedef struct {
	int    PicX;
	int    PicY;
	int    BitPerSample[3];
	int    Compression; // 1 for uncompressed / 6 for compressed(jpeg)
	int    PixelComposition; // 2 for RGB / 6 for YCbCr
	int    SamplePerPixel;
	int    PlanrConfig; // 1 for chunky / 2 for planar
	int    YCbCrSubSample; // 00020002 for YCbCr 4:2:0 / 00020001 for YCbCr 4:2:2
	Uint32 JpegOffset;
	Uint32 JpegThumbSize;
} EXIF_INFO;

typedef struct {
	EXIF_INFO ExifInfo;
	int       ThumbType;

	int       Version;
	BYTE      Pallette[256][3];
} THUMB_INFO;

typedef struct {
	// for Nieuport
	int picWidth;
	int picHeight;
	int alignedWidth;
	int alignedHeight;
	int frameOffset;
	int consumeByte;
	int ecsPtr;
	int pagePtr;
	int wordPtr;
	int bitPtr;
	int format;
	int rstIntval;

	int userHuffTab;

	int huffDcIdx;
	int huffAcIdx;
	int Qidx;

	BYTE huffVal[4][162];
	BYTE huffBits[4][256];
	BYTE cInfoTab[4][6];
	BYTE qMatTab[4][64];

	Uint32 huffMin[4][16];
	Uint32 huffMax[4][16];
	BYTE huffPtr[4][16];

	int busReqNum;
	int compNum;
	int mcuBlockNum;
	int compInfo[3];

	int frameIdx;
	int seqInited;

	BYTE *pBitStream;
	vpu_getbit_context_t gbc;//GetBitContext gbc;

	int lineBufferMode;
	BYTE * pJpgChunkBase;
	int chunkSize;

	int iHorScaleMode;
	int iVerScaleMode;

#ifdef SUPPORT_MJPEG_ERROR_CONCEAL
	//error conceal
	struct{
		int bError;
		int rstMarker;
		int errPosX;
		int errPosY;
	} errInfo;

	int nextOffset;
	int currOffset;

	int curRstIdx;
	int nextRstIdx;
	int setPosX;
	int setPosY;
	int mcuWidth;
	int mcuHeight;
	int gbuStartPtr;         // entry point in stream buffer before pic_run
#	ifdef MJPEG_ERROR_CONCEAL_WORKAROUND
	int errorRstIdx;
#	endif
#	ifdef MJPEG_ERROR_CONCEAL_DEBUG
	int numRstMakerRounding;
#	endif
#endif

	int thumbNailEn;

	THUMB_INFO ThumbInfo;

	struct{
		int MbSize;
		int DecFormat;
		int LumaMbHeight;
		int LumaMbWidth;
		int PicX;
		int PicY;
		int CPicX;
		int CPicY;
		int MbNumX;
		int MbNumY;
	} thumbInfo;
} JpgDecInfo;

#endif //TCC_VPU_2K_IP != 0x0630

typedef struct {
	DecOpenParam openParam;
	DecInitialInfo initialInfo;
	PhysicalAddress workBufBaseAddr;
	PhysicalAddress streamWrPtr;
	PhysicalAddress streamRdPtr;
	PhysicalAddress streamRdPtrBackUp;
	int streamEndflag;
	int frameDisplayFlag;
	int clearDisplayIndexes;
	int setDisplayIndexes;
	PhysicalAddress streamRdPtrRegAddr;
	PhysicalAddress streamWrPtrRegAddr;
	PhysicalAddress streamBufStartAddr;
	PhysicalAddress streamBufEndAddr;
	PhysicalAddress frameDisplayFlagRegAddr;
	int streamBufSize;
	int numFrameBuffers;
	FrameBuffer *frameBufPool;

	int stride;
	int rotationEnable;
	int mirrorEnable;
	int deringEnable;
	MirrorDirection mirrorDirection;
	int rotationAngle;
	FrameBuffer rotatorOutput;
	int rotatorStride;
	int rotatorOutputValid;
	int initialInfoObtained;

	int vc1BframeDisplayValid;
	int mapType;
	int tiled2LinearEnable;
	SecAxiUse secAxiUse;
	MaverickCacheConfig cacheConfig;
	int chunkSize;
	int isFirstFrame;
#ifdef TEST_INTERLACE_BOTTOM_TEST
	int framenum;	// 20120214
#endif
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	JpgDecInfo jpgInfo;
#endif
	int seqInitEscape;
	PhysicalAddress userDataBufAddr;	// Report Information
	int userDataEnable;	// User Data
	int userDataBufSize;
	int userDataReportMode;	// User Data report mode (0 : interrupt mode, 1 interrupt disable mode)

	int m_iPendingReason;
	int frameStartPos;
	int frameEndPos;
	int frameDelay;
	int mp4PackedP_decidx;
	int mp4PackedP_dispidx_prev;
	int isMp4PackedPBstream;
	AVCErrorConcealMode avcErrorConcealMode;
	int avcNpfFieldIdx;
	int reorderEnable;
} DecInfo;

typedef struct {
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	EncOpenParam openParam;
	EncInitialInfo initialInfo;
#endif
	PhysicalAddress workBufBaseAddr;
	PhysicalAddress streamRdPtr;
	PhysicalAddress streamWrPtr;
	int streamEndflag;
	PhysicalAddress streamRdPtrRegAddr;
	PhysicalAddress streamWrPtrRegAddr;
	PhysicalAddress streamBufStartAddr;
	PhysicalAddress streamBufEndAddr;
	int streamBufSize;
	int linear2TiledEnable;
	int mapType;
	FrameBuffer * frameBufPool;
	int numFrameBuffers;
	int stride;
	int rotationEnable;
	int mirrorEnable;
	MirrorDirection mirrorDirection;
	int rotationAngle;
	int initialInfoObtained;
	int ringBufferEnable;
	SecAxiUse secAxiUse;
	MaverickCacheConfig cacheConfig;
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	EncSubFrameSyncConfig subFrameSyncConfig;
	JpgEncInfo jpgInfo;
#endif
	int enReportMBInfo;
	PhysicalAddress picParaBaseAddr;
	PhysicalAddress picMbInfoAddr;
} EncInfo;

typedef struct CodecInst {
	int instIndex;
	int inUse;
	int codecMode;
	int codecModeAux;
	unsigned int bufAlignSize;	// Aligned size of buffer (>= 4Kbytes : VPU_C7_MIN_BUF_SIZE_ALIGN / VPU_D6_MIN_BUF_SIZE_ALIGN )
	unsigned int bufStartAddrAlign;	// Aligned start address of buffer (>= 4Kbytes : VPU_C7_MIN_BUF_START_ADDR_ALIGN / VPU_D6_MIN_BUF_START_ADDR_ALIGN)
	union {
		DecInfo decInfo;
		EncInfo encInfo;
	} CodecInfo;
	unsigned int m_iVersionRTL;
} CodecInst;


#ifdef __cplusplus
extern "C" {
#endif

	RetCode BitLoadFirmware_tc(codec_addr_t codeBase, const Uint16 *codeWord, int codeSize);
	RetCode BitLoadFirmware(codec_addr_t codeBase, const Uint16 *codeWord, int codeSize);

	void BitIssueCommand(CodecInst *inst, int cmd);
	RetCode GetCodecInstance(CodecInst **ppInst);
	void FreeCodecInstance(CodecInst *pCodecInst);
	RetCode CheckInstanceValidity(CodecInst *pci);

	RetCode CheckDecInstanceValidity(DecHandle handle);
	RetCode CheckDecOpenParam(DecOpenParam *pop);
	int DecBitstreamBufEmpty(DecInfo *pDecInfo);
	RetCode SetParaSet(DecHandle handle, int paraSetType, DecParamSet *para);

	int GetMvColBufSize(int framebufFormat, int picWidth, int picHeight, int mapType);

#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)

	int EncGetFrameBufSize(int framebufFormat, int picWidth, int picHeight, int mapType);

	int JpuGbuInit(vpu_getbit_context_t *ctx, BYTE *buffer, int size);
	int JpuGbuGetUsedBitCount(vpu_getbit_context_t *ctx);
	int JpuGbuGetLeftBitCount(vpu_getbit_context_t *ctx);
	unsigned int JpuGbuGetBit(vpu_getbit_context_t *ctx, int bit_num);
	unsigned int JpuGguShowBit(vpu_getbit_context_t *ctx, int bit_num);

	int JpegDecodeHeader(DecInfo *pDecInfo);
	int JpgDecQMatTabSetUp(DecInfo *pDecInfo);
	int JpgDecHuffTabSetUp(DecInfo *pDecInfo);
	void JpgDecGramSetup(DecInfo *pDecInfo);
#	ifdef SUPPORT_MJPEG_ERROR_CONCEAL
	int JpegDecodeConcealError(DecInfo *pDecInfo);
#	endif

	RetCode CheckEncInstanceValidity(EncHandle handle);
	RetCode CheckEncOpenParam(EncOpenParam *pop);
	RetCode CheckEncParam(EncHandle handle, EncParam *param);
	RetCode GetEncHeader(EncHandle handle, EncHeaderParam *encHeaderParam);
	void EncParaSet(EncHandle handle, int paraSetType);
	RetCode SetGopNumber(EncHandle handle, Uint32 *gopNumber);
	RetCode SetIntraQp(EncHandle handle, Uint32 *intraQp);
	RetCode SetBitrate(EncHandle handle, Uint32 *bitrate);
	RetCode SetFramerate(EncHandle handle, Uint32 *framerate);
	RetCode SetIntraRefreshNum(EncHandle handle, Uint32 *pIntraRefreshNum);
	RetCode SetSliceMode(EncHandle handle, EncSliceMode *pSliceMode);
	RetCode SetHecMode(EncHandle handle, int mode);
	void EncSetHostParaAddr(PhysicalAddress baseAddr, PhysicalAddress paraAddr);

	int JpgEncLoadHuffTab(EncInfo *pEncInfo);
	int JpgEncLoadQMatTab(EncInfo *pEncInfo);
#	ifdef JPEG_THUMBNAIL_EN
	int JpgEncEncodeThumbnailHeader(EncHandle handle, EncParamSet *para);
#	endif
	int JpgEncEncodeHeader(EncHandle handle, EncParamSet *para);

#endif	//TCC_VPU_2K_IP != 0x0630

	void *GetPendingInstPointer(void);
	void SetPendingInst(CodecInst *inst);
	void ClearPendingInst();
	CodecInst *GetPendingInst();

#ifdef USER_OVERRIDE_MPEG4_PROFILE_LEVEL
	int LevelCalculationMp4(int width, int height, int frameRate, int BitRatekbps);
#endif
#ifdef USER_OVERRIDE_H264_PROFILE_LEVEL
	int LevelCalculationAvc(int width, int height, int frameRate, int BitRatekbps, int SliceNum);
#endif
#if TCC_VPU_2K_IP != 0x0630	//VPU_D6(0x0630) VPU_C7(0x0700)
	//int LevelCalculation(int MbNumX, int MbNumY, int frameRateInfo, int interlaceFlag, int BitRate, int SliceNum);
#endif

#ifdef __cplusplus
}
#endif

#endif // endif VPUAPI_UTIL_H_INCLUDED
