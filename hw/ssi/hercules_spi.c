/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"

#include "sysemu/dma.h"

#include "hw/ssi/hercules_spi.h"

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);


/*
 *  Current assumptions in this MIBSPI implementation
 *
 *   - Only MIBSPI mode is implemented, no compatability mode
 *
 *   - For MIBSPI mode, we are assuming that the first buffer in a
 *     transfer group has the same control settings as the rest (number
 *     of bits, shift direction, etc)
 *
 *   - We only implement the SW trigger one-shot mode
 *
 *   - We only are only using a single chip select per device
 *
 *   - Only support word length of 8 / 16
 *
 */

#define HERCULES_KEY_ENABLE     0xA
#define HERCULES_KEY_DISABLE    0x5

enum HerculesMibSpiRegisters {
    SPIGCR0         = 0x000,
    SPIGCR1         = 0x004,
    SPIEN           = BIT(24),
    SPIINT0         = 0x008,
    DMAREQEN        = BIT(16),
    SPIFLG          = 0x010,
    SPIFLG_W1C_MASK = 0x15f,
    SPIPC0          = 0x014,
    ENAFUN          = BIT(8),
    SPIPC1          = 0x018,
    SPIPC8          = 0x034,
    SPIDAT0         = 0x038,
    SPIDAT1         = 0x03C,
    SPIBUF          = 0x040,
    SPIFMT0         = 0x050,
    SPIFMT1         = 0x054,
    SPIFMT2         = 0x058,
    SPIFMT3         = 0x05C,
    MIBSPIE         = 0x070,
    RXRAM_ACCESS    = BIT(16),
    TGINTFLAG       = 0x084,
    LTGPEND         = 0x094,
    TG0CTRL         = 0x098,
    TG14CTRL        = 0x0D0,
    TG15CTRL        = 0x0D4,
    DMA0CTRL        = 0x0DF,
    RXDMAENA        = BIT(15),
    TXDMAENA        = BIT(14),
    DMA8CTRL        = 0x0F4,
    DMA0COUNT       = 0x0F8,
    DMA8COUNT       = 0x112,
    DMACNTLEN       = 0x118,
    TGENA           = BIT(31),
    ONESHOTx        = BIT(30),
    PAR_ECC_CTRL    = 0x120,
    PAR_ECC_STAT    = 0x124,
    UERR_FLG0       = BIT(0),
    UERR_FLG1       = BIT(1),
    SBE_FLG0        = BIT(8),
    SBE_FLG1        = BIT(9),
    UERRADDR1       = 0x128,
    UERRADDR0       = 0x12C,
    IOLPBKTSTCR     = 0x134,
    IOLPBKSTENA     = 0x0A,
    ECCDIAG_CTRL    = 0x140,
    ECCDIAG_STAT    = 0x144,
    SEFLG0          = BIT(0),
    SEFLG1          = BIT(1),
    DEFLG0          = BIT(16),
    DEFLG1          = BIT(17),
    SBERRADDR1      = 0x148,
    SBERRADDR0      = 0x14C,
};

#define INTFLGRDY(n)               BIT((n) + 16)

#define TXRAM_CSNR(w)              extract32(w, 16, 8)
#define TXRAM_DFSEL(w)             extract32(w, 24, 2)
#define TXRAM_TXDATA(w, l)         extract32(w, 0, l)
#define TXRAM_CSHOLD               BIT(28)

#define SPIFMT_CHARLEN(w)          extract32(w, 0, 5)

#define TGxCTRL_PSTART(w)          extract32(w, 8, 8)

#define IOLPBKTSTCR_IOLPBKSTENA(w) extract32(w, 8, 4)

#define TXDMA_MAPx(w)              extract32(w, 16, 4)
#define RXDMA_MAPx(w)              extract32(w, 20, 4)

#define SBE_EVT_EN(w)              extract32(w, 24, 4)

static uint16_t hercules_spi_tx_single(HerculesMibSpiState *s, uint32_t txword)
{
    const uint8_t  dfsel   = TXRAM_DFSEL(txword);
    const uint32_t spifmt  = s->spifmt[dfsel];
    const uint8_t  charlen = SPIFMT_CHARLEN(spifmt);
    const uint16_t txdata  = TXRAM_TXDATA(txword, charlen);

    /*
     * FIXME: Should probably cache this as a pivate boolean flag
     */
    if (unlikely(IOLPBKTSTCR_IOLPBKSTENA(s->iolpbktstcr) == IOLPBKSTENA)) {
        return txdata;
    }

    return ssi_transfer(s->ssi, txdata);
}

