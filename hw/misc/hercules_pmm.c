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

#include "hw/misc/hercules_pmm.h"

enum {
    HERCULES_PMM_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesPMMRegisters {
    PRCKEYREG                    = 0xAC,
    MKEY_LOCK_STEP               = 0x0,
    MKEY_SELF_TEST               = 0x6,
    MKEY_ERROR_FORCING           = 0x9,
    MKEY_SELF_TEST_ERROR_FORCING = 0xF,
    LPDDCSTAT1                   = 0xB0,
    LPDDCSTAT2                   = 0xB4,

    LPDDCSTAT1_LCMPE_ALL = BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16),
    LPDDCSTAT1_LSTC_ALL  = BIT(4)  | BIT(3)  | BIT(2)  | BIT(1)  | BIT(0),
};

static uint64_t hercules_pmm_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesPMMState *s = opaque;

    switch (offset) {
    case PRCKEYREG:
        return s->prckeyreg;
    case LPDDCSTAT1:
        return s->lpddcstat1;
    case LPDDCSTAT2:
        return s->lpddcstat2;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_pmm_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesPMMState *s = opaque;
    uint32_t val = val64;
    qemu_irq error = NULL;

    switch (offset) {
    case PRCKEYREG:
        s->prckeyreg = val & 0xF;

        switch (s->prckeyreg) {
        case MKEY_ERROR_FORCING:
            error = s->compare_error;
            break;
        case MKEY_SELF_TEST_ERROR_FORCING:
            error = s->self_test_error;
            break;
        case MKEY_SELF_TEST:
            break;
        default:
            return;
        }

        s->lpddcstat1 |= LPDDCSTAT1_LSTC_ALL;

        if (error) {
            qemu_irq_raise(error);
        }

        break;
    case LPDDCSTAT1:
        val &= LPDDCSTAT1_LCMPE_ALL;
        s->lpddcstat1 &= ~val;
    case LPDDCSTAT2:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_pmm_ops = {
    .read = hercules_pmm_read,
    .write = hercules_pmm_write,
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

static void hercules_pmm_realize(DeviceState *dev, Error **errp)
{
    HerculesPMMState *s = HERCULES_PMM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_pmm_ops,
                          s, TYPE_HERCULES_PMM ".io", HERCULES_PMM_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->compare_error);
    sysbus_init_irq(sbd, &s->self_test_error);
}

static void hercules_pmm_reset(DeviceState *d)
{
    HerculesPMMState *s = HERCULES_PMM(d);

    s->prckeyreg  = 0;
    s->lpddcstat1 = 0;
    s->lpddcstat2 = 0;

    qemu_irq_lower(s->compare_error);
    qemu_irq_lower(s->self_test_error);
}

static void hercules_pmm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_pmm_reset;
    dc->realize = hercules_pmm_realize;
}

static const TypeInfo hercules_pmm_info = {
    .name          = TYPE_HERCULES_PMM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesPMMState),
    .class_init    = hercules_pmm_class_init,
};

static void hercules_pmm_register_types(void)
{
    type_register_static(&hercules_pmm_info);
}

type_init(hercules_pmm_register_types)
