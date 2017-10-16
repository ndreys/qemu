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
#include "hw/dma/imx_sdma.h"
#include "qemu/log.h"

static void imx_sdma_reset(DeviceState *dev)
{
    IMXSDMAState *s = IMX_SDMA(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static uint64_t imx_sdma_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMXSDMAState *s = opaque;
    return s->regs[offset / sizeof(uint32_t)];
}

static void imx_sdma_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMXSDMAState *s = opaque;
    s->regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx_sdma_ops = {
    .read = imx_sdma_read,
    .write = imx_sdma_write,
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

static void imx_sdma_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMXSDMAState *s = IMX_SDMA(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx_sdma_ops,
                          s,
                          TYPE_IMX_SDMA ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
}

static const VMStateDescription vmstate_imx_sdma = {
    .name = TYPE_IMX_SDMA,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMXSDMAState, SDMA_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_sdma_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = imx_sdma_reset;
    dc->vmsd  = &vmstate_imx_sdma;
    dc->desc  = "i.MX IOMUXC Module";
}

static const TypeInfo imx_sdma_info = {
    .name          = TYPE_IMX_SDMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMXSDMAState),
    .instance_init = imx_sdma_init,
    .class_init    = imx_sdma_class_init,
};

static void imx_sdma_register_type(void)
{
    type_register_static(&imx_sdma_info);
}
type_init(imx_sdma_register_type)