static bool hercules_spi_ram_big_endian(HerculesMibSpiState *s)
{
    return s->io.regs.ops->endianness == DEVICE_BIG_ENDIAN;
}

static uint32_t hercules_spi_ram_read(HerculesMibSpiState *s, uint32_t *word)
{
    return hercules_spi_ram_big_endian(s) ? ldl_be_p(word) : ldl_le_p(word);
}

static void hercules_spi_ram_write(HerculesMibSpiState *s, uint32_t *word,
                                   uint32_t value)
{
    if (hercules_spi_ram_big_endian(s)) {
        stl_be_p(word, value);
    } else {
        stl_le_p(word, value);
    }
}

static void hercules_spi_assert_cs(HerculesMibSpiState *s,
                                   unsigned long csnr, int val)
{
    int i;

    for_each_set_bit(i, &csnr, HERCULES_SPI_NUM_CS_LINES) {
        qemu_set_irq(s->cs_lines[i], val);
    }
}

static void hercules_ecc_error_raise(HerculesMibSpiState *s,
                                     unsigned int idx,
                                     unsigned int err_idx,
                                     uint32_t uerr_flg, uint32_t sbe_flg)
{
    if (s->par_ecc_stat & uerr_flg &&
        s->uerraddr[err_idx] == idx * sizeof(uint32_t)) {
        qemu_irq_raise(s->uncorrectable_error);
    }

    if (s->par_ecc_stat & sbe_flg &&
        s->sberraddr[err_idx] == idx * sizeof(uint32_t)) {
        qemu_irq_raise(s->single_bit_error);
    }
}

static void ___hercules_spi_process_tg(HerculesMibSpiState *s, unsigned int n,
                                      unsigned int end)
{
    const bool tgena = s->tgxctrl[n] & TGENA;
    const bool oneshot = s->tgxctrl[n] & ONESHOTx;

    if (tgena && oneshot) {
        const unsigned int start = TGxCTRL_PSTART(s->tgxctrl[n]);
        unsigned int i;


        for (i = start; i <= end; i++) {
            uint32_t txword;
            uint32_t rxword;
            uint8_t csnr;

            hercules_ecc_error_raise(s, i, 0, UERR_FLG0, SBE_FLG0);

            /*
             * FIXME: Handle CS assertion here
             */
            txword = hercules_spi_ram_read(s, &s->txram[i]);
            csnr   = ~TXRAM_CSNR(txword);

            hercules_spi_assert_cs(s, csnr, 0);

            rxword = hercules_spi_tx_single(s, txword);
            hercules_spi_ram_write(s, &s->rxram[i], rxword);

            if (txword & TXRAM_CSHOLD) {
                continue;
            }

            hercules_spi_assert_cs(s, csnr, 1);
        }

        s->tgintflag |= INTFLGRDY(n);
    }
}

static void __hercules_spi_process_tg(HerculesMibSpiState *s, unsigned int n,
                                      unsigned int end)
{
    if (s->spiena != SPIENA_IDLE) {
        s->pending_tg.n   = n;
        s->pending_tg.end = end;
        s->spiena = SPIENA_PENDING_TG;
        return;
    }

    ___hercules_spi_process_tg(s, n, end);
}

static void hercules_spi_process_tg(HerculesMibSpiState *s,
                                    unsigned int n)
{
    const unsigned int end = TGxCTRL_PSTART(s->tgxctrl[n + 1]) - 1;

    __hercules_spi_process_tg(s, n, end);
}

static void hercules_spi_process_tg_last(HerculesMibSpiState *s)
{

    const unsigned int end = TGxCTRL_PSTART(s->ltgpend);

    __hercules_spi_process_tg(s, 15, end);
}

