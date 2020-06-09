/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qapi/error.h"

#include "hw/adc/hercules_mibadc.h"
#include "hw/arm/hercules.h"

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesMibAdcRegisters {
    ADOPMODECR     = 0x004,
    _10_12_BIT     = BIT(31),
    COS            = BIT(24),
    ADEVINTFLG     = 0x34,
    ADG1INTFLG     = 0x38,
    ADG2INTFLG     = 0x3C,
    ADGxINTFLG_END = BIT(3),
    ADG1THRINTCR   = 0x44,
    ADG2THRINTCR   = 0x48,
    ADBNDCR        = 0x58,
    ADBNDEND       = 0x5C,
    ADEVSR         = 0x6C,
    ADG1SR         = 0x70,
    ADG2SR         = 0x74,
    ADGxSR_END     = BIT(0),
    ADEVSEL        = 0x78,
    ADG1SEL        = 0x7C,
    ADG2SEL        = 0x80,
    ADG1BUFFER0    = 0xB0,
    ADG1BUFFER7    = 0xCC,
    ADG2BUFFER0    = 0xD0,
    ADG2BUFFER7    = 0xEC,

    ADPARCR        = 0x180,
    TEST           = BIT(8),
    ADPARADDR      = 0x184,
};

enum {
    HERCULES_MIBADC_CONTAINER_SIZE = 8 * 1024,
    HERCULES_MIBADC_RAM_OFFSET     = 0,
    HERCULES_MIBADC_ECC_OFFSET     = HERCULES_MIBADC_CONTAINER_SIZE / 2,

    HERCULES_MIBADC_REGS_SIZE      = 512,
};

#define Gx_EMPTY(adopmodecr)    ((adopmodecr & _10_12_BIT) ? BIT(31) : BIT(15))
static bool hercules_mibadc_group_invalid(HerculesMibAdcGroup *group)
{
    if (group->start >= group->end) {
        return true;
    }

    return false;
}

static void hercules_mibadc_group_reset(HerculesMibAdcGroup *group)
{
    group->rdidx = group->wridx = group->start;
}

static void hercules_mibadc_push_result(HerculesMibAdcState *s,
                                        HerculesMibAdcGroup *group,
                                        uint32_t chid,
                                        uint32_t result)
{
    unsigned int start;

    if (hercules_mibadc_group_invalid(group)) {
        return;
    }

    if (group->wridx == group->end) {
        /*
         * Results RAM is full. Ingore new result
         */
        return;
    }

    start = (s->adopmodecr & _10_12_BIT) ? 16 : 10;
    s->results[group->wridx++] = deposit32(result, start, 5, chid);
}

static uint32_t hercules_mibadc_pop_result(HerculesMibAdcState *s,
                                           HerculesMibAdcGroup *group)
{
    const uint32_t empty = Gx_EMPTY(s->adopmodecr);
    uint32_t result;

    if (hercules_mibadc_group_invalid(group)) {
        return empty;
    }

    if (group->wridx == group->start) {
        /*
         * Results RAM is empty
         */
        return empty;
    }

    result = s->results[group->rdidx];
    s->results[group->rdidx++] = empty;

    if (group->rdidx == group->wridx) {
        /*
         * All results were read out we can rewind results RAM indices
         */
        hercules_mibadc_group_reset(group);
    }

    return result;
}

static void hercules_mibadc_do_conversion(HerculesMibAdcState *s,
                                          HerculesMibAdcGroup *group)
{
    unsigned long sel = group->sel;
    uint32_t chid;

    for_each_set_bit(chid, &sel, ARRAY_SIZE(s->channel)) {
        hercules_mibadc_push_result(s, group, chid, s->channel[chid]);
    }

    group->sr     |= ADGxSR_END;
    group->intflg |= ADGxINTFLG_END;
}

#define IDX(o, s)    (((o) - (s)) / sizeof(uint32_t))

static void hercules_mibadc_ram_write(void *opaque, hwaddr offset,
                                      uint64_t val, unsigned size)
{
    HerculesMibAdcState *s = opaque;

    s->results[IDX(offset, 0)] = val;
}

static uint64_t hercules_mibadc_ram_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    HerculesMibAdcState *s = opaque;
    unsigned int idx = IDX(offset, 0);

    if (s->ecc[idx]) {
        /*
         * TODO this isn't how real HW would do it, but its enough to
         * pass our ADC error signalling functionality test
         */
        s->adparaddr = offset;
        qemu_irq_raise(s->parity_error);
    }

    return s->results[idx];
}

static void hercules_mibadc_ecc_write(void *opaque, hwaddr offset,
                                      uint64_t val, unsigned size)
{
    HerculesMibAdcState *s = opaque;

    if (s->adparcr & TEST) {
        s->ecc[IDX(offset, 0)] = val;
    }
}

static uint64_t hercules_mibadc_ecc_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    HerculesMibAdcState *s = opaque;

    if (s->adparcr & TEST) {
        return s->ecc[IDX(offset, 0)];
    }

    return 0;
}

