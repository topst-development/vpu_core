/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU_C7_LOG__H
#define VPU_C7_LOG__H

extern void (*g_pfPrintCb_VpuC7)(const char *, ...); /**< printk callback */

#define V_PRINTF(fmt, ...) \
do { \
    if (g_pfPrintCb_VpuC7 != NULL) { \
        g_pfPrintCb_VpuC7(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#define PRT_PREFIX "[Corelib-Vpu-C7] "

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

// Global variables for controlling log levels and conditions in the decoder.
// Macro to create log variable names with prefixes
#define LOG_VAR(level) g_bLog##level##_VpuC7
// Log level variables for each prefix
extern int LOG_VAR(Verbose);
extern int LOG_VAR(Debug);
extern int LOG_VAR(Info);
extern int LOG_VAR(Warn);
extern int LOG_VAR(Error);
extern int LOG_VAR(Assert);
extern int LOG_VAR(Func);
extern int LOG_VAR(Trace);
extern int LOG_VAR(DecodeSuccess);

#define CLOGV(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RESET   PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGD(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_YELLOW  PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGI(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_GREEN   PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGW(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGE(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RED     PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGA(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_CYAN    PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGF(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)
#define CLOGT(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_BLUE    PRT_PREFIX "[%s %d] " fmt COL_RESET , __func__, __LINE__, ##__VA_ARGS__)

#define DLOGV(fmt, ...)    if (LOG_VAR(Verbose) == 1) { CLOGV(fmt, ##__VA_ARGS__); }
#define DLOGD(fmt, ...)    if (LOG_VAR(Debug)   == 1) { CLOGD(fmt, ##__VA_ARGS__); }
#define DLOGI(fmt, ...)    if (LOG_VAR(Info)    == 1) { CLOGI(fmt, ##__VA_ARGS__); }
#define DLOGW(fmt, ...)    if (LOG_VAR(Warn)    == 1) { CLOGW(fmt, ##__VA_ARGS__); }
#define DLOGE(fmt, ...)    if (LOG_VAR(Error)   == 1) { CLOGE(fmt, ##__VA_ARGS__); }
#define DLOGA(fmt, ...)    if (LOG_VAR(Assert)  == 1) { CLOGA(fmt, ##__VA_ARGS__); }
#define DLOGF(fmt, ...)    if (LOG_VAR(Func)    == 1) { CLOGF(fmt, ##__VA_ARGS__); }
#define DLOGT(fmt, ...)    if (LOG_VAR(Trace)   == 1) { CLOGT(fmt, ##__VA_ARGS__); }

#define DLOG_DEC_SUCCESS(fmt, ...)    if (LOG_VAR(DecodeSuccess) == 1) { CLOGV(fmt, ##__VA_ARGS__); }

#define DLOGF_BEGIN DLOGF("==== Begin ====")
#define DLOGF_END   DLOGF("====  End  ====")

#endif // VPU_C7_LOG__H
