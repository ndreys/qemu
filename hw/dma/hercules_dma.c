/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "sysemu/dma.h"
#include "trace.h"

#include "hw/dma/hercules_dma.h"

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesDMARegisters {
    GCTRL    = 0x000,
    HWCHENAS = 0x014,
    HWCHENAR = 0x01C,
    DREQASI0 = 0x054,
    DREQASI7 = 0x070,
    PAR0     = 0x094,
    PAR3     = 0x0A0,
    BTCFLAG  = 0x13C,
    PTCRL    = 0x178,
    TTYPE    = BIT(8),

    ISADDR   = 0x00,
    IDADDR   = 0x04,
    ITCOUNT  = 0x08,
    CHCTRL   = 0x10,
    ADDM_POST_INCREMENT = 1,
    ADDM_INDEXED        = 2,

    EIOFF    = 0x14,
    FIOFF    = 0x18,

    CSADDR   = 0x00,
    CDADDR   = 0x04,
    CTCOUNT  = 0x08,
};

enum {
    HERCULES_DMA_SIZE       = 1024,
    HERCULES_DMA_RAM_SIZE   = 4 * 1024,

    HERCULES_DMA_PCP_OFFSET = 0x000,
    HERCULES_DMA_PCP_SIZE   = 32,

    HERCULES_DMA_WCP_OFFSET = 0x800,
    HERCULES_DMA_WCP_SIZE   = 32,
};

#define CHCTRL_ADDMR(w)    extract32(w,  3, 2)
#define CHCTRL_ADDMW(w)    extract32(w,  1, 2)
#define CHCTRL_RES(w)      extract32(w, 14, 2)
#define CHCTRL_WES(w)      extract32(w, 12, 2)

static void hercules_dma_adjust_addr(uint32_t *caddr, unsigned int addm,
                                     unsigned int es, unsigned int eidx,
                                     unsigned int fidx, int cetcount)
{
    switch (addm) {
    case ADDM_INDEXED:
        *caddr += es * ((cetcount == 1) ? fidx : eidx);
        break;
    case ADDM_POST_INCREMENT:
        *caddr += es;
        break;
    default:
        break;
    }
}

static void hercules_dma_set_request(void *opaque, int req, int level)
{
    HerculesDMAState *s = opaque;
    const uint32_t enabled = s->reqmap[req] & s->hwchena;

    if (level && enabled) {
        const unsigned int channel = ctzl(enabled);
        HerculesDMAChannel *ch = &s->channel[channel];
        const uint8_t addmr = CHCTRL_ADDMR(ch->pcp.chctrl);
        const uint8_t addmw = CHCTRL_ADDMW(ch->pcp.chctrl);
        const unsigned int res = CHCTRL_RES(ch->pcp.chctrl) + 1;
        const unsigned int wes = CHCTRL_WES(ch->pcp.chctrl) + 1;

        while (ch->wcp.cftcount) {
            uint64_t buffer = 0;
            int i;

            for (i = ch->wcp.cetcount; i > 0; i--) {
                trace_hercules_dma_transfer(channel,
                                            (int)ch->wcp.cftcount,
                                            (int)ch->wcp.cetcount,
                                            ch->wcp.csaddr, res,
                                            ch->wcp.cdaddr, wes);

                dma_memory_read(&address_space_memory, ch->wcp.csaddr,
                                &buffer, res);
                hercules_dma_adjust_addr(&ch->wcp.csaddr, addmr, res,
                                         ch->pcp.eidxs, ch->pcp.fidxs, i);

                dma_memory_write(&address_space_memory, ch->wcp.cdaddr,
                                 &buffer, wes);
                hercules_dma_adjust_addr(&ch->wcp.cdaddr, addmw, wes,
                                         ch->pcp.eidxd, ch->pcp.fidxd, i);
            }

            if (!--ch->wcp.cftcount) {
                s->btcflag |= BIT(channel);
            }

            if (!(ch->pcp.chctrl & TTYPE)) {
                /*
                 * Exit this loop early if we are configured for
                 * frame transfer
                 */
                break;
            }
        }
    }
}

#define IDX(o, s)    (((o) - (s)) / sizeof(uint32_t))

