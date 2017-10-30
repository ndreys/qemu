#ifndef IMX_LCDIF_H
#define IMX_LCDIF_H

#include "hw/sysbus.h"

typedef struct IMXLCDState {
    /*< private >*/
    SysBusDevice parent_obj;

    MemoryRegion iomem;
} IMXLCDState;

#define TYPE_IMX_LCDIF "imx:lcdif"
#define IMX_LCDIF(obj) OBJECT_CHECK(IMXLCDState, (obj), TYPE_IMX_LCDIF)

#endif /* IMX_LCDIF_H */
