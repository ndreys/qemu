/*
 * MCIMX7D_SABRE Board System emulation.
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
#include "sysemu/sysemu.h"
#include "qemu/error-report.h"
#include "sysemu/qtest.h"
#include "net/net.h"

typedef struct {
    FslIMX7State soc;
    MemoryRegion ram;
} MCIMX7Sabre;

/* No need to do any particular setup for secondary boot */
static void mcimx7d_sabre_write_secondary(ARMCPU *cpu,
                                          const struct arm_boot_info *info)
{
}

/* Secondary cores are reset through SRC device */
static void mcimx7d_sabre_reset_secondary(ARMCPU *cpu,
                                          const struct arm_boot_info *info)
{
}

static void mcimx7d_sabre_init(MachineState *machine)
{
    static struct arm_boot_info boot_info;
    MCIMX7Sabre *s = g_new0(MCIMX7Sabre, 1);
    Object *soc;
    int i;
    DeviceState *carddev;
    DriveInfo *di;
    BlockBackend *blk;

    /* Check the amount of memory is compatible with the SOC */
    if (machine->ram_size > FSL_IMX7_MMDC_SIZE) {
        error_report("RAM size " RAM_ADDR_FMT " above max supported (%08x)",
                     machine->ram_size, FSL_IMX7_MMDC_SIZE);
        exit(1);
    }

    boot_info = (struct arm_boot_info) {
        /* DDR memory start */
        .loader_start = FSL_IMX7_MMDC_ADDR,
        /* No board ID, we boot from DT tree */
        .board_id = -1,

        .ram_size = machine->ram_size,
        .kernel_filename = machine->kernel_filename,
        .kernel_cmdline = machine->kernel_cmdline,
        .initrd_filename = machine->initrd_filename,
        .nb_cpus = smp_cpus,
        .secure_boot = true,
        .write_secondary_boot = mcimx7d_sabre_write_secondary,
        .secondary_cpu_reset_hook = mcimx7d_sabre_reset_secondary,
    };

    object_initialize(&s->soc, sizeof(s->soc), TYPE_FSL_IMX7);
    soc = OBJECT(&s->soc);
    object_property_add_child(OBJECT(machine), "soc", soc, &error_fatal);
    object_property_set_bool(soc, true, "realized", &error_fatal);

    memory_region_allocate_system_memory(&s->ram, NULL, "mcimx7d-sabre.ram",
                                         machine->ram_size);
    memory_region_add_subregion(get_system_memory(),
                                FSL_IMX7_MMDC_ADDR, &s->ram);

    for (i = 0; i < FSL_IMX7_NUM_USDHCS; i++) {
        di = drive_get_next(IF_SD);

        blk = di ? blk_by_legacy_dinfo(di) : NULL;
        carddev = qdev_create(qdev_get_child_bus(DEVICE(&s->soc.usdhc[i]),
                                                 "sd-bus"), TYPE_SD_CARD);
        qdev_prop_set_drive(carddev, "drive", blk, &error_fatal);
        object_property_set_bool(OBJECT(carddev), true, "realized", &error_fatal);
    }

    if (!qtest_enabled()) {
        arm_load_kernel(&s->soc.cpu[0], &boot_info);
    }
}

static void mcimx7d_sabre_machine_init(MachineClass *mc)
{
    mc->desc = "Freescale i.MX7 DUAL SABRE (Cortex A7)";
    mc->init = mcimx7d_sabre_init;
    mc->max_cpus = FSL_IMX7_NUM_CPUS;
}
DEFINE_MACHINE("mcimx7d-sabre", mcimx7d_sabre_machine_init)
