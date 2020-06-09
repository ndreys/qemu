/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_GPIO_H
#define HERCULES_GPIO_H

#include "hw/misc/unimp.h"

typedef struct HerculesGpio {
    uint32_t dir;
    uint32_t din;
    uint32_t dout;
    uint32_t dset;
    uint32_t dclr;
    uint32_t pdr;
    uint32_t puldis;
    uint32_t psl;

    unsigned int bank;
} HerculesGpio;

typedef struct HerculesGioState {
    SysBusDevice parent_obj;

    /* CTRL registers */
    struct {
        MemoryRegion gioa;
        MemoryRegion giob;
        MemoryRegion regs;
        MemoryRegion container;
    } io;

    HerculesGpio gpio[2];

    uint32_t gioena;
    uint32_t giolvl;
    uint32_t gioflg;

} HerculesGioState;

enum {
    HERCULES_N2HET_REG_SIZE = 256,
    HERCULES_N2HET_RAM_SIZE = 128 * 1024,
};

typedef struct HerculesN2HetState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t hetlbpdir;
    HerculesGpio gpio;

    UnimplementedDeviceState ram;
} HerculesN2HetState;

#define TYPE_HERCULES_GIO "ti-hercules-gio"
#define HERCULES_GIO(obj) OBJECT_CHECK(HerculesGioState, (obj), \
                                       TYPE_HERCULES_GIO)

#define TYPE_HERCULES_N2HET "ti-hercules-n2het"
#define HERCULES_N2HET(obj) OBJECT_CHECK(HerculesN2HetState, (obj),     \
                                         TYPE_HERCULES_N2HET)

#endif
