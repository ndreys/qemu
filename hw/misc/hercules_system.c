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
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "hw/arm/hercules.h"
#include "hw/misc/hercules_system.h"

#define NAME_SIZE 20

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesSysRegisters {
    CSDIS        = 0x30,
    CSDISSET     = 0x34,
    CSDISCLR     = 0x38,
    GHVSRC       = 0x48,
    CSVSTAT      = 0x54,
    MSTGCR       = 0x58,
    MINITGCR     = 0x5C,
    MSINENA      = 0x60,
    MSTCGSTAT    = 0x68,
    MSTDONE      = BIT(0),
    MINIDONE     = BIT(8),
    MINISTAT     = 0x6C,
    PLLCTL1      = 0x70,
    ROS          = BIT(31),
    SSIR1        = 0xB0,
    SSIR2        = 0xB4,
    SSIR3        = 0xB8,
    SSIR4        = 0xBC,
    SYSECR       = 0xE0,
    SYSESR       = 0xE4,
    PORST        = BIT(15),
    DBGRST       = BIT(11),
    ICRST        = BIT(7),
    CPURST       = BIT(5),
    SWRST        = BIT(4),
    EXTRST       = BIT(3),
    GLBSTAT      = 0xEC,
    RFSLIP       = BIT(8),
    SSIVEC       = 0xF4,
};

#define SYSECR_RESET(v)    extract32(v, 14, 2)

enum HerculesSys2Registers {
    PLLCTL3      = 0x00,
};

