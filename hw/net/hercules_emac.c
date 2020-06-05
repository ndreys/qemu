/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#include <endian.h>
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "net/eth.h"
#include "sysemu/dma.h"

#include "hw/net/hercules_emac.h"
#include "hw/arm/hercules.h"

typedef struct QEMU_PACKED HerculesCppiDescriptor {
    uint32_t next;

    uint32_t buffer_pointer;
    uint16_t buffer_length;
    uint16_t buffer_offset;

    uint16_t packet_length;
    uint16_t flags;
} HerculesCppiDescriptor;

#define HERCULES_CPPI_RAM_SIZE  (8 * 1024)

enum HerculesCppiDescriptorFlags {
    SOP   = BIT(31 - 16),
    EOP   = BIT(30 - 16),
    OWNER = BIT(29 - 16),
    EOQ   = BIT(28 - 16),
};

#define qemu_log_bad_offset(offset) \
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %" HWADDR_PRIx "\n", \
                  __func__, offset);

enum HerculesEmacControlRegisters {
    SOFTRESET = 0x04,
    RESET = BIT(0),
};

enum HerculesEmacModuleRegister {
    TXCONTROL       = 0x004,
    RXCONTROL       = 0x014,
    RXEN            = BIT(0),
    RXMBPENABLE     = 0x100,
    RXMULTEN        = BIT(5),
    RXBROADEN       = BIT(13),
    RXNOCHAIN       = BIT(28),
    RXUNICASTSET    = 0x104,
    RXUNICASTCLEAR  = 0x108,
    VALID           = BIT(20),
    MATCHFILT       = BIT(19),
    RXBUFFEROFFSET  = 0x110,
    MACCONTROL      = 0x160,
    MACHASH1        = 0x1D8,
    MACHASH2        = 0x1DC,
    MACADDRLO       = 0x500,
    MACADDRHI       = 0x504,
    MACINDEX        = 0x508,
    TX0HDP          = 0x600,
    TX7HDP          = 0x61C,
    RX0HDP          = 0x620,
    RX7HDP          = 0x63C,
    TX0CP           = 0x640,
    TX7CP           = 0x65C,
    RX0CP           = 0x660,
    RX7CP           = 0x67C,
};

#define MACADDRLO_CHANNEL(s, idx)  extract32(s->mac_lo[idx], 16, 3)
#define RXMPBENABLE_RXMULTCH(s)  extract32(s->rxmbpenable, 0, 3)
#define RXMPBENABLE_BROADCH(s)  extract32(s->rxmbpenable, 8, 3)

static uint64_t hercules_emac_control_read(void *opaque, hwaddr offset,
                                           unsigned size)
{
    qemu_log_bad_offset(offset);
    return 0;
}

static void hercules_emac_control_write(void *opaque, hwaddr offset,
                                        uint64_t val, unsigned size)
{
    switch (offset) {
    case SOFTRESET:
        if (val & RESET) {
            device_cold_reset(opaque);
        }
        break;
    default:
        qemu_log_bad_offset(offset);
        break;
    }
}

static void hercules_emac_unmap_iov(QEMUIOVector *qiov)
{
    int i;

    for (i = 0; i < qiov->niov; i++) {
        dma_memory_unmap(&address_space_memory,
                         qiov->iov[i].iov_base,
                         qiov->iov[i].iov_len,
                         DMA_DIRECTION_TO_DEVICE,
                         qiov->iov[i].iov_len);
    };
}

