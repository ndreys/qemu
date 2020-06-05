/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "chardev/char-fe.h"
#include "cpu.h"
#include "elf.h"
#include "exec/address-spaces.h"
#include "hw/arm/boot.h"
#include "hw/arm/hercules.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "net/net.h"
#include "qemu/error-report.h"
#include "sysemu/reset.h"
#include "sysemu/sysemu.h"
#include "sysemu/block-backend.h"
#include "sysemu/qtest.h"


#ifdef HOST_WORDS_BIGENDIAN
#pragma message "Hercules emulation not tested on Big Endian hosts"
#endif

static const int
HERCULES_MIBSPIn_DMAREQ[HERCULES_NUM_MIBSPIS][HERCULES_SPI_NUM_DMAREQS] = {
    {
        [0]  = 1,  [1]  = 0,  [2]  = 4,  [3]  = 5,
        [4]  = 8,  [5]  = 9,  [6]  = 12, [7]  = 13,
        [8]  = 16, [9]  = 17, [10] = 22, [11] = 23,
        [12] = 26, [13] = 27, [14] = 30, [15] = 31,
    },
    {
        [0]  = 3,  [1]  = 2,  [2]  = 32, [3]  = 33,
        [4]  = 34, [5]  = 35, [6]  = 36, [7]  = 37,
        [8]  = 38, [9]  = 39, [10] = 40, [11] = 41,
        [12] = 42, [13] = 43, [14] = 44, [15] = 45,
    },
    {
        [0]  = 15, [1]  = 14, [2]  = 4,  [3]  = 5,
        [4]  = 8,  [5]  = 9,  [6]  = 12, [7]  = 13,
        [8]  = 16, [9]  = 17, [10] = 22, [11] = 23,
        [12] = 26, [13] = 27, [14] = 30, [15] = 31,
    },
    {
        [0]  = 25, [1]  = 24, [2]  = 32, [3]  = 33,
        [4]  = 34, [5]  = 35, [6]  = 36, [7]  = 37,
        [8]  = 38, [9]  = 39, [10] = 40, [11] = 41,
        [12] = 42, [13] = 43, [14] = 44, [15] = 45,
    },
    {
        [0]  = 31, [1]  = 30, [2]  = 6,  [3]  = 7,
        [4]  = 10, [5]  = 11, [6]  = 14, [7]  = 15,
        [8]  = 18, [9]  = 19, [10] = 22, [11] = 23,
        [12] = 24, [13] = 25, [14] = 28, [15] = 29,
    },
};

