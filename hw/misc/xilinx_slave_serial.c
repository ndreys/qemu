/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * Code to emulate programming "port" of Xilinx FPGA in Slave Serial
 * configuration connected via SPI, for more deatils see (p. 27):
 *
 * See https://www.xilinx.com/support/documentation/user_guides/ug380.pdf
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/xilinx_slave_serial.h"
#include "qemu/log.h"

enum {
    XILINX_SLAVE_SERIAL_STATE_RESET,
    XILINX_SLAVE_SERIAL_STATE_RECONFIGURATION,
    XILINX_SLAVE_SERIAL_STATE_DONE,
};

static void xilinx_slave_serial_update_outputs(XilinxSlaveSerialState *xlnxss)
{
    qemu_set_irq(xlnxss->done,
                 xlnxss->state == XILINX_SLAVE_SERIAL_STATE_DONE);
}

static void xilinx_slave_serial_reset(DeviceState *dev)
{
    XilinxSlaveSerialState *xlnxss = XILINX_SLAVE_SERIAL(dev);

    xlnxss->state = XILINX_SLAVE_SERIAL_STATE_RESET;

    xilinx_slave_serial_update_outputs(xlnxss);
}

static void xilinx_slave_serial_prog_b(void *opaque, int n, int level)
{
    XilinxSlaveSerialState *xlnxss = XILINX_SLAVE_SERIAL(opaque);
    assert(n == 0);

    if (level) {
        xlnxss->state = XILINX_SLAVE_SERIAL_STATE_RECONFIGURATION;
    }

    xilinx_slave_serial_update_outputs(xlnxss);
}

static void xilinx_slave_serial_realize(SSISlave *ss, Error **errp)
{
    DeviceState *dev = DEVICE(ss);
    XilinxSlaveSerialState *xlnxss = XILINX_SLAVE_SERIAL(ss);

    qdev_init_gpio_in_named(dev,
                            xilinx_slave_serial_prog_b,
                            XILINX_SLAVE_SERIAL_GPIO_PROG_B,
                            1);
    qdev_init_gpio_out_named(dev, &xlnxss->done,
                             XILINX_SLAVE_SERIAL_GPIO_DONE, 1);
}

static uint32_t xilinx_slave_serial_transfer(SSISlave *ss, uint32_t tx)
{
    XilinxSlaveSerialState *xlnxss = XILINX_SLAVE_SERIAL(ss);

    if (xlnxss->state == XILINX_SLAVE_SERIAL_STATE_RECONFIGURATION) {
        xlnxss->state = XILINX_SLAVE_SERIAL_STATE_DONE;
    }

    xilinx_slave_serial_update_outputs(xlnxss);
    return 0;
}

static void xilinx_slave_serial_class_init(ObjectClass *klass, void *data)
{
    DeviceClass   *dc = DEVICE_CLASS(klass);
    SSISlaveClass *k  = SSI_SLAVE_CLASS(klass);

    dc->reset      = xilinx_slave_serial_reset;
    dc->desc       = "Xilinx Slave Serial";
    k->realize     = xilinx_slave_serial_realize;
    k->transfer    = xilinx_slave_serial_transfer;
    /*
     * Slave Serial configuration is not technically SPI and there's
     * no CS signal
     */
    k->set_cs      = NULL;
    k->cs_polarity = SSI_CS_NONE;
}

static const TypeInfo xilinx_slave_serial_info = {
    .name          = TYPE_XILINX_SLAVE_SERIAL,
    .parent        = TYPE_SSI_SLAVE,
    .instance_size = sizeof(XilinxSlaveSerialState),
    .class_init    = xilinx_slave_serial_class_init,
};

static void xilinx_slave_serial_register_type(void)
{
    type_register_static(&xilinx_slave_serial_info);
}
type_init(xilinx_slave_serial_register_type)
