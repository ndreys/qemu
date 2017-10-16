/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 CCM, PMU and ANALOG IP blocks emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef IMX7_CCM_H
#define IMX7_CCM_H

#include "hw/misc/imx_ccm.h"
#include "qemu/bitops.h"

enum IMX7AnalogRegisters {
    CCM_ANALOG_PLL_ARM,
    CCM_ANALOG_PLL_ARM_SET,
    CCM_ANALOG_PLL_ARM_CLR,
    CCM_ANALOG_PLL_ARM_TOG,
    CCM_ANALOG_PLL_DDR,
    CCM_ANALOG_PLL_DDR_SET,
    CCM_ANALOG_PLL_DDR_CLR,
    CCM_ANALOG_PLL_DDR_TOG,
    CCM_ANALOG_PLL_DDR_SS,
    CCM_ANALOG_PLL_DDR_SS_SET,
    CCM_ANALOG_PLL_DDR_SS_CLR,
    CCM_ANALOG_PLL_DDR_SS_TOG,
    CCM_ANALOG_PLL_DDR_NUM,
    CCM_ANALOG_PLL_DDR_NUM_SET,
    CCM_ANALOG_PLL_DDR_NUM_CLR,
    CCM_ANALOG_PLL_DDR_NUM_TOG,
    CCM_ANALOG_PLL_DDR_DENOM,
    CCM_ANALOG_PLL_DDR_DENOM_SET,
    CCM_ANALOG_PLL_DDR_DENOM_CLR,
    CCM_ANALOG_PLL_DDR_DENOM_TOG,
    CCM_ANALOG_PLL_480,
    CCM_ANALOG_PLL_480_SET,
    CCM_ANALOG_PLL_480_CLR,
    CCM_ANALOG_PLL_480_TOG,
    CCM_ANALOG_PLL_480A,
    CCM_ANALOG_PLL_480A_SET,
    CCM_ANALOG_PLL_480A_CLR,
    CCM_ANALOG_PLL_480A_TOG,
    CCM_ANALOG_PLL_480B,
    CCM_ANALOG_PLL_480B_SET,
    CCM_ANALOG_PLL_480B_CLR,
    CCM_ANALOG_PLL_480B_TOG,
    CCM_ANALOG_PLL_ENET,
    CCM_ANALOG_PLL_ENET_SET,
    CCM_ANALOG_PLL_ENET_CLR,
    CCM_ANALOG_PLL_ENET_TOG,
    CCM_ANALOG_PLL_AUDIO,
    CCM_ANALOG_PLL_AUDIO_SET,
    CCM_ANALOG_PLL_AUDIO_CLR,
    CCM_ANALOG_PLL_AUDIO_TOG,
    CCM_ANALOG_PLL_AUDIO_SS,
    CCM_ANALOG_PLL_AUDIO_SS_SET,
    CCM_ANALOG_PLL_AUDIO_SS_CLR,
    CCM_ANALOG_PLL_AUDIO_SS_TOG,
    CCM_ANALOG_PLL_AUDIO_NUM,
    CCM_ANALOG_PLL_AUDIO_NUM_SET,
    CCM_ANALOG_PLL_AUDIO_NUM_CLR,
    CCM_ANALOG_PLL_AUDIO_NUM_TOG,
    CCM_ANALOG_PLL_AUDIO_DENOM,
    CCM_ANALOG_PLL_AUDIO_DENOM_SET,
    CCM_ANALOG_PLL_AUDIO_DENOM_CLR,
    CCM_ANALOG_PLL_AUDIO_DENOM_TOG,
    CCM_ANALOG_PLL_VIDEO,
    CCM_ANALOG_PLL_VIDEO_SET,
    CCM_ANALOG_PLL_VIDEO_CLR,
    CCM_ANALOG_PLL_VIDEO_TOG,
    CCM_ANALOG_PLL_VIDEO_SS,
    CCM_ANALOG_PLL_VIDEO_SS_SET,
    CCM_ANALOG_PLL_VIDEO_SS_CLR,
    CCM_ANALOG_PLL_VIDEO_SS_TOG,
    CCM_ANALOG_PLL_VIDEO_NUM,
    CCM_ANALOG_PLL_VIDEO_NUM_SET,
    CCM_ANALOG_PLL_VIDEO_NUM_CLR,
    CCM_ANALOG_PLL_VIDEO_NUM_TOG,
    CCM_ANALOG_PLL_VIDEO_DENOM,
    CCM_ANALOG_PLL_VIDEO_DENOM_SET,
    CCM_ANALOG_PLL_VIDEO_DENOM_CLR,
    CCM_ANALOG_PLL_VIDEO_DENOM_TOG,
    CCM_ANALOG_PLL_MISC0,
    CCM_ANALOG_PLL_MISC0_SET,
    CCM_ANALOG_PLL_MISC0_CLR,
    CCM_ANALOG_PLL_MISC0_TOG,

    CCM_ANALOG_DIGPROG = 0x800 / sizeof(uint32_t),
    CCM_ANALOG_MAX,

    CCM_ANALOG_PLL_LOCK = BIT(31)
};

enum IMX7CCMRegisters {
    CCM_MAX = 0xBE00 / sizeof(uint32_t) + 1,
};

enum IMX7PMURegisters {
    PMU_MAX = 0x140 / sizeof(uint32_t),
};

#undef REG_SET_CLR_TOG

#define TYPE_IMX7_CCM "imx7.ccm"
#define IMX7_CCM(obj) OBJECT_CHECK(IMX7CCMState, (obj), TYPE_IMX7_CCM)

typedef struct IMX7CCMState {
    /* <private> */
    IMXCCMState parent_obj;

    /* <public> */
    struct {
        MemoryRegion container;
        MemoryRegion ccm;
        MemoryRegion pmu;
        MemoryRegion analog;
        MemoryRegion digprog;
    } mmio;

    uint32_t ccm[CCM_MAX];
    uint32_t pmu[PMU_MAX];
    uint32_t analog[CCM_ANALOG_MAX];

} IMX7CCMState;

#endif /* IMX7_CCM_H */