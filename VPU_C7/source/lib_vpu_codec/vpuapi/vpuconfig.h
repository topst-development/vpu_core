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
// File         : vpuconfig.h
// Description  :
//-----------------------------------------------------------------------------


#ifndef VPU_CONFIG_H_INCLUDED
#define VPU_CONFIG_H_INCLUDED

#include "config.h"
#include "vputypes.h"


#define VPU_FRAME_ENDIAN                0	// VDI_LITTLE_ENDIAN
#define VPU_STREAM_ENDIAN               0	// VDI_LITTLE_ENDIAN

#define MAX_DEC_PIC_WIDTH               1920
#define MAX_DEC_PIC_HEIGHT              1088

#define MAX_ENC_PIC_WIDTH               2032 //1920
#define MAX_ENC_PIC_HEIGHT              2032 //1088

#define MAX_ENC_MJPG_PIC_WIDTH          8192
#define MAX_ENC_MJPG_PIC_HEIGHT         8192

//#define MAX_FRAME                       (19*MAX_NUM_INSTANCE) // For AVC decoder, 16(reference) + 2(current) + 1(rotator)
#define MAX_FRAME                       (16+1+4+11)     // AVC REF 16, REC 1 , FULL DUP , Additional 11. (in total of 32)

#define STREAM_FILL_SIZE                (512 * 16)	//  4 * 1024 | 512 | 512+256( wrap around test )
#define STREAM_END_SIZE                 0
#define STREAM_READ_SIZE                (512 * 8)

#define USE_BIT_INTERNAL_BUF            1
#define USE_IP_INTERNAL_BUF             1
#define USE_DBKY_INTERNAL_BUF           1
#define USE_DBKC_INTERNAL_BUF           1
#define USE_OVL_INTERNAL_BUF            1
#define USE_BTP_INTERNAL_BUF            1

#define USE_ME_INTERNAL_BUF             1

#define VPU_REORDER_ENABLE              1	// it can be set to 1 to handle reordering DPB in host side.

#define VPU_ENABLE_BWB                  1

#define	CBCR_INTERLEAVE                 0	//[default 1 for BW checking with CnMViedo Conformance] 0 (chroma separate mode), 1 (chroma interleave mode) // if the type of tiledmap uses the kind of MB_RASTER_MAP. must set to enable CBCR_INTERLEAVE

#define VPU_GBU_SIZE                    1024
#define VPU_SPP_CHUNK_SIZE              512
#define JPU_GBU_SIZE                    512

#define VPU_GMC_PROCESS_METHOD          0

#define VPU_REPORT_USERDATA             0
#define VPU_USER_DATA_ENDIAN            VDI_LITTLE_ENDIAN



//********************************************//
//      External Memory Map Table
//********************************************//

#define DRAM_BASE                       0x00000000	//0x80000000

//----- bitstream ----------------------------//
#define	ADDR_BIT_STREAM                 (DRAM_BASE+0x000000)
#define	STREAM_BUF_SIZE                 0x100000	//0x040000 * max 4 instance

//----- slice save buffer ---------------------//
#define	ADDR_VP8_MB_SAVE_BUFFER         (DRAM_BASE+0x200000)
//#define	VP8_MB_SAVE_SIZE                0x100000

#define	ADDR_SLICE_SAVE_BUFFER          (DRAM_BASE+0x200000)
//#define	SLICE_SAVE_SIZE                 0x180000        //1.5MB >= 1920*1088*3/4 = (MAX_DEC_PIC_WIDTH*MAX_DEC_PIC_HEIGHT*3/4) : buffer for ASO/FMO

//----- ps save buffer ---------------------//
#define	ADDR_PS_SAVE_BUFFER             (DRAM_BASE+0x300000)

//----- report + user data---------------------//
#define	SIZE_PARA_BUF                   0x10000
#define	SIZE_USER_BUF                   0x10000
#define	SIZE_MV_DATA                    0x20000
#define	SIZE_MB_DATA                    0x4000
#define	SIZE_MBMV_DATA                  0x4000
#define	SIZE_FRAME_BUF_STAT             0x100
#define	USER_DATA_INFO_OFFSET           8*17
#define	ADDR_USER_BASE_ADDR             (DRAM_BASE+0x350000)

#define	ADDR_PIC_PARA_BASE_ADDR         (ADDR_USER_BASE_ADDR        + SIZE_USER_BUF)
#define	ADDR_MV_BASE_ADDR               (ADDR_PIC_PARA_BASE_ADDR    + SIZE_PARA_BUF)
#define	ADDR_MB_BASE_ADDR               (ADDR_MV_BASE_ADDR          + SIZE_MV_DATA)
#define	ADDR_FRAME_BUF_STAT_BASE_ADDR   (ADDR_MB_BASE_ADDR          + SIZE_MB_DATA)

#define ADDR_END_OF_RPT_BUF             (ADDR_FRAME_BUF_STAT_BASE_ADDR  + SIZE_FRAME_BUF_STAT)
//#define ADDR_END_OF_RPT_BUF           (ADDR_MBMV_BASE_ADDR + SIZE_MBMV_DATA)

//----- code + temp + para ---------------------//
#define	ADDR_BPU_BASE                   (DRAM_BASE+0x3B8600)

#define	ADDR_BIT_WORK                   (ADDR_BPU_BASE + TEMP_BUF_SIZE + PARA_BUF_SIZE + CODE_BUF_SIZE)

#if (ADDR_BPU_BASE < ADDR_END_OF_RPT_BUF)
#error "ERROR : ADDR_CODE_BUF"
#endif


//----- frame buffer ---------------------------//
#define ADDR_FRAME_BASE                 (DRAM_BASE+0x500000)

#define MAX_FRAME_BASE                  (DRAM_BASE+0x8000000)		// End address of the DPB(128MB-5MB)

#define MVC_DISP_BASE                   (MAX_FRAME_BASE-(MAX_DEC_PIC_WIDTH*MAX_DEC_PIC_HEIGHT*1.5))

#if (ADDR_FRAME_BASE < ADDR_BIT_WORK + SIZE_BIT_WORK)
#error "ERROR : ADDR_FRAME_BASE"
#endif

//********************************************//
//      End of external Memory Map Table
//********************************************//

// DRAM configuration for TileMap access
#define EM_RAS                          13
#define EM_BANK                         2
#define EM_CAS                          9
#define EM_WIDTH                        3

#define SDRAM_BUS_WIDTH                 8
#define SDRAM_BUS_ALIGN                 3

// project version
#define	PRJ_CODAHX_14_00                0xE10E
#define	PRJ_CODAHX_14_01                0xE00E
#define	PRJ_CODAHX_14_02                0xFD0E
#define	PRJ_CODAHX_14_03                0xE41B
#define	PRJ_CODA_8550_00                0xE519
#define	PRJ_CODA_8550_01                0xFC19

#define BODA950_CODE        0x9500
#define CODA960_CODE        0x9600

#endif	/* VPU_CONFIG_H_INCLUDED */