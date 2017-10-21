/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 SRC block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/imx7_src.h"
#include "qemu/log.h"

static void imx7_src_reset(DeviceState *dev)
{
    IMX7SRCState *s = IMX7_SRC(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx7_src_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMX7SRCState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx7_src_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMX7SRCState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx7_src_ops = {
    .read       = imx7_src_read,
    .write      = imx7_src_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned       = false,
    },
};

static void imx7_src_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX7SRCState *s = IMX7_SRC(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx7_src_ops,
                          s,
                          TYPE_IMX7_SRC ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx7_src = {
    .name = TYPE_IMX7_SRC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMX7SRCState, SRC_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx7_src_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx7_src_reset;
    dc->vmsd  = &vmstate_imx7_src;
    dc->desc  = "i.MX7 System Reset Controller";
}

static const TypeInfo imx7_src_info = {
    .name          = TYPE_IMX7_SRC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX7SRCState),
    .instance_init = imx7_src_init,
    .class_init    = imx7_src_class_init,
};

static void imx7_src_register_type(void)
{
    type_register_static(&imx7_src_info);
}
type_init(imx7_src_register_type)
