#ifndef IMX_USBMISC_H
#define IMX_USBMISC_H

#include "hw/sysbus.h"

enum IMXUSBMiscRegisters {
    USBMISC_NUM = 0x24 / sizeof(uint32_t) + 1,
};

typedef struct IMXUSBMiscState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[USBMISC_NUM];
} IMXUSBMiscState;

#define TYPE_IMX_USBMISC "imx-usbmisc"
#define IMX_USBMISC(obj) OBJECT_CHECK(IMXUSBMiscState, (obj), TYPE_IMX_USBMISC)

#endif /* IMX_USBMISC_H */
