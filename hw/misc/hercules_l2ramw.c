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
#include "qemu/main-loop.h"
#include "qapi/error.h"

#include "hw/misc/hercules_l2ramw.h"

enum {
    HERCULES_L2RAMW_CONTAINER_SIZE = 8 * 1024 * 1024,
    HERCULES_L2RAMW_SRAM_SIZE      = 512 * 1024,
    HERCULES_L2RAMW_SRAM_OFFSET    = 0,
    HERCULES_L2RAMW_ECC_SIZE       = HERCULES_L2RAMW_SRAM_SIZE,
    HERCULES_L2RAMW_ECC_OFFSET     = HERCULES_L2RAMW_CONTAINER_SIZE / 2,
    HERCULES_L2RAMW_SIZE           = 256,
};

enum HerculesL2RAMWRegisters {
    RAMCTRL            = 0x0000,
    RAMERRSTATUS       = 0x0010,
    DRDE               = BIT(22),
    DRSE               = BIT(21),
    DWDE               = BIT(20),
    DWSE               = BIT(19),
    ADDE               = BIT(4),
    ADE                = BIT(2),
    DIAG_DATA_VECTOR_H = 0x0024,
    DIAG_DATA_VECTOR_L = 0x0028,
    DIAG_ECC           = 0x002C,
    RAMTEST            = 0x0030,
    TRIGGER            = BIT(8),
    RAMADDRDEC_VECT    = 0x0038,
    MEMINIT_DOMAIN     = 0x003C,
    BANK_DOMAIN_MAP0   = 0x0044,
    BANK_DOMAIN_MAP1   = 0x0048,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

#define TEST_ENABLE(w)    extract32(w, 0, 4)
#define TEST_MODE(w)      extract32(w, 6, 2)

static void hercules_l2ramw_write(void *opaque, hwaddr offset, uint64_t val64,
                                  unsigned size)
{
    HerculesL2RamwState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case RAMCTRL:
        s->ramctrl = val;
        break;
    case RAMTEST:
        s->ramtest = val;
        if (s->ramtest & TRIGGER) {
            s->ramtest &= ~TRIGGER;

            switch (s->diag_ecc) {
            case 0x03:
                s->ramerrstatus |= DRDE | DWDE;
                qemu_irq_raise(s->uncorrectable_error);
                break;
            case 0xCE:
                s->ramerrstatus |= DRSE | DWSE;
                qemu_irq_raise(s->uncorrectable_error);
                break;
            default:
                break;
            }
        }
        break;
    case RAMERRSTATUS:
        s->ramerrstatus &= ~val;
        break;
    case DIAG_ECC:
        s->diag_ecc = val;
        break;
    case DIAG_DATA_VECTOR_H:
    case DIAG_DATA_VECTOR_L:
    case RAMADDRDEC_VECT:
    case MEMINIT_DOMAIN:
    case BANK_DOMAIN_MAP0:
    case BANK_DOMAIN_MAP1:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static uint64_t hercules_l2ramw_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    HerculesL2RamwState *s = opaque;

    switch (offset) {
    case RAMCTRL:
        return s->ramctrl;
    case RAMTEST:
        return s->ramtest;
    case RAMERRSTATUS:
        return s->ramerrstatus;
    case DIAG_ECC:
        return s->diag_ecc;
    case DIAG_DATA_VECTOR_H:
    case DIAG_DATA_VECTOR_L:
    case RAMADDRDEC_VECT:
    case MEMINIT_DOMAIN:
    case BANK_DOMAIN_MAP0:
    case BANK_DOMAIN_MAP1:
        break;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static const MemoryRegionOps hercules_l2ramw_ops = {
    .read       = hercules_l2ramw_read,
    .write      = hercules_l2ramw_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void hercules_l2ramw_ecc_write(void *opaque, hwaddr offset,
                                      uint64_t val, unsigned size)
{
    qemu_log_bad_offset(offset);
}

static uint64_t hercules_l2ramw_ecc_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    qemu_log_bad_offset(offset);
    return 0;
}

static const MemoryRegionOps hercules_l2ramw_ecc_ops = {
    .read       = hercules_l2ramw_ecc_read,
    .write      = hercules_l2ramw_ecc_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void hercules_l2ramw_realize(DeviceState *dev, Error **errp)
{
    HerculesL2RamwState *s = HERCULES_L2RAMW(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->io.ecc, OBJECT(dev), &hercules_l2ramw_ecc_ops, s,
                          TYPE_HERCULES_L2RAMW ".io.ecc",
                          HERCULES_L2RAMW_ECC_SIZE);

    memory_region_init_ram(&s->io.sram, OBJECT(dev),
                           TYPE_HERCULES_L2RAMW ".io.sram",
                           HERCULES_L2RAMW_SRAM_SIZE, &error_fatal);

    memory_region_init(&s->io.container, OBJECT(dev),
                       TYPE_HERCULES_L2RAMW ".io",
                       HERCULES_L2RAMW_CONTAINER_SIZE);

    memory_region_add_subregion(&s->io.container, HERCULES_L2RAMW_SRAM_OFFSET,
                                &s->io.sram);
    memory_region_add_subregion(&s->io.container, HERCULES_L2RAMW_ECC_OFFSET,
                                &s->io.ecc);

    sysbus_init_mmio(sbd, &s->io.container);

    memory_region_init_io(&s->io.regs, OBJECT(dev), &hercules_l2ramw_ops, s,
                          TYPE_HERCULES_L2RAMW ".io.regs",
                          HERCULES_L2RAMW_SIZE);

    sysbus_init_mmio(sbd, &s->io.regs);
    sysbus_init_irq(sbd, &s->uncorrectable_error);
}

static void hercules_l2ramw_reset(DeviceState *dev)
{
    HerculesL2RamwState *s = HERCULES_L2RAMW(dev);

    s->ramctrl      = 0;
    s->ramtest      = 0;
    s->ramerrstatus = 0;
    s->diag_ecc     = 0;
}

static void hercules_l2ramw_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_l2ramw_reset;
    dc->realize = hercules_l2ramw_realize;

    dc->desc = "Hercules Level II RAM Module";
}

static const TypeInfo hercules_l2ramw_info = {
    .name          = TYPE_HERCULES_L2RAMW,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesL2RamwState),
    .class_init    = hercules_l2ramw_class_init,
};

static void hercules_l2ramw_register_types(void)
{
    type_register_static(&hercules_l2ramw_info);
}

type_init(hercules_l2ramw_register_types)
