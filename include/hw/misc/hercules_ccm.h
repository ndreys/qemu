/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_CCM_H
#define HERCULES_CCM_H

#include "hercules_system.h"

typedef struct HerculesCCMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t ccmsr[3];

    qemu_irq error[3];
    qemu_irq error_self_test;
} HerculesCCMState;

#define TYPE_HERCULES_CCM "ti-hercules-ccm"
#define HERCULES_CCM(obj) OBJECT_CHECK(HerculesCCMState, (obj), \
                                       TYPE_HERCULES_CCM)
#endif
