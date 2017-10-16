#ifndef IMX_FLEXCAN_H
#define IMX_FLEXCAN_H

#include "hw/sysbus.h"

enum IMXFlexCANRegisters {
    FLEXCAN_NUM = 0x9E0 / sizeof(uint32_t) + 1,
};

typedef struct IMXFlexCANState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[FLEXCAN_NUM];
} IMXFlexCANState;

#define TYPE_IMX_FLEXCAN "imx-flexcan"
#define IMX_FLEXCAN(obj) OBJECT_CHECK(IMXFlexCANState, (obj), TYPE_IMX_FLEXCAN)

#endif /* IMX_FLEXCAN_H */
