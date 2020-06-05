/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_H
#define HERCULES_H

#include "qemu/osdep.h"
#include "hw/arm/boot.h"
#include "hw/misc/hercules_l2ramw.h"
#include "hw/char/hercules_rtp.h"
#include "hw/intc/hercules_vim.h"
#include "hw/misc/hercules_system.h"
#include "hw/gpio/hercules_gpio.h"
#include "hw/adc/hercules_mibadc.h"
#include "hw/timer/hercules_rti.h"
#include "hw/net/hercules_emac.h"
#include "hw/ssi/hercules_spi.h"
#include "hw/dma/hercules_dma.h"
#include "hw/misc/hercules_scm.h"
#include "hw/misc/hercules_esm.h"
#include "hw/misc/hercules_efuse.h"
#include "hw/misc/hercules_pmm.h"
#include "hw/misc/hercules_stc.h"
#include "hw/misc/hercules_pbist.h"
#include "hw/misc/hercules_ccm.h"
#include "hw/misc/hercules_l2fmc.h"
#include "hw/misc/hercules_ecap.h"

#define TYPE_HERCULES_SOC "ti-hercules"

enum HerculesConfiguration {
    HERCULES_NUM_N2HETS = 2,
    HERCULES_NUM_MIBADCS = 2,
    HERCULES_NUM_MIBSPIS = 5,
    HERCULES_NUM_ECAPS = 6,
};

struct ARMCPU;

typedef struct HerculesState {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/

    /* properties */
    BlockBackend *blk_eeprom;
    bool is_tms570;
    /* end properties */

    struct ARMCPU* cpu;
    HerculesL2RamwState l2ramw;
    HerculesRTPState rtp;

    HerculesVimState vim;
    HerculesSystemState system;
    HerculesGioState gio;
    HerculesN2HetState n2het[HERCULES_NUM_N2HETS];
    HerculesMibAdcState mibadc[HERCULES_NUM_MIBADCS];
    HerculesRtiState rti;
    HerculesEmacState emac;
    HerculesDMAState dma;
    HerculesMibSpiState mibspi[HERCULES_NUM_MIBSPIS];
    HerculesSCMState scm;
    HerculesESMState esm;
    HerculesEFuseState efuse;
    HerculesPMMState pmm;
    HerculesSTCState stc;
    HerculesPBISTState pbist;
    HerculesCCMState ccm;
    HerculesL2FMCState l2fmc;
    HerculesECAPState ecap[HERCULES_NUM_ECAPS];
} HerculesState;

#define HERCULES_SOC(obj) OBJECT_CHECK(HerculesState, (obj), TYPE_HERCULES_SOC)

enum HerculesMemoryMap {
    HERCULES_FLASH_ADDR       = 0x00000000,
    HERCULES_FLASH_SIZE       = 4 * 1024 * 1024,

    HERCULES_RAM_ADDR         = 0x08000000,

    HERCULES_EMIF_CS1_ADDR    = 0x80000000,

    HERCULES_OTP_BANK1_ADDR   = 0xF0080000,
    HERCULES_OTP_BANK1_SIZE   = 8 * 1024,

    HERCULES_EEPROM_ADDR      = 0xF0200000,
    HERCULES_EEPROM_SIZE      = 128 * 1024,

    HERCULES_SDR_MMR_ADDR     = 0xFA000000,

    HERCULES_EMAC_CPPI_ADDR   = 0xFC520000,

    HERCULES_EMAC_MODULE_ADDR = 0xFCF78000,
    HERCULES_EMAC_CTRL_ADDR   = 0xFCF78800,
    HERCULES_EMAC_MDIO_ADDR   = 0xFCF78900,

    HERCULES_ECAP1_ADDR       = 0xFCF79300,
    HERCULES_ECAP2_ADDR       = 0xFCF79400,
    HERCULES_ECAP3_ADDR       = 0xFCF79500,
    HERCULES_ECAP4_ADDR       = 0xFCF79600,
    HERCULES_ECAP5_ADDR       = 0xFCF79700,
    HERCULES_ECAP6_ADDR       = 0xFCF79800,

    HERCULES_NMPU_ADDR        = 0xFCFF1800,
    HERCULES_NMPU_SIZE        = 512,

    HERCULES_PCR2_ADDR        = 0xFCFF1000,

    HERCULES_EMIF_ADDR        = 0xFCFFE800,
    HERCULES_EMIF_SIZE        = 256,

