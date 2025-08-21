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

#ifndef WAVE5_VPU_CONFIGURATION_H
#define WAVE5_VPU_CONFIGURATION_H

#include "wave5config.h"

#define WAVE4_NUM_BPU_CORE      1
#define WAVE4_COMMAND_TIMEOUT   0xffff  /* MAX: 0xffff, 1 == 32768 ticks, 1 sec. == vCPU clocks */

#define COMMAND_TIMEOUT         20      /* decoding timout in second */
#define VCPU_CLOCK_IN_MHZ       16      /* Vcpu clock in MHz */

#endif