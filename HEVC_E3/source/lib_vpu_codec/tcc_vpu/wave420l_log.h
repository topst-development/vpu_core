/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef WAVE420L_LOG__H
#define WAVE420L_LOG__H

extern void (*g_pfPrintCb)(const char *, ...); /**< printk callback */

#define V_PRINTF(fmt, ...) \
do { \
    if (g_pfPrintCb != NULL) { \
        g_pfPrintCb(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#define PRT_PREFIX "[Corelib-Vpu-hevc-enc] "

#define VPU_KERN_EMERG     "\0010"         /* system is unusable */
#define VPU_KERN_ALERT     "\0011"         /* action must be taken immediately */
#define VPU_KERN_CRIT      "\0012"         /* critical conditions */
#define VPU_KERN_ERR       "\0013"         /* error conditions */
#define VPU_KERN_WARNING   "\0014"         /* warning conditions */
#define VPU_KERN_NOTICE    "\0015"         /* normal but significant condition */
#define VPU_KERN_INFO      "\0016"         /* informational */
#define VPU_KERN_DEBUG     "\0017"         /* debug-level messages */
#define V_PRINTF_PREFIX    VPU_KERN_ALERT

#define COL_RESET       "\x1b[0m"
#if 1
#define COL_RED         "\x1b[31m"
#define COL_GREEN       "\x1b[32m"
#define COL_BLUE        "\x1b[34m"
#define COL_YELLOW      "\x1b[33m"
#define COL_MAGENTA     "\x1b[35m"
#define COL_CYAN        "\x1b[36m"
#define COL_WHITE       "\x1b[39m"
#else //bright
#define COL_RED         "\x1b[31;1m"
#define COL_GREEN       "\x1b[32;1m"
#define COL_BLUE        "\x1b[34;1m"
#define COL_YELLOW      "\x1b[33;1m"
#define COL_MAGENTA     "\x1b[35;1m"
#define COL_CYAN        "\x1b[36;1m"
#define COL_WHITE       "\x1b[39;1m"
#endif

// Global variables for controlling log levels and conditions in the VPU 4K D2 decoder.
extern int g_bLogVerbose;
extern int g_bLogDebug;
extern int g_bLogInfo;
extern int g_bLogWarn;
extern int g_bLogError;
extern int g_bLogAssert;
extern int g_bLogFunc;
extern int g_bLogTrace;
extern int g_bLogEncodeSuccess;

#define CLOGV(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RESET   PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGD(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_YELLOW  PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGI(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_GREEN   PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGW(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGE(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RED     PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGA(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_CYAN    PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGF(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGT(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_BLUE    PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)

#define DLOGV(fmt, ...)    if (g_bLogVerbose == 1) { CLOGV(fmt, ##__VA_ARGS__); }
#define DLOGD(fmt, ...)    if (g_bLogDebug   == 1) { CLOGD(fmt, ##__VA_ARGS__); }
#define DLOGI(fmt, ...)    if (g_bLogInfo    == 1) { CLOGI(fmt, ##__VA_ARGS__); }
#define DLOGW(fmt, ...)    if (g_bLogWarn    == 1) { CLOGW(fmt, ##__VA_ARGS__); }
#define DLOGE(fmt, ...)    if (g_bLogError   == 1) { CLOGE(fmt, ##__VA_ARGS__); }
#define DLOGA(fmt, ...)    if (g_bLogAssert  == 1) { CLOGA(fmt, ##__VA_ARGS__); }
#define DLOGF(fmt, ...)    if (g_bLogFunc    == 1) { CLOGF(fmt, ##__VA_ARGS__); }
#define DLOGT(fmt, ...)    if (g_bLogTrace   == 1) { CLOGT(fmt, ##__VA_ARGS__); }

#define DLOG_ENC_SUCCESS(fmt, ...)    if (g_bLogEncodeSuccess == 1) { CLOGV(fmt, ##__VA_ARGS__); }

#define DLOGF_BEGIN DLOGF("==== Begin ====")
#define DLOGF_END   DLOGF("====  End  ====")

#endif // WAVE420L_LOG__H
