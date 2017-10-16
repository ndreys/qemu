#ifndef IMX_SDMA_H
#define IMX_SDMA_H

#include "hw/sysbus.h"

enum IMXSDMARegisters {
    SDMA_NUM = 0x300 / sizeof(uint32_t) + 1,
};

typedef struct IMXSDMAState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[SDMA_NUM];
} IMXSDMAState;

#define TYPE_IMX_SDMA "imx-sdma"
#define IMX_SDMA(obj) OBJECT_CHECK(IMXSDMAState, (obj), TYPE_IMX_SDMA)

#endif /* IMX_SDMA_H */
