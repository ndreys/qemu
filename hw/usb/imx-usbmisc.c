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
#include "hw/usb/imx-usbmisc.h"
#include "qemu/log.h"

static void imx_usbmisc_reset(DeviceState *dev)
{
    IMXUSBMiscState *s = IMX_USBMISC(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx_usbmisc_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMXUSBMiscState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx_usbmisc_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMXUSBMiscState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx_usbmisc_ops = {
    .read = imx_usbmisc_read,
    .write = imx_usbmisc_write,
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

static void imx_usbmisc_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMXUSBMiscState *s = IMX_USBMISC(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx_usbmisc_ops,
                          s,
                          TYPE_IMX_USBMISC ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx_usbmisc = {
    .name = TYPE_IMX_USBMISC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMXUSBMiscState, USBMISC_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_usbmisc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx_usbmisc_reset;
    dc->vmsd  = &vmstate_imx_usbmisc;
    dc->desc  = "i.MX IOMUXC Module";
}

static const TypeInfo imx_usbmisc_info = {
    .name          = TYPE_IMX_USBMISC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMXUSBMiscState),
    .instance_init = imx_usbmisc_init,
    .class_init    = imx_usbmisc_class_init,
};

static void imx_usbmisc_register_type(void)
{
    type_register_static(&imx_usbmisc_info);
}
type_init(imx_usbmisc_register_type)
