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
#include "qemu/log.h"
#include "qapi/error.h"
#include "cpu.h"

#include "hw/intc/hercules_vim.h"

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesVimRegisters {
    IRQINDEX   = 0x00,
    FIQINDEX   = 0x04,
    FIRQPR0    = 0x10,
    FIRQPR1    = 0x14,
    FIRQPR2    = 0x18,
    FIRQPR3    = 0x1C,
    REQENASET0 = 0x30,
    REQENASET1 = 0x34,
    REQENASET2 = 0x38,
    REQENASET3 = 0x3C,
    REQENACLR0 = 0x40,
    REQENACLR1 = 0x44,
    REQENACLR2 = 0x48,
    REQENACLR3 = 0x4C,
    IRQVECREG  = 0x70,
    FIQVECREG  = 0x74,
    CHANCTRL0  = 0x80,
    CHANCTRL31 = 0xFC,
};

static void hercules_vim_update_line(HerculesVimState *s,
                                     unsigned long *mask,
                                     qemu_irq irq)
{
    int group;

    for (group = 0; group < HERCULES_NUM_IRQ_GROUP; group++) {
        if (s->intreq[group] & s->reqena[group] & mask[group]) {
            qemu_irq_raise(irq);
            return;
        }
    }

    qemu_irq_lower(irq);
}

/* Update interrupts.  */
static void hercules_vim_update(HerculesVimState *s)
{
    hercules_vim_update_line(s, s->rpqrif, s->irq);
    hercules_vim_update_line(s, s->firqpr, s->fiq);
}

static void hercules_vim_set_irq(void *opaque, int irq, int level)
{
    HerculesVimState *s = opaque;
    int group, index;
    unsigned long bit;
    /*
     * Map physical IRQ line to a channel
     */
    irq = s->chanctrl[irq];

    group = irq / HERCULES_IRQ_GROUP_WIDTH;
    index = irq % HERCULES_IRQ_GROUP_WIDTH;
    bit   = BIT(index);

    if (level) {
        s->intreq[group] |= bit;
    } else {
        s->intreq[group] &= ~bit;
    }

    if (unlikely(s->firqpr[group] & bit)) {
        hercules_vim_update_line(s, s->firqpr, s->fiq);
    } else {
        hercules_vim_update_line(s, s->rpqrif, s->irq);
    }
}

static uint32_t hercules_vim_line_index(HerculesVimState *s,
                                        unsigned long *mask)
{
    int group;

    for (group = 0; group < HERCULES_NUM_IRQ_GROUP; group++) {
        const unsigned long active = s->intreq[group] & mask[group];
        if (active) {
            return HERCULES_IRQ_GROUP_WIDTH * group + ctzl(active) + 1;
        }
    }

    return 0;
}

static uint32_t hercules_vim_irq_index(HerculesVimState *s)
{
    return hercules_vim_line_index(s, s->rpqrif);
}

static uint32_t hercules_vim_fiq_index(HerculesVimState *s)
{
    return hercules_vim_line_index(s, s->firqpr);
}

static uint32_t hercules_vim_read_vector(HerculesVimState *s, int idx)
{
    /*
     * FIXME: Is this the best way to deal with endianness RM57 vs
     * TMS570
     */
    const bool big_endian = s->iomem.ops->endianness == DEVICE_BIG_ENDIAN;
    const uint32_t *vector = &s->vectors[idx];

    return big_endian ? ldl_be_p(vector) : ldl_le_p(vector);
}

