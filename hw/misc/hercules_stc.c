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
#include "qemu/main-loop.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"

#include "hw/misc/hercules_stc.h"

enum {
    HERCULES_STC_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesSTCRegisters {
    STCGCR0   = 0x0000,
    STCGCR1   = 0x0004,
    STCTPR    = 0x0008,
    STCGSTAT  = 0x0014,
    TEST_DONE = BIT(0),
    TEST_FAIL = BIT(1),
    STCFSTAT  = 0x0018,
    STCSCSCR  = 0x003C,
    FAULT_INS = BIT(4),
    STCCLKDIV = 0x0044,
};

#define STC_ENA(w)            extract32(w, 0, 4)

static uint64_t hercules_stc_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesSTCState *s = opaque;

    switch (offset) {
    case STCGCR0:
        return s->stcgcr[0];
    case STCGCR1:
        return s->stcgcr[1];
    case STCTPR:
        return s->stctpr;
    case STCFSTAT:
        break;
    case STCGSTAT:
        return s->stcgstat;
    case STCSCSCR:
        return s->stcscscr;
    case STCCLKDIV:
        return s->stcclkdiv;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_stc_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesSTCState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case STCGCR0:
        s->stcgcr[0] = val;
        break;
    case STCGCR1:
        s->stcgcr[1] = val;

        if (STC_ENA(val) == 0xA) {
            qemu_bh_schedule(s->self_test);
        }
        break;
    case STCTPR:
        s->stctpr = val;
        break;
    case STCGSTAT:
        s->stcgstat &= ~val;
        break;
    case STCFSTAT:
        break;
    case STCSCSCR:
        s->stcscscr = val;
        break;
    case STCCLKDIV:
        s->stcclkdiv = val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_stc_ops = {
    .read = hercules_stc_read,
    .write = hercules_stc_write,
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

static void hercules_stc_self_test(void *opaque)
{
    HerculesSTCState *s = opaque;
    CPUState *cpu = qemu_get_cpu(0);

    if (cpu->halted) {
        s->stcgstat |= TEST_DONE;
        if (s->stcscscr & FAULT_INS) {
            s->stcgstat |= TEST_FAIL;
        }

        qemu_irq_raise(s->cpurst);
        return;
    }

    qemu_bh_schedule(s->self_test);
}

static void hercules_stc_realize(DeviceState *dev, Error **errp)
{
    HerculesSTCState *s = HERCULES_STC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_stc_ops,
                          s, TYPE_HERCULES_STC ".io", HERCULES_STC_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    s->self_test = qemu_bh_new(hercules_stc_self_test, s);
    sysbus_init_irq(sbd, &s->cpurst);
}

static void hercules_stc_reset(DeviceState *d)
{
    HerculesSTCState *s = HERCULES_STC(d);
    (void)s;
}

static void hercules_stc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_stc_reset;
    dc->realize = hercules_stc_realize;
}

static const TypeInfo hercules_stc_info = {
    .name          = TYPE_HERCULES_STC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesSTCState),
    .class_init    = hercules_stc_class_init,
};

static void hercules_stc_register_types(void)
{
    type_register_static(&hercules_stc_info);
}

type_init(hercules_stc_register_types)