static void hercules_emac_channel_process_tx(HerculesEmacState *s, int idx)
{
    NetClientState *nc = qemu_get_queue(s->nic);
    QEMUIOVector qiov;

    qemu_iovec_init(&qiov, 1);

    while (s->txhdp[idx]) {
        HerculesCppiDescriptor txd;
        dma_addr_t addr, len;
        void *chunk;

        dma_memory_read(&address_space_memory, s->txhdp[idx], &txd,
                        sizeof(txd));

        addr = le32toh(txd.buffer_pointer);
        len  = le16toh(txd.buffer_length);
        if (txd.flags & SOP) {
            addr += le16toh(txd.buffer_offset);
        }

        chunk = dma_memory_map(&address_space_memory,
                               addr, &len, DMA_DIRECTION_TO_DEVICE);

        qemu_iovec_add(&qiov, chunk, (int)len);

        if (le16toh(txd.flags) & EOP) {
            qemu_sendv_packet(nc, qiov.iov, qiov.niov);
            hercules_emac_unmap_iov(&qiov);
            qemu_iovec_reset(&qiov);
        }
        /*
         * We cheat here and clear ownership flag early
         */
        txd.flags &= htole16(~OWNER);
        if (!txd.next) {
            txd.flags |= htole16(EOQ);
        }

        dma_memory_write(&address_space_memory, s->txhdp[idx], &txd,
                         sizeof(txd));

        s->txhdp[idx] = le32toh(txd.next);
    }
    /*
     * Should normally be a no-op. Would be necessary for malformed
     * descriptor chain (lacking EOP marker).
     */
    hercules_emac_unmap_iov(&qiov);
    qemu_iovec_destroy(&qiov);
}

static uint64_t hercules_emac_module_read(void *opaque, hwaddr offset,
                                          unsigned size)
{
    HerculesEmacState *s = opaque;
    NetClientState *nc = qemu_get_queue(s->nic);
    unsigned int base;
    uint32_t *reg;

    switch (offset) {
    case TXCONTROL:
        return s->txcontrol;
    case RXCONTROL:
        return s->rxcontrol;
    case RXMBPENABLE:
        return s->rxmbpenable;
    case RXUNICASTSET:
        /* fall-through */
    case RXUNICASTCLEAR:
        return s->rxunicast;
    case RXBUFFEROFFSET:
        return s->rxbufferoffset;
    case MACCONTROL:
        return s->maccontrol;
    case MACHASH1:
        return s->machash[0];
    case MACHASH2:
        return s->machash[1];
    case MACADDRLO:
        return s->mac_lo[s->macindex];
    case MACADDRHI:
        return s->mac_hi;
    case MACINDEX:
        return s->macindex;
    default:
        qemu_log_bad_offset(offset);
        return 0;
        /*
         * Indexed register handling below
         */
    case TX0HDP ... TX7HDP:
        base = TX0HDP;
        reg = s->txhdp;
        break;
    case RX0HDP ... RX7HDP:
        base = RX0HDP;
        reg = s->rxhdp;
        break;
    case TX0CP ... TX7CP:
        base = TX0CP;
        reg  = s->txcp;
        break;
    case RX0CP ... RX7CP:
        base = RX0CP;
        reg  = s->rxcp;
        break;
    }

    qemu_flush_queued_packets(nc);
    return reg[(offset - base) / sizeof(uint32_t)];
}

static void hercules_emac_update_active_channels(HerculesEmacState *s)
{
    int i;
    s->active_channels = 0;

    for (i = 0; i < HERCULES_EMAC_NUM_CHANNELS; i++) {
        int channel = MACADDRLO_CHANNEL(s, i);

        if (s->mac_lo[i] & VALID && s->mac_lo[i] & MATCHFILT &&
            s->rxunicast & BIT(channel)) {
            s->active_channels |= BIT(i);
        }
    }
}