static uint64_t hercules_spi_read32(void *opaque, hwaddr offset,
                                    unsigned size)
{
    HerculesMibSpiState *s = opaque;
    unsigned int idx;
    uint32_t erraddr;

    switch (offset) {
    case SPIGCR0:
        return s->spigcr0;
    case SPIGCR1:
        return s->spigcr1;
    case SPIINT0:
        return s->spiint0;
    case SPIFLG:
        return s->spiflg;
    case SPIPC0:
        return s->spipc[0];
    case SPIPC1 ... SPIPC8:
        return 0;
    case SPIFMT0 ... SPIFMT3:
        idx = (offset - SPIFMT0) / sizeof(uint32_t);
        return s->spifmt[idx];
    case MIBSPIE:
        return s->mibspie;
    case TGINTFLAG:
        return s->tgintflag;
    case LTGPEND:
        return s->ltgpend;
    case TG0CTRL ... TG15CTRL:
        idx = (offset - TG0CTRL) / sizeof(uint32_t);
        return s->tgxctrl[idx];
    case PAR_ECC_CTRL:
        return s->par_ecc_ctrl;
    case PAR_ECC_STAT:
        return s->par_ecc_stat;
    case IOLPBKTSTCR:
        return s->iolpbktstcr;
    case ECCDIAG_CTRL:
        return s->eccdiag_ctrl;
    case ECCDIAG_STAT:
        return s->eccdiag_stat;
    case SBERRADDR1:
        erraddr = s->sberraddr[1] + sizeof(s->txram);
        s->sberraddr[1] = 0;
        return erraddr;
    case SBERRADDR0:
        erraddr = s->sberraddr[0];
        s->sberraddr[0] = 0;
        return erraddr;
    case UERRADDR1:
        erraddr = s->uerraddr[1] + sizeof(s->txram);
        s->uerraddr[1] = 0;
        return erraddr;
    case UERRADDR0:
        erraddr = s->uerraddr[0];
        s->uerraddr[0] = 0;
        return erraddr;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static uint64_t hercules_spi_read16(void *opaque, hwaddr offset,
                                    unsigned size)
{
    HerculesMibSpiState *s = opaque;

    /* FIXME: This assumes BE */
    switch (offset) {
    case SPIBUF:
        return extract32(s->spibuf, 16, 16);
    case SPIBUF + sizeof(uint16_t):
        return extract32(s->spibuf, 0, 16);
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static uint64_t hercules_spi_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    switch (size) {
    case sizeof(uint16_t):
        return hercules_spi_read16(opaque, offset, size);
    case sizeof(uint32_t):
        return hercules_spi_read32(opaque, offset, size);
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad size %u\n",
                      __func__, size);
        break;
    }

    return 0;
}

static bool hercules_spi_compatibility_dma_enabled(HerculesMibSpiState *s)
{
    return s->spiint0 & DMAREQEN && s->spigcr1 & SPIEN;
}

static void hercules_spi_assert_dmareq(void *opaque)
{
    HerculesMibSpiState *s = opaque;

    if (s->spiint0 & DMAREQEN) {
        qemu_irq_raise(s->dmareq[0]);
    }
}

static void hercules_spi_process_compatibility_dma(HerculesMibSpiState *s)
{
    if (hercules_spi_compatibility_dma_enabled(s)) {
        qemu_irq_raise(s->dmareq[0]);
    } else {
        qemu_bh_cancel(s->compatibility_dma_req);
    }
}

static void __hercules_spi_process_spidata(HerculesMibSpiState *s)
{
    s->spibuf = hercules_spi_tx_single(s, s->spidat1);

    if (hercules_spi_compatibility_dma_enabled(s)) {
        qemu_irq_raise(s->dmareq[1]);
        qemu_bh_schedule(s->compatibility_dma_req);
    }
}

static void hercules_spi_process_spidata(HerculesMibSpiState *s)
{
    if (s->spiena != SPIENA_IDLE) {
        s->spiena = SPIENA_PENDING_SPIDATA;
        return;
    }

    __hercules_spi_process_spidata(s);
}

static void hercules_spi_write16(void *opaque, hwaddr offset, uint64_t val64,
                                 unsigned size)
{
    HerculesMibSpiState *s = opaque;
    const uint16_t val = val64;

    /* FIXME: This assumes BE */
    switch (offset) {
    case SPIDAT1:
        s->spidat1 = deposit32(s->spidat1, 16, 16, val);
        break;
    case SPIDAT1 + sizeof(uint16_t):
        s->spidat1 = deposit32(s->spidat1, 0, 16, val);
        hercules_spi_process_spidata(s);
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_spi_write32(void *opaque, hwaddr offset, uint64_t val64,
                                 unsigned size)
{
    HerculesMibSpiState *s = opaque;
    const uint32_t val = val64;
    unsigned int idx;

    switch (offset) {
    case SPIGCR0:
        s->spigcr0 = val;
        break;
    case SPIGCR1:
        s->spigcr1 = val;
        hercules_spi_process_compatibility_dma(s);
        break;
    case SPIINT0:
        s->spiint0 = val;
        hercules_spi_process_compatibility_dma(s);
        break;
    case SPIFLG:
        s->spiflg &= ~(val & SPIFLG_W1C_MASK);
        break;
    case SPIPC0:
        s->spipc[0] = val;
        break;
    case SPIPC1 ... SPIPC8:
        break;
    case SPIDAT1:
        s->spidat1 = val;
        break;
    case SPIFMT0 ... SPIFMT3:
        idx = (offset - SPIFMT0) / sizeof(uint32_t);
        s->spifmt[idx] = val;
        break;
    case MIBSPIE:
        s->mibspie = val;
        break;
    case TGINTFLAG:
        s->tgintflag &= ~val;
        break;
    case LTGPEND:
        s->ltgpend = val;
        break;
    case TG0CTRL ... TG14CTRL:
        idx = (offset - TG0CTRL) / sizeof(uint32_t);
        s->tgxctrl[idx] = val;
        hercules_spi_process_tg(s, idx);
        break;
    case TG15CTRL:
        s->tgxctrl[15] = val;
        hercules_spi_process_tg_last(s);
        break;
    case PAR_ECC_CTRL:
        s->par_ecc_ctrl = val;
        break;
    case PAR_ECC_STAT:
        s->par_ecc_stat &= ~val;
        break;
    case IOLPBKTSTCR:
        s->iolpbktstcr = val;
        break;
    case ECCDIAG_CTRL:
        s->eccdiag_ctrl = val;
        break;
    case ECCDIAG_STAT:
        s->eccdiag_stat &= ~val;
        break;
    case SBERRADDR1:
    case SBERRADDR0:
    case UERRADDR1:
    case UERRADDR0:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

static void hercules_spi_write(void *opaque, hwaddr offset, uint64_t val,
                               unsigned size)
{
    switch (size) {
    case sizeof(uint16_t):
        hercules_spi_write16(opaque, offset, val, size);
        break;
    case sizeof(uint32_t):
        hercules_spi_write32(opaque, offset, val, size);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad size %u\n",
                      __func__, size);
        break;
    }
}

static void hercules_spi_rxram_write(void *opaque, hwaddr offset,
                                     uint64_t val, unsigned size)
{
    HerculesMibSpiState *s = opaque;

    if (s->mibspie & RXRAM_ACCESS) {
        if (s->eccdiag_ctrl == 0x5) {
            switch (ctpop32(val)) {
            default:
                s->par_ecc_stat |= UERR_FLG1;
                s->eccdiag_stat |= DEFLG1;
                s->uerraddr[1]  = offset;
                break;
            case 1:
                s->par_ecc_stat |= SBE_FLG1;
                s->eccdiag_stat |= SEFLG1;
                s->sberraddr[1] = offset;
            case 0:
                break;
            }
        }

        switch (size) {
        case 1:
            *(uint8_t *)((void *)s->rxram + offset)  = (uint8_t)val;
            break;
        case 2:
            *(uint16_t *)((void *)s->rxram + offset) = (uint16_t)val;
            break;
        case 4:
            *(uint32_t *)((void *)s->rxram + offset) = (uint32_t)val;
            break;
        }
    }
}

static uint64_t hercules_spi_rxram_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    HerculesMibSpiState *s = opaque;
    const unsigned int idx = offset / sizeof(uint32_t);
    uint64_t data = (uint64_t)~0;

    hercules_ecc_error_raise(s, idx, 1, UERR_FLG1, SBE_FLG1);

    switch (size) {
    case 1:
        data = *(uint8_t *)((void *)s->rxram + offset);
        break;
    case 2:
        data = *(uint16_t *)((void *)s->rxram + offset);
        break;
    case 4:
        data = *(uint32_t *)((void *)s->rxram + offset);
        break;
    }

    return data;
}

static void hercules_spi_set_spiena(void *opaque, int req, int level)
{
    HerculesMibSpiState *s = opaque;
    const bool asserted = !level;

    if (likely(s->spipc[0] & ENAFUN)) {
        switch (s->spiena) {
        case SPIENA_PENDING_TG:
            if (asserted) {
                ___hercules_spi_process_tg(s, s->pending_tg.n,
                                           s->pending_tg.end);
                s->spiena = SPIENA_IDLE;
            }
            break;
        case SPIENA_PENDING_SPIDATA:
            if (asserted) {
                __hercules_spi_process_spidata(s);
                s->spiena = SPIENA_IDLE;
            }
            break;
        case SPIENA_IDLE:
            if (level) {
                s->spiena = SPIENA_BUSY;
            }
            break;
        case SPIENA_BUSY:
            if (asserted) {
                s->spiena = SPIENA_IDLE;
            }
        }
    }
}

static const MemoryRegionOps hercules_spi_rxram_ops = {
    .read = hercules_spi_rxram_read,
    .write = hercules_spi_rxram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 1,
        .max_access_size = 4,
        .unaligned = true,
    },
};

static const MemoryRegionOps hercules_spi_ops = {
    .read       = hercules_spi_read,
    .write      = hercules_spi_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        .min_access_size = 2,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void hercules_spi_realize(DeviceState *dev, Error **errp)
{
    HerculesMibSpiState *s = HERCULES_SPI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    int i;

    memory_region_init_io(&s->io.regs, OBJECT(dev), &hercules_spi_ops, s,
                          TYPE_HERCULES_SPI ".io", HERCULES_SPI_SIZE);
    sysbus_init_mmio(sbd, &s->io.regs);

    memory_region_init_ram_ptr(&s->io.txram, OBJECT(dev),
                               TYPE_HERCULES_SPI ".ram.tx",
                               sizeof(s->txram), s->txram);

    memory_region_init_io(&s->io.rxram, OBJECT(dev),
                          &hercules_spi_rxram_ops,
                          s, TYPE_HERCULES_SPI ".ram.rx",
                          sizeof(s->rxram));

    memory_region_init(&s->io.ram, OBJECT(dev),
                       TYPE_HERCULES_SPI ".ram",
                       sizeof(s->txram) +
                       sizeof(s->rxram));

    memory_region_add_subregion(&s->io.ram,
                                0x0,
                                &s->io.txram);
    memory_region_add_subregion(&s->io.ram,
                                sizeof(s->txram),
                                &s->io.rxram);

    sysbus_init_mmio(sbd, &s->io.ram);

    sysbus_init_irq(sbd, &s->irq[0]);
    sysbus_init_irq(sbd, &s->irq[1]);

    s->ssi = ssi_create_bus(dev, "ssi");
    ssi_auto_connect_slaves(DEVICE(s), s->cs_lines, s->ssi);

    for (i = 0; i < ARRAY_SIZE(s->cs_lines); ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    for (i = 0; i < ARRAY_SIZE(s->dmareq); i++) {
        sysbus_init_irq(sbd, &s->dmareq[i]);
    }

    s->compatibility_dma_req = qemu_bh_new(hercules_spi_assert_dmareq, s);

    qdev_init_gpio_in_named(dev, hercules_spi_set_spiena,
                            HERCULES_SPI_SPIENA, 1);

    sysbus_init_irq(sbd, &s->single_bit_error);
    sysbus_init_irq(sbd, &s->uncorrectable_error);
}

static void hercules_spi_reset(DeviceState *d)
{
    HerculesMibSpiState *s = HERCULES_SPI(d);

    s->mibspie     = 5 << 8;
    s->spigcr0     = 0;
    s->spigcr1     = 0;
    s->spiint0     = 0;
    s->spiflg      = 0;
    s->spipc[0]    = 0;
    s->tgintflag   = 0;
    s->iolpbktstcr = 0;
    s->ltgpend     = 0;
    s->spiena      = SPIENA_IDLE;

    memset(s->spifmt,  0, sizeof(s->spifmt));
    memset(s->tgxctrl, 0, sizeof(s->tgxctrl));
    memset(s->txram,   0, sizeof(s->txram));
    memset(s->rxram,   0, sizeof(s->rxram));
}

static void hercules_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_spi_reset;
    dc->realize = hercules_spi_realize;

    dc->desc = "Hercules MiBSPI Controller";
}

static const TypeInfo hercules_spi_info = {
    .name          = TYPE_HERCULES_SPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesMibSpiState),
    .class_init    = hercules_spi_class_init,
};

static void hercules_spi_register_types(void)
{
    type_register_static(&hercules_spi_info);
}

type_init(hercules_spi_register_types)
