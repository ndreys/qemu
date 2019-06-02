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

#include "hw/misc/hercules_scm.h"

enum {
    HERCULES_SCM_SIZE         = 256,
    HERCULES_SDR_MMR_SIZE     = 16 * 1024 * 1024,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesSCMRegisters {
    SCMCNTRL = 0x04,
};

#define DTC_SOFT_RESET(w)    extract32(w, 8, 4)

enum HerculesSDCRegisters {
    SDC_STATUS = 0x00,
    NT_OK = BIT(3),
    PT_OK = BIT(1),
};

static void hercules_scm_self_test(void *opaque)
{
    HerculesSCMState *s = opaque;
    CPUState *cpu = qemu_get_cpu(0);

    if (cpu->halted) {
        s->sdc_status |= NT_OK | PT_OK;
        qemu_irq_raise(s->icrst);
        return;
    }

    qemu_bh_schedule(s->self_test);
}

static uint64_t hercules_scm_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesSCMState *s = opaque;

    switch (offset) {
    case SCMCNTRL:
        return s->scmcntrl;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_scm_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesSCMState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case SCMCNTRL:
        if (DTC_SOFT_RESET(val) == 0xA) {
            qemu_bh_schedule(s->self_test);
        }
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static uint64_t hercules_sdr_mmr_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    HerculesSCMState *s = opaque;

    switch (offset) {
    case SDC_STATUS:
        return s->sdc_status;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_sdr_mmr_write(void *opaque, hwaddr offset,
                                   uint64_t val64, unsigned size)
{
    switch (offset) {
    default:
        qemu_log_bad_offset(offset);
    }
}

static const MemoryRegionOps hercules_scm_ops = {
    .read = hercules_scm_read,
    .write = hercules_scm_write,
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

static const MemoryRegionOps hercules_sdr_mmr_ops = {
    .read = hercules_sdr_mmr_read,
    .write = hercules_sdr_mmr_write,
    /*
     * This is not BE on TMS570 as per Device#51 errata
     */
    .endianness = DEVICE_NATIVE_ENDIAN,
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

static void hercules_scm_realize(DeviceState *dev, Error **errp)
{
    HerculesSCMState *s = HERCULES_SCM(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->io.scm, OBJECT(dev), &hercules_scm_ops,
                          s, TYPE_HERCULES_SCM ".io.scm", HERCULES_SCM_SIZE);
    sysbus_init_mmio(sbd, &s->io.scm);

    memory_region_init_io(&s->io.sdr_mmr, OBJECT(dev), &hercules_sdr_mmr_ops,
                          s, TYPE_HERCULES_SCM ".io.sdr-mmr",
                          HERCULES_SDR_MMR_SIZE);
    sysbus_init_mmio(sbd, &s->io.sdr_mmr);

    s->self_test = qemu_bh_new(hercules_scm_self_test, s);

    sysbus_init_irq(sbd, &s->icrst);
}

static void hercules_scm_reset(DeviceState *d)
{
    HerculesSCMState *s = HERCULES_SCM(d);

    s->scmcntrl = 0x05050505;

    /*
     * s->sdc_status is left alone on purpose
     */

    qemu_bh_cancel(s->self_test);
}

static void hercules_scm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_scm_reset;
    dc->realize = hercules_scm_realize;
}

static const TypeInfo hercules_scm_info = {
    .name          = TYPE_HERCULES_SCM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesSCMState),
    .class_init    = hercules_scm_class_init,
};

static void hercules_scm_register_types(void)
{
    type_register_static(&hercules_scm_info);
}

type_init(hercules_scm_register_types)
