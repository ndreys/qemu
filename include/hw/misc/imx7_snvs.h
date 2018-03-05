/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 SNVS block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef IMX7_SNVS_H
#define IMX7_SNVS_H

#include "qemu/bitops.h"
#include "hw/sysbus.h"


enum IMX7SNVSRegisters {
    SNVS_LPCR = 0x38,
    SNVS_LPCR_TOP   = BIT(6),
    SNVS_LPCR_DP_EN = BIT(5),

    SNVS_LPGPR0 = 0x90,
    SNVS_LPGPR1 = 0x94,
    SNVS_LPGPR2 = 0x98,
    SNVS_LPGPR3 = 0x9c,
    SNVS_LPGPR_NUM = 4,
};

#define TYPE_IMX7_SNVS "imx7.snvs"
#define IMX7_SNVS(obj) OBJECT_CHECK(IMX7SNVSState, (obj), TYPE_IMX7_SNVS)

typedef struct IMX7SNVSState {
    /* <private> */
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    BlockBackend *lpgpr;
} IMX7SNVSState;

#endif /* IMX7_SNVS_H */
