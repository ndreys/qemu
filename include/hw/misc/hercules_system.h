/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_SYSTEM_H
#define HERCULES_SYSTEM_H

#include "hw/misc/unimp.h"

enum {
    HERCULES_SYSTEM_SYS_SIZE  = 256,
    HERCULES_SYSTEM_SYS2_SIZE = 256,
    HERCULES_SYSTEM_PCR_SIZE  = 2 * 1024,
    HERCULES_SYSTEM_NUM_PCRS  = 3,
};

enum HerculesSystemSignals {
    HERCULES_SYSTEM_ICRST,
    HERCULES_SYSTEM_CPURST,
    HERCULES_SYSTEM_MSTDONE,
    HERCULES_SYSTEM_NUM_SIGNALS,
};

typedef struct HerculesSystemState {
    SysBusDevice parent_obj;

    uint32_t csdis;
    uint32_t minitgcr;
    uint32_t msinena;
    uint32_t ministat;
    uint32_t sysesr;
    uint32_t mstcgstat;
    uint32_t mstgcr;

    uint32_t ghvsrc;
    uint32_t glbstat;
    uint32_t pllctl1;
    uint32_t pllctl3;

    MemoryRegion sys;
    MemoryRegion sys2;
    UnimplementedDeviceState pcr[HERCULES_SYSTEM_NUM_PCRS];

    qemu_irq irq;
    qemu_irq pll1_slip_error;
    qemu_irq pll2_slip_error;
} HerculesSystemState;

#define TYPE_HERCULES_SYSTEM "ti-hercules-system"
#define HERCULES_SYSTEM(obj) OBJECT_CHECK(HerculesSystemState, (obj), \
                                          TYPE_HERCULES_SYSTEM)
#endif
