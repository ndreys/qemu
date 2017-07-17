/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 SoC definitions
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * Based on hw/arm/fsl-imx6.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/fsl-imx7.h"
#include "sysemu/sysemu.h"
#include "qemu/error-report.h"

#define NAME_SIZE 20

#define for_each_cpu(i)		for (i = 0; i < smp_cpus; i++)

struct IMX7IPBlock;
typedef struct IMX7IPBlock IMX7IPBlock;

struct IMX7IPBlock {
    unsigned int index;
    void *instance;
    size_t size;
    const char *name;
    const char *type;

    hwaddr addr;
    int irq;
    void (*prepare) (const IMX7IPBlock *, Error **);
    void (*connect_irq) (const IMX7IPBlock *, FslIMX7State *);
};

#define IMX7_IP_BLOCK_MPCORE(s)                     \
    {                                               \
        .instance    = &s->a7mpcore,                \
        .size        = sizeof(s->a7mpcore),         \
        .name        = "a7mpcore",                  \
        .type        = TYPE_A15MPCORE_PRIV,         \
        .addr        = FSL_IMX7_A7MPCORE_ADDR,      \
        .irq         = -1,                          \
        .prepare     = fsl_imx7_mpcore_prepare,     \
        .connect_irq = fsl_imx7_mpcore_connect_irq, \
    }

#define IMX7_IP_BLOCK_CCM(s)                    \
    {                                           \
        .instance = &s->ccm,                    \
        .size     = sizeof(s->ccm),             \
        .name     = "ccm",                      \
        .type     = TYPE_IMX7_CCM,              \
        .addr     = FSL_IMX7_CCM_ADDR,          \
    }

#define IMX7_IP_BLOCK_UART(s, i)                     \
    {                                                \
        .index       = i - 1,                        \
        .instance    = &s->uart[i - 1],              \
        .size        = sizeof(s->uart[i - 1]),       \
        .name        = "uart" #i,                    \
        .type        = TYPE_IMX_SERIAL,              \
        .addr        = FSL_IMX7_UART##i##_ADDR,      \
        .irq         = FSL_IMX7_UART##i##_IRQ,       \
        .prepare     = fsl_imx7_uart_prepare,        \
        .connect_irq = fsl_imx7_generic_connect_irq, \
    }

#define IMX7_IP_BLOCK_ETH(s, i)                      \
    {                                                \
        .index       = i - 1,                        \
        .instance    = &s->eth[i - 1],               \
        .size        = sizeof(s->eth[i - 1]),        \
        .name        = "eth" #i,                     \
        .type        = TYPE_IMX_ENET,                \
        .addr        = FSL_IMX7_ENET##i##_ADDR,      \
        .prepare     = fsl_imx7_enet_prepare,        \
        .connect_irq = fsl_imx7_enet_connect_irq,    \
    }

#define IMX7_IP_BLOCK_USDHC(s, i)                    \
    {                                                \
        .index       = i - 1,                        \
        .instance    = &s->usdhc[i - 1],             \
        .size        = sizeof(s->usdhc[i - 1]),      \
        .name        = "usdhc" #i,                   \
        .type        = TYPE_IMX_USDHC,               \
        .addr        = FSL_IMX7_USDHC##i##_ADDR,     \
        .irq         = FSL_IMX7_USDHC##i##_IRQ,      \
        .connect_irq = fsl_imx7_generic_connect_irq, \
    }

#define IMX7_IP_BLOCK_SNVS(s)                   \
    {                                           \
        .instance = &s->snvs,                   \
        .size     = sizeof(s->snvs),            \
        .name     = "snvs",                     \
        .type     = TYPE_IMX7_SNVS,             \
        .addr     = FSL_IMX7_SNVS_ADDR,         \
    }

#define IMX7_SOC_BLOCKS(s)          \
    IMX7_IP_BLOCK_MPCORE(s),        \
                                    \
    IMX7_IP_BLOCK_CCM(s),           \
                                    \
    IMX7_IP_BLOCK_SNVS(s),          \
                                    \
    IMX7_IP_BLOCK_UART(s, 1),       \
    IMX7_IP_BLOCK_UART(s, 2),       \
    IMX7_IP_BLOCK_UART(s, 3),       \
    IMX7_IP_BLOCK_UART(s, 4),       \
    IMX7_IP_BLOCK_UART(s, 5),       \
    IMX7_IP_BLOCK_UART(s, 6),       \
    IMX7_IP_BLOCK_UART(s, 7),       \
                                    \
    IMX7_IP_BLOCK_ETH(s, 1),        \
    IMX7_IP_BLOCK_ETH(s, 2),        \
                                    \
    IMX7_IP_BLOCK_USDHC(s, 1),      \
    IMX7_IP_BLOCK_USDHC(s, 2),      \
    IMX7_IP_BLOCK_USDHC(s, 3),

static void fsl_imx7_generic_connect_irq(const IMX7IPBlock *b, FslIMX7State *s)
{
    sysbus_connect_irq(SYS_BUS_DEVICE(b->instance), 0,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore), b->irq));
}

