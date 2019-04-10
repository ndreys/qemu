/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qapi/error.h"

#include "hw/gpio/hercules_gpio.h"

#include "trace.h"

enum {
    HERCULES_GIO_REGS_SIZE   = 0x34,
    HERCULES_GIO_GIO_SIZE    = 0x20,
    HERCULES_GIO_REGS_OFFSET = 0x00,
    HERCULES_GIO_GIOA_OFFSET = 0x34,
    HERCULES_GIO_GIOB_OFFSET = 0x54,
};

enum HerculesGioRegRegisters {
    GIOGCR0   = 0x00,
    GIOINTDET = 0x08,
    GIOPOL    = 0x0C,
    GIOENASET = 0x10,
    GIOENACLR = 0x14,
    GIOLVLSET = 0x18,
    GIOLVLCLR = 0x1C,
    GIOFLG    = 0x20,
    GIOOFF1   = 0x24,
    GIOOFF2   = 0x28,
    GIOEMU1   = 0x2C,
    GIOEMU2   = 0x30,
};

enum HerculesGioGioRegisters {
    GIODIR    = 0x00,
    GIODIN    = 0x04,
    GIODOUT   = 0x08,
    GIODSET   = 0x0C,
    GIODCLR   = 0x10,
    GIOPDR    = 0x14,
    GIOPULDIS = 0x18,
    GIOPSL    = 0x1C,
};

enum HerculesHetRegisters {
    HETDIR    = 0x4C,
    HETDIN    = 0x50,
    HETDOUT   = 0x54,
    HETDSET    = 0x58,
    HETDCLR    = 0x5c,
    HETLBPSEL = 0x8C,
    HETLBPDIR = 0x90,
    HETPINDIS = 0x94,
};

#define HETLBPDIR_LBPTSTENA(v)   extract32(v, 16, 4)

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

