/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_L2RAMW_H
#define HERCULES_L2RAMW_H

#include "hw/sysbus.h"

typedef struct HerculesL2RamwState {
    SysBusDevice parent_obj;

    struct {
        MemoryRegion ecc;
        MemoryRegion sram;
        MemoryRegion container;

        MemoryRegion regs;
    } io;

    uint32_t ramctrl;
    uint32_t ramtest;
    uint32_t ramerrstatus;
    uint32_t diag_ecc;

    qemu_irq uncorrectable_error;
} HerculesL2RamwState;

#define TYPE_HERCULES_L2RAMW "ti-hercules-l2ramw"
#define HERCULES_L2RAMW(obj) OBJECT_CHECK(HerculesL2RamwState, (obj), \
                                          TYPE_HERCULES_L2RAMW)
#endif
