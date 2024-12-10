/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 ARM Limited
 * Copyright (c) 2017-2018 Linaro Limited
 */

#ifndef PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H
#define PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H

#if !defined(__arm__)
#error Use this file only when compiling for ARM architecture
#endif

#include "../default/odp_atomic.h"

#ifdef __ARM_FEATURE_UNALIGNED
#define _ODP_UNALIGNED 1
#else
#define _ODP_UNALIGNED 0
#endif

#endif  /* PLATFORM_LINUXGENERIC_ARCH_ARM_ODP_CPU_H */
