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
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

static void fsl_imx7_init(Object *obj)
{
    BusState *sysbus = sysbus_get_default();
    FslIMX7State *s = FSL_IMX7(obj);
    char name[NAME_SIZE];
    int i;

    if (smp_cpus > FSL_IMX7_NUM_CPUS) {
        error_report("%s: Only %d CPUs are supported (%d requested)",
                     TYPE_FSL_IMX7, FSL_IMX7_NUM_CPUS, smp_cpus);
        exit(1);
    }

    for (i = 0; i < smp_cpus; i++) {
        object_initialize(&s->cpu[i], sizeof(s->cpu[i]),
                          "cortex-a7-" TYPE_ARM_CPU);
        snprintf(name, NAME_SIZE, "cpu%d", i);
        object_property_add_child(obj, name, OBJECT(&s->cpu[i]),
                                  &error_fatal);
    }

    /*
     * A7MPCORE
     */
    object_initialize(&s->a7mpcore, sizeof(s->a7mpcore), TYPE_A15MPCORE_PRIV);
    qdev_set_parent_bus(DEVICE(&s->a7mpcore), sysbus);
    object_property_add_child(obj, "a7mpcore",
                              OBJECT(&s->a7mpcore), &error_fatal);

    object_initialize(&s->dap, sizeof(s->dap), TYPE_A7MPCORE_DAP);
    qdev_set_parent_bus(DEVICE(&s->dap), sysbus);
    object_property_add_child(obj, "a7mpcore-dap",
                              OBJECT(&s->dap), &error_fatal);

    /*
     * GPIOs 1 to 7
     */
    for (i = 0; i < FSL_IMX7_NUM_GPIOS; i++) {
        object_initialize(&s->gpio[i], sizeof(s->gpio[i]),
                          TYPE_IMX_GPIO);
        qdev_set_parent_bus(DEVICE(&s->gpio[i]), sysbus);
        snprintf(name, NAME_SIZE, "gpio%d", i);
        object_property_add_child(obj, name,
                                  OBJECT(&s->gpio[i]), &error_fatal);
    }

    /*
     * IOMUXC and IOMUXC_LPSR
     */
    for (i = 0; i < FSL_IMX7_NUM_IOMUXCS; i++) {
        object_initialize(&s->iomuxc[i], sizeof(s->iomuxc[i]),
                          TYPE_IMX7_IOMUXC);
        qdev_set_parent_bus(DEVICE(&s->iomuxc[i]), sysbus);
        snprintf(name, NAME_SIZE, "iomuxc%d", i);
        object_property_add_child(obj, name,
                                  OBJECT(&s->iomuxc[i]), &error_fatal);
    }

    /*
     * GPT1, 2, 3, 4
     */
    for (i = 0; i < FSL_IMX7_NUM_GPTS; i++) {
        object_initialize(&s->gpt[i], sizeof(s->gpt[i]), TYPE_IMX7_GPT);
        qdev_set_parent_bus(DEVICE(&s->gpt[i]), sysbus);
        snprintf(name, NAME_SIZE, "gpt%d", i);
        object_property_add_child(obj, name, OBJECT(&s->gpt[i]),
                                  &error_fatal);
    }

    /*
     * CCM
     */
    object_initialize(&s->ccm, sizeof(s->ccm), TYPE_IMX7_CCM);
    qdev_set_parent_bus(DEVICE(&s->ccm), sysbus);
    object_property_add_child(obj, "ccm", OBJECT(&s->ccm), &error_fatal);

    /*
     * GPCv2
     */
    object_initialize(&s->gpcv2, sizeof(s->gpcv2), TYPE_IMX_GPCV2);
    qdev_set_parent_bus(DEVICE(&s->gpcv2), sysbus);
    object_property_add_child(obj, "gpcv2", OBJECT(&s->gpcv2), &error_fatal);

    for (i = 0; i < FSL_IMX7_NUM_ECSPIS; i++) {
        object_initialize(&s->spi[i], sizeof(s->spi[i]), TYPE_IMX_SPI);
        qdev_set_parent_bus(DEVICE(&s->spi[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "spi%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->spi[i]), NULL);
    }


    for (i = 0; i < FSL_IMX7_NUM_I2CS; i++) {
        object_initialize(&s->i2c[i], sizeof(s->i2c[i]), TYPE_IMX_I2C);
        qdev_set_parent_bus(DEVICE(&s->i2c[i]), sysbus_get_default());
        snprintf(name, NAME_SIZE, "i2c%d", i + 1);
        object_property_add_child(obj, name, OBJECT(&s->i2c[i]), NULL);
    }

    /*
     * UART
     */
    for (i = 0; i < FSL_IMX7_NUM_UARTS; i++) {
            object_initialize(&s->uart[i], sizeof(s->uart[i]), TYPE_IMX_SERIAL);
            qdev_set_parent_bus(DEVICE(&s->uart[i]), sysbus);
            snprintf(name, NAME_SIZE, "uart%d", i);
            object_property_add_child(obj, name, OBJECT(&s->uart[i]),
                                      &error_fatal);
    }

    /*
     * FlexCAN1 and 2
     */

    for (i = 0; i < FSL_IMX7_NUM_FLEXCANS; i++) {
            object_initialize(&s->flexcan[i],
                              sizeof(s->flexcan[i]), TYPE_IMX_FLEXCAN);
            qdev_set_parent_bus(DEVICE(&s->flexcan[i]), sysbus);
            snprintf(name, NAME_SIZE, "flexcan%d", i);
            object_property_add_child(obj, name, OBJECT(&s->flexcan[i]),
                                      &error_fatal);
    }

    /*
     * Ethernet
     */
    for (i = 0; i < FSL_IMX7_NUM_ETHS; i++) {
            object_initialize(&s->eth[i], sizeof(s->eth[i]), TYPE_IMX_ENET);
            qdev_set_parent_bus(DEVICE(&s->eth[i]), sysbus);
            snprintf(name, NAME_SIZE, "eth%d", i);
            object_property_add_child(obj, name, OBJECT(&s->eth[i]),
                                      &error_fatal);
    }

    /*
     * SDHCI
     */
    for (i = 0; i < FSL_IMX7_NUM_USDHCS; i++) {
            object_initialize(&s->usdhc[i], sizeof(s->usdhc[i]),
                              TYPE_IMX_USDHC);
            qdev_set_parent_bus(DEVICE(&s->usdhc[i]), sysbus);
            snprintf(name, NAME_SIZE, "usdhc%d", i);
            object_property_add_child(obj, name, OBJECT(&s->usdhc[i]),
                                      &error_fatal);
    }

    /*
     * SNVS
     */
    object_initialize(&s->snvs, sizeof(s->snvs), TYPE_IMX7_SNVS);
    qdev_set_parent_bus(DEVICE(&s->snvs), sysbus);
    object_property_add_child(obj, "snvs", OBJECT(&s->snvs), &error_fatal);

    /*
     * SRC
     */
    object_initialize(&s->src, sizeof(s->src), TYPE_IMX7_SRC);
    qdev_set_parent_bus(DEVICE(&s->src), sysbus);
    object_property_add_child(obj, "src", OBJECT(&s->src), &error_fatal);

    /*
     * Watchdog
     */
    for (i = 0; i < FSL_IMX7_NUM_WDTS; i++) {
            object_initialize(&s->wdt[i], sizeof(s->wdt[i]), TYPE_IMX2_WDT);
            qdev_set_parent_bus(DEVICE(&s->wdt[i]), sysbus);
            snprintf(name, NAME_SIZE, "wdt%d", i);
            object_property_add_child(obj, name, OBJECT(&s->wdt[i]),
                                      &error_fatal);
    }

    /*
     * SDMA
     */
    object_initialize(&s->sdma, sizeof(s->sdma), TYPE_IMX_SDMA);
    qdev_set_parent_bus(DEVICE(&s->sdma), sysbus);
    object_property_add_child(obj, "sdma", OBJECT(&s->sdma), &error_fatal);

    /*
     * GPR
     */
    object_initialize(&s->gpr, sizeof(s->gpr), TYPE_IMX7_GPR);
    qdev_set_parent_bus(DEVICE(&s->gpr), sysbus);
    object_property_add_child(obj, "gpr", OBJECT(&s->gpr), &error_fatal);

    object_initialize(&s->pcie, sizeof(s->pcie), TYPE_DESIGNWARE_PCIE_HOST);
    qdev_set_parent_bus(DEVICE(&s->pcie), sysbus);
    object_property_add_child(obj, "pcie", OBJECT(&s->pcie), &error_fatal);

    for (i = 0; i < FSL_IMX7_NUM_USBS; i++) {
        object_initialize(&s->usb[i],
                          sizeof(s->usb[i]), TYPE_CHIPIDEA);
        qdev_set_parent_bus(DEVICE(&s->usb[i]), sysbus);
        snprintf(name, NAME_SIZE, "usb%d", i);
        object_property_add_child(obj, name,
                                  OBJECT(&s->usb[i]), &error_fatal);

        object_initialize(&s->usbmisc[i],
                          sizeof(s->usbmisc[i]), TYPE_IMX_USBMISC);
        qdev_set_parent_bus(DEVICE(&s->usbmisc[i]), sysbus);
        snprintf(name, NAME_SIZE, "usbmisc%d", i);
        object_property_add_child(obj, name,
                                  OBJECT(&s->usbmisc[i]), &error_fatal);
    }

    for (i = 0; i < FSL_IMX7_NUM_ADCS; i++) {
        object_initialize(&s->adc[i], sizeof(s->adc[i]), TYPE_IMX7_ADC);
        qdev_set_parent_bus(DEVICE(&s->adc[i]), sysbus);
        snprintf(name, NAME_SIZE, "adc%d", i);
        object_property_add_child(obj, name,
                                  OBJECT(&s->adc[i]), &error_fatal);
    }

    /*
     * LCD
     */
    object_initialize(&s->lcdif, sizeof(s->lcdif), TYPE_IMX_LCDIF);
    qdev_set_parent_bus(DEVICE(&s->lcdif), sysbus);
    object_property_add_child(obj, "lcdif", OBJECT(&s->lcdif), &error_fatal);

}

static void fsl_imx7_realize(DeviceState *dev, Error **errp)
{
    FslIMX7State *s = FSL_IMX7(dev);
    Object *o;
    int i;
    qemu_irq irq;

    for (i = 0; i < smp_cpus; i++) {
        o = OBJECT(&s->cpu[i]);

        object_property_set_int(o, QEMU_PSCI_CONDUIT_SMC,
                                "psci-conduit", &error_abort);

        /* On uniprocessor, the CBAR is set to 0 */
        if (smp_cpus > 1) {
            object_property_set_int(o, FSL_IMX7_A7MPCORE_ADDR,
                                    "reset-cbar", &error_abort);
        }

        if (i) {
            /* Secondary CPUs start in PSCI powered-down state */
            object_property_set_bool(o, true,
                                     "start-powered-off", &error_abort);
        }

        object_property_set_bool(o, true, "realized", &error_abort);
    }

    /*
     * A7MPCORE
     */
    object_property_set_int(OBJECT(&s->a7mpcore), smp_cpus, "num-cpu",
                            &error_abort);
    object_property_set_int(OBJECT(&s->a7mpcore),
                            FSL_IMX7_MAX_IRQ + GIC_INTERNAL,
                            "num-irq", &error_abort);

    object_property_set_bool(OBJECT(&s->a7mpcore), true, "realized",
                             &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->a7mpcore), 0, FSL_IMX7_A7MPCORE_ADDR);

    for (i = 0; i < smp_cpus; i++) {
        SysBusDevice *sbd = SYS_BUS_DEVICE(&s->a7mpcore);
        DeviceState  *d   = DEVICE(qemu_get_cpu(i));

        irq = qdev_get_gpio_in(d, ARM_CPU_IRQ);
        sysbus_connect_irq(sbd, i, irq);
        irq = qdev_get_gpio_in(d, ARM_CPU_FIQ);
        sysbus_connect_irq(sbd, i + smp_cpus, irq);
    }

    object_property_set_bool(OBJECT(&s->dap), true, "realized",
                             &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->dap), 0, FSL_IMX7_A7MPCORE_DAP_ADDR);

    /*
     * GPT1, 2, 3, 4
     */
    for (i = 0; i < FSL_IMX7_NUM_GPTS; i++) {
        static const hwaddr FSL_IMX7_GPTn_ADDR[FSL_IMX7_NUM_GPTS] = {
            FSL_IMX7_GPT1_ADDR,
            FSL_IMX7_GPT2_ADDR,
            FSL_IMX7_GPT3_ADDR,
            FSL_IMX7_GPT4_ADDR,
        };

        s->gpt[i].ccm = IMX_CCM(&s->ccm);
        object_property_set_bool(OBJECT(&s->gpt[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpt[i]), 0, FSL_IMX7_GPTn_ADDR[i]);
    }

    for (i = 0; i < FSL_IMX7_NUM_GPIOS; i++) {
        static const hwaddr FSL_IMX7_GPIOn_ADDR[FSL_IMX7_NUM_GPIOS] = {
            FSL_IMX7_GPIO1_ADDR,
            FSL_IMX7_GPIO2_ADDR,
            FSL_IMX7_GPIO3_ADDR,
            FSL_IMX7_GPIO4_ADDR,
            FSL_IMX7_GPIO5_ADDR,
            FSL_IMX7_GPIO6_ADDR,
            FSL_IMX7_GPIO7_ADDR,
        };

        object_property_set_bool(OBJECT(&s->gpio[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0, FSL_IMX7_GPIOn_ADDR[i]);
    }

    /*
     * IOMUXC and IOMUXC_LPSR
     */
    for (i = 0; i < FSL_IMX7_NUM_IOMUXCS; i++) {
        static const hwaddr FSL_IMX7_IOMUXCn_ADDR[FSL_IMX7_NUM_IOMUXCS] = {
            FSL_IMX7_IOMUXC_ADDR,
            FSL_IMX7_IOMUXC_LPSR_ADDR,
        };

        object_property_set_bool(OBJECT(&s->iomuxc[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->iomuxc[i]), 0,
                        FSL_IMX7_IOMUXCn_ADDR[i]);
    }

    /*
     * CCM
     */
    object_property_set_bool(OBJECT(&s->ccm), true, "realized", &error_abort);

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 0, FSL_IMX7_CCM_ADDR);

    /*
     * GPCv2
     */
    object_property_set_bool(OBJECT(&s->gpcv2), true,
                             "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpcv2), 0, FSL_IMX7_GPC_ADDR);

    /* Initialize all ECSPI */
    for (i = 0; i < FSL_IMX7_NUM_ECSPIS; i++) {
        static const hwaddr FSL_IMX7_SPIn_ADDR[FSL_IMX7_NUM_ECSPIS] = {
            FSL_IMX7_ECSPI1_ADDR,
            FSL_IMX7_ECSPI2_ADDR,
            FSL_IMX7_ECSPI3_ADDR,
            FSL_IMX7_ECSPI4_ADDR,
        };

        static const hwaddr FSL_IMX7_SPIn_IRQ[FSL_IMX7_NUM_ECSPIS] = {
            FSL_IMX7_ECSPI1_IRQ,
            FSL_IMX7_ECSPI2_IRQ,
            FSL_IMX7_ECSPI3_IRQ,
            FSL_IMX7_ECSPI4_IRQ,
        };

        /* Initialize the SPI */
        object_property_set_bool(OBJECT(&s->spi[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 0,
                        FSL_IMX7_SPIn_ADDR[i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->spi[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                            FSL_IMX7_SPIn_IRQ[i]));
    }

    for (i = 0; i < FSL_IMX7_NUM_I2CS; i++) {
        static const hwaddr FSL_IMX7_I2Cn_ADDR[FSL_IMX7_NUM_I2CS] = {
            FSL_IMX7_I2C1_ADDR,
            FSL_IMX7_I2C2_ADDR,
            FSL_IMX7_I2C3_ADDR,
            FSL_IMX7_I2C4_ADDR,
        };

        static const hwaddr FSL_IMX7_I2Cn_IRQ[FSL_IMX7_NUM_I2CS] = {
            FSL_IMX7_I2C1_IRQ,
            FSL_IMX7_I2C2_IRQ,
            FSL_IMX7_I2C3_IRQ,
            FSL_IMX7_I2C4_IRQ,
        };

        object_property_set_bool(OBJECT(&s->i2c[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c[i]), 0, FSL_IMX7_I2Cn_ADDR[i]);

        sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c[i]), 0,
                           qdev_get_gpio_in(DEVICE(&s->a7mpcore),
                                            FSL_IMX7_I2Cn_IRQ[i]));
    }

    /*
     * UART
     */
    for (i = 0; i < FSL_IMX7_NUM_UARTS; i++) {
        static const hwaddr FSL_IMX7_UARTn_ADDR[FSL_IMX7_NUM_UARTS] = {
            FSL_IMX7_UART1_ADDR,
            FSL_IMX7_UART2_ADDR,
            FSL_IMX7_UART3_ADDR,
            FSL_IMX7_UART4_ADDR,
            FSL_IMX7_UART5_ADDR,
            FSL_IMX7_UART6_ADDR,
            FSL_IMX7_UART7_ADDR,
        };

        static const int FSL_IMX7_UARTn_IRQ[FSL_IMX7_NUM_UARTS] = {
            FSL_IMX7_UART1_IRQ,
            FSL_IMX7_UART2_IRQ,
            FSL_IMX7_UART3_IRQ,
            FSL_IMX7_UART4_IRQ,
            FSL_IMX7_UART5_IRQ,
            FSL_IMX7_UART6_IRQ,
            FSL_IMX7_UART7_IRQ,
        };


        if (i < MAX_SERIAL_PORTS) {
            qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", serial_hds[i]);
        }

        object_property_set_bool(OBJECT(&s->uart[i]), true, "realized",
                                 &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, FSL_IMX7_UARTn_ADDR[i]);

        irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_UARTn_IRQ[i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0, irq);
    }

    for (i = 0; i < FSL_IMX7_NUM_FLEXCANS; i++) {
        static const hwaddr FSL_IMX7_FLEXCANn_ADDR[FSL_IMX7_NUM_FLEXCANS] = {
            FSL_IMX7_FLEXCAN1_ADDR,
            FSL_IMX7_FLEXCAN2_ADDR,
        };

        object_property_set_bool(OBJECT(&s->flexcan[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->flexcan[i]), 0,
                        FSL_IMX7_FLEXCANn_ADDR[i]);
    }

    /*
     * Ethernet
     */
    for (i = 0; i < FSL_IMX7_NUM_ETHS; i++) {
        static const hwaddr FSL_IMX7_ENETn_ADDR[FSL_IMX7_NUM_ETHS] = {
            FSL_IMX7_ENET1_ADDR,
            FSL_IMX7_ENET2_ADDR,
        };

        object_property_set_uint(OBJECT(&s->eth[i]), FSL_IMX7_ETH_NUM_TX_RINGS,
                                 "tx-ring-num", &error_abort);
        qdev_set_nic_properties(DEVICE(&s->eth[i]), &nd_table[i]);
        object_property_set_bool(OBJECT(&s->eth[i]), true, "realized",
                                 &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->eth[i]), 0, FSL_IMX7_ENETn_ADDR[i]);

        irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_ENET_IRQ(i, 0));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth[i]), 0, irq);
        irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_ENET_IRQ(i, 3));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth[i]), 1, irq);
    }

    /*
     * USDHC
     */
    for (i = 0; i < FSL_IMX7_NUM_USDHCS; i++) {
        static const hwaddr FSL_IMX7_USDHCn_ADDR[FSL_IMX7_NUM_USDHCS] = {
            FSL_IMX7_USDHC1_ADDR,
            FSL_IMX7_USDHC2_ADDR,
            FSL_IMX7_USDHC3_ADDR,
        };

        static const int FSL_IMX7_USDHCn_IRQ[FSL_IMX7_NUM_USDHCS] = {
            FSL_IMX7_USDHC1_IRQ,
            FSL_IMX7_USDHC2_IRQ,
            FSL_IMX7_USDHC3_IRQ,
        };

        object_property_set_bool(OBJECT(&s->usdhc[i]), true, "realized",
                                 &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usdhc[i]), 0,
                        FSL_IMX7_USDHCn_ADDR[i]);

        irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_USDHCn_IRQ[i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->usdhc[i]), 0, irq);
    }

    /*
     * SNVS
     */
    object_property_set_bool(OBJECT(&s->snvs), true, "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->snvs), 0, FSL_IMX7_SNVS_ADDR);

    /*
     * SRC
     */
    object_property_set_bool(OBJECT(&s->src), true, "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->src), 0, FSL_IMX7_SRC_ADDR);

    /*
     * Watchdog
     */
    for (i = 0; i < FSL_IMX7_NUM_WDTS; i++) {
        static const hwaddr FSL_IMX7_WDOGn_ADDR[FSL_IMX7_NUM_WDTS] = {
            FSL_IMX7_WDOG1_ADDR,
            FSL_IMX7_WDOG2_ADDR,
            FSL_IMX7_WDOG3_ADDR,
            FSL_IMX7_WDOG4_ADDR,
        };

        object_property_set_bool(OBJECT(&s->wdt[i]), true, "realized",
                                 &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->wdt[i]), 0, FSL_IMX7_WDOGn_ADDR[i]);
    }

    /*
     * SDMA
     */
    object_property_set_bool(OBJECT(&s->sdma), true, "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sdma), 0, FSL_IMX7_SDMA_ADDR);

    object_property_set_bool(OBJECT(&s->gpr), true, "realized",
                             &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpr), 0, FSL_IMX7_GPR_ADDR);

    object_property_set_bool(OBJECT(&s->pcie), true,
                             "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->pcie), 0, FSL_IMX7_PCIE_REG_ADDR);

    irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_PCI_INTA_IRQ);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 0, irq);
    irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_PCI_INTB_IRQ);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 1, irq);
    irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_PCI_INTC_IRQ);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 2, irq);
    irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_PCI_INTD_IRQ);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 3, irq);


    for (i = 0; i < FSL_IMX7_NUM_USBS; i++) {
        static const hwaddr FSL_IMX7_USBMISCn_ADDR[FSL_IMX7_NUM_USBS] = {
            FSL_IMX7_USBMISC1_ADDR,
            FSL_IMX7_USBMISC2_ADDR,
            FSL_IMX7_USBMISC3_ADDR,
        };

        static const hwaddr FSL_IMX7_USBn_ADDR[FSL_IMX7_NUM_USBS] = {
            FSL_IMX7_USB1_ADDR,
            FSL_IMX7_USB2_ADDR,
            FSL_IMX7_USB3_ADDR,
        };

        static const hwaddr FSL_IMX7_USBn_IRQ[FSL_IMX7_NUM_USBS] = {
            FSL_IMX7_USB1_IRQ,
            FSL_IMX7_USB2_IRQ,
            FSL_IMX7_USB3_IRQ,
        };

        object_property_set_bool(OBJECT(&s->usb[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usb[i]), 0,
                        FSL_IMX7_USBn_ADDR[i]);

        irq = qdev_get_gpio_in(DEVICE(&s->a7mpcore), FSL_IMX7_USBn_IRQ[i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->usb[i]), 0, irq);

        object_property_set_bool(OBJECT(&s->usbmisc[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usbmisc[i]), 0,
                        FSL_IMX7_USBMISCn_ADDR[i]);
    }

    for (i = 0; i < FSL_IMX7_NUM_ADCS; i++) {
        static const hwaddr FSL_IMX7_ADCn_ADDR[FSL_IMX7_NUM_ADCS] = {
            FSL_IMX7_ADC1_ADDR,
            FSL_IMX7_ADC2_ADDR,
        };

        object_property_set_bool(OBJECT(&s->adc[i]), true, "realized",
                                 &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->adc[i]), 0,
                        FSL_IMX7_ADCn_ADDR[i]);
    }

    /*
     * LCD
     */
    object_property_set_bool(OBJECT(&s->lcdif), true,
                             "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->lcdif), 0, FSL_IMX7_LCDIF_ADDR);
}

static void fsl_imx7_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fsl_imx7_realize;

    /* Reason: Uses serial_hds and nd_table in realize() directly */
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
