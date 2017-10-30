/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX LCD block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/display/imx_lcdif.h"
#include "qemu/log.h"

static uint64_t imx_lcdif_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    return 0;
}

static void imx_lcdif_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
}

static const struct MemoryRegionOps imx_lcdif_ops = {
    .read       = imx_lcdif_read,
    .write      = imx_lcdif_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned       = false,
    },
};

static void imx_lcdif_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMXLCDState *s = IMX_LCDIF(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx_lcdif_ops,
                          s,
                          TYPE_IMX_LCDIF ".iomem",
                          0x10000);
    sysbus_init_mmio(sd, &s->iomem);
}

static void imx_lcdif_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc  = "i.MX LCD Controller";
}

static const TypeInfo imx_lcdif_info = {
    .name          = TYPE_IMX_LCDIF,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMXLCDState),
    .instance_init = imx_lcdif_init,
    .class_init    = imx_lcdif_class_init,
};

static void imx_lcdif_register_type(void)
{
    type_register_static(&imx_lcdif_info);
}
type_init(imx_lcdif_register_type)
