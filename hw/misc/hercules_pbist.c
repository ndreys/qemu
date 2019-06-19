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

#include "hw/misc/hercules_pbist.h"

enum {
    HERCULES_PBIST_SIZE         = 512,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesPBISTRegisters {
    RAMT   = 0x160,
    DLR    = 0x164,
    DLR2   = BIT(2),
    STC    = 0x16C,
    PACT   = 0x180,
    FSRF0  = 0x190,
    FSRF1  = 0x194,
    FSRFx  = BIT(0),
    OVER   = 0x188,
    ROM    = 0x1C0,
    ALGO   = 0x1C4,
    RINFOL = 0x1C8,
    RINFOU = 0x1CC,
};

static uint64_t hercules_pbist_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesPBISTState *s = opaque;

    switch (offset) {
    case PACT:
        return s->pact;
    case FSRF0:
        return s->fsrf[0];
    case FSRF1:
        return s->fsrf[1];
    case OVER:
        return s->over;
    case ROM:
        return s->rom;
    case ALGO:
        return s->algo;
    case RINFOL:
        return s->rinfol;
    case RINFOU:
        return s->rinfou;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_pbist_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesPBISTState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
        /*
         * Magic undocumented registers used by PBIST code
         */
    case 0x00 ... 0x18:
    case 0x40 ... 0x58:
        break;
    case RAMT:
    case STC:
        break;
    case PACT:
        s->pact = val;
        break;
    case DLR:
        /*
         * Not how HW works, but good enough to get things running
         */
        if (val & DLR2) {
            s->fsrf[0] = 0;
            s->fsrf[1] = 0;
        } else {
            s->fsrf[0] = FSRFx;
            s->fsrf[1] = FSRFx;
        }
        qemu_irq_raise(s->mstdone);
        break;
    case OVER:
        s->over = val;
        break;
    case ROM:
        s->rom = val;
        break;
    case ALGO:
        s->algo = val;
        break;
    case RINFOL:
        s->rinfol = val;
        break;
    case RINFOU:
        s->rinfou = val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_pbist_ops = {
    .read = hercules_pbist_read,
    .write = hercules_pbist_write,
    .endianness = DEVICE_BIG_ENDIAN,
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

static void hercules_pbist_realize(DeviceState *dev, Error **errp)
{
    HerculesPBISTState *s = HERCULES_PBIST(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_pbist_ops,
                          s, TYPE_HERCULES_PBIST ".io", HERCULES_PBIST_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->mstdone);
}

static void hercules_pbist_reset(DeviceState *d)
{
    HerculesPBISTState *s = HERCULES_PBIST(d);

    s->pact = 0;
}

static void hercules_pbist_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_pbist_reset;
    dc->realize = hercules_pbist_realize;
}

static const TypeInfo hercules_pbist_info = {
    .name          = TYPE_HERCULES_PBIST,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesPBISTState),
    .class_init    = hercules_pbist_class_init,
};

static void hercules_pbist_register_types(void)
{
    type_register_static(&hercules_pbist_info);
}

type_init(hercules_pbist_register_types)