static void hercules_emac_module_write(void *opaque, hwaddr offset,
                                       uint64_t val64, unsigned size)
{
    HerculesEmacState *s = opaque;
    unsigned int idx, base;
    const uint32_t val = val64;
    uint32_t *reg;

    switch (offset) {
    case TXCONTROL:
        s->txcontrol = val;
        return;
    case RXCONTROL:
        s->rxcontrol = val;
        return;
    case RXMBPENABLE:
        s->rxmbpenable = val;
        return;
    case RXUNICASTSET:
        s->rxunicast |= val;
        hercules_emac_update_active_channels(s);
        return;
    case RXUNICASTCLEAR:
        s->rxunicast &= ~val;
        hercules_emac_update_active_channels(s);
        return;
    case RXBUFFEROFFSET:
        s->rxbufferoffset = val;
        return;
    case MACCONTROL:
        s->maccontrol = val;
        return;
    case MACHASH1:
        s->machash[0] = val;
        return;
    case MACHASH2:
        s->machash[1] = val;
        return;
    case MACADDRLO:
        s->mac_lo[s->macindex] = val;
        hercules_emac_update_active_channels(s);
        return;
    case MACADDRHI:
        s->mac_hi = val;
        return;
    case MACINDEX:
        s->macindex = val & 0b111;
        return;
    default:
        qemu_log_bad_offset(offset);
        return;
        /*
         * Indexed register handling below
         */
    case TX0HDP ... TX7HDP:
        base = TX0HDP;
        reg = s->txhdp;
        break;
    case RX0HDP ... RX7HDP:
        base = RX0HDP;
        reg = s->rxhdp;
        break;
    case TX0CP ... TX7CP:
        base = TX0CP;
        reg  = s->txcp;
        break;
    case RX0CP ... RX7CP:
        base = RX0CP;
        reg  = s->rxcp;
        break;
    }

    idx = (offset - base) / sizeof(uint32_t);
    reg[idx] = val;
    if (base == TX0HDP) {
        hercules_emac_channel_process_tx(s, idx);
    }
}

static bool emac_can_receive(NetClientState *nc)
{
    HerculesEmacState *s = qemu_get_nic_opaque(nc);

    return s->rxcontrol & RXEN;
}

static ssize_t hercules_emac_channel_process_rx(HerculesEmacState *s, int idx,
                                                const uint8_t *buf,
                                                size_t size)
{
    HerculesCppiDescriptor rxd;
    size_t residue = size;
    size_t chunk;
    uint16_t available;
    uint32_t addr;

    while (s->rxhdp[idx] && residue) {
        dma_memory_read(&address_space_memory, s->rxhdp[idx], &rxd,
                        sizeof(rxd));

        /*
         * If this is the first buffer we are processing mark it with SOP
         */
        if (residue == size) {
            rxd.flags |= le16toh(SOP);
            rxd.packet_length = le16toh(size);
            rxd.buffer_offset = s->rxbufferoffset;
        }

        available = le16toh(rxd.buffer_length) - le16toh(rxd.buffer_offset);
        chunk = MIN(size, available);

        if (s->rxmbpenable & RXNOCHAIN) {
            /*
             * If RXNOCHAIN is set we only process as much data as
             * would fit in a single buffer and drop the reset, so we
             * adjust 'residue' accordingly
             */
            residue = chunk;
        }

        addr = le32toh(rxd.buffer_pointer) + le16toh(rxd.buffer_offset);
        dma_memory_write(&address_space_memory,
                         addr,
                         buf, chunk);

        rxd.buffer_length = htole16(chunk);
        rxd.flags &= htole16(~OWNER);

        residue -= chunk;
        buf += chunk;
        /*
         * If there's no more packet data to DMA, mark this buffer
         * with EOP
         */
        if (!residue) {
            rxd.flags |= htole16(EOP);
        }
        /*
         * If this is the last descriptor we can process, set EOQ to
         * indicate that
         */
        if (!rxd.next) {
            rxd.flags |= htole16(EOQ);
        }

        dma_memory_write(&address_space_memory, s->rxhdp[idx], &rxd,
                         sizeof(rxd));

        s->rxcp[idx] = s->rxhdp[idx];
        s->rxhdp[idx] = le32toh(rxd.next);
    }

    return size;
}

