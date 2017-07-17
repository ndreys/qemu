/*
 * IMX7 Clock Control Module
 *
 * Copyright (c) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
n *
 * To get the timer frequencies right, we need to emulate at least part of
 * the CCM.
 */

#include "qemu/osdep.h"
#include "qemu/sizes.h"
#include "hw/misc/imx7_snvs.h"
#include "qemu/log.h"
#include "sysemu/sysemu.h"

static uint64_t imx7_snvs_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void imx7_snvs_write(void *opaque, hwaddr offset,
                            uint64_t v, unsigned size)
{
    const uint32_t value = v;
    const uint32_t mask  = SNVS_LPCR_TOP | SNVS_LPCR_DP_EN;

    if (offset == SNVS_LPCR && ((value & mask) == mask)) {
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
    }
}

static const struct MemoryRegionOps imx7_snvs_ops = {
    .read = imx7_snvs_read,
    .write = imx7_snvs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
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

    memory_region_init_io(&s->mmio,
                          obj,
                          &imx7_snvs_ops,
                          s,
                          TYPE_IMX7_SNVS,
                          0x1000);

    sysbus_init_mmio(sd, &s->mmio);
}

/* static const VMStateDescription vmstate_imx7_ccm = { */
/*     .name = TYPE_IMX7_CCM, */
/*     .version_id = 1, */
/*     .minimum_version_id = 1, */
/*     .fields = (VMStateField[]) { */
/*         VMSTATE_UINT32_ARRAY(ccm, IMX7CCMState, CCM_MAX), */
/*         VMSTATE_UINT32_ARRAY(analog, IMX7CCMState, CCM_ANALOG_MAX), */
/*         VMSTATE_END_OF_LIST() */
/*     }, */
/* }; */

static void imx7_snvs_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    /* dc->reset = imx7_ccm_reset; */
    /* dc->vmsd  = &vmstate_imx7_ccm; */
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
