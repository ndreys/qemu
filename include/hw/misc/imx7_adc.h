#ifndef IMX7_ADC_H
#define IMX7_ADC_H

#include "hw/sysbus.h"

enum IMX7ADCRegisters {
    ADC_NUM = 0x130 / sizeof(uint32_t) + 1,
};

typedef struct IMX7ADCState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
    uint32_t     regs[ADC_NUM];
} IMX7ADCState;

#define TYPE_IMX7_ADC "imx7-adc"
#define IMX7_ADC(obj) OBJECT_CHECK(IMX7ADCState, (obj), TYPE_IMX7_ADC)

#endif /* IMX7_ADC_H */
