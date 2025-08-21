/*
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2025 Telechips Inc.
 * Contact: shkim@telechips.com
 */

#ifndef VPU4K_D2_LOG__H
#define VPU4K_D2_LOG__H

//********************************************************************************
// Common code - This section defines the common macros and functions used for
// logging across different modules, allowing for modularity and flexibility.
//********************************************************************************

// Declare global log variables for a given module NAME.
// This macro creates external references for the variables that will be defined
// elsewhere (e.g., in a .c file).
#define DECLARE_VPU_LOG_VARS(NAME) \
	extern void (*g_pfPrintCb_##NAME)(const char *, ...); /**< printk callback */ \
	extern int g_bLogVerbose_##NAME; \
	extern int g_bLogDebug_##NAME; \
	extern int g_bLogInfo_##NAME; \
	extern int g_bLogWarn_##NAME; \
	extern int g_bLogError_##NAME; \
	extern int g_bLogAssert_##NAME; \
	extern int g_bLogFunc_##NAME; \
	extern int g_bLogTrace_##NAME; \
	extern int g_bLogDecodeSuccess_##NAME;

// Macro to log using the specific print callback for the given module NAME.
// It checks if the print callback is set before calling it.
#define DECLARE_V_PRINTF(NAME, fmt, ...) \
do { \
	if (g_pfPrintCb_##NAME != NULL) { \
		g_pfPrintCb_##NAME(fmt, ##__VA_ARGS__); \
	} \
} while (0)

// Defines a log prefix for a given module NAME.
// This is used to distinguish log messages from different modules.
#define DECLARE_PRT_PREFIX(NAME) "[" #NAME "]"

//********************************************************************************
// Declare global log variables for Vpu4K_D2
// The following section is for the specific module `Vpu4K_D2`.
// It declares global log variables and sets up the macros for logging.
//********************************************************************************
DECLARE_VPU_LOG_VARS(Vpu4K_D2)
#define V_PRINTF(fmt, ...)  DECLARE_V_PRINTF(Vpu4K_D2, fmt, ##__VA_ARGS__)
#define PRT_PREFIX          DECLARE_PRT_PREFIX(Vpu4K_D2)
// Macros for accessing log-related variables and functions
#define LOG_VAR(level)       g_bLog##level##_Vpu4K_D2  // Log level variables
#define LOG_CALLBACK(type)   g_pf##type##Cb_Vpu4K_D2   // Log callback function
#define LOG_CONDITION(name)  g_bLog##name##Success_Vpu4K_D2  // Log condition (e.g., Decode success)

//********************************************************************************
// Macro to define global log variables for a given module NAME.
// This should be used in the respective .c file to define the variables that
// are declared using the `DECLARE_VPU_LOG_VARS` macro.
//********************************************************************************
#define DEFINE_VPU_LOG_VARS(NAME)                \
		void (*g_pfPrintCb_##NAME)(const char *, ...) = NULL; /**< printk callback */ \
	int g_bLogVerbose_##NAME = 0;                 \
	int g_bLogDebug_##NAME = 0;                   \
	int g_bLogInfo_##NAME = 0;                    \
	int g_bLogWarn_##NAME = 0;                    \
	int g_bLogError_##NAME = 0;                   \
	int g_bLogAssert_##NAME = 0;                  \
	int g_bLogFunc_##NAME = 0;                    \
	int g_bLogTrace_##NAME = 0;                   \
	int g_bLogDecodeSuccess_##NAME = 0;

//********************************************************************************
// Debugging logs
//********************************************************************************
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

#define CLOGV(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RESET   PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGD(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_YELLOW  PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGI(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_GREEN   PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGW(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGE(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_RED     PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGA(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_CYAN    PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGF(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_MAGENTA PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define CLOGT(fmt, ...)    V_PRINTF(V_PRINTF_PREFIX COL_BLUE    PRT_PREFIX "[%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

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

#endif //VPU4K_D2_LOG__H