static uint64_t hercules_dma_read(void *opaque, hwaddr offset, unsigned size)
{
    HerculesDMAState *s = opaque;

    switch (offset) {
    case GCTRL:
        return s->gctrl;
    case HWCHENAS:
    case HWCHENAR:
        return s->hwchena;
    case BTCFLAG:
        return s->btcflag;
    case DREQASI0 ... DREQASI7:
        return s->dreqasi[IDX(offset, DREQASI0)];
    case PTCRL:
    case PAR0 ... PAR3:
        break;
    default:
        qemu_log_bad_offset(offset);
    }

    return 0;
}

static void hercules_dma_update_reqmap(HerculesDMAState *s)
{
    int i, channel;
    uint8_t req;

    memset(s->reqmap, 0, sizeof(s->reqmap));

    for (i = 0, channel = 0; i < ARRAY_SIZE(s->dreqasi); i++) {
        req = extract32(s->dreqasi[i], 24, 6);
        s->reqmap[req] |= BIT(channel++);
        req = extract32(s->dreqasi[i], 16, 6);
        s->reqmap[req] |= BIT(channel++);
        req = extract32(s->dreqasi[i],  8, 6);
        s->reqmap[req] |= BIT(channel++);
        req = extract32(s->dreqasi[i],  0, 6);
        s->reqmap[req] |= BIT(channel++);
    }
}

static void hercules_dma_write(void *opaque, hwaddr offset,
                               uint64_t val64, unsigned size)
{
    HerculesDMAState *s = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case GCTRL:
        s->gctrl = val;
        break;
    case HWCHENAS:
        s->hwchena |= val;
        break;
    case HWCHENAR:
        s->hwchena &= ~val;
        break;
    case BTCFLAG:
        s->btcflag &= ~val;
        break;
    case DREQASI0 ... DREQASI7:
        s->dreqasi[IDX(offset, DREQASI0)] = val;
        hercules_dma_update_reqmap(s);
        break;
    case PTCRL:
    case PAR0 ... PAR3:
        break;
    default:
        qemu_log_bad_offset(offset);
    }
}

#undef IDX

static void hercules_dma_ram_pcp_write(void *opaque, hwaddr offset,
                                       uint64_t val64, unsigned size)
{
    HerculesDMAChannel *ch = opaque;
    const uint32_t val = val64;

    switch (offset) {
    case ISADDR:
        ch->wcp.csaddr = ch->pcp.isaddr = val;
        break;
    case IDADDR:
        ch->wcp.cdaddr = ch->pcp.idaddr = val;
        break;
    case ITCOUNT:
        ch->wcp.cetcount = ch->pcp.ietcount = extract32(val,  0, 16);
        ch->wcp.cftcount = ch->pcp.iftcount = extract32(val, 16, 16);
        break;
    case CHCTRL:
        ch->pcp.chctrl = val;
        break;
    case EIOFF:
        ch->pcp.eidxs = extract32(val,  0, 16);
        ch->pcp.eidxd = extract32(val, 16, 16);
        break;
    case FIOFF:
        ch->pcp.fidxs = extract32(val,  0, 16);
        ch->pcp.fidxd = extract32(val, 16, 16);
        break;
    default:
        qemu_log_bad_offset(offset);
        break;
    };
}

static uint64_t hercules_dma_ram_pcp_read(void *opaque, hwaddr offset,
                                          unsigned size)
{
    HerculesDMAChannel *ch = opaque;

    switch (offset) {
    case ISADDR:
        return ch->pcp.isaddr;
    case IDADDR:
        return ch->pcp.idaddr;
    case ITCOUNT:
        return deposit32(ch->pcp.ietcount, 16, 16, ch->pcp.iftcount);
    case CHCTRL:
        return ch->pcp.chctrl;
    case EIOFF:
        return deposit32(ch->pcp.eidxs, 16, 16, ch->pcp.eidxd);
    case FIOFF:
        return deposit32(ch->pcp.fidxs, 16, 16, ch->pcp.fidxd);
    default:
        qemu_log_bad_offset(offset);
    };

    return 0;
}

