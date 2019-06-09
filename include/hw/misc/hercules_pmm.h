/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_PMM_H
#define HERCULES_PMM_H

typedef struct HerculesPMMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t prckeyreg;
    uint32_t lpddcstat1;
    uint32_t lpddcstat2;

    qemu_irq compare_error;
    qemu_irq self_test_error;
} HerculesPMMState;

#define TYPE_HERCULES_PMM "ti-hercules-pmm"
#define HERCULES_PMM(obj) OBJECT_CHECK(HerculesPMMState, (obj), \
                                       TYPE_HERCULES_PMM)
#endif
