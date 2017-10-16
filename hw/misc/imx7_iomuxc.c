/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 IOMUXC block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/imx7_iomuxc.h"
#include "qemu/log.h"

static void imx7_iomuxc_reset(DeviceState *dev)
{
    IMX7IOMUXCState *s = IMX7_IOMUXC(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx7_iomuxc_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMX7IOMUXCState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx7_iomuxc_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMX7IOMUXCState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx7_iomuxc_ops = {
    .read = imx7_iomuxc_read,
    .write = imx7_iomuxc_write,
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

static void imx7_iomuxc_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX7IOMUXCState *s = IMX7_IOMUXC(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx7_iomuxc_ops,
                          s,
                          TYPE_IMX7_IOMUXC ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx7_iomuxc = {
    .name = TYPE_IMX7_IOMUXC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMX7IOMUXCState, IOMUXC_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx7_iomuxc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx7_iomuxc_reset;
    dc->vmsd  = &vmstate_imx7_iomuxc;
    dc->desc  = "i.MX IOMUXC Module";
}

static const TypeInfo imx7_iomuxc_info = {
    .name          = TYPE_IMX7_IOMUXC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX7IOMUXCState),
    .instance_init = imx7_iomuxc_init,
    .class_init    = imx7_iomuxc_class_init,
};

static void imx7_iomuxc_register_type(void)
{
    type_register_static(&imx7_iomuxc_info);
}
type_init(imx7_iomuxc_register_type)
