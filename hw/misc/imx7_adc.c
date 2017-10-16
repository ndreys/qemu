/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 ADC block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/imx7_adc.h"
#include "qemu/log.h"

static void imx7_adc_reset(DeviceState *dev)
{
    IMX7ADCState *s = IMX7_ADC(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx7_adc_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMX7ADCState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx7_adc_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMX7ADCState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx7_adc_ops = {
    .read = imx7_adc_read,
    .write = imx7_adc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void imx7_adc_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX7ADCState *s = IMX7_ADC(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx7_adc_ops,
                          s,
                          TYPE_IMX7_ADC ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx7_adc = {
    .name = TYPE_IMX7_ADC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMX7ADCState, ADC_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx7_adc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx7_adc_reset;
    dc->vmsd  = &vmstate_imx7_adc;
    dc->desc  = "i.MX ADC Module";
}

static const TypeInfo imx7_adc_info = {
    .name          = TYPE_IMX7_ADC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX7ADCState),
    .instance_init = imx7_adc_init,
    .class_init    = imx7_adc_class_init,
};

static void imx7_adc_register_type(void)
{
    type_register_static(&imx7_adc_info);
}
type_init(imx7_adc_register_type)
