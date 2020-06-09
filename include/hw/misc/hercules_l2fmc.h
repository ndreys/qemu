/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_L2FMC_H
#define HERCULES_L2FMC_H

typedef struct HerculesL2FMCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion epc;

    uint32_t fdiagctrl;
    uint32_t fraw_addr;
    uint32_t fprim_add_tag;
    uint32_t fdup_add_tag;
    uint32_t fedac_pastatus;
    uint32_t fedac_pbstatus;
    uint32_t femu_ecc;

    uint32_t camavailstat;
    uint32_t cam_index[7];
    uint32_t cam_content[32];

    uint32_t ecc_1bit_address;
    uint32_t ecc_1bit_femu_ecc;

    qemu_irq uncorrectable_error;
    qemu_irq bus_error;
    qemu_irq correctable_error;
} HerculesL2FMCState;

#define TYPE_HERCULES_L2FMC "ti-hercules-l2fmc"
#define HERCULES_L2FMC(obj) OBJECT_CHECK(HerculesL2FMCState, (obj), \
                                         TYPE_HERCULES_L2FMC)
#endif
