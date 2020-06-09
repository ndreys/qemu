/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_PBIST_H
#define HERCULES_PBIST_H

typedef struct HerculesPBISTState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t pact;
    uint32_t fsrf[2];
    uint32_t over;
    uint32_t rom;
    uint32_t algo;
    uint32_t rinfol;
    uint32_t rinfou;

    qemu_irq mstdone;
} HerculesPBISTState;

#define TYPE_HERCULES_PBIST "ti-hercules-pbist"
#define HERCULES_PBIST(obj) OBJECT_CHECK(HerculesPBISTState, (obj), \
                                       TYPE_HERCULES_PBIST)
#endif
