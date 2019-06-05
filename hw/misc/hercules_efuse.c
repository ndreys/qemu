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

#include "hw/misc/hercules_efuse.h"

enum {
    HERCULES_EFUSE_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesEFuseRegisters {
    EFCBOUND                = 0x1C,
    EFCBOUND_SELF_TEST_ERR  = BIT(21),
    EFCBOUND_SINGLE_BIT_ERR = BIT(20),
    EFCBOUND_INSTR_ERR      = BIT(19),
    EFCBOUND_AUTOLOAD_ERR   = BIT(18),
    EFCBOUND_OUTPUT_EN      = (0xf << 14),
    EFCBOUND_INPUT_EN       = (0xf <<  0),
    EFCBOUND_SELF_TEST_EN   = BIT(13),

    EFCPINS                 = 0x2C,
    EFCPINS_SELF_TEST_DONE  = BIT(15),
    EFCPINS_SELF_TEST_ERR   = BIT(14),
    EFCPINS_SINGLE_BIT_ERR  = BIT(12),
    EFCPINS_INSTR_ERR       = BIT(11),
    EFCPINS_AUTOLOAD_ERR    = BIT(10),
    EFCERRSTAT              = 0x3C,

    EFCSTCY                 = 0x48,
    EFCSTSIG                = 0x4C,
};

static uint64_t hercules_efuse_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesEFuseState *s = opaque;

    switch (offset) {
    case EFCPINS:
        return s->efcpins;
    case EFCBOUND:
    case EFCERRSTAT:
        break;
    case EFCSTCY:
        return s->efcstcy;
    case EFCSTSIG:
        return s->efcstsig;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_efuse_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesEFuseState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case EFCBOUND:
        if ((val & EFCBOUND_INPUT_EN) == EFCBOUND_INPUT_EN) {
            if (val & EFCBOUND_SELF_TEST_EN &&
                s->efcstcy  == 0x00000258 &&
                s->efcstsig == 0x5362F97F) {
                s->efcpins = EFCPINS_SELF_TEST_DONE;
            }
        }

        if ((val & EFCBOUND_OUTPUT_EN) == EFCBOUND_OUTPUT_EN) {
            if (val & EFCBOUND_AUTOLOAD_ERR) {
                s->efcpins |= EFCPINS_AUTOLOAD_ERR;
                qemu_irq_raise(s->autoload_error);
            }

            if (val & EFCBOUND_INSTR_ERR) {
                s->efcpins |= EFCPINS_INSTR_ERR;
            }

            if (val & EFCBOUND_SINGLE_BIT_ERR) {
                s->efcpins |= EFCPINS_SINGLE_BIT_ERR;
            }

            if (val & EFCBOUND_SELF_TEST_ERR) {
                s->efcpins |= EFCPINS_SELF_TEST_ERR;
                qemu_irq_raise(s->self_test_error);
            }
        }
    case EFCPINS:
    case EFCERRSTAT:
        break;
    case EFCSTCY:
        s->efcstcy = val;
        break;
    case EFCSTSIG:
        s->efcstsig = val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_efuse_ops = {
    .read = hercules_efuse_read,
    .write = hercules_efuse_write,
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

static void hercules_efuse_realize(DeviceState *dev, Error **errp)
{
    HerculesEFuseState *s = HERCULES_EFUSE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_efuse_ops,
                          s, TYPE_HERCULES_EFUSE ".io", HERCULES_EFUSE_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->autoload_error);
    sysbus_init_irq(sbd, &s->self_test_error);
    sysbus_init_irq(sbd, &s->single_bit_error);
}

static void hercules_efuse_reset(DeviceState *d)
{
    HerculesEFuseState *s = HERCULES_EFUSE(d);

    s->efcpins = 0;
}

static void hercules_efuse_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_efuse_reset;
    dc->realize = hercules_efuse_realize;
}

static const TypeInfo hercules_efuse_info = {
    .name          = TYPE_HERCULES_EFUSE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesEFuseState),
    .class_init    = hercules_efuse_class_init,
};

static void hercules_efuse_register_types(void)
{
    type_register_static(&hercules_efuse_info);
}

type_init(hercules_efuse_register_types)
