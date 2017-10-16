/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX FlexCAN block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/imx_flexcan.h"
#include "qemu/log.h"

static void imx_flexcan_reset(DeviceState *dev)
{
    IMXFlexCANState *s = IMX_FLEXCAN(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx_flexcan_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMXFlexCANState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx_flexcan_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMXFlexCANState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx_flexcan_ops = {
    .read = imx_flexcan_read,
    .write = imx_flexcan_write,
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

static void imx_flexcan_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMXFlexCANState *s = IMX_FLEXCAN(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx_flexcan_ops,
                          s,
                          TYPE_IMX_FLEXCAN ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx_flexcan = {
    .name = TYPE_IMX_FLEXCAN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMXFlexCANState, FLEXCAN_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_flexcan_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx_flexcan_reset;
    dc->vmsd  = &vmstate_imx_flexcan;
    dc->desc  = "i.MX FlexCAN Module";
}

static const TypeInfo imx_flexcan_info = {
    .name          = TYPE_IMX_FLEXCAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMXFlexCANState),
    .instance_init = imx_flexcan_init,
    .class_init    = imx_flexcan_class_init,
};

static void imx_flexcan_register_type(void)
{
    type_register_static(&imx_flexcan_info);
}
type_init(imx_flexcan_register_type)