static bool hercules_emac_machash_filter(HerculesEmacState *s,
                                         const struct eth_header *ethpacket)
{
    /*
     * Taken from 32.5.37 MAC Hash Address Register 1 (MACHASH1)
     */
    const uint64_t hash_fun[] = {
        BIT(0)  | BIT(6)  | BIT(12) | BIT(18) |
        BIT(24) | BIT(30) | BIT(36) | BIT(42),

        BIT(1)  | BIT(7)  | BIT(13) | BIT(19) |
        BIT(25) | BIT(31) | BIT(37) | BIT(43),

        BIT(2)  | BIT(8)  | BIT(14) | BIT(20) |
        BIT(26) | BIT(32) | BIT(38) | BIT(44),

        BIT(3)  | BIT(9)  | BIT(15) | BIT(21) |
        BIT(27) | BIT(33) | BIT(39) | BIT(45),

        BIT(4)  | BIT(10) | BIT(16) | BIT(22) |
        BIT(28) | BIT(34) | BIT(40) | BIT(46),

        BIT(5)  | BIT(11) | BIT(17) | BIT(23) |
        BIT(29) | BIT(35) | BIT(41) | BIT(47),
    };
    const uint64_t da = be64toh(*(const uint64_t *)ethpacket->h_dest) >> 16;
    unsigned idx = 0;
    int i;

    for (i = 0; i < ARRAY_SIZE(hash_fun); i++) {
        /*
         * Result of XOR is going to be 1 is the number of 1's is odd
         * and 0 if number of 1's is even
         */
        idx |= ctpop64(da & hash_fun[i]) % 2;
        idx <<= 1;
    }

    return s->machash[idx / 32] & BIT(idx % 32);
}

static ssize_t hercules_emac_receive(NetClientState *nc, const uint8_t *buf,
                                     size_t size)
{
    HerculesEmacState *s = qemu_get_nic_opaque(nc);
    const struct eth_header *ethpacket = (const struct eth_header *)buf;
    const uint32_t *h_dest_hi = (const uint32_t *)ethpacket->h_dest;
    const uint16_t *h_dest_lo =
        (const uint16_t *)(ethpacket->h_dest + sizeof(uint32_t));
    unsigned long active_channels = s->active_channels;
    uint32_t channel;
    int i;

    /* Broadcast */
    if (s->rxmbpenable & RXBROADEN &&
        is_broadcast_ether_addr(ethpacket->h_dest)) {

        channel = RXMPBENABLE_BROADCH(s);
        return hercules_emac_channel_process_rx(s, channel, buf, size);
    }

    if (s->rxmbpenable & RXMULTEN &&
        is_multicast_ether_addr(ethpacket->h_dest)) {

        /* Returns true if packet should be filtered */
        if (hercules_emac_machash_filter(s, ethpacket)) {
            return size;
        }

        channel = RXMPBENABLE_RXMULTCH(s);
        return hercules_emac_channel_process_rx(s, channel, buf, size);
    }

    /*
     * Both MACADDRHI stores MAC address as follows:
     *
     *  31-24 MACADDR2 MAC source address bits 23-16 (byte 2).
     *  23-16 MACADDR3 MAC source address bits 31-24 (byte 3).
     *   15-8 MACADDR4 MAC source address bits 39-32 (byte 4).
     *    7-0 MACADDR5 MAC source address bits 47-40 (byte 5).
     *
     * so we intentionally read h_dest_hi as LE
     */
    if (s->mac_hi == le32toh(*h_dest_hi)) {
        for_each_set_bit(i, &active_channels, HERCULES_EMAC_NUM_CHANNELS) {
            /*
             * h_dest_lo is the same as h_dest_hi (see comment above)
             */
            if ((s->mac_lo[i] & 0xffff) == le16toh(*h_dest_lo)) {
                channel = MACADDRLO_CHANNEL(s, i);
                return hercules_emac_channel_process_rx(s, channel, buf, size);
            }
        }
    }

    return size;
}

static void hercules_emac_set_link_status(NetClientState *nc)
{
    /* We don't do anything for now */
}

static void hercules_emac_initfn(Object *obj)
{
    HerculesEmacState *s = HERCULES_EMAC(obj);

    sysbus_init_child_obj(obj, "mdio", &s->mdio,
                          sizeof(UnimplementedDeviceState),
                          TYPE_UNIMPLEMENTED_DEVICE);
}

