/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_STC_H
#define HERCULES_STC_H

typedef struct HerculesSTCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t stcgcr[2];
    uint32_t stctpr;
    uint32_t stcscscr;
    uint32_t stcclkdiv;
    uint32_t stcgstat;

    QEMUBH *self_test;
    qemu_irq cpurst;
} HerculesSTCState;

#define TYPE_HERCULES_STC "ti-hercules-stc"
#define HERCULES_STC(obj) OBJECT_CHECK(HerculesSTCState, (obj), \
                                       TYPE_HERCULES_STC)
#endif
