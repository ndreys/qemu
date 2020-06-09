/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_RTI_H
#define HERCULES_RTI_H

#include "hw/ptimer.h"

enum HerculesRtiInterruptGroup {
    HERCULES_RTI_INT_GROUP_COMPARE = 0,
    HERCULES_RTI_INT_GROUP_DMA,
    HERCULES_RTI_INT_GROUP_RESERVED,
    HERCULES_RTI_INT_GROUP_TBOVL,
    HERCULES_RTI_INT_GROUP_NUM,
    HERCULES_RTI_INT_PER_GROUP = 4,
};

enum HerculesRtiInterruptLine {
    HERCULES_RTI_INT_LINE_COMPARE0 = 0,
    HERCULES_RTI_INT_LINE_COMPARE1,
    HERCULES_RTI_INT_LINE_COMPARE2,
    HERCULES_RTI_INT_LINE_COMPARE3,
    HERCULES_RTI_INT_LINE_COMPARE_NUM,

    HERCULES_RTI_INT_LINE_DMA0     = 0,
    HERCULES_RTI_INT_LINE_DMA1,
    HERCULES_RTI_INT_LINE_DMA2,
    HERCULES_RTI_INT_LINE_DMA3,
    HERCULES_RTI_INT_LINE_DMA_NUM,

    HERCULES_RTI_INT_LINE_TB       = 0,
    HERCULES_RTI_INT_LINE_OVL0,
    HERCULES_RTI_INT_LINE_OVL1,
    HERCULES_RTI_INT_LINE_TBOVL_NUM,
};

enum HerculesRtiInterrupt {
    HERCULES_RTI_INT_COMPARE0 = 0,
    HERCULES_RTI_INT_COMPARE1,
    HERCULES_RTI_INT_COMPARE2,
    HERCULES_RTI_INT_COMPARE3,
    HERCULES_RTI_INT_DMA0,
    HERCULES_RTI_INT_DMA1,
    HERCULES_RTI_INT_DMA2,
    HERCULES_RTI_INT_DMA3,
    HERCULES_RTI_INT_TIMEBASE,
    HERCULES_RTI_INT_OVERFLOW0,
    HERCULES_RTI_INT_OVERFLOW1,
    HERCULES_RTI_INT_NUM
};

enum {
    HERCULES_RTI_FRC_NUM = 2,
    HERCULES_RTI_SIZE    = 256,
};

struct HerculesRtiState;
typedef struct HerculesRtiState HerculesRtiState;

typedef struct HerculesRtiFrc {
    HerculesRtiState *rti;
    uint32_t counter;
    uint32_t cpuc;
    uint32_t period;
    int64_t  timestamp;
    uint32_t gctrl_en;
    bool enabled;
} HerculesRtiFrc;

typedef struct HerculesRtiCompareModule {
    HerculesRtiState *rti;
    HerculesRtiFrc *frc;
    QEMUTimer *timer;
    uint32_t comp;
    uint32_t udcp;
    uint32_t mask;
    int64_t  udcp_ns;
} HerculesRtiCompareModule;

struct HerculesRtiState {
    SysBusDevice parent_obj;

    qemu_irq *irq[HERCULES_RTI_INT_GROUP_NUM];
    MemoryRegion iomem;

    uint32_t gctrl;
    uint32_t intflag;
    uint32_t intena;
    uint32_t compctrl;

    HerculesRtiFrc frc[HERCULES_RTI_FRC_NUM];
    HerculesRtiCompareModule compare[HERCULES_RTI_INT_LINE_COMPARE_NUM];
};

#define TYPE_HERCULES_RTI "ti-hercules-rti"
#define HERCULES_RTI(obj) OBJECT_CHECK(HerculesRtiState, (obj), \
                                       TYPE_HERCULES_RTI)


void hercules_rti_counter_enable(HerculesRtiState *s, uint32_t idx,
                                 bool enable);
void hercules_rti_counter_advance(HerculesRtiState *s, uint32_t idx,
                                  uint32_t delta);
#endif