static void emac_reset(DeviceState *d)
{
    HerculesEmacState *s = HERCULES_EMAC(d);
    NetClientState *const nc = qemu_get_queue(s->nic);

    qemu_purge_queued_packets(nc);

    s->txcontrol       = 0;
    s->rxcontrol       = 0;
    s->maccontrol      = 0;
    s->rxbufferoffset  = 0;
    s->mac_hi          = 0;
    s->macindex        = 0;
    s->rxmbpenable     = 0;
    s->rxunicast       = 0;
    s->active_channels = 0;

    memset(s->mac_lo,  0, sizeof(s->mac_lo));
    memset(s->machash, 0, sizeof(s->machash));
    memset(s->txhdp,   0, sizeof(s->txhdp));
    memset(s->rxhdp,   0, sizeof(s->rxhdp));
    memset(s->txcp,    0, sizeof(s->txcp));
    memset(s->rxcp,    0, sizeof(s->rxcp));
}

static NetClientInfo net_emac_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = emac_can_receive,
    .receive = hercules_emac_receive,
    .link_status_changed = hercules_emac_set_link_status,
};

static void hercules_emac_realize(DeviceState *dev, Error **errp)
{
    HerculesEmacState *s = HERCULES_EMAC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);
    MemoryRegion *io;

    static MemoryRegionOps hercules_emac_module_ops = {
        .read = hercules_emac_module_read,
        .write = hercules_emac_module_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
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

    static MemoryRegionOps hercules_emac_control_ops = {
        .read = hercules_emac_control_read,
        .write = hercules_emac_control_write,
        .endianness = DEVICE_LITTLE_ENDIAN,
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

    if (parent->is_tms570)
    {
        hercules_emac_module_ops.endianness = DEVICE_BIG_ENDIAN;
        hercules_emac_control_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    /*
     * Init controller mmap'd interface
     * 0x000 - 0x800 : emac
     * 0x800 - 0x900 : ctrl
     * 0x900 - 0xA00 : mdio
     */
    memory_region_init_io(&s->module, obj, &hercules_emac_module_ops,
                          s, TYPE_HERCULES_EMAC ".io.module",
                          HERCULES_EMAC_MODULE_SIZE);
    sysbus_init_mmio(sbd, &s->module);

    memory_region_init_io(&s->control, obj, &hercules_emac_control_ops,
                          s, TYPE_HERCULES_EMAC ".io.control",
                          HERCULES_EMAC_CONTROL_SIZE);
    sysbus_init_mmio(sbd, &s->control);

    qdev_prop_set_string(DEVICE(&s->mdio), "name",
                         TYPE_HERCULES_EMAC ".io.mdio");
    qdev_prop_set_uint64(DEVICE(&s->mdio), "size",
                         HERCULES_EMAC_MDIO_SIZE);
    object_property_set_bool(OBJECT(&s->mdio), true, "realized", &error_fatal);
    io = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->mdio), 0);
    sysbus_init_mmio(sbd, io);

    memory_region_init_ram(&s->ram, obj,
                           TYPE_HERCULES_EMAC ".cppi-ram",
                           HERCULES_CPPI_RAM_SIZE, &error_fatal);
    sysbus_init_mmio(sbd, &s->ram);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);

    s->nic = qemu_new_nic(&net_emac_info, &s->conf,
                          object_get_typename(obj),
                          dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static Property hercules_emac_properties[] = {
    DEFINE_NIC_PROPERTIES(HerculesEmacState, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void hercules_emac_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, hercules_emac_properties);
    dc->reset = emac_reset;
    dc->realize = hercules_emac_realize;

    dc->desc = "Hercules EMAC Controller";

    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo hercules_emac_info = {
    .name          = TYPE_HERCULES_EMAC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesEmacState),
    .instance_init = hercules_emac_initfn,
    .class_init    = hercules_emac_class_init,
};

static void hercules_emac_register_types(void)
{
    type_register_static(&hercules_emac_info);
}

type_init(hercules_emac_register_types)
