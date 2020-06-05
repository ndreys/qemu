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

#include "hw/misc/hercules_l2fmc.h"
#include "hw/arm/hercules.h"

enum {
    HERCULES_L2FMC_SIZE       = 4 * 1024,
    HERCULES_EPC_SIZE         = 1024,
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesL2FMCRegisters {
    FRDCNTL         = 0x0000,
    EE_FEDACCTRL1   = 0x0008,
    FEDAC_PASTATUS  = 0x0014,
    FEDAC_PBSTATUS  = 0x0018,
    ADD_PAR_ERR     = BIT(10),
    ADD_TAG_ERR     = BIT(11),
    FEDAC_GBLSTATUS = 0x001C,
    FEDACSDIS       = 0x0024,
    FPRIM_ADD_TAG   = 0x0028,
    FDUP_ADD_TAG    = 0x002C,
    FBPROT          = 0x0030,
    FBSE            = 0x0034,
    FBBUSY          = 0x0038,
    FBAC            = 0x003C,
    FBPWRMODE       = 0x0040,
    FBPRDY          = 0x0044,
    FPAC1           = 0x0048,
    FMAC            = 0x0050,
    FMSTAT          = 0x0054,
    FEMU_DMSW       = 0x0058,
    FEMU_DLSW       = 0x005C,
    FEMU_ECC        = 0x0060,
    FLOCK           = 0x0064,
    FDIAGCTRL       = 0x006C,
    DIAG_TRIG       = BIT(24),
    FRAW_ADDR       = 0x0074,
    FPAR_OVR        = 0x007C,
    RCR_VALID       = 0x00B4,
    ACC_THRESHOLD   = 0x00B8,
    FEDACSDIS2      = 0x00C0,
    RCR_VALUE0      = 0x00D0,
    RCR_VALUE1      = 0x00D4,
    FSM_WR_ENA      = 0x0288,
    EEPROM_CONFIG   = 0x02B8,
    FSM_SECTOR1     = 0x02C0,
    FSM_SECTOR2     = 0x02C4,
    FCFG_BANK       = 0x02B8,
};

#define DIAG_EN_KEY(w)          extract32(w, 16, 4)
#define DIAG_BUF_SEL(w)         extract32(w, 8, 3)
#define DIAGMODE(w)             extract32(w, 0, 3)
#define DIAGMODE_ADDR           0x5
#define DIAGMODE_ECC            0x7

enum HerculesEPCRegisters {
    EPCREVnID     = 0x0000,
    EPCCNTRL     = 0x0004,
    UERRSTAT     = 0x0008,
    EPCERRSTAT   = 0x000C,
    FIFOFULLSTAT = 0x0010,
    OVRFLWSTAT   = 0x0014,
    CAMAVAILSTAT = 0x0018,
};

#define UERRADDR(n)    (0x0020 + (n) * sizeof(uint32_t))
#define CAM_CONTENT(n) (0x00A0 + (n) * sizeof(uint32_t))
#define CAM_INDEX(n)   (0x0200 + (n) * sizeof(uint32_t))

static uint64_t hercules_epc_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesL2FMCState *s = opaque;

    switch (offset) {
    case CAMAVAILSTAT:
        if (s->camavailstat) {
            return ctzl(s->camavailstat) + 1;
        }
        break;
    case CAM_CONTENT(0) ... CAM_CONTENT(31):
        return s->cam_content[(offset - CAM_CONTENT(0)) / sizeof(uint32_t)];
    case CAM_INDEX(0) ... CAM_INDEX(7):
        return s->cam_index[(offset - CAM_INDEX(0)) / sizeof(uint32_t)];
    }

    return 0;
}

static void hercules_epc_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesL2FMCState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case CAMAVAILSTAT:
        break;
    case CAM_CONTENT(0) ... CAM_CONTENT(31):
        s->cam_content[(offset - CAM_CONTENT(0)) / sizeof(uint32_t)] = val;
        break;
    case CAM_INDEX(0) ... CAM_INDEX(7):
        s->cam_index[(offset - CAM_INDEX(0)) / sizeof(uint32_t)] = val;
        s->camavailstat = 0;
        break;
    }
}

