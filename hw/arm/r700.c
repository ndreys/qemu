/*
 * Copyright (c) 2017, Impinj, Inc.
 *
 * R700 Board System emulation.
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This code is licensed under the GPL, version 2 or later.
 * See the file `COPYING' in the top level directory.
 *
 * It (partially) emulates a mcimx7d_sabre board, with a Freescale
 * i.MX7 SoC
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/fsl-imx7.h"
#include "hw/boards.h"
#include "hw/misc/xilinx_slave_serial.h"
#include "sysemu/sysemu.h"
#include "sysemu/device_tree.h"
#include "qemu/error-report.h"
#include "sysemu/qtest.h"
#include "net/net.h"

typedef struct {
    FslIMX7State soc;
    MemoryRegion ram;
} R700;

static void r700_add_psci_node(const struct arm_boot_info *boot_info,
                                  void *fdt)
{
    const char comp[] = "arm,psci-0.2\0arm,psci";

    qemu_fdt_add_subnode(fdt, "/psci");
    qemu_fdt_setprop(fdt, "/psci", "compatible", comp, sizeof(comp));
    qemu_fdt_setprop_string(fdt, "/psci", "method", "smc");
}

static void r700_init(MachineState *machine)
{
    static struct arm_boot_info boot_info;
    R700 *s = g_new0(R700, 1);
    qemu_irq gpio;
    DeviceState *xlnxss;
    Object *soc;
    int i;

    if (machine->ram_size > FSL_IMX7_MMDC_SIZE) {
        error_report("RAM size " RAM_ADDR_FMT " above max supported (%08x)",
                     machine->ram_size, FSL_IMX7_MMDC_SIZE);
        exit(1);
    }

    boot_info = (struct arm_boot_info) {
        .loader_start = FSL_IMX7_MMDC_ADDR,
        .board_id = -1,
        .ram_size = machine->ram_size,
        .kernel_filename = machine->kernel_filename,
        .kernel_cmdline = machine->kernel_cmdline,
        .initrd_filename = machine->initrd_filename,
        .nb_cpus = smp_cpus,
        .modify_dtb = r700_add_psci_node,
    };

    object_initialize(&s->soc, sizeof(s->soc), TYPE_FSL_IMX7);
    soc = OBJECT(&s->soc);
    object_property_add_child(OBJECT(machine), "soc", soc, &error_fatal);
    object_property_set_bool(soc, true, "realized", &error_fatal);

    memory_region_allocate_system_memory(&s->ram, NULL, "r700.ram",
                                         machine->ram_size);
    memory_region_add_subregion(get_system_memory(),
                                FSL_IMX7_MMDC_ADDR, &s->ram);

    for (i = 0; i < FSL_IMX7_NUM_USDHCS; i++) {
        BusState *bus;
        DeviceState *carddev;
        DriveInfo *di;
        BlockBackend *blk;

        di = drive_get_next(IF_SD);
        blk = di ? blk_by_legacy_dinfo(di) : NULL;
        bus = qdev_get_child_bus(DEVICE(&s->soc.usdhc[i]), "sd-bus");
        carddev = qdev_create(bus, TYPE_SD_CARD);
        qdev_prop_set_drive(carddev, "drive", blk, &error_fatal);
        object_property_set_bool(OBJECT(carddev), true,
                                 "realized", &error_fatal);
    }

    xlnxss = ssi_create_slave(s->soc.spi[1].bus, TYPE_XILINX_SLAVE_SERIAL);

    gpio = qdev_get_gpio_in_named(xlnxss, XILINX_SLAVE_SERIAL_GPIO_PROG_B, 0);
    qdev_connect_gpio_out(DEVICE(&s->soc.gpio[3]), 23, gpio);

    gpio = qdev_get_gpio_in(DEVICE(&s->soc.gpio[1]), 2);
    qdev_connect_gpio_out_named(xlnxss,
                                XILINX_SLAVE_SERIAL_GPIO_DONE, 0, gpio);

    if (!qtest_enabled()) {
        arm_load_kernel(&s->soc.cpu[0], &boot_info);
    }
}

static void r700_machine_init(MachineClass *mc)
{
    mc->desc = "";
    mc->init = r700_init;
    mc->max_cpus = FSL_IMX7_NUM_CPUS;
}
DEFINE_MACHINE("r700", r700_machine_init)
