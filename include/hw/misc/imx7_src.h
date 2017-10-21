#ifndef IMX7_SRC_H
#define IMX7_SRC_H

#include "hw/sysbus.h"

enum IMX7SRCRegisters {
    SRC_NUM = 0x1000 / sizeof(uint32_t) + 1,
};

typedef struct IMX7SRCState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[SRC_NUM];
} IMX7SRCState;

#define TYPE_IMX7_SRC "imx7-src"
#define IMX7_SRC(obj) OBJECT_CHECK(IMX7SRCState, (obj), TYPE_IMX7_SRC)

#endif /* IMX7_SRC_H */
