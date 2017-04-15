/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * i.MX7 SoC definitions
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
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

#ifndef FSL_IMX7_H
#define FSL_IMX7_H

#include "hw/arm/arm.h"
#include "hw/cpu/a15mpcore.h"
#include "hw/misc/imx7_ccm.h"
#include "hw/misc/imx6_src.h"
#include "hw/char/imx_serial.h"
#include "hw/timer/imx_gpt.h"
#include "hw/timer/imx_epit.h"
#include "hw/i2c/imx_i2c.h"
#include "hw/gpio/imx_gpio.h"
#include "hw/sd/sdhci.h"
#include "hw/ssi/imx_spi.h"
#include "hw/net/imx_fec.h"
#include "exec/memory.h"
#include "cpu.h"
#include "qemu/sizes.h"

#define TYPE_FSL_IMX7 "fsl,imx7"
#define FSL_IMX7(obj) OBJECT_CHECK(FslIMX7State, (obj), TYPE_FSL_IMX7)

enum FslIMX7Configuration {
    FSL_IMX7_NUM_CPUS  = 2,
    FSL_IMX7_NUM_UARTS = 7,
    FSL_IMX7_NUM_ETHS  = 2,
};

typedef struct FslIMX7State {
    /*< private >*/
    DeviceState    parent_obj;

    /*< public >*/
    ARMCPU         cpu[FSL_IMX7_NUM_CPUS];
    A15MPPrivState a7mpcore;
    IMX7CCMState   ccm;
    IMXSerialState uart[FSL_IMX7_NUM_UARTS];
    IMXFECState    eth[FSL_IMX7_NUM_ETHS];
} FslIMX7State;

enum FslIMX7MemoryMap {
    FSL_IMX7_MMDC_ADDR      = 0x80000000,
    FSL_IMX7_MMDC_SIZE      = SZ_2G,

    FSL_IMX7_CCM_ADDR       = 0x30360000,

    FSL_IMX7_UART1_ADDR     = 0x30860000,
    FSL_IMX7_UART2_ADDR     = 0x30870000,
    FSL_IMX7_UART3_ADDR     = 0x30880000,    
    FSL_IMX7_UART4_ADDR     = 0x30A60000,
    FSL_IMX7_UART5_ADDR     = 0x30A70000,
    FSL_IMX7_UART6_ADDR     = 0x30A80000,
    FSL_IMX7_UART7_ADDR     = 0x30A90000,

    FSL_IMX7_ENET1_ADDR     = 0x30BE0000,
    FSL_IMX7_ENET2_ADDR     = 0x30BF0000,

    FSL_IMX7_A7MPCORE_ADDR  = 0x31000000,
};

enum FslIMX7IRQs {
    FSL_IMX7_UART1_IRQ = 26,
    FSL_IMX7_UART2_IRQ = 27,
    FSL_IMX7_UART3_IRQ = 28,
    FSL_IMX7_UART4_IRQ = 29,
    FSL_IMX7_UART5_IRQ = 30,
    FSL_IMX7_UART6_IRQ = 16,
    FSL_IMX7_UART7_IRQ = 126,

#define FSL_IMX7_ENET_IRQ(i, n)  ((n) + ((i) ? 100 : 118))

    FSL_IMX7_MAX_IRQ   = 128,
};

#endif /* FSL_IMX7_H */