static void hercules_initfn(Object *obj)
{
    HerculesState *s = HERCULES_SOC(obj);
    Object *cpu_obj = OBJECT(&s->cpu);
    int i;

    object_initialize(cpu_obj, sizeof(s->cpu),
                      ARM_CPU_TYPE_NAME("cortex-r5f"));

    object_property_add_child(obj, "cpu", cpu_obj);

    sysbus_init_child_obj(obj, "l2ramw", &s->l2ramw, sizeof(s->l2ramw),
                          TYPE_HERCULES_L2RAMW);

    sysbus_init_child_obj(obj, "rtp", &s->rtp, sizeof(s->rtp),
                          TYPE_HERCULES_RTP);

    sysbus_init_child_obj(obj, "vim", &s->vim, sizeof(s->vim),
                          TYPE_HERCULES_VIM);

    sysbus_init_child_obj(obj, "system", &s->system, sizeof(s->system),
                          TYPE_HERCULES_SYSTEM);

    sysbus_init_child_obj(obj, "gio", &s->gio, sizeof(s->gio),
                          TYPE_HERCULES_GIO);

    for (i = 0; i < HERCULES_NUM_N2HETS; i++) {
        sysbus_init_child_obj(obj, "n2het[*]", &s->n2het[i],
                              sizeof(s->n2het[i]), TYPE_HERCULES_N2HET);
    }
    s->n2het[0].gpio.bank = 2;
    s->n2het[1].gpio.bank = 3;

    for (i = 0; i < HERCULES_NUM_MIBADCS; i++) {
        sysbus_init_child_obj(obj, "mibadc[*]", &s->mibadc[i],
                              sizeof(s->mibadc[i]), TYPE_HERCULES_MIBADC);
    }

    sysbus_init_child_obj(obj, "rti", &s->rti, sizeof(s->rti),
                          TYPE_HERCULES_RTI);

    sysbus_init_child_obj(obj, "emac", &s->emac, sizeof(s->emac),
                          TYPE_HERCULES_EMAC);

    sysbus_init_child_obj(obj, "dma", &s->dma, sizeof(s->dma),
                          TYPE_HERCULES_DMA);

    for (i = 0; i < HERCULES_NUM_MIBSPIS; i++) {
        sysbus_init_child_obj(obj, "mibspi[*]", &s->mibspi[i],
                              sizeof(s->mibspi), TYPE_HERCULES_SPI);
    }

    sysbus_init_child_obj(obj, "scm", &s->scm, sizeof(s->scm),
                          TYPE_HERCULES_SCM);
    sysbus_init_child_obj(obj, "esm", &s->esm, sizeof(s->esm),
                          TYPE_HERCULES_ESM);
    sysbus_init_child_obj(obj, "efuse", &s->efuse, sizeof(s->efuse),
                          TYPE_HERCULES_EFUSE);
    sysbus_init_child_obj(obj, "pmm", &s->pmm, sizeof(s->pmm),
                          TYPE_HERCULES_PMM);

    for (i = 0; i < HERCULES_NUM_ECAPS; i++) {
        sysbus_init_child_obj(obj, "ecap[*]", &s->ecap[i],
                              sizeof(s->ecap), TYPE_HERCULES_ECAP);
    }

    sysbus_init_child_obj(obj, "stc", &s->stc, sizeof(s->stc),
                          TYPE_HERCULES_STC);
    sysbus_init_child_obj(obj, "pbist", &s->pbist, sizeof(s->pbist),
                          TYPE_HERCULES_PBIST);
    sysbus_init_child_obj(obj, "ccm", &s->ccm, sizeof(s->ccm),
                          TYPE_HERCULES_CCM);
    sysbus_init_child_obj(obj, "l2fmc", &s->l2fmc, sizeof(s->l2fmc),
                          TYPE_HERCULES_L2FMC);
}

static void hercules_cpu_reset(void *opaque)
{
    ARMCPU *cpu = opaque;

    cpu_reset(CPU(cpu));
}