static uint64_t hercules_vim_read(void *opaque, hwaddr offset, unsigned size)
{
    HerculesVimState *s = opaque;

    switch (offset) {
    case IRQINDEX:
        return hercules_vim_irq_index(s);
    case FIQINDEX:
        return hercules_vim_fiq_index(s);
    case FIRQPR0:
        return s->firqpr0;
    case FIRQPR1:
        return s->firqpr1;
    case FIRQPR2:
        return s->firqpr2;
    case FIRQPR3:
        return s->firqpr3;
    case IRQVECREG:
        return hercules_vim_read_vector(s, hercules_vim_irq_index(s));
    case FIQVECREG:
        return hercules_vim_read_vector(s, hercules_vim_fiq_index(s));
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_vim_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesVimState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case IRQINDEX:
    case IRQVECREG:
    case FIQVECREG:
    case FIQINDEX:
        /* no-op */
        break;
    case FIRQPR0:
        s->firqpr0 = val;
        s->rpqrif0 = ~val;
        break;
    case FIRQPR1:
        s->firqpr1 = val;
        s->rpqrif1 = ~val;
        break;
    case FIRQPR2:
        s->firqpr2 = val;
        s->rpqrif2 = ~val;
        break;
    case FIRQPR3:
        s->firqpr3 = val;
        s->rpqrif3 = ~val;
        break;
    case REQENASET0:
        s->reqena0 |= val;
        hercules_vim_update(s);
        break;
    case REQENACLR0:
        s->reqena0 &= ~val;
        hercules_vim_update(s);
        break;
    case REQENASET1:
        s->reqena1 |= val;
        hercules_vim_update(s);
        break;
    case REQENACLR1:
        s->reqena1 &= ~val;
        hercules_vim_update(s);
        break;
    case REQENASET2:
        s->reqena2 |= val;
        hercules_vim_update(s);
        break;
    case REQENACLR2:
        s->reqena2 &= ~val;
        hercules_vim_update(s);
        break;
    case REQENASET3:
        s->reqena3 |= val;
        hercules_vim_update(s);
        break;
    case REQENACLR3:
        s->reqena3 &= ~val;
        hercules_vim_update(s);
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_vim_ops = {
    .read = hercules_vim_read,
    .write = hercules_vim_write,
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

static void hercules_vim_reset(DeviceState *d)
{
    HerculesVimState *s = HERCULES_VIM(d);
    int i;

    memset(s->vectors, 0, sizeof(s->vectors));

    for (i = 0; i < HERCULES_NUM_IRQ_GROUP; i++) {
        s->intreq[i] = 0;
        s->reqena[i] = 0;
        s->firqpr[i] = 0;
        s->rpqrif[i] = ~s->firqpr[i];
    }

    s->firqpr[0] = 0b11;
    s->rpqrif[0] = ~s->firqpr[0];

    for (i = 0; i < HERCULES_NUM_IRQ; i++) {
        s->chanctrl[i] = i;
    }

    hercules_vim_update(s);
}

static void hercules_vim_initfn(Object *obj)
{
    HerculesVimState *s = HERCULES_VIM(obj);

    sysbus_init_child_obj(obj, "ecc-regs", &s->ecc,
                          sizeof(UnimplementedDeviceState),
                          TYPE_UNIMPLEMENTED_DEVICE);
}

static void hercules_vim_realize(DeviceState *dev, Error **errp)
{
    HerculesVimState *s = HERCULES_VIM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    qdev_prop_set_string(DEVICE(&s->ecc), "name", "ecc-regs");
    qdev_prop_set_uint64(DEVICE(&s->ecc), "size", 256);
    object_property_set_bool(OBJECT(&s->ecc), true, "realized", &error_fatal);
    sysbus_init_mmio(sbd, sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->ecc), 0));

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_vim_ops,
                          s, "hercules.vim", 256);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_ram_ptr(&s->ram, OBJECT(dev), TYPE_HERCULES_VIM ".ram",
                               sizeof(s->vectors), s->vectors);
    sysbus_init_mmio(sbd, &s->ram);

    qdev_init_gpio_in(dev, hercules_vim_set_irq, HERCULES_NUM_IRQ);

    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->fiq);
}

static void hercules_vim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_vim_reset;
    dc->realize = hercules_vim_realize;
}

static const TypeInfo hercules_vim_info = {
    .name          = TYPE_HERCULES_VIM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesVimState),
    .instance_init = hercules_vim_initfn,
    .class_init    = hercules_vim_class_init,
};

static void hercules_vim_register_types(void)
{
    type_register_static(&hercules_vim_info);
}

type_init(hercules_vim_register_types)
