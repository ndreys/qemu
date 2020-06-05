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

#include "hw/misc/hercules_ccm.h"
#include "hw/arm/hercules.h"

enum {
    HERCULES_CCM_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesCCMRegisters {
    CCMSR1                        = 0x00,
    CCMKEYR1                      = 0x04,
    CCMSR2                        = 0x08,
    CCMKEYR2                      = 0x0C,
    CCMSR3                        = 0x10,
    CCMKEYR3                      = 0x14,
    MKEYn_SELF_TEST               = 0x6,
    MKEYn_ERROR_FORCING           = 0x9,
    MKEYn_SELF_TEST_ERROR_FORCING = 0xF,
    STCn                          = BIT(8),
};

static uint64_t hercules_ccm_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesCCMState *s = opaque;

    switch (offset) {

    case CCMSR1:
        return s->ccmsr[0];
    case CCMSR2:
        return s->ccmsr[1];
    case CCMSR3:
        return s->ccmsr[2];
    case CCMKEYR1:
    case CCMKEYR2:
    case CCMKEYR3:
        break;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_ccm_test(HerculesCCMState *s, unsigned int idx,
                              uint32_t val)
{
    switch (val) {
    case MKEYn_SELF_TEST:
        s->ccmsr[idx] |= STCn;
        break;
    case MKEYn_ERROR_FORCING:
        qemu_irq_raise(s->error[idx]);
        qemu_irq_raise(s->error_self_test);
        break;
    case MKEYn_SELF_TEST_ERROR_FORCING:
        qemu_irq_raise(s->error_self_test);
        break;
    }
}

static void hercules_ccm_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesCCMState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case CCMSR1:
        s->ccmsr[0] &= ~val;
        break;
    case CCMSR2:
        s->ccmsr[1] &= ~val;
        break;
    case CCMSR3:
        s->ccmsr[2] &= ~val;
        break;
    case CCMKEYR1:
        hercules_ccm_test(s, 0, val);
        break;
    case CCMKEYR2:
        hercules_ccm_test(s, 1, val);
        break;
    case CCMKEYR3:
        hercules_ccm_test(s, 2, val);
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_ccm_realize(DeviceState *dev, Error **errp)
{
    HerculesCCMState *s = HERCULES_CCM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    static MemoryRegionOps hercules_ccm_ops = {
        .read = hercules_ccm_read,
        .write = hercules_ccm_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
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

    if (parent->is_tms570)
    {
        hercules_ccm_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->iomem, obj, &hercules_ccm_ops,
                          s, TYPE_HERCULES_CCM ".io", HERCULES_CCM_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->error[0]);
    sysbus_init_irq(sbd, &s->error[1]);
    sysbus_init_irq(sbd, &s->error[2]);
    sysbus_init_irq(sbd, &s->error_self_test);
}

static void hercules_ccm_reset(DeviceState *d)
{
    HerculesCCMState *s = HERCULES_CCM(d);

    memset(s->ccmsr, 0, sizeof(s->ccmsr));
}

static void hercules_ccm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_ccm_reset;
    dc->realize = hercules_ccm_realize;
}

static const TypeInfo hercules_ccm_info = {
    .name          = TYPE_HERCULES_CCM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesCCMState),
    .class_init    = hercules_ccm_class_init,
};

static void hercules_ccm_register_types(void)
{
    type_register_static(&hercules_ccm_info);
}
type_init(hercules_ccm_register_types)
