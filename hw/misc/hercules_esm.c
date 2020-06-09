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

#include "hw/misc/hercules_esm.h"
#include "hw/arm/hercules.h"

enum {
    HERCULES_ESM_SIZE         = 256,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesESMRegisters {
    ESMDEPAPR1 = 0x00,
    ESMEEPAPR1 = 0x04,
    ESMIESR1   = 0x08,
    ESMIECR1   = 0x0C,
    ESMILSR1   = 0x10,
    ESMILCR1   = 0x14,
    ESMSR1     = 0x18,
    ESMSR2     = 0x1C,
    ESMSR3     = 0x20,
    ESMEPSR    = 0x24,
    ESMIOFFHR  = 0x28,
    ESMIOFFLR  = 0x2C,

    ESMLTCPR   = 0x34,
    ESMEKR     = 0x38,
    ESMSSR2    = 0x3C,

    ESMIEPSR4  = 0x40,
    ESMIEPCR4  = 0x44,
    ESMIESR4   = 0x48,
    ESMIECR4   = 0x4C,
    ESMILSR4   = 0x50,
    ESMILCR4   = 0x54,
    ESMSR4     = 0x58,

    ESMIEPSR7  = 0x80,
    ESMIEPSC7  = 0x84,
    ESMIESR7   = 0x88,
    ESMIECR7   = 0x8C,
    ESMILSR7   = 0x90,
    ESMILCR7   = 0x94,
    ESMSR7     = 0x98,
};

enum {
    ESM_R1,
    ESM_R4,
    ESM_R7,
    ESM_R2,
    ESM_R3,
};

static void hercules_esm_irq_lower(HerculesESMState *s, int n)
{
    const unsigned int bit = BIT(n);

    if (s->irq_state & bit) {
        qemu_irq_lower(s->irq[n]);
        s->irq_state &= ~bit;
    }
}

static void hercules_esm_irq_raise(HerculesESMState *s, int n)
{
    const unsigned int bit = BIT(n);

    if (s->irq_state & bit) {
        return;
    }

    s->irq_state |= bit;
    qemu_irq_raise(s->irq[n]);
}

static void hercules_esm_set_error(void *opaque, int error, int level)
{
    HerculesESMState *s = opaque;
    unsigned int idx;
    unsigned int bit;

    if (level) {
        switch (error) {
        case 0 ... 95:
            idx = error / 32;
            bit = BIT(error % 32);

            s->esmsr[idx] |= bit;

            if (s->esmie[idx] & bit) {
                hercules_esm_irq_raise(s, (s->esmil[idx] & bit) ?
                                       HERCULES_ESM_IRQ_HIGH :
                                       HERCULES_ESM_IRQ_LOW);
            }
            break;
        case 96 ... 127:
            /* group 2 */
            s->esmsr[ESM_R2] |= BIT(error - 96);
            hercules_esm_irq_raise(s, HERCULES_ESM_IRQ_HIGH);
            break;
        case 128 ... 159:
            s->esmsr[ESM_R3] |= BIT(error - 128);
            /* FIXME: Need to abort here */
            /* fall through */
        default:
            return;
        }
    }
}

enum HerculesESMIOffMask {
    ESMIOFF_HIGH = 0x00000000,
    ESMIOFF_LOW  = 0xFFFFFFFF,
};

static unsigned int hercules_esm_interrupt_offset_high(HerculesESMState *s)
{
    const uint32_t pending = s->esmie[ESM_R2] & s->esmsr[ESM_R2];

    if (pending) {
        return 0x21 + ctzl(pending);
    }

    return 0;
}

static unsigned int
hercules_esm_interrupt_offset_low(HerculesESMState *s,
                                  enum HerculesESMIOffMask mask)
{
    uint32_t pending;

    pending = s->esmie[ESM_R1] & (s->esmsr[ESM_R1] ^ mask);
    if (pending) {
        return 0x01 + ctzl(pending);
    }

    pending = s->esmie[ESM_R4] & (s->esmsr[ESM_R4] ^ mask);
    if (pending) {
        return 0x41 + ctzl(pending);
    }

    pending = s->esmie[ESM_R7] & (s->esmsr[ESM_R7] ^ mask);
    if (pending) {
        return 0x81 + ctzl(pending);
    }

    return 0;
}

static uint64_t hercules_esm_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesESMState *s = opaque;
    int irq;

