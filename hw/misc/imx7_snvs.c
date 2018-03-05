/*
 * IMX7 Secure Non-Volatile Storage
 *
 * Copyright (c) 2018, Impinj, Inc.
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * Bare minimum emulation code needed to support being able to shut
 * down linux guest gracefully.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/misc/imx7_snvs.h"
#include "qemu/log.h"
#include "sysemu/sysemu.h"
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"

static uint64_t imx7_snvs_read(void *opaque, hwaddr offset, unsigned size)
{
    IMX7SNVSState *s = IMX7_SNVS(opaque);
    uint32_t value = 0xdeadbeef;

    switch (offset) {
    case SNVS_LPGPR0: /* FALLTHROUGH */
    case SNVS_LPGPR1: /* FALLTHROUGH */
    case SNVS_LPGPR2: /* FALLTHROUGH */
    case SNVS_LPGPR3:
        if (s->lpgpr) {
            blk_pread(s->lpgpr, offset - SNVS_LPGPR0, &value, sizeof(value));
        }
        return value;
    }

    return 0;
}

static void imx7_snvs_write(void *opaque, hwaddr offset,
                            uint64_t v, unsigned size)
{
    IMX7SNVSState *s = IMX7_SNVS(opaque);
    const uint32_t value = v;

    switch (offset) {
    case SNVS_LPCR: {
        const uint32_t mask  = SNVS_LPCR_TOP | SNVS_LPCR_DP_EN;

        if ((value & mask) == mask) {
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        }
        break;
    }
    case SNVS_LPGPR0: /* FALLTHROUGH */
    case SNVS_LPGPR1: /* FALLTHROUGH */
    case SNVS_LPGPR2: /* FALLTHROUGH */
    case SNVS_LPGPR3:
        if (s->lpgpr) {
            blk_pwrite(s->lpgpr, offset - SNVS_LPGPR0, &value,
                       sizeof(value), 0);
        }
        break;
    default:
        break;
    }
}

static const struct MemoryRegionOps imx7_snvs_ops = {
    .read = imx7_snvs_read,
    .write = imx7_snvs_write,
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

static void imx7_snvs_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX7SNVSState *s = IMX7_SNVS(obj);

    memory_region_init_io(&s->mmio, obj, &imx7_snvs_ops, s,
                          TYPE_IMX7_SNVS, 0x1000);

    sysbus_init_mmio(sd, &s->mmio);

    s->lpgpr = blk_by_name("snvs-lpgpr");
    if (s->lpgpr) {
        if (blk_getlength(s->lpgpr) < SNVS_LPGPR_NUM * sizeof(uint32_t)) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: BlockBackend is too small. Ignoring it.\n",
                          __func__);
            s->lpgpr = NULL;
            return;
        }

        blk_set_perm(s->lpgpr, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                     BLK_PERM_ALL, &error_fatal);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: No BlockBackend provided to store LPGPR state\n",
                      __func__);
    }
}

static void imx7_snvs_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc  = "i.MX7 Secure Non-Volatile Storage Module";
}

static const TypeInfo imx7_snvs_info = {
    .name          = TYPE_IMX7_SNVS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX7SNVSState),
    .instance_init = imx7_snvs_init,
    .class_init    = imx7_snvs_class_init,
};

static void imx7_snvs_register_type(void)
{
    type_register_static(&imx7_snvs_info);
}
type_init(imx7_snvs_register_type)