static uint64_t hercules_sys_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesSystemState *s = opaque;

    switch (offset) {
    case CSDIS:
    case CSDISSET:
        return s->csdis;
    case GHVSRC:
        return s->ghvsrc;
    case CSVSTAT:
        return (~s->csdis) & 0xff;
    case MSTGCR:
        return s->mstgcr;
    case MINITGCR:
        return s->minitgcr;
    case MSINENA:
        return s->msinena;
    case MINISTAT:
        return s->ministat;
    case PLLCTL1:
        return s->pllctl1;
    case MSTCGSTAT:
        return s->mstcgstat;
    case SSIR1:
    case SSIR2:
    case SSIR3:
    case SSIR4:
        return 0;
    case SYSESR:
        return s->sysesr;
    case GLBSTAT:
        return s->glbstat;
    case SSIVEC:
        qemu_irq_lower(s->irq);
        return 0;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_sys_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesSystemState *s = opaque;
    const uint32_t val = val64;
    unsigned int reset;

    switch (offset) {
    case CSDIS:
        s->csdis = val;
        break;
    case CSDISSET:
        s->csdis |= val;
        break;
    case CSDISCLR:
        s->csdis &= ~val;
        break;
    case GHVSRC:
        s->ghvsrc = val;
        break;
    case MSTGCR:
        s->mstgcr = val;
        break;
    case MINITGCR:
        s->minitgcr = val;
        break;
    case MSINENA:
        s->msinena = val;
        if (val & 0x1 && s->minitgcr & 0xA) {
            s->ministat = 0x100;
        }
        break;
    case MINISTAT:
        s->ministat &= ~val;
        break;
    case PLLCTL1:
        s->pllctl1 = val;

        if (!s->ghvsrc && !(s->pllctl1 & ROS)) {
            s->glbstat |= RFSLIP;
            qemu_irq_raise(s->pll1_slip_error);
        }
        break;
    case MSTCGSTAT:
        s->mstcgstat &= ~val;
        break;
    case SSIR1:
    case SSIR2:
    case SSIR3:
    case SSIR4:
        /*
         * TODO: We might want to emulate the fact that writes to this
         * register are actually keyed
         */
        qemu_irq_raise(s->irq);
        break;
    case SYSECR:
        reset = SYSECR_RESET(val);
        if (reset != 0x1) {
            s->sysesr |= SWRST;
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    case SYSESR:
        s->sysesr &= ~val;
        break;
    case GLBSTAT:
        s->glbstat &= ~val;
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static uint64_t hercules_sys2_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesSystemState *s = opaque;

    switch (offset) {
    case PLLCTL3:
        return s->pllctl3;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_sys2_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesSystemState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case PLLCTL3:
        s->pllctl3 = val;

        if (!s->ghvsrc) {
            s->glbstat |= RFSLIP;
            qemu_irq_raise(s->pll2_slip_error);
        }
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_system_set_signal(void *opaque, int sig, int level)
{
    HerculesSystemState *s = opaque;

    CPUState *cpu = qemu_get_cpu(0);

    switch (sig) {
    case HERCULES_SYSTEM_ICRST:
        s->sysesr |= ICRST;
        cpu_reset(cpu);
        break;
    case HERCULES_SYSTEM_CPURST:
        s->sysesr |= CPURST;
        cpu_reset(cpu);
        break;
    case HERCULES_SYSTEM_MSTDONE:
        s->mstcgstat |= MSTDONE;
        break;
    }
}

static void hercules_system_initfn(Object *obj)
{
    HerculesSystemState *s = HERCULES_SYSTEM(obj);

    int i;

    for (i = 0; i < HERCULES_SYSTEM_NUM_PCRS; i++) {
        sysbus_init_child_obj(obj, "pcr[*]", &s->pcr[i],
                              sizeof(UnimplementedDeviceState),
                              TYPE_UNIMPLEMENTED_DEVICE);
    }

    s->sysesr |= PORST;
}

static void hercules_system_realize(DeviceState *dev, Error **errp)
{
    HerculesSystemState *s = HERCULES_SYSTEM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    char name[NAME_SIZE];
    DeviceState *d;
    int i;

    static MemoryRegionOps hercules_system_sys_ops = {
        .read = hercules_sys_read,
        .write = hercules_sys_write,
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

    static MemoryRegionOps hercules_system_sys2_ops = {
        .read = hercules_sys2_read,
        .write = hercules_sys2_write,
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
        hercules_system_sys_ops.endianness = DEVICE_BIG_ENDIAN;
        hercules_system_sys2_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->sys, obj, &hercules_system_sys_ops,
                          s, TYPE_HERCULES_SYSTEM ".io.sys",
                          HERCULES_SYSTEM_SYS_SIZE);
    sysbus_init_mmio(sbd, &s->sys);

    memory_region_init_io(&s->sys2, obj, &hercules_system_sys2_ops,
                          s, TYPE_HERCULES_SYSTEM ".io.sys2",
                          HERCULES_SYSTEM_SYS2_SIZE);
    sysbus_init_mmio(sbd, &s->sys2);

    for (i = 0; i < HERCULES_SYSTEM_NUM_PCRS; i++) {
        d = DEVICE(&s->pcr[i]);
        snprintf(name, NAME_SIZE, "pcr%d", i);
        qdev_prop_set_string(d, "name", name);
        qdev_prop_set_uint64(d, "size", HERCULES_SYSTEM_PCR_SIZE);
        object_property_set_bool(OBJECT(d), true, "realized", &error_fatal);

        sysbus_init_mmio(sbd, sysbus_mmio_get_region(SYS_BUS_DEVICE(d), 0));
    }

    sysbus_init_irq(sbd, &s->irq);

    qdev_init_gpio_in(dev, hercules_system_set_signal,
                      HERCULES_SYSTEM_NUM_SIGNALS);

    sysbus_init_irq(sbd, &s->pll1_slip_error);
    sysbus_init_irq(sbd, &s->pll2_slip_error);
}

static void hercules_system_reset(DeviceState *d)
{
    HerculesSystemState *s = HERCULES_SYSTEM(d);

    /*
     * If this wasn't a SW reset or POR reset, set DBGRST for now
     */
    if (s->sysesr == 0) {
        s->sysesr |= DBGRST;
    }

    s->minitgcr = 0x5;
    s->msinena  = 0;
    s->ministat = 0;
    s->csdis = 0b11001110;
    s->mstcgstat = MINIDONE;
    s->mstgcr = 0x5;

    qemu_irq_lower(s->irq);
}

static void hercules_system_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_system_reset;
    dc->realize = hercules_system_realize;
}

static const TypeInfo hercules_system_info = {
    .name          = TYPE_HERCULES_SYSTEM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesSystemState),
    .instance_init = hercules_system_initfn,
    .class_init    = hercules_system_class_init,
};

static void hercules_system_register_types(void)
{
    type_register_static(&hercules_system_info);
}

type_init(hercules_system_register_types)
