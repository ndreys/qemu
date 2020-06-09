/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_VIM_H
#define HERCULES_VIM_H

#include "hw/misc/unimp.h"

enum HerculesVimInterruprs {
    HERCULES_ESM_HIGH_LEVEL_IRQ = 0,
    HERCULES_RTI_COMPARE0_IRQ = 2,
    HERCULES_RTI_COMPARE1_IRQ = 3,
    HERCULES_RTI_COMPARE2_IRQ = 4,
    HERCULES_RTI_COMPARE3_IRQ = 5,
    HERCULES_RTI_OVERFLOW0_IRQ = 6,
    HERCULES_RTI_OVERFLOW1_IRQ = 7,
    HERCULES_RTI_TIME_BASE_IRQ = 8,

    HERCULES_ESM_LOW_LEVEL_IRQ = 20,
    HERCULES_SSI_IRQ = 21,
    HERCULES_MIBSPI1_L0_IRQ = 12,
    HERCULES_MIBSPI1_L1_IRQ = 26,
    HERCULES_MIBSPI2_L0_IRQ = 17,
    HERCULES_MIBSPI2_L1_IRQ = 30,
    HERCULES_MIBSPI3_L0_IRQ = 37,
    HERCULES_MIBSPI3_L1_IRQ = 38,
    HERCULES_MIBSPI4_L0_IRQ = 49,
    HERCULES_MIBSPI4_L1_IRQ = 54,
    HERCULES_MIBSPI5_L0_IRQ = 53,
    HERCULES_MIBSPI5_L1_IRQ = 56,

    HERCULES_NUM_IRQ = 128,

    HERCULES_IRQ_GROUP_WIDTH = BITS_PER_LONG,
    HERCULES_NUM_IRQ_GROUP = HERCULES_NUM_IRQ / BITS_PER_LONG,
};

#define HERCULES_VIM_BITFIELD(_name)                    \
    union {                                             \
        unsigned long _name[HERCULES_NUM_IRQ_GROUP];    \
        struct QEMU_PACKED {                            \
            uint32_t _name##0;                          \
            uint32_t _name##1;                          \
            uint32_t _name##2;                          \
            uint32_t _name##3;                          \
        };                                              \
    }

typedef struct HerculesVimState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion ram;
    UnimplementedDeviceState ecc;
    uint32_t vectors[HERCULES_NUM_IRQ];

    HERCULES_VIM_BITFIELD(intreq);
    HERCULES_VIM_BITFIELD(reqena);
    HERCULES_VIM_BITFIELD(firqpr);
    /*
     * firqpr inverted
     */
    HERCULES_VIM_BITFIELD(rpqrif);

    uint8_t chanctrl[HERCULES_NUM_IRQ];

    qemu_irq irq;
    qemu_irq fiq;
} HerculesVimState;

#define TYPE_HERCULES_VIM "ti-hercules-vim"
#define HERCULES_VIM(obj) OBJECT_CHECK(HerculesVimState, (obj), \
                                       TYPE_HERCULES_VIM)
#endif