    switch (offset) {
    case ESMIESR1:
    case ESMIECR1:
        return s->esmie[ESM_R1];
    case ESMILSR1:
    case ESMILCR1:
        return s->esmil[ESM_R1];
    case ESMSR1:
        return s->esmsr[ESM_R1];
    case ESMSR2:
        return s->esmsr[ESM_R2];
    case ESMSR3:
        return s->esmsr[ESM_R3];
    case ESMIOFFHR:
        irq = hercules_esm_interrupt_offset_high(s);
        if (irq) {
            return irq;
        }
        return hercules_esm_interrupt_offset_low(s, ESMIOFF_HIGH);
    case ESMIOFFLR:
        return hercules_esm_interrupt_offset_low(s, ESMIOFF_LOW);
    case ESMIEPSR4:
    case ESMIEPCR4:
        return s->esmiep[ESM_R4];
    case ESMIESR4:
    case ESMIECR4:
        return s->esmie[ESM_R4];
    case ESMILSR4:
    case ESMILCR4:
        return s->esmil[ESM_R4];
    case ESMSR4:
        return s->esmsr[ESM_R4];
    case ESMIEPSR7:
    case ESMIEPSC7:
        return s->esmiep[ESM_R7];
    case ESMIESR7:
    case ESMIECR7:
        return s->esmie[ESM_R7];
    case ESMILSR7:
    case ESMILCR7:
        return s->esmil[ESM_R7];
    case ESMSR7:
        return s->esmsr[ESM_R7];
    case ESMDEPAPR1:
    case ESMEEPAPR1:
    case ESMEPSR:
        return 0;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_esm_update_irq_high(HerculesESMState *s)
{
    if (s->esmsr[ESM_R2] ||
        s->esmsr[ESM_R1] & s->esmie[ESM_R1] & s->esmil[ESM_R1] ||
        s->esmsr[ESM_R4] & s->esmie[ESM_R4] & s->esmil[ESM_R4] ||
        s->esmsr[ESM_R7] & s->esmie[ESM_R7] & s->esmil[ESM_R7]) {
        return;
    }

    hercules_esm_irq_lower(s, HERCULES_ESM_IRQ_HIGH);
}

static void hercules_esm_update_irq_low(HerculesESMState *s)
{
    if (s->esmsr[ESM_R1] & s->esmie[ESM_R1] & ~s->esmil[ESM_R1] ||
        s->esmsr[ESM_R4] & s->esmie[ESM_R4] & ~s->esmil[ESM_R4] ||
        s->esmsr[ESM_R7] & s->esmie[ESM_R7] & ~s->esmil[ESM_R7]) {
        return;
    }

    hercules_esm_irq_lower(s, HERCULES_ESM_IRQ_LOW);
}


static void hercules_esm_update_irqs(HerculesESMState *s)
{
    hercules_esm_update_irq_high(s);
    hercules_esm_update_irq_low(s);
}

static void hercules_esm_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesESMState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case ESMIESR1:
        s->esmie[ESM_R1] |= val;
        break;
    case ESMIECR1:
        s->esmie[ESM_R1] &= ~val;
        break;
    case ESMILSR1:
        s->esmil[ESM_R1] |= val;
        break;
    case ESMILCR1:
        s->esmil[ESM_R1] &= ~val;
        break;
    case ESMSR1:
        s->esmsr[ESM_R1] &= ~val;
        hercules_esm_update_irqs(s);
        break;
    case ESMSR2:
        s->esmsr[ESM_R2] &= ~val;
        hercules_esm_update_irq_high(s);
        break;
    case ESMSR3:
        s->esmsr[ESM_R3] &= ~val;
        break;
    case ESMIEPSR4:
        s->esmiep[ESM_R4] |= val;
        break;
    case ESMIEPCR4:
        s->esmiep[ESM_R4] &= ~val;
        break;
    case ESMIESR4:
        s->esmie[ESM_R4] |= val;
        hercules_esm_update_irqs(s);
        break;
    case ESMIECR4:
        s->esmie[ESM_R4] &= ~val;
        hercules_esm_update_irqs(s);
        break;
    case ESMILSR4:
        s->esmil[ESM_R4] |= val;
        break;
    case ESMILCR4:
        s->esmil[ESM_R4] &= ~val;
        break;
    case ESMSR4:
        s->esmsr[ESM_R4] &= ~val;
        hercules_esm_update_irqs(s);
        break;
    case ESMIEPSR7:
        s->esmiep[ESM_R7] |= val;
        break;
    case ESMIEPSC7:
        s->esmiep[ESM_R7] &= ~val;
        break;
    case ESMIESR7:
        s->esmie[ESM_R7] |= val;
        hercules_esm_update_irqs(s);
        break;
    case ESMIECR7:
        s->esmie[ESM_R7] &= ~val;
        hercules_esm_update_irqs(s);
        break;
    case ESMILSR7:
        s->esmil[ESM_R7] |= val;
        break;
    case ESMILCR7:
        s->esmil[ESM_R7] &= ~val;
        break;
    case ESMSR7:
        s->esmsr[ESM_R7] &= ~val;
        hercules_esm_update_irqs(s);
        break;
    case ESMIOFFHR:
    case ESMIOFFLR:
    case ESMDEPAPR1:
    case ESMEEPAPR1:
    case ESMLTCPR:
    case ESMEKR:
    case ESMSSR2:
        /* No op */
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_esm_realize(DeviceState *dev, Error **errp)
{
    HerculesESMState *s = HERCULES_ESM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    static MemoryRegionOps hercules_esm_ops = {
        .read = hercules_esm_read,
        .write = hercules_esm_write,
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
        hercules_esm_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_esm_ops,
                          s, TYPE_HERCULES_ESM ".io", HERCULES_ESM_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    qdev_init_gpio_in(dev, hercules_esm_set_error, HERCULES_NUM_ESM_CHANNELS);

    sysbus_init_irq(sbd, &s->irq[HERCULES_ESM_IRQ_HIGH]);
    sysbus_init_irq(sbd, &s->irq[HERCULES_ESM_IRQ_LOW]);
}

static void hercules_esm_reset(DeviceState *d)
{
    HerculesESMState *s = HERCULES_ESM(d);

    memset(s->esmsr,  0, sizeof(s->esmsr));
    memset(s->esmil,  0, sizeof(s->esmil));
    memset(s->esmie,  0, sizeof(s->esmie));
    memset(s->esmiep, 0, sizeof(s->esmiep));

    hercules_esm_irq_lower(s, HERCULES_ESM_IRQ_LOW);
    hercules_esm_irq_lower(s, HERCULES_ESM_IRQ_HIGH);
}

static void hercules_esm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_esm_reset;
    dc->realize = hercules_esm_realize;
}

static const TypeInfo hercules_esm_info = {
    .name          = TYPE_HERCULES_ESM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesESMState),
    .class_init    = hercules_esm_class_init,
};

static void hercules_esm_register_types(void)
{
    type_register_static(&hercules_esm_info);
}

type_init(hercules_esm_register_types)