    HERCULES_MIBSPI4_RAM_ADDR = 0xFF060000,

    HERCULES_MIBSPI2_RAM_ADDR = 0xFF080000,

    HERCULES_MIBSPI5_RAM_ADDR = 0xFF0A0000,

    HERCULES_MIBSPI3_RAM_ADDR = 0xFF0C0000,

    HERCULES_MIBSPI1_RAM_ADDR = 0xFF0E0000,

    HERCULES_MIBADC2_RAM_ADDR = 0xFF3A0000,

    HERCULES_MIBADC1_RAM_ADDR = 0xFF3E0000,

    HERCULES_N2HET2_RAM_ADDR  = 0xFF440000,

    HERCULES_N2HET1_RAM_ADDR  = 0xFF460000,

    HERCULES_DEBUG_ROM_ADDR   = 0xFFA00000,
    HERCULES_DEBUG_ADDR       = 0xFFA01000,
    HERCULES_ETM_ADDR         = 0xFFA02000,
    HERCULES_TPIU_ADDR        = 0xFFA03000,
    HERCULES_POM_ADDR         = 0xFFA04000,
    HERCULES_CTI1_ADDR        = 0xFFA07000,
    HERCULES_CTI2_ADDR        = 0xFFA07000,
    HERCULES_CTI3_ADDR        = 0xFFA07000,
    HERCULES_CTI4_ADDR        = 0xFFA07000,
    HERCULES_CTSF_ADDR        = 0xFFA07000,
    HERCULES_DEBUG_SIZE       = 4 * 1024,

    HERCULES_PCR_ADDR         = 0xFFFF1000,

    HERCULES_PINMUX_ADDR      = 0xFFFF1C00,
    HERCULES_PINMUX_SIZE      = 1024,

    HERCULES_PCR3_ADDR        = 0xFFF78000,

    HERCULES_N2HET1_ADDR      = 0xFFF7B800,

    HERCULES_N2HET2_ADDR      = 0xFFF7B900,

    HERCULES_GIO_ADDR         = 0xFFF7BC00,

    HERCULES_MIBADC1_ADDR     = 0xFFF7C000,

    HERCULES_MIBADC2_ADDR     = 0xFFF7C200,

    HERCULES_LIN1_ADDR        = 0xFFF7E400,
    HERCULES_LIN1_SIZE        = 256,

    HERCULES_MIBSPI1_CTRL_ADDR = 0xFFF7F400,

    HERCULES_MIBSPI2_CTRL_ADDR = 0xFFF7F600,

    HERCULES_MIBSPI3_CTRL_ADDR = 0xFFF7F800,

    HERCULES_MIBSPI4_CTRL_ADDR = 0xFFF7FA00,

    HERCULES_MIBSPI5_CTRL_ADDR = 0xFFF7FC00,

    HERCULES_DMA_RAM_ADDR     = 0xFFF80000,

    HERCULES_VIM_RAM_ADDR     = 0xFFF82000,

    HERCULES_L2FMC_ADDR       = 0xFFF87000,

    HERCULES_EFUSE_ADDR       = 0xFFF8C000,

    HERCULES_PMM_ADDR         = 0xFFFF0000,

    HERCULES_SCM_ADDR         = 0xFFFF0A00,

    HERCULES_EPC_ADDR         = 0xFFFF0C00,

    HERCULES_RTP_ADDR         = 0xFFFFFA00,

    HERCULES_DMA_ADDR         = 0xFFFFF000,

    HERCULES_SYS2_ADDR        = 0xFFFFE100,

    HERCULES_PBIST_ADDR       = 0xFFFFE400,

    HERCULES_STC1_ADDR        = 0xFFFFE600,

    HERCULES_DCC1_ADDR        = 0xFFFFEC00,
    HERCULES_DCC1_SIZE        = 256,

    HERCULES_ESM_ADDR         = 0xFFFFF500,

    HERCULES_CCM_ADDR         = 0xFFFFF600,

    HERCULES_L2RAMW_ADDR      = 0xFFFFF900,

    HERCULES_RTI_ADDR         = 0xFFFFFC00,

    HERCULES_VIM_ECC_ADDR     = 0xFFFFFD00,

    HERCULES_VIM_CONTROL_ADDR = 0xFFFFFE00,

    HERCULES_SYS_ADDR         = 0xFFFFFF00,
};

#endif