static void fsl_imx7_uart_prepare(const IMX7IPBlock *b,  Error **errp)
{
    if (b->index < MAX_SERIAL_PORTS) {
        Chardev *chr;
        chr = serial_hds[b->index];

        if (!chr) {
            char *label = g_strdup_printf("imx7.uart%d", b->index + 1);
            chr = qemu_chr_new(label, "null");
            g_free(label);
            serial_hds[b->index] = chr;
        }

        qdev_prop_set_chr(DEVICE(b->instance), "chardev", chr);
    }
}

static void fsl_imx7_mpcore_prepare(const IMX7IPBlock *block, Error **errp)
{
    Error *err = NULL;
    Object *o = OBJECT(block->instance);

    object_property_set_int(o, smp_cpus, "num-cpu", &err);
    if (!err) {
        object_property_set_int(o, FSL_IMX7_MAX_IRQ + GIC_INTERNAL,
                                "num-irq", &err);
    }

    error_propagate(errp, err);
}

static void fsl_imx7_mpcore_connect_irq(const IMX7IPBlock *b, FslIMX7State *s)
{
    unsigned int i;

    for_each_cpu(i) {
        SysBusDevice *sbd = SYS_BUS_DEVICE(&s->a7mpcore);
        DeviceState  *d   = DEVICE(&s->cpu[i]);

        sysbus_connect_irq(sbd, i, qdev_get_gpio_in(d, ARM_CPU_IRQ));
        sysbus_connect_irq(sbd, i + smp_cpus, qdev_get_gpio_in(d, ARM_CPU_FIQ));
    }
}

static void fsl_imx7_enet_prepare(const IMX7IPBlock *b, Error **errp)
{
    qdev_set_nic_properties(DEVICE(b->instance), &nd_table[b->index]);
}

static void fsl_imx7_enet_connect_irq(const IMX7IPBlock *b, FslIMX7State *s)
{
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth[b->index]), 0,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                        FSL_IMX7_ENET_IRQ(b->index, 0)));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth[b->index]), 1,
                       qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                        FSL_IMX7_ENET_IRQ(b->index, 3)));
}

static void fsl_imx7_init(Object *obj)
{
    BusState *sysbus = sysbus_get_default();
    FslIMX7State *s = FSL_IMX7(obj);
    char name[NAME_SIZE];
    int i;

    const IMX7IPBlock *b, blocks[] = { IMX7_SOC_BLOCKS(s) };

    if (smp_cpus > FSL_IMX7_NUM_CPUS) {
        error_report("%s: Only %d CPUs are supported (%d requested)",
                     TYPE_FSL_IMX7, FSL_IMX7_NUM_CPUS, smp_cpus);
        exit(1);
    }

    for_each_cpu(i) {
        object_initialize(&s->cpu[i], sizeof(s->cpu[i]),
                          "cortex-a7-" TYPE_ARM_CPU);
        snprintf(name, NAME_SIZE, "cpu%d", i);
        object_property_add_child(obj, name, OBJECT(&s->cpu[i]),
                                  &error_fatal);
    }

    for (i = 0; i < ARRAY_SIZE(blocks); i++) {
        b = &blocks[i];

        object_initialize(b->instance, b->size, b->type);
        qdev_set_parent_bus(DEVICE(b->instance), sysbus);
        object_property_add_child(obj, b->name,
                                  OBJECT(b->instance), &error_fatal);
    }
}

static void fsl_imx7_realize(DeviceState *dev, Error **errp)
{
    FslIMX7State *s = FSL_IMX7(dev);
    Error *err = NULL;
    Object *o;
    int i;

    const IMX7IPBlock *b, blocks[] = { IMX7_SOC_BLOCKS(s) };

    for_each_cpu(i) {
	o = OBJECT(&s->cpu[i]);
        /* On uniprocessor, the CBAR is set to 0 */
        if (smp_cpus > 1) {
            object_property_set_int(o, FSL_IMX7_A7MPCORE_ADDR,
                                    "reset-cbar", &err);
            if (err) {
                goto error;
            }
        }

        /* All CPU but CPU 0 start in power off mode */
        if (i) {
            object_property_set_bool(o, true,
                                     "start-powered-off", &err);
            if (err) {
                goto error;
            }
        }

        object_property_set_bool(o, true, "realized", &err);
        if (err) {
            goto error;
        }
    }

    for (i = 0; i < ARRAY_SIZE(blocks); i++) {
        b = &blocks[i];

        if (b->prepare) {
            b->prepare(b, &err);
            if (err) {
                goto error;
            }
        }

        object_property_set_bool(OBJECT(b->instance), true,
                                 "realized", &err);
        if (err) {
            goto error;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(b->instance), 0, b->addr);

        if (b->connect_irq) {
            b->connect_irq(b, s);
        }
    }

 error:
    error_propagate(errp, err);
}

static void fsl_imx7_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fsl_imx7_realize;

    /*
     * Reason: creates an ARM CPU, thus use after free(), see
     * arm_cpu_class_init()
     */
    dc->user_creatable = false;
    dc->desc = "i.MX7 SOC";
}

static const TypeInfo fsl_imx7_type_info = {
    .name = TYPE_FSL_IMX7,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(FslIMX7State),
    .instance_init = fsl_imx7_init,
    .class_init = fsl_imx7_class_init,
};

static void fsl_imx7_register_types(void)
{
    type_register_static(&fsl_imx7_type_info);
}
type_init(fsl_imx7_register_types)