static void hercules_realize(DeviceState *dev, Error **errp)
{
    HerculesState *s = HERCULES_SOC(dev);
    Object *cpu_obj = OBJECT(&s->cpu);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *eeprom = g_new(MemoryRegion, 1);
    MemoryRegion *otp_bank1 = g_new(MemoryRegion, 1);
    SysBusDevice *sbd;
    DeviceState *vim, *dma, *esm;
    qemu_irq irq, error;
    int i;

    s->cpu.ctr = 0x1d192992; /* 32K icache 32K dcache */
    set_feature(&s->cpu.env, ARM_FEATURE_DUMMY_C15_REGS);

    if (s->is_tms570) {
        object_property_set_bool(cpu_obj, true, "cfgend", &error_fatal);
        object_property_set_bool(cpu_obj, true, "cfgend-instr", &error_fatal);
    }

    object_property_set_bool(OBJECT(&s->cpu), true, "realized", &error_fatal);
    qemu_register_reset(hercules_cpu_reset, ARM_CPU(&s->cpu));

    memory_region_init_rom(flash, OBJECT(dev), "hercules.flash",
                           HERCULES_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, HERCULES_FLASH_ADDR, flash);

    memory_region_init_rom(eeprom, OBJECT(dev), "hercules.eeprom",
                           HERCULES_EEPROM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, HERCULES_EEPROM_ADDR, eeprom);

    if (s->blk_eeprom) {
        int64_t size;

        size = MIN(blk_getlength(s->blk_eeprom), HERCULES_EEPROM_SIZE);
        if (size <= 0) {
            error_setg(errp, "failed to get flash size");
            return;
        }

        if (blk_pread(s->blk_eeprom, 0, memory_region_get_ram_ptr(eeprom),
                      size) < 0) {
            error_setg(errp, "failed to read EEPROM content");
            return;
        }
    }

    qdev_prop_set_chr(DEVICE(&s->rtp), "chardev", serial_hd(0));
    object_property_set_bool(OBJECT(&s->rtp), true, "realized", &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->rtp), 0, HERCULES_RTP_ADDR);

    /*
     * ARM Debug peripherals
     */
    create_unimplemented_device("debug-rom", HERCULES_DEBUG_ROM_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("debug", HERCULES_DEBUG_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("etm", HERCULES_ETM_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("tpiu", HERCULES_TPIU_ADDR ,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("pom", HERCULES_POM_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("cti1", HERCULES_CTI1_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("cti2", HERCULES_CTI2_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("cti3", HERCULES_CTI3_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("cti4", HERCULES_CTI4_ADDR,
                                HERCULES_DEBUG_SIZE);
    create_unimplemented_device("ctsf", HERCULES_CTSF_ADDR,
                                HERCULES_DEBUG_SIZE);

    /*
     * VIM
     */
    object_property_set_bool(OBJECT(&s->vim), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->vim);
    irq = qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ);
    sysbus_connect_irq(sbd, 0, irq);
    irq = qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_FIQ);
    sysbus_connect_irq(sbd, 1, irq);
    sysbus_mmio_map(sbd, 0, HERCULES_VIM_ECC_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_VIM_CONTROL_ADDR);
    sysbus_mmio_map(sbd, 2, HERCULES_VIM_RAM_ADDR);

    vim = DEVICE(&s->vim);

    object_property_set_bool(OBJECT(&s->esm), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->esm);
    sysbus_mmio_map(sbd, 0, HERCULES_ESM_ADDR);
    irq = qdev_get_gpio_in(vim, HERCULES_ESM_HIGH_LEVEL_IRQ);
    sysbus_connect_irq(sbd, 0, irq);
    irq = qdev_get_gpio_in(vim, HERCULES_ESM_LOW_LEVEL_IRQ);
    sysbus_connect_irq(sbd, 1, irq);

    esm = DEVICE(&s->esm);

    object_property_set_bool(OBJECT(&s->l2ramw), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->l2ramw);
    sysbus_mmio_map(sbd, 0, HERCULES_RAM_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_L2RAMW_ADDR);
    error = qdev_get_gpio_in(esm, HERCULES_L2RAMW_TYPE_B_UNCORRECTABLE_ERROR);
    sysbus_connect_irq(sbd, 0, error);

    object_property_set_bool(OBJECT(&s->system), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->system);
    sysbus_mmio_map(sbd, 0, HERCULES_SYS_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_SYS2_ADDR);
    sysbus_mmio_map(sbd, 2, HERCULES_PCR_ADDR);
    sysbus_mmio_map(sbd, 3, HERCULES_PCR2_ADDR);
    sysbus_mmio_map(sbd, 4, HERCULES_PCR3_ADDR);
    sysbus_connect_irq(sbd, 0, qdev_get_gpio_in(vim, HERCULES_SSI_IRQ));
    error = qdev_get_gpio_in(esm, HERCULES_PLL1_SLIP_ERROR);
    sysbus_connect_irq(sbd, 1, error);
    error = qdev_get_gpio_in(esm, HERCULES_PLL2_SLIP_ERROR);
    sysbus_connect_irq(sbd, 2, error);

    create_unimplemented_device("pinmux", HERCULES_PINMUX_ADDR,
                                HERCULES_PINMUX_SIZE);

    object_property_set_bool(OBJECT(&s->l2fmc), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->l2fmc);
    sysbus_mmio_map(sbd, 0, HERCULES_L2FMC_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_EPC_ADDR);
    error = qdev_get_gpio_in(esm, HERCULES_L2FMC_UNCORRECTABLE_ERROR);
    sysbus_connect_irq(sbd, 0, error);
    error = qdev_get_gpio_in(esm, HERCULES_CR5F_FATAL_BUS_ERROR);
    sysbus_connect_irq(sbd, 1, error);
    error = qdev_get_gpio_in(esm, HERCULES_EPC_CORRECTABLE_ERROR);
    sysbus_connect_irq(sbd, 2, error);

    memory_region_init_rom(otp_bank1, OBJECT(dev),
                           TYPE_HERCULES_SOC ".otp.bank1",
                           HERCULES_OTP_BANK1_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory,
                                HERCULES_OTP_BANK1_ADDR, otp_bank1);

    create_unimplemented_device("emif", HERCULES_EMIF_ADDR,
                                HERCULES_EMIF_SIZE);


    object_property_set_bool(OBJECT(&s->gio), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->gio);
    sysbus_mmio_map(sbd, 0, HERCULES_GIO_ADDR);

    for (i = 0; i < HERCULES_NUM_N2HETS; i++) {
        static const hwaddr HERCULES_N2HETn_ADDR[HERCULES_NUM_N2HETS] = {
            HERCULES_N2HET1_ADDR,
            HERCULES_N2HET2_ADDR,
        };

        static const hwaddr HERCULES_N2HETn_RAM_ADDR[HERCULES_NUM_N2HETS] = {
            HERCULES_N2HET1_RAM_ADDR,
            HERCULES_N2HET2_RAM_ADDR,
        };

        object_property_set_bool(OBJECT(&s->n2het[i]), true, "realized",
                                 &error_abort);
        sbd = SYS_BUS_DEVICE(&s->n2het[i]);
        sysbus_mmio_map(sbd, 0, HERCULES_N2HETn_ADDR[i]);
        sysbus_mmio_map(sbd, 1, HERCULES_N2HETn_RAM_ADDR[i]);
    }

    create_unimplemented_device("lin1", HERCULES_LIN1_ADDR,
                                HERCULES_LIN1_SIZE);

    for (i = 0; i < HERCULES_NUM_MIBADCS; i++) {
        static const hwaddr HERCULES_MIBADCn_ADDR[HERCULES_NUM_MIBADCS] = {
            HERCULES_MIBADC1_ADDR,
            HERCULES_MIBADC2_ADDR,
        };
        static const hwaddr HERCULES_MIBADCn_RAM_ADDR[HERCULES_NUM_MIBADCS] = {
            HERCULES_MIBADC1_RAM_ADDR,
            HERCULES_MIBADC2_RAM_ADDR,
        };
        static const int
            HERCULES_MIBADCn_PARITY_ERRROR[HERCULES_NUM_MIBADCS] = {
            HERCULES_MIBADC1_PARITY_ERROR,
            HERCULES_MIBADC2_PARITY_ERROR,
        };

        object_property_set_bool(OBJECT(&s->mibadc[i]), true, "realized",
                                 &error_abort);
        sbd = SYS_BUS_DEVICE(&s->mibadc[i]);
        sysbus_mmio_map(sbd, 0, HERCULES_MIBADCn_ADDR[i]);
        sysbus_mmio_map(sbd, 1, HERCULES_MIBADCn_RAM_ADDR[i]);
        error = qdev_get_gpio_in(esm, HERCULES_MIBADCn_PARITY_ERRROR[i]);
        sysbus_connect_irq(sbd, 0, error);
    }

    /*
     * RTI
     */
    object_property_set_bool(OBJECT(&s->rti), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->rti);

    irq = qdev_get_gpio_in(vim, HERCULES_RTI_COMPARE0_IRQ);
    sysbus_connect_irq(sbd, HERCULES_RTI_INT_COMPARE0, irq);
    irq = qdev_get_gpio_in(vim, HERCULES_RTI_COMPARE1_IRQ);
    sysbus_connect_irq(sbd, HERCULES_RTI_INT_COMPARE1, irq);
    irq = qdev_get_gpio_in(vim, HERCULES_RTI_COMPARE2_IRQ);
    sysbus_connect_irq(sbd, HERCULES_RTI_INT_COMPARE2, irq);
    irq = qdev_get_gpio_in(vim, HERCULES_RTI_COMPARE3_IRQ);
    sysbus_connect_irq(sbd, HERCULES_RTI_INT_COMPARE3, irq);

    sysbus_mmio_map(sbd, 0, HERCULES_RTI_ADDR);

    qdev_set_nic_properties(DEVICE(&s->emac), &nd_table[0]);
    object_property_set_bool(OBJECT(&s->emac), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->emac);
    sysbus_mmio_map(sbd, 0, HERCULES_EMAC_MODULE_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_EMAC_CTRL_ADDR);
    sysbus_mmio_map(sbd, 2, HERCULES_EMAC_MDIO_ADDR);
    sysbus_mmio_map(sbd, 3, HERCULES_EMAC_CPPI_ADDR);

    create_unimplemented_device("nmpu", HERCULES_NMPU_ADDR,
                                HERCULES_NMPU_SIZE);

    create_unimplemented_device("dcc1", HERCULES_DCC1_ADDR,
                                HERCULES_DCC1_SIZE);

    object_property_set_bool(OBJECT(&s->dma), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->dma);
    sysbus_mmio_map(sbd, 0, HERCULES_DMA_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_DMA_RAM_ADDR);

    dma = DEVICE(&s->dma);

    for (i = 0; i < HERCULES_NUM_MIBSPIS; i++) {
        static const hwaddr
            HERCULES_MIBSPIn_RAM_ADDR[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_RAM_ADDR,
            HERCULES_MIBSPI2_RAM_ADDR,
            HERCULES_MIBSPI3_RAM_ADDR,
            HERCULES_MIBSPI4_RAM_ADDR,
            HERCULES_MIBSPI5_RAM_ADDR,
        };
        static const hwaddr
            HERCULES_MIBSPIn_CTRL_ADDR[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_CTRL_ADDR,
            HERCULES_MIBSPI2_CTRL_ADDR,
            HERCULES_MIBSPI3_CTRL_ADDR,
            HERCULES_MIBSPI4_CTRL_ADDR,
            HERCULES_MIBSPI5_CTRL_ADDR,
        };
        static const int HERCULES_MIBSPIn_L0_IRQ[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_L0_IRQ,
            HERCULES_MIBSPI2_L0_IRQ,
            HERCULES_MIBSPI3_L0_IRQ,
            HERCULES_MIBSPI4_L0_IRQ,
            HERCULES_MIBSPI5_L0_IRQ,
        };
        static const int HERCULES_MIBSPIn_L1_IRQ[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_L1_IRQ,
            HERCULES_MIBSPI2_L1_IRQ,
            HERCULES_MIBSPI3_L1_IRQ,
            HERCULES_MIBSPI4_L1_IRQ,
            HERCULES_MIBSPI5_L1_IRQ,
        };
        static const int
            HERCULES_MIBSPIn_SINGLE_BIT_ERROR[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_SINGLE_BIT_ERROR,
            HERCULES_MIBSPI2_SINGLE_BIT_ERROR,
            HERCULES_MIBSPI3_SINGLE_BIT_ERROR,
            HERCULES_MIBSPI4_SINGLE_BIT_ERROR,
            HERCULES_MIBSPI5_SINGLE_BIT_ERROR,
        };
        static const int
            HERCULES_MIBSPIn_UNCORRECTABLE_ERROR[HERCULES_NUM_MIBSPIS] = {
            HERCULES_MIBSPI1_UNCORRECTABLE_ERROR,
            HERCULES_MIBSPI2_UNCORRECTABLE_ERROR,
            HERCULES_MIBSPI3_UNCORRECTABLE_ERROR,
            HERCULES_MIBSPI4_UNCORRECTABLE_ERROR,
            HERCULES_MIBSPI5_UNCORRECTABLE_ERROR,
        };
        int irq_nr = 0;
        int j;

        object_property_set_bool(OBJECT(&s->mibspi[i]), true, "realized",
                                 &error_abort);
        sbd = SYS_BUS_DEVICE(&s->mibspi[i]);
        sysbus_mmio_map(sbd, 0, HERCULES_MIBSPIn_CTRL_ADDR[i]);
        sysbus_mmio_map(sbd, 1, HERCULES_MIBSPIn_RAM_ADDR[i]);
        irq = qdev_get_gpio_in(vim, HERCULES_MIBSPIn_L0_IRQ[i]);
        sysbus_connect_irq(sbd, irq_nr++, irq);
        irq = qdev_get_gpio_in(vim, HERCULES_MIBSPIn_L1_IRQ[i]);
        sysbus_connect_irq(sbd, irq_nr++, irq);

        irq_nr += HERCULES_SPI_NUM_CS_LINES;

        for (j = 0; j < HERCULES_SPI_NUM_DMAREQS; j++) {
            irq = qdev_get_gpio_in(dma, HERCULES_MIBSPIn_DMAREQ[i][j]);
            sysbus_connect_irq(sbd, irq_nr++, irq);
        }

        error = qdev_get_gpio_in(esm, HERCULES_MIBSPIn_SINGLE_BIT_ERROR[i]);
        sysbus_connect_irq(sbd, irq_nr++, error);
        error = qdev_get_gpio_in(esm, HERCULES_MIBSPIn_UNCORRECTABLE_ERROR[i]);
        sysbus_connect_irq(sbd, irq_nr++, error);
    }

    object_property_set_bool(OBJECT(&s->scm), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->scm);
    sysbus_mmio_map(sbd, 0, HERCULES_SCM_ADDR);
    sysbus_mmio_map(sbd, 1, HERCULES_SDR_MMR_ADDR);
    irq = qdev_get_gpio_in(DEVICE(&s->system), HERCULES_SYSTEM_ICRST);
    sysbus_connect_irq(sbd, 0, irq);

    object_property_set_bool(OBJECT(&s->efuse), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->efuse);
    sysbus_mmio_map(sbd, 0, HERCULES_EFUSE_ADDR);
    error = qdev_get_gpio_in(esm, HERCULES_EFUSE_AUTOLOAD_ERROR);
    sysbus_connect_irq(sbd, 0, error);
    error = qdev_get_gpio_in(esm, HERCULES_EFUSE_SELF_TEST_ERROR);
    sysbus_connect_irq(sbd, 1, error);
    error = qdev_get_gpio_in(esm, HERCULES_EFUSE_SINGLE_BIT_ERROR);
    sysbus_connect_irq(sbd, 2, error);

    object_property_set_bool(OBJECT(&s->pmm), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->pmm);
    sysbus_mmio_map(sbd, 0, HERCULES_PMM_ADDR);
    error = qdev_get_gpio_in(esm, HERCULES_PMM_COMPARE_ERROR);
    sysbus_connect_irq(sbd, 0, error);
    error = qdev_get_gpio_in(esm, HERCULES_PMM_SELF_TEST_ERROR);
    sysbus_connect_irq(sbd, 1, error);

    for (i = 0; i < HERCULES_NUM_ECAPS; i++) {
        static const hwaddr HERCULES_ECAPn_ADDR[HERCULES_NUM_ECAPS] = {
            HERCULES_ECAP1_ADDR,
            HERCULES_ECAP2_ADDR,
            HERCULES_ECAP3_ADDR,
            HERCULES_ECAP4_ADDR,
            HERCULES_ECAP5_ADDR,
            HERCULES_ECAP6_ADDR,
        };

        object_property_set_bool(OBJECT(&s->ecap[i]), true, "realized",
                                 &error_abort);
        sbd = SYS_BUS_DEVICE(&s->ecap[i]);
        sysbus_mmio_map(sbd, 0, HERCULES_ECAPn_ADDR[i]);
    }

    object_property_set_bool(OBJECT(&s->stc), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->stc);
    sysbus_mmio_map(sbd, 0, HERCULES_STC1_ADDR);
    irq = qdev_get_gpio_in(DEVICE(&s->system), HERCULES_SYSTEM_CPURST);
    sysbus_connect_irq(sbd, 0, irq);

    object_property_set_bool(OBJECT(&s->pbist), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->pbist);
    sysbus_mmio_map(sbd, 0, HERCULES_PBIST_ADDR);
    irq = qdev_get_gpio_in(DEVICE(&s->system), HERCULES_SYSTEM_MSTDONE);
    sysbus_connect_irq(sbd, 0, irq);

    object_property_set_bool(OBJECT(&s->ccm), true, "realized",
                             &error_abort);
    sbd = SYS_BUS_DEVICE(&s->ccm);
    sysbus_mmio_map(sbd, 0, HERCULES_CCM_ADDR);
    error = qdev_get_gpio_in(esm, HERCULES_CCMR5F_CPU_COMPARE_ERROR);
    sysbus_connect_irq(sbd, 0, error);
    error = qdev_get_gpio_in(esm, HERCULES_CCMR5F_VIM_COMPARE_ERROR);
    sysbus_connect_irq(sbd, 1, error);
    error = qdev_get_gpio_in(esm, HERCULES_CPU1_AXIM_BUS_MONITOR_ERROR);
    sysbus_connect_irq(sbd, 2, error);
    error = qdev_get_gpio_in(esm, HERCULES_CCMR5F_SELF_TEST_ERROR);
    sysbus_connect_irq(sbd, 3, error);

#ifdef HOST_WORDS_BIGENDIAN
    error_setg(errp, "failed realize on big endian host");
    return;
#endif
}

static Property hercules_properties[] = {
    DEFINE_PROP_DRIVE("eeprom", HerculesState, blk_eeprom),
    DEFINE_PROP_BOOL("tms570", HerculesState, is_tms570, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void hercules_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = hercules_realize;
    dc->desc = "TI Hercules";
    device_class_set_props(dc, hercules_properties);

    /* Reason: Uses serial_hds in realize() directly */
    dc->user_creatable = false;
}

static const TypeInfo hercules_type_info = {
    .name          = TYPE_HERCULES_SOC,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(HerculesState),
    .instance_init = hercules_initfn,
    .class_init    = hercules_class_init,
};

static void hercules_register_types(void)
{
    type_register_static(&hercules_type_info);
}
type_init(hercules_register_types)

static void hercules_xx57_init(MachineState *machine, bool is_tms570)
{
    Object *dev;
    MemoryRegion *sdram = g_new(MemoryRegion, 1);
    DriveInfo *eeprom = drive_get(IF_MTD, 0, 0);
    const char *file;

    dev = object_new(TYPE_HERCULES_SOC);
    qdev_prop_set_drive(DEVICE(dev), "eeprom",
                        eeprom ? blk_by_legacy_dinfo(eeprom) : NULL,
                        &error_abort);
    object_property_set_bool(dev, is_tms570, "tms570", &error_fatal);
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

static void tms570lc43_init(MachineState *machine)
{
    hercules_xx57_init(machine, true);
}

static void rm57l843_init(MachineState *machine)
{
    hercules_xx57_init(machine, false);
}

static void tms570lc43_machine_init(MachineClass *mc)
{
    mc->desc = "Texas Instruments Hercules TMS570LC43";
    mc->init = tms570lc43_init;
    mc->max_cpus = 1;
}

static void rm57l843_machine_init(MachineClass *mc)
{
    mc->desc = "Texas Instruments Hercules RM57L843";
    mc->init = rm57l843_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("tms570lc43", tms570lc43_machine_init)
DEFINE_MACHINE("rm57l843", rm57l843_machine_init)
