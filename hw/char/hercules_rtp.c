/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/char/hercules_rtp.h"
#include "chardev/char-fe.h"
#include "qemu/log.h"

#define RTPDDMW     0x2c

static uint64_t hercules_rtp_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void hercules_rtp_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    HerculesRTPState *s = HERCULES_RTP(opaque);
    uint8_t ch = value;

    switch (offset) {
    case RTPDDMW:
        qemu_chr_fe_write_all(&s->chr, &ch, sizeof(ch));
        break;
    }
}

static const MemoryRegionOps hercules_rtp_ops = {
    .read = hercules_rtp_read,
    .write = hercules_rtp_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void hercules_rtp_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    HerculesRTPState *s = HERCULES_RTP(obj);

    memory_region_init_io(&s->iomem, obj, &hercules_rtp_ops, s,
                          TYPE_HERCULES_RTP ".io", HERCULES_RTP_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void hercules_rtp_realize(DeviceState *dev, Error **errp)
{
    HerculesRTPState *s = HERCULES_RTP(dev);

    qemu_chr_fe_set_handlers(&s->chr, NULL, NULL, NULL, NULL, s, NULL, true);
}

static Property hercules_rtp_properties[] = {
    DEFINE_PROP_CHR("chardev", HerculesRTPState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void hercules_rtp_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = hercules_rtp_realize;
    device_class_set_props(dc, hercules_rtp_properties);
}

static const TypeInfo hercules_rtp_info = {
    .name          = TYPE_HERCULES_RTP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesRTPState),
    .instance_init = hercules_rtp_init,
    .class_init    = hercules_rtp_class_init,
};

static void hercules_rtp_register_types(void)
{
    type_register_static(&hercules_rtp_info);
}

type_init(hercules_rtp_register_types)