static uint64_t hercules_l2fmc_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    HerculesL2FMCState *s = opaque;

    switch (offset) {
    case FDIAGCTRL:
        return s->fdiagctrl;
    case FRAW_ADDR:
        return s->fraw_addr;
    case FPRIM_ADD_TAG:
        return s->fprim_add_tag;
    case FDUP_ADD_TAG:
        return s->fdup_add_tag;
    case FEDAC_PASTATUS:
        return s->fedac_pastatus;
    case FEDAC_PBSTATUS:
        return s->fedac_pbstatus;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_l2fmc_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesL2FMCState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case FDIAGCTRL:
        s->fdiagctrl = val;

        if (s->fdiagctrl & DIAG_TRIG &&
            DIAG_EN_KEY(s->fdiagctrl) == 0x5) {
            uint32_t err_bit;
            qemu_irq error;

            if (DIAGMODE(s->fdiagctrl) == DIAGMODE_ADDR) {
                err_bit = ADD_TAG_ERR;
                error   = s->uncorrectable_error;
            } else {
                err_bit = ADD_PAR_ERR;
                /*
                 * FIXME, we should calculate those value against
                 * reading all zeros or all Fs
                 */
                if (s->femu_ecc == 0xCE) {
                    error = s->correctable_error;
                    s->camavailstat |= BIT(0);
                    s->cam_content[0] = s->ecc_1bit_address;
                } else {
                    error = s->bus_error;
                }
            }

            switch (DIAG_BUF_SEL(s->fdiagctrl)) {
            case 2:
            case 3:
                return;
            case 0:
            case 1:
                s->fedac_pastatus |= err_bit;
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                s->fedac_pbstatus |= err_bit;
                break;
            }

            s->fdiagctrl &= ~DIAG_TRIG;
            qemu_irq_raise(error);
        }
        break;
    case FRAW_ADDR:
        s->fraw_addr = val;
        break;
    case FPRIM_ADD_TAG:
        s->fprim_add_tag = val;
        break;
    case FDUP_ADD_TAG:
        s->fdup_add_tag = val;
        break;
    case FEDAC_PASTATUS:
        s->fedac_pastatus &= ~val;
        break;
    case FEDAC_PBSTATUS:
        s->fedac_pbstatus &= ~val;
        break;
    case FEMU_ECC:
        s->femu_ecc = val;
        break;
    case FEMU_DMSW:
    case FEMU_DLSW:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_l2fmc_realize(DeviceState *dev, Error **errp)
{
    HerculesL2FMCState *s = HERCULES_L2FMC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);

    static MemoryRegionOps hercules_epc_ops = {
        .read = hercules_epc_read,
        .write = hercules_epc_write,
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

    static MemoryRegionOps hercules_l2fmc_ops = {
        .read = hercules_l2fmc_read,
        .write = hercules_l2fmc_write,
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
        hercules_epc_ops.endianness = DEVICE_BIG_ENDIAN;
        hercules_l2fmc_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->iomem, obj, &hercules_l2fmc_ops,
                          s, TYPE_HERCULES_L2FMC ".io", HERCULES_L2FMC_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->uncorrectable_error);
    sysbus_init_irq(sbd, &s->bus_error);
    sysbus_init_irq(sbd, &s->correctable_error);

    /*
     * Technically EPC is a separate IP block, but our only use-case
     * for it involves flash controller so dealing with it here
     * simplifies things
     */
    memory_region_init_io(&s->epc, obj, &hercules_epc_ops,
                          s, TYPE_HERCULES_L2FMC ".epc", HERCULES_EPC_SIZE);
    sysbus_init_mmio(sbd, &s->epc);

    s->ecc_1bit_address = 0x00000008;
    s->ecc_1bit_femu_ecc = 0xCE;
}

static void hercules_l2fmc_reset(DeviceState *d)
{
    HerculesL2FMCState *s = HERCULES_L2FMC(d);

    s->fdiagctrl      = 0;
    s->fraw_addr      = 0;
    s->fprim_add_tag  = 0;
    s->fdup_add_tag   = 0;
    s->fedac_pastatus = 0;
    s->fedac_pbstatus = 0;
    s->femu_ecc       = 0;
}

static void hercules_l2fmc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_l2fmc_reset;
    dc->realize = hercules_l2fmc_realize;
}

static const TypeInfo hercules_l2fmc_info = {
    .name          = TYPE_HERCULES_L2FMC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesL2FMCState),
    .class_init    = hercules_l2fmc_class_init,
};

static void hercules_l2fmc_register_types(void)
{
    type_register_static(&hercules_l2fmc_info);
}
type_init(hercules_l2fmc_register_types)
