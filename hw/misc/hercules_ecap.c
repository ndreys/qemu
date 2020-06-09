/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/core/cpu.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"

#include "hw/misc/hercules_ecap.h"
#include "hw/arm/hercules.h"

enum {
    HERCULES_ECAP_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesECAPRegisters {
    TSCTR  = 0x00,
    CTRPHS = 0x04,
    CAP1   = 0x08,
    CAP2   = 0x0C,
    CAP3   = 0x10,
    CAP4   = 0x14,

    ECCTL2 = 0x28,
    ECCTL1 = 0x2A,
    ECFLG  = 0x2C,
    ECEINT = 0x2E,
    ECFRC  = 0x30,
    ECCLR  = 0x32,
};

static uint64_t hercules_ecap_read(void *opaque, hwaddr offset,
                                   unsigned size)
{
    HerculesECAPState *s = opaque;

    switch (size) {
    case sizeof(uint16_t):
        switch (offset) {
        case ECCTL1:
        case ECCTL2:
        case ECEINT:
        case ECFRC:
        case ECCLR:
            return 0;
        case ECFLG:
            return s->ecflg;
        }
        break;
    case sizeof(uint32_t):
        switch (offset) {
        case TSCTR:
            return 0;
        case CAP1 ... CAP4:
            return s->cap[(offset - CAP1) / sizeof(uint32_t)];
        }
        break;
    }

    qemu_log_bad_offset(offset);
    return 0;
}

static void hercules_ecap_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesECAPState *s = opaque;
    const uint32_t val = val64;

    switch (size) {
    case sizeof(uint16_t):
        switch (offset) {
        case ECCTL1:
        case ECCTL2:
        case ECEINT:
        case ECFRC:
            break;
        case ECCLR:
            /*
             * Currently a no-op on purpose. Once a given capture
             * register and corresponding bit in ECFLG is set by an
             * external entity, we want it to remain set in order to
             * make guest think that we are constantly capturing a
             * waveform of given frequency
             */
            break;
        }
        break;
    case sizeof(uint32_t):
        switch (offset) {
        case TSCTR:
            break;
        case CAP1 ... CAP4:
            s->cap[(offset - CAP1) / sizeof(uint32_t)] = val;
            break;
        default:
            qemu_log_bad_offset(offset);
        }
        break;

    }
}

static void hercules_ecap_realize(DeviceState *dev, Error **errp)
{
    HerculesECAPState *s = HERCULES_ECAP(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    static MemoryRegionOps hercules_ecap_ops = {
        .read = hercules_ecap_read,
        .write = hercules_ecap_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
        .impl = {
            .min_access_size = 2,
            .max_access_size = 4,
            .unaligned = false,
        },
    };

    if (parent->is_tms570)
    {
        hercules_ecap_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->iomem, obj, &hercules_ecap_ops,
                          s, TYPE_HERCULES_ECAP ".io", HERCULES_ECAP_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void hercules_ecap_reset(DeviceState *d)
{
    HerculesECAPState *s = HERCULES_ECAP(d);

    memset(s->cap, 0, sizeof(s->cap));
    s->ecflg = 0;
}

static void hercules_ecap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_ecap_reset;
    dc->realize = hercules_ecap_realize;
}

static const TypeInfo hercules_ecap_info = {
    .name          = TYPE_HERCULES_ECAP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesECAPState),
    .class_init    = hercules_ecap_class_init,
};

static void hercules_ecap_register_types(void)
{
    type_register_static(&hercules_ecap_info);
}
type_init(hercules_ecap_register_types)