static uint64_t hercules_mibadc_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    HerculesMibAdcState *s = opaque;

    switch (offset) {
    case ADOPMODECR:
        return s->adopmodecr | COS;
    case ADEVINTFLG ... ADG2INTFLG:
        return s->adg[IDX(offset, ADEVINTFLG)].intflg;
    case ADEVSR ... ADG2SR:
        return s->adg[IDX(offset, ADEVSR)].sr;
    case ADG1BUFFER0 ... ADG1BUFFER7:
        return hercules_mibadc_pop_result(s, &s->adg[1]);
    case ADG2BUFFER0 ... ADG2BUFFER7:
        return hercules_mibadc_pop_result(s, &s->adg[2]);
    case ADPARCR:
        return s->adparcr;
    case ADBNDCR:
    case ADBNDEND:
    case ADPARADDR:
        break;
    case ADG1THRINTCR ... ADG2THRINTCR:
        return 0;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_mibadc_write(void *opaque, hwaddr offset,
                                  uint64_t val64, unsigned size)
{
    HerculesMibAdcState *s = opaque;
    const uint32_t val = val64;
    HerculesMibAdcGroup *group;

    switch (offset) {
    case ADG1SEL:
    case ADG2SEL:
        group = &s->adg[IDX(offset, ADEVSEL)];
        group->sel = val;
        hercules_mibadc_do_conversion(s, group);
        break;
    case ADOPMODECR:
        s->adopmodecr = val;
        break;
    case ADBNDCR:
        /*
         * We don't support more than 64 word buffer, so we only
         * extract 6 bits
         */
        s->adg[0].end = s->adg[1].start = 2 * extract32(val, 16, 6);
        s->adg[1].end = s->adg[2].start = 2 * extract32(val,  0, 6);

        hercules_mibadc_group_reset(&s->adg[1]);
        hercules_mibadc_group_reset(&s->adg[2]);
        break;
    case ADBNDEND:
        s->adg[2].end = 16 << MIN(extract32(val, 0, 2), 2);
        break;
    case ADEVSR ... ADG2SR:
        s->adg[IDX(offset, ADEVSR)].sr &= ~(val & ADGxSR_END);
        break;
    case ADEVINTFLG ... ADG2INTFLG:
        s->adg[IDX(offset, ADEVINTFLG)].intflg &= ~(val & ADGxINTFLG_END);
        break;
    case ADPARCR:
        s->adparcr = val;
        break;
    case ADPARADDR:
    case ADG1BUFFER0 ... ADG2BUFFER7:
        return;
    default:
        qemu_log_bad_offset(offset);
    }
}

#undef IDX

static void hercules_mibadc_realize(DeviceState *dev, Error **errp)
{
    HerculesMibAdcState *s = HERCULES_MIBADC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    static MemoryRegionOps hercules_mibadc_ecc_ops = {
        .read = hercules_mibadc_ecc_read,
        .write = hercules_mibadc_ecc_write,
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

    static MemoryRegionOps hercules_mibadc_ram_ops = {
        .read = hercules_mibadc_ram_read,
        .write = hercules_mibadc_ram_write,
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

    static MemoryRegionOps hercules_mibadc_ops = {
        .read = hercules_mibadc_read,
        .write = hercules_mibadc_write,
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
        hercules_mibadc_ecc_ops.endianness = DEVICE_BIG_ENDIAN;
        hercules_mibadc_ram_ops.endianness = DEVICE_BIG_ENDIAN;
        hercules_mibadc_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->regs, OBJECT(dev), &hercules_mibadc_ops, s,
                          TYPE_HERCULES_MIBADC ".regs",
                          HERCULES_MIBADC_REGS_SIZE);
    sysbus_init_mmio(sbd, &s->regs);

    memory_region_init_io(&s->io.ram, OBJECT(dev), &hercules_mibadc_ram_ops,
                          s, TYPE_HERCULES_MIBADC ".io.ram",
                          sizeof(s->results));

    memory_region_init_io(&s->io.ecc, OBJECT(dev), &hercules_mibadc_ecc_ops,
                          s, TYPE_HERCULES_MIBADC ".io.ecc",
                          sizeof(s->ecc));

    memory_region_init(&s->io.container, OBJECT(dev),
                       TYPE_HERCULES_MIBADC ".io.container",
                       HERCULES_MIBADC_CONTAINER_SIZE);

    memory_region_add_subregion(&s->io.container, HERCULES_MIBADC_RAM_OFFSET,
                                &s->io.ram);
    memory_region_add_subregion(&s->io.container, HERCULES_MIBADC_ECC_OFFSET,
                                &s->io.ecc);
    sysbus_init_mmio(sbd, &s->io.container);

    sysbus_init_irq(sbd, &s->parity_error);
}

static void hercules_mibadc_reset(DeviceState *dev)
{
    HerculesMibAdcState *s = HERCULES_MIBADC(dev);

    s->adopmodecr = 0;

    memset(s->adg,     0, sizeof(s->adg));
    memset(s->results, 0, sizeof(s->results));

    qemu_irq_lower(s->parity_error);
}

static void hercules_mibadc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_mibadc_reset;
    dc->realize = hercules_mibadc_realize;
}

static const TypeInfo hercules_mibadc_info = {
    .name          = TYPE_HERCULES_MIBADC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesMibAdcState),
    .class_init    = hercules_mibadc_class_init,
};

static void hercules_mibadc_register_types(void)
{
    type_register_static(&hercules_mibadc_info);
}

type_init(hercules_mibadc_register_types)
