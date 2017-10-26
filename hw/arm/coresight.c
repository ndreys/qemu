/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * CoreSight block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/arm/coresight.h"
#include "qemu/log.h"

static uint64_t coresight_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    return 0;
}

static void coresight_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
}

static const struct MemoryRegionOps coresight_ops = {
    .read = coresight_read,
    .write = coresight_write,
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

static void a7mpcore_dap_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    A7MPCoreDAPState *s = A7MPCORE_DAP(obj);

    memory_region_init(&s->container, obj, "a7mpcore-dap-container", 0x100000);
    sysbus_init_mmio(sd, &s->container);

    memory_region_init_io(&s->ca7_atb_funnel,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".ca7-atb-funnel",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x41000, &s->ca7_atb_funnel);

    memory_region_init_io(&s->cpu0_etm,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".cpu0-etm",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x7C000, &s->cpu0_etm);

    memory_region_init_io(&s->atb_funnel,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".atb-funnel",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x83000, &s->atb_funnel);

    memory_region_init_io(&s->tmc_etb,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".tmc-etb",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x84000, &s->tmc_etb);

    memory_region_init_io(&s->tmc_etr,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".tmc-etr",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x86000, &s->tmc_etr);

    memory_region_init_io(&s->tpiu,
                          obj,
                          &coresight_ops,
                          s,
                          TYPE_A7MPCORE_DAP ".tpiu",
                          0x1000);
    memory_region_add_subregion(&s->container, 0x87000, &s->tpiu);
}

static void a7mpcore_dap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "A7MPCore DAP Module";
}

static const TypeInfo a7mpcore_dap_info = {
    .name          = TYPE_A7MPCORE_DAP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(A7MPCoreDAPState),
    .instance_init = a7mpcore_dap_init,
    .class_init    = a7mpcore_dap_class_init,
};

static void coresight_register_type(void)
{
    type_register_static(&a7mpcore_dap_info);
}
type_init(coresight_register_type)
