/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "sysemu/qtest.h"
#include "hw/loader.h"
#include "elf.h"

#include "hw/arm/hercules.h"

static void tms570lc43_init(MachineState *machine)
{
    Object *dev;
    MemoryRegion *sdram = g_new(MemoryRegion, 1);
    DriveInfo *eeprom = drive_get(IF_MTD, 0, 0);
    const char *file;

    dev = object_new(TYPE_HERCULES_SOC);
    qdev_prop_set_drive(DEVICE(dev), "eeprom",
                        eeprom ? blk_by_legacy_dinfo(eeprom) : NULL,
                        &error_abort);
    object_property_set_bool(dev, true, "realized", &error_fatal);

    memory_region_init_ram(sdram, OBJECT(dev), "hercules.sdram",
                           0x00800000, &error_fatal);
    memory_region_add_subregion(get_system_memory(), HERCULES_EMIF_CS1_ADDR,
                                sdram);

    if (qtest_enabled()) {
        return;
    }

    if (machine->kernel_filename) {
        uint64_t e, l;

        file = machine->kernel_filename;
        if (load_elf(file, NULL, NULL, NULL, &e, &l, NULL, NULL,
                     1, EM_ARM, 1, 0) < 0) {
            goto exit;
        }
    } else if (machine->firmware) {
        file = machine->firmware;
        if (load_image_targphys(file, HERCULES_FLASH_ADDR,
                                HERCULES_FLASH_SIZE) < 0) {
            goto exit;
        }
    }

    return;
exit:
    error_report("Could not load '%s'", file);
    exit(1);
}

static void tms570lc43_machine_init(MachineClass *mc)
{
    mc->desc = "TMS570LC43";
    mc->init = tms570lc43_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("tms570lc43", tms570lc43_machine_init)
