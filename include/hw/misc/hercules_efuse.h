/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_EFUSE_H
#define HERCULES_EFUSE_H

typedef struct HerculesEFuseState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t efcpins;
    uint32_t efcstcy;
    uint32_t efcstsig;

    qemu_irq autoload_error;
    qemu_irq self_test_error;
    qemu_irq single_bit_error;
} HerculesEFuseState;

#define TYPE_HERCULES_EFUSE "ti-hercules-efuse"
#define HERCULES_EFUSE(obj) OBJECT_CHECK(HerculesEFuseState, (obj), \
                                       TYPE_HERCULES_EFUSE)
#endif
