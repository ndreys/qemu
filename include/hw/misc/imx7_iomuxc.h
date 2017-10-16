#ifndef IMX7_IOMUXC_H
#define IMX7_IOMUXC_H

#include "hw/sysbus.h"

enum IMX7IOMUXCRegisters {
    IOMUXC_NUM = 0x740 / sizeof(uint32_t),
};

typedef struct IMX7IOMUXCState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[IOMUXC_NUM];
} IMX7IOMUXCState;

#define TYPE_IMX7_IOMUXC "imx7-iomuxc"
#define IMX7_IOMUXC(obj) OBJECT_CHECK(IMX7IOMUXCState, (obj), TYPE_IMX7_IOMUXC)

#endif /* IMX7_IOMUXC_H */