static uint64_t hercules_dma_ram_wcp_read(void *opaque, hwaddr offset,
                                          unsigned size)
{
    HerculesDMAChannel *ch = opaque;

    switch (offset) {
    case CSADDR:
        return ch->wcp.csaddr;
    case CDADDR:
        return ch->wcp.cdaddr;
    case CTCOUNT:
        return deposit32(ch->wcp.cetcount, 16, 16, ch->wcp.cftcount);
    default:
        qemu_log_bad_offset(offset);
        break;
    };

    return 0;
}

static const MemoryRegionOps hercules_dma_ops = {
    .read = hercules_dma_read,
    .write = hercules_dma_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static const MemoryRegionOps hercules_dma_ram_pcp_ops = {
    .read = hercules_dma_ram_pcp_read,
    .write = hercules_dma_ram_pcp_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static const MemoryRegionOps hercules_dma_ram_wcp_ops = {
    .read = hercules_dma_ram_wcp_read,
    .endianness = DEVICE_BIG_ENDIAN,
    .impl = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void hercules_dma_reset(DeviceState *d)
{
    HerculesDMAState *s = HERCULES_DMA(d);
    int i;

    for (i = 0; i < HERCULES_DMA_CHANNEL_NUM; i++) {
        HerculesDMAChannel *ch = &s->channel[i];

        ch->pcp.isaddr   = 0;
        ch->pcp.idaddr   = 0;
        ch->pcp.iftcount = 0;
        ch->pcp.ietcount = 0;
        ch->pcp.chctrl   = 0;
        ch->pcp.eidxd    = 0;
        ch->pcp.eidxs    = 0;
        ch->pcp.fidxd    = 0;
        ch->pcp.fidxs    = 0;

        ch->wcp.csaddr   = 0;
        ch->wcp.cdaddr   = 0;
        ch->wcp.cftcount = 0;
        ch->wcp.cetcount = 0;
    }

    s->hwchena = 0;
    s->btcflag = 0;
    s->gctrl   = 0;

    for (i = 0; i < ARRAY_SIZE(s->dreqasi); i++) {
        s->dreqasi[i] = 0x00010203 + i * 0x04040404;
    }
    hercules_dma_update_reqmap(s);
}

static void hercules_dma_initfn(Object *obj)
{
}

static void hercules_dma_realize(DeviceState *dev, Error **errp)
{
    HerculesDMAState *s = HERCULES_DMA(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    int i;

    memory_region_init_io(&s->iomem, OBJECT(dev), &hercules_dma_ops,
                          s, TYPE_HERCULES_DMA ".io", HERCULES_DMA_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init(&s->ram, OBJECT(dev),
                       TYPE_HERCULES_DMA ".ram",
                       HERCULES_DMA_RAM_SIZE);

    for (i = 0; i < HERCULES_DMA_CHANNEL_NUM; i++) {
        MemoryRegion *io, *parent = &s->ram;
        hwaddr offset;

        io = &s->channel[i].pcp.io;
        memory_region_init_io(io, OBJECT(dev), &hercules_dma_ram_pcp_ops,
                              &s->channel[i],
                              TYPE_HERCULES_DMA ".pcp.io",
                              HERCULES_DMA_PCP_SIZE);
        offset = HERCULES_DMA_PCP_OFFSET + i * HERCULES_DMA_PCP_SIZE;
        memory_region_add_subregion(parent, offset, io);

        io = &s->channel[i].wcp.io;
        memory_region_init_io(io, OBJECT(dev), &hercules_dma_ram_wcp_ops,
                              &s->channel[i],
                              TYPE_HERCULES_DMA ".wcp.io",
                              HERCULES_DMA_WCP_SIZE);
        offset = HERCULES_DMA_WCP_OFFSET + i * HERCULES_DMA_WCP_SIZE;
        memory_region_add_subregion(parent, offset, io);
    }

    sysbus_init_mmio(sbd, &s->ram);

    qdev_init_gpio_in(dev, hercules_dma_set_request, HERCULES_DMA_REQUEST_NUM);
}

static void hercules_dma_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_dma_reset;
    dc->realize = hercules_dma_realize;
}

static const TypeInfo hercules_dma_info = {
    .name          = TYPE_HERCULES_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesDMAState),
    .instance_init = hercules_dma_initfn,
    .class_init    = hercules_dma_class_init,
};

static void hercules_dma_register_types(void)
{
    type_register_static(&hercules_dma_info);
}

type_init(hercules_dma_register_types)