static uint64_t hercules_gio_gio_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    HerculesGpio *gpio = opaque;

    switch (offset) {
    case GIODIR:
        return gpio->dir;
    case GIODIN:
        return gpio->din;
    case GIODSET:
    case GIODCLR:
    case GIODOUT:
        return gpio->dout;
    case GIOPDR:
        return gpio->pdr;
    case GIOPULDIS:
        return gpio->puldis;
    case GIOPSL:
        return gpio->psl;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_gio_update_din(HerculesGpio *gpio)
{
    trace_hercules_gio_update(gpio->bank,
                              gpio->din, gpio->dir, gpio->dout);

    gpio->din &= ~gpio->dir;
    gpio->din |= gpio->dout & gpio->dir;

    trace_hercules_gio_update(gpio->bank,
                              gpio->din, gpio->dir, gpio->dout);
}

static void hercules_gio_gio_write(void *opaque, hwaddr offset,
                                   uint64_t val64, unsigned size)
{
    HerculesGpio *gpio = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case GIODIR:
        gpio->dir = val;
        hercules_gio_update_din(gpio);
        break;
    case GIODIN:
        break;
    case GIODOUT:
        gpio->dout = val;
        hercules_gio_update_din(gpio);
        break;
    case GIODSET:
        gpio->dout |= val;
        hercules_gio_update_din(gpio);
        break;
    case GIODCLR:
        gpio->dout &= ~val;
        hercules_gio_update_din(gpio);
        break;
    case GIOPDR:
        gpio->pdr = val;
        break;
    case GIOPULDIS:
        gpio->puldis = val;
        break;
    case GIOPSL:
        gpio->psl = val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static uint64_t hercules_gio_reg_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    HerculesGioState *s = opaque;

    switch (offset) {
    case GIOENACLR:
        return s->gioena;
    case GIOLVLSET:
    case GIOLVLCLR:
        return s->giolvl;
    case GIOFLG:
        return s->gioflg;
    case GIOGCR0:
    case GIOINTDET:
    case GIOPOL:
    case GIOENASET:
    case GIOOFF1:
    case GIOOFF2:
    case GIOEMU1:
    case GIOEMU2:
        break;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_gio_reg_write(void *opaque, hwaddr offset,
                                   uint64_t val64, unsigned size)
{
    HerculesGioState *s = opaque;
    uint32_t val = val64;

    switch (offset) {
    case GIOENASET:
        s->gioena |= val;
        break;
    case GIOENACLR:
        s->gioena &= ~val;
        break;
    case GIOLVLSET:
        s->giolvl |= val;
        break;
    case GIOLVLCLR:
        s->giolvl &= ~val;
        break;
    case GIOFLG:
        s->gioflg = val;
        break;
    case GIOGCR0:
    case GIOINTDET:
    case GIOPOL:
    case GIOOFF1:
    case GIOOFF2:
    case GIOEMU1:
    case GIOEMU2:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static uint64_t hercules_n2het_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    HerculesN2HetState *s = opaque;

    switch (offset) {
    case HETDIN:
        return s->gpio.din;
    case HETDIR:
        return s->gpio.dir;
    case HETDSET:
    case HETDCLR:
    case HETDOUT:
        return s->gpio.dout;
    case HETLBPDIR:
        return s->hetlbpdir;
    case HETLBPSEL:
    case HETPINDIS:
        /* always zero */
        break;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_n2het_update_gpios(HerculesN2HetState *s)
{
    const bool loopback = HETLBPDIR_LBPTSTENA(s->hetlbpdir) == 0xA;

    if (unlikely(loopback)) {
        int i, n;

        s->gpio.din = s->gpio.dout;

        for (n = 0, i = 0; i < 16; i++, n += 2) {
            const uint32_t bits = extract32(s->gpio.dout, n, 2);
            const uint8_t pattern = 0b00111100;
            unsigned int shift;

            if (bits == 0b00 || bits == 0b11) {
                /* Nothing to do both bits are equal */
                continue;
            }

            shift = bits & ~BIT(0);    /* 0b01 -> 0, 0b10 -> 2 */

            /*
             * false: [n + 1] -> [n]
             * true:  [n] -> [n + 1]
             */
            if (BIT(i) & s->hetlbpdir) {
                shift += 4;
            }

            s->gpio.din = deposit32(s->gpio.din, n, 2, pattern >> shift);
        }
    } else {
        hercules_gio_update_din(&s->gpio);
    }
}

static void hercules_n2het_write(void *opaque, hwaddr offset,
                                 uint64_t val64, unsigned size)
{
    HerculesN2HetState *s = opaque;
    uint32_t val = val64;

    switch (offset) {
    case HETLBPSEL:
    case HETPINDIS:
    case HETDIN:
        break;
    case HETDIR:
        s->gpio.dir = val;
        hercules_n2het_update_gpios(s);
        break;
    case HETDOUT:
        s->gpio.dout = val;
        hercules_n2het_update_gpios(s);
        break;
    case HETDSET:
        s->gpio.dout |= val;
        hercules_n2het_update_gpios(s);
        break;
    case HETDCLR:
        s->gpio.dout &= ~val;
        hercules_n2het_update_gpios(s);
        break;
    case HETLBPDIR:
        s->hetlbpdir = val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_gio_gio_ops = {
    .read = hercules_gio_gio_read,
    .write = hercules_gio_gio_write,
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

static const MemoryRegionOps hercules_gio_regs_ops = {
    .read = hercules_gio_reg_read,
    .write = hercules_gio_reg_write,
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

static void hercules_gio_realize(DeviceState *dev, Error **errp)
{
    HerculesGioState *s = HERCULES_GIO(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->io.regs, OBJECT(dev), &hercules_gio_regs_ops,
                          s, TYPE_HERCULES_GIO ".io.regs",
                          HERCULES_GIO_REGS_SIZE);
    memory_region_init_io(&s->io.gioa, OBJECT(dev), &hercules_gio_gio_ops,
                          &s->gpio[0], TYPE_HERCULES_GIO ".io.gioa",
                          HERCULES_GIO_GIO_SIZE);
    memory_region_init_io(&s->io.giob, OBJECT(dev), &hercules_gio_gio_ops,
                          &s->gpio[1], TYPE_HERCULES_GIO ".io.giob",
                          HERCULES_GIO_GIO_SIZE);

    memory_region_init(&s->io.container, OBJECT(dev),
                       TYPE_HERCULES_GIO ".io",
                       HERCULES_GIO_REGS_SIZE +
                       HERCULES_GIO_GIO_SIZE +
                       HERCULES_GIO_GIO_SIZE);

    memory_region_add_subregion(&s->io.container, HERCULES_GIO_REGS_OFFSET,
                                &s->io.regs);
    memory_region_add_subregion(&s->io.container, HERCULES_GIO_GIOA_OFFSET,
                                &s->io.gioa);
    memory_region_add_subregion(&s->io.container, HERCULES_GIO_GIOB_OFFSET,
                                &s->io.giob);

    sysbus_init_mmio(sbd, &s->io.container);
}

static void hercules_gio_reset(DeviceState *d)
{
    HerculesGioState *s = HERCULES_GIO(d);

    memset(s->gpio, 0, sizeof(s->gpio));

    s->gpio[0].bank = 0;
    s->gpio[1].bank = 1;
}

static void hercules_gio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_gio_reset;
    dc->realize = hercules_gio_realize;
}

static const MemoryRegionOps hercules_n2het_ops = {
    .read = hercules_n2het_read,
    .write = hercules_n2het_write,
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

static void hercules_n2het_initfn(Object *obj)
{
    HerculesN2HetState *s = HERCULES_N2HET(obj);

    sysbus_init_child_obj(obj, "n2het-ram", &s->ram,
                          sizeof(UnimplementedDeviceState),
                          TYPE_UNIMPLEMENTED_DEVICE);
}

static void hercules_n2het_realize(DeviceState *dev, Error **errp)
{
    HerculesN2HetState *s = HERCULES_N2HET(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_n2het_ops, s,
                          TYPE_HERCULES_N2HET ".io", HERCULES_N2HET_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    qdev_prop_set_string(DEVICE(&s->ram), "name", "n2het-ram");
    qdev_prop_set_uint64(DEVICE(&s->ram), "size", HERCULES_N2HET_RAM_SIZE);
    object_property_set_bool(OBJECT(&s->ram), true, "realized",
                             &error_fatal);

    sysbus_init_mmio(sbd, sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->ram), 0));
}

static void hercules_n2het_reset(DeviceState *d)
{
    HerculesN2HetState *s = HERCULES_N2HET(d);

    s->hetlbpdir = 0x00050000;

    uint32_t bank = s->gpio.bank;
    memset(&s->gpio, 0, sizeof(s->gpio));
    s->gpio.bank = bank;
}

static void hercules_n2het_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_n2het_reset;
    dc->realize = hercules_n2het_realize;
}

static const TypeInfo hercules_gio_info = {
    .name          = TYPE_HERCULES_GIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesGioState),
    .class_init    = hercules_gio_class_init,
};

static const TypeInfo hercules_n2het_info = {
    .name          = TYPE_HERCULES_N2HET,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesN2HetState),
    .instance_init = hercules_n2het_initfn,
    .class_init    = hercules_n2het_class_init,
};

static void hercules_gpio_register_types(void)
{
    type_register_static(&hercules_gio_info);
    type_register_static(&hercules_n2het_info);
}

type_init(hercules_gpio_register_types)
