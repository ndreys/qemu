/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_MIBADC_H
#define HERCULES_MIBADC_H

#include "hw/sysbus.h"

typedef struct HerculesMibAdcGroup {
    uint32_t sr;
    uint32_t sel;
    uint32_t intflg;

    uint8_t start;
    uint8_t end;
    uint8_t wridx;
    uint8_t rdidx;
} HerculesMibAdcGroup;

typedef struct HerculesMibAdcState {
    SysBusDevice parent_obj;

    MemoryRegion regs;
    struct {
        MemoryRegion container;
        MemoryRegion ram;
        MemoryRegion ecc;
    } io;

    uint32_t adopmodecr;
    uint32_t adparcr;
    uint32_t adparaddr;
    HerculesMibAdcGroup adg[3];

    uint32_t results[64];
    uint32_t ecc[64];
    uint16_t channel[32];

    qemu_irq parity_error;
} HerculesMibAdcState;

#define TYPE_HERCULES_MIBADC "ti-hercules-mibadc"
#define HERCULES_MIBADC(obj) OBJECT_CHECK(HerculesMibAdcState, (obj), \
                                          TYPE_HERCULES_MIBADC)

#endif
