/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_SPI_H
#define HERCULES_SPI_H

#include "hw/ssi/ssi.h"

enum {
    HERCULES_SPI_RAM_SIZE         = 128,
    HERCULES_SPI_SIZE             = 512,
    HERCULES_SPI_NUM_IRQ_LINES    = 2,
    HERCULES_SPI_NUM_CS_LINES     = 8,
    HERCULES_SPI_NUM_DMAREQS      = 16,
};

typedef struct HerculesMibSpiState {
    SysBusDevice parent_obj;

    struct {
        MemoryRegion regs;
        MemoryRegion txram;
        MemoryRegion rxram;
        MemoryRegion ram;
    } io;

    uint32_t txram[HERCULES_SPI_RAM_SIZE];
    uint32_t rxram[HERCULES_SPI_RAM_SIZE];

    uint32_t spigcr0;
    uint32_t spigcr1;
    uint32_t spiint0;
    uint32_t spiflg;
    uint32_t spipc[1];
    uint32_t spidat0;
    uint32_t spidat1;
    uint32_t spibuf;
    uint32_t spifmt[4];
    uint32_t mibspie;
    uint32_t tgintflag;
    uint32_t tgxctrl[16];
    uint32_t iolpbktstcr;
    uint32_t ltgpend;
    uint32_t par_ecc_ctrl;
    uint32_t par_ecc_stat;
    uint32_t uerraddr[2];
    uint32_t eccdiag_ctrl;
    uint32_t eccdiag_stat;
    uint32_t sberraddr[2];
    qemu_irq irq[HERCULES_SPI_NUM_IRQ_LINES];

    SSIBus *ssi;
    qemu_irq cs_lines[HERCULES_SPI_NUM_CS_LINES];

    qemu_irq dmareq[HERCULES_SPI_NUM_DMAREQS];

    qemu_irq single_bit_error;
    qemu_irq uncorrectable_error;

    enum {
        SPIENA_IDLE,
        SPIENA_BUSY,
        SPIENA_PENDING_TG,
        SPIENA_PENDING_SPIDATA,
    } spiena;

    struct {
        unsigned int n;
        unsigned int end;
    } pending_tg;

    QEMUBH *compatibility_dma_req;
} HerculesMibSpiState;

#define TYPE_HERCULES_SPI "ti-hercules-spi"
#define HERCULES_SPI(obj) OBJECT_CHECK(HerculesMibSpiState, (obj), \
                                       TYPE_HERCULES_SPI)

#define HERCULES_SPI_SPIENA TYPE_HERCULES_SPI "-spiena"

#endif
