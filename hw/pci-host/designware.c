/*
 * Xilinx PCIe host controller emulation.
 *
 * Copyright (c) 2016 Imagination Technologies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/pci/msi.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_port.h"
#include "hw/pci-host/designware.h"

#define PCIE_PORT_LINK_CONTROL		0x710

#define PCIE_PHY_DEBUG_R1               0x72C
#define PCIE_PHY_DEBUG_R1_XMLH_LINK_UP	BIT(4)

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C

#define PCIE_MSI_ADDR_LO		0x820
#define PCIE_MSI_ADDR_HI		0x824
#define PCIE_MSI_INTR0_ENABLE		0x828
#define PCIE_MSI_INTR0_MASK		0x82C
#define PCIE_MSI_INTR0_STATUS		0x830

#define PCIE_ATU_VIEWPORT		0x900
#define PCIE_ATU_REGION_INBOUND		(0x1 << 31)
#define PCIE_ATU_REGION_OUTBOUND	(0x0 << 31)
#define PCIE_ATU_REGION_INDEX2		(0x2 << 0)
#define PCIE_ATU_REGION_INDEX1		(0x1 << 0)
#define PCIE_ATU_REGION_INDEX0		(0x0 << 0)
#define PCIE_ATU_CR1			0x904
#define PCIE_ATU_TYPE_MEM		(0x0 << 0)
#define PCIE_ATU_TYPE_IO		(0x2 << 0)
#define PCIE_ATU_TYPE_CFG0		(0x4 << 0)
#define PCIE_ATU_TYPE_CFG1		(0x5 << 0)
#define PCIE_ATU_CR2			0x908
#define PCIE_ATU_ENABLE			(0x1 << 31)
#define PCIE_ATU_BAR_MODE_ENABLE	(0x1 << 30)
#define PCIE_ATU_LOWER_BASE		0x90C
#define PCIE_ATU_UPPER_BASE		0x910
#define PCIE_ATU_LIMIT			0x914
#define PCIE_ATU_LOWER_TARGET		0x918
#define PCIE_ATU_BUS(x)			(((x) >> 24) & 0xff)
#define PCIE_ATU_DEVFN(x)		(((x) >> 16) & 0xff)
#define PCIE_ATU_UPPER_TARGET		0x91C

static DesignwarePCIEHost *
designware_pcie_root_to_host(DesignwarePCIERoot *root)
{
    BusState *bus = qdev_get_parent_bus(DEVICE(root));
    return DESIGNWARE_PCIE_HOST(bus->parent);
}

static void designware_pcie_root_msi_write(void *opaque, hwaddr addr,
                                           uint64_t val, unsigned len)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(opaque);
    DesignwarePCIEHost *host = designware_pcie_root_to_host(root);

    root->msi.intr[0].status |= (1 << val) & root->msi.intr[0].enable;

    if (root->msi.intr[0].status & ~root->msi.intr[0].mask)
        qemu_set_irq(host->pci.irqs[0], 1);
}

const MemoryRegionOps designware_pci_host_msi_ops = {
    .write = designware_pcie_root_msi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void designware_pcie_root_update_msi_mapping(DesignwarePCIERoot *root)

{
    DesignwarePCIEHost *host = designware_pcie_root_to_host(root);
    MemoryRegion *address_space = &host->pci.memory;
    MemoryRegion *mem = &root->msi.iomem;
    const uint64_t base = root->msi.base;
    const bool enable = root->msi.intr[0].enable;

    if (memory_region_is_mapped(mem)) {
        memory_region_del_subregion(address_space, mem);
        object_unparent(OBJECT(mem));
    }

    if (enable) {
        memory_region_init_io(mem, OBJECT(root),  &designware_pci_host_msi_ops,
                              root, "pcie-msi", 0x1000);

        memory_region_add_subregion(address_space, base, mem);
    }
}

static DesignwarePCIEViewport *
designware_pcie_root_get_current_viewport(DesignwarePCIERoot *root)
{
    const unsigned int idx = root->atu_viewport & 0xF;
    const unsigned int dir = !!(root->atu_viewport & PCIE_ATU_REGION_INBOUND);
    return &root->viewports[dir][idx];
}

static uint32_t
designware_pcie_root_config_read(PCIDevice *d, uint32_t address, int len)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(d);
    DesignwarePCIEViewport *viewport =
        designware_pcie_root_get_current_viewport(root);

    uint32_t val;

    switch (address) {
    case PCIE_PORT_LINK_CONTROL:
    case PCIE_LINK_WIDTH_SPEED_CONTROL:
        val = 0xdeadbeef;
        /* No-op */
        break;

    case PCIE_MSI_ADDR_LO:
        val = root->msi.base;
        break;

    case PCIE_MSI_ADDR_HI:
        val = root->msi.base >> 32;
        break;

    case PCIE_MSI_INTR0_ENABLE:
        val = root->msi.intr[0].enable;
        break;

    case PCIE_MSI_INTR0_MASK:
        val = root->msi.intr[0].mask;
        break;

    case PCIE_MSI_INTR0_STATUS:
        val = root->msi.intr[0].status;
        break;

    case PCIE_PHY_DEBUG_R1:
        val = PCIE_PHY_DEBUG_R1_XMLH_LINK_UP;
        break;

    case PCIE_ATU_VIEWPORT:
        val = root->atu_viewport;
        break;

    case PCIE_ATU_LOWER_BASE:
        val = viewport->base;
        break;

    case PCIE_ATU_UPPER_BASE:
        val = viewport->base >> 32;
        break;

    case PCIE_ATU_LOWER_TARGET:
        val = viewport->target;
        break;

    case PCIE_ATU_UPPER_TARGET:
        val = viewport->target >> 32;
        break;

    case PCIE_ATU_LIMIT:
        val = viewport->limit;
        break;

    case PCIE_ATU_CR1:
    case PCIE_ATU_CR2:          /* FALLTHROUGH */
        val = viewport->cr[(address - PCIE_ATU_CR1) / sizeof (uint32_t)];
        break;

    default:
        val = pci_default_read_config(d, address, len);
        break;
    }

    return val;
}

static uint64_t designware_pcie_root_data_read(void *opaque,
                                               hwaddr addr, unsigned len)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(opaque);
    DesignwarePCIEViewport *viewport =
        designware_pcie_root_get_current_viewport(root);

    const uint8_t busnum = PCIE_ATU_BUS(viewport->target);
    const uint8_t devfn  = PCIE_ATU_DEVFN(viewport->target);
    PCIBus    *pcibus    = PCI_DEVICE(root)->bus;
    PCIDevice *pcidev    = pci_find_device(pcibus, busnum, devfn);

    if (!pcidev)
        return UINT64_MAX;

    addr &= PCI_CONFIG_SPACE_SIZE - 1;

    return pci_host_config_read_common(pcidev, addr,
                                       PCI_CONFIG_SPACE_SIZE, len);
}

static void designware_pcie_root_data_write(void *opaque, hwaddr addr,
                                            uint64_t val, unsigned len)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(opaque);
    DesignwarePCIEViewport *viewport =
        designware_pcie_root_get_current_viewport(root);
    const uint8_t busnum = PCIE_ATU_BUS(viewport->target);
    const uint8_t devfn  = PCIE_ATU_DEVFN(viewport->target);
    PCIBus    *pcibus    = PCI_DEVICE(root)->bus;
    PCIDevice *pcidev    = pci_find_device(pcibus, busnum, devfn);

    if (pcidev) {
        addr &= PCI_CONFIG_SPACE_SIZE - 1;
        pci_host_config_write_common(pcidev, addr,
                                     PCI_CONFIG_SPACE_SIZE,
                                     val, len);
    }
}

const MemoryRegionOps designware_pci_host_conf_ops = {
    .read = designware_pcie_root_data_read,
    .write = designware_pcie_root_data_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void designware_pcie_update_viewport(DesignwarePCIERoot *root,
                                            DesignwarePCIEViewport *viewport)
{
    DesignwarePCIEHost *host = designware_pcie_root_to_host(root);

    MemoryRegion *mem     = &viewport->memory;
    const uint64_t target = viewport->target;
    const uint64_t base   = viewport->base;
    const uint64_t size   = (uint64_t)viewport->limit - base + 1;
    const bool inbound    = viewport->inbound;

    MemoryRegion *source, *destination;
    const char *direction;
    char *name;

    if (inbound) {
        source      = &host->pci.address_space_root;
        destination = get_system_memory();
        direction   = "Inbound";
    } else {
        source      = get_system_memory();
        destination = &host->pci.memory;
        direction   = "Outbound";
    }

    if (memory_region_is_mapped(mem)) {
        /* Before we modify anything, unmap and destroy the region */
        memory_region_del_subregion(source, mem);
        object_unparent(OBJECT(mem));
    }

    name = g_strdup_printf("PCI %s Viewport %p", direction, viewport);

    switch (viewport->cr[0]) {
    case PCIE_ATU_TYPE_MEM:
        memory_region_init_alias(mem, OBJECT(root), name,
                                 destination, target, size);
        break;
    case PCIE_ATU_TYPE_CFG0:
    case PCIE_ATU_TYPE_CFG1:    /* FALLTHROUGH */
        if (inbound)
            goto exit;

        memory_region_init_io(mem, OBJECT(root),
                              &designware_pci_host_conf_ops,
                              root, name, size);
        break;
    }

    if (inbound)
        memory_region_add_subregion_overlap(source, base,
                                            mem, -1);
    else
        memory_region_add_subregion(source, base, mem);

 exit:
    g_free(name);
}

static void designware_pcie_root_config_write(PCIDevice *d, uint32_t address,
                                              uint32_t val, int len)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(d);
    DesignwarePCIEHost *host = designware_pcie_root_to_host(root);
    DesignwarePCIEViewport *viewport =
        designware_pcie_root_get_current_viewport(root);

    switch (address) {
    case PCIE_PORT_LINK_CONTROL:
    case PCIE_LINK_WIDTH_SPEED_CONTROL:
    case PCIE_PHY_DEBUG_R1:
        /* No-op */
        break;

    case PCIE_MSI_ADDR_LO:
        root->msi.base &= 0xFFFFFFFF00000000ULL;
        root->msi.base |= val;
        break;

    case PCIE_MSI_ADDR_HI:
        root->msi.base &= 0x00000000FFFFFFFFULL;
        root->msi.base |= (uint64_t)val << 32;
        break;

    case PCIE_MSI_INTR0_ENABLE: {
        const bool update_msi_mapping = !root->msi.intr[0].enable ^ !!val;

        root->msi.intr[0].enable = val;

        if (update_msi_mapping) {
            designware_pcie_root_update_msi_mapping(root);
        }
        break;
    }

    case PCIE_MSI_INTR0_MASK:
        root->msi.intr[0].mask = val;
        break;

    case PCIE_MSI_INTR0_STATUS:
        root->msi.intr[0].status ^= val;
        if (!root->msi.intr[0].status)
            qemu_set_irq(host->pci.irqs[0], 0);
        break;

    case PCIE_ATU_VIEWPORT:
        root->atu_viewport = val;
        break;

    case PCIE_ATU_LOWER_BASE:
        viewport->base &= 0xFFFFFFFF00000000ULL;
        viewport->base |= val;
        break;

    case PCIE_ATU_UPPER_BASE:
        viewport->base &= 0x00000000FFFFFFFFULL;
        viewport->base |= (uint64_t)val << 32;
        break;

    case PCIE_ATU_LOWER_TARGET:
        viewport->target &= 0xFFFFFFFF00000000ULL;
        viewport->target |= val;
        break;

    case PCIE_ATU_UPPER_TARGET:
        viewport->target &= 0x00000000FFFFFFFFULL;
        viewport->target |= val;
        break;

    case PCIE_ATU_LIMIT:
        viewport->limit = val;
        break;

    case PCIE_ATU_CR1:
        viewport->cr[0] = val;
        break;
    case PCIE_ATU_CR2:
        viewport->cr[1] = val;

        if (viewport->cr[1] & PCIE_ATU_ENABLE) {
            designware_pcie_update_viewport(root, viewport);
         }
        break;

    default:
        pci_bridge_write_config(d, address, val, len);
        break;
    }
}

static int designware_pcie_root_init(PCIDevice *dev)
{
    DesignwarePCIERoot *root = DESIGNWARE_PCIE_ROOT(dev);
    PCIBridge *br = PCI_BRIDGE(dev);
    DesignwarePCIEViewport *viewport;
    size_t i;

    br->bus_name  = "dw-pcie";

    pci_set_word(dev->config + PCI_COMMAND,
                 PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

    pci_config_set_interrupt_pin(dev->config, 1);
    pci_bridge_initfn(dev, TYPE_PCI_BUS);

    pcie_port_init_reg(dev);

    pcie_cap_init(dev, 0x70, PCI_EXP_TYPE_ROOT_PORT,
                  0, &error_fatal);

    msi_nonbroken = true;
    msi_init(dev, 0x50, 32, true, true, &error_fatal);

    for (i = 0; i < DESIGNWARE_PCIE_NUM_VIEWPORTS; i++) {
        viewport = &root->viewports[DESIGNWARE_PCIE_VIEWPORT_INBOUND][i];
        viewport->inbound = true;
    }

    /*
     * If no inbound iATU windows are configured, HW defaults to
     * letting inbound TLPs to pass in. We emulate that by exlicitly
     * configuring first inbound window to cover all of target's
     * address space.
     *
     * NOTE: This will not work correctly for the case when first
     * configured inbound window is window 0
     */
    viewport = &root->viewports[DESIGNWARE_PCIE_VIEWPORT_INBOUND][0];
    viewport->base   = 0x0000000000000000ULL;
    viewport->target = 0x0000000000000000ULL;
    viewport->limit  = UINT32_MAX;
    viewport->cr[0]  = PCIE_ATU_TYPE_MEM;

    designware_pcie_update_viewport(root, viewport);

    return 0;
}

static void designware_pcie_set_irq(void *opaque, int irq_num, int level)
{
    DesignwarePCIEHost *host = DESIGNWARE_PCIE_HOST(opaque);

    qemu_set_irq(host->pci.irqs[irq_num], level);
}

static const char *designware_pcie_host_root_bus_path(PCIHostState *host_bridge,
                                                      PCIBus *rootbus)
{
    return "0000:00";
}


static void designware_pcie_root_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);

    k->vendor_id = PCI_VENDOR_ID_SYNOPSYS;
    k->device_id = 0xABCD;
    k->revision = 0;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    k->is_express = true;
    k->is_bridge = true;
    k->init = designware_pcie_root_init;
    k->exit = pci_bridge_exitfn;
    dc->reset = pci_bridge_reset;
    k->config_read = designware_pcie_root_config_read;
    k->config_write = designware_pcie_root_config_write;

    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
}

static uint64_t designware_pcie_host_mmio_read(void *opaque, hwaddr addr,
                                               unsigned int size)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(opaque);
    PCIDevice *device = pci_find_device(pci->bus, 0, 0);

    return pci_host_config_read_common(device,
                                       addr,
                                       pci_config_size(device),
                                       size);
}

static void designware_pcie_host_mmio_write(void *opaque, hwaddr addr,
                                            uint64_t val, unsigned int size)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(opaque);
    PCIDevice *device = pci_find_device(pci->bus, 0, 0);

    return pci_host_config_write_common(device,
                                        addr,
                                        pci_config_size(device),
                                        val, size);
}

static const MemoryRegionOps designware_pci_mmio_ops = {
    .read       = designware_pcie_host_mmio_read,
    .write      = designware_pcie_host_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static AddressSpace *designware_pcie_host_set_iommu(PCIBus *bus, void *opaque,
                                                    int devfn)
{
    DesignwarePCIEHost *s = DESIGNWARE_PCIE_HOST(opaque);

    return &s->pci.address_space;
}

static void designware_pcie_host_realize(DeviceState *dev, Error **errp)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(dev);
    DesignwarePCIEHost *s = DESIGNWARE_PCIE_HOST(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    size_t i;

    for (i = 0; i < ARRAY_SIZE(s->pci.irqs); i++) {
        sysbus_init_irq(sbd, &s->pci.irqs[i]);
    }

    memory_region_init_io(&s->mmio,
                          OBJECT(s),
                          &designware_pci_mmio_ops,
                          s,
                          "pcie.reg", 4 * 1024);
    sysbus_init_mmio(sbd, &s->mmio);

    memory_region_init(&s->pci.io, OBJECT(s), "pcie-pio", 16);
    memory_region_init(&s->pci.memory, OBJECT(s),
                       "pcie-bus-memory",
                       UINT64_MAX);

    pci->bus = pci_register_bus(dev, "pcie",
                                designware_pcie_set_irq,
                                pci_swizzle_map_irq_fn,
                                s,
                                &s->pci.memory,
                                &s->pci.io,
                                0, 4,
                                TYPE_PCIE_BUS);

    memory_region_init(&s->pci.address_space_root,
                       OBJECT(s),
                       "pcie-bus-address-space-root",
                       UINT64_MAX);
    memory_region_add_subregion(&s->pci.address_space_root,
                                0x0, &s->pci.memory);
    address_space_init(&s->pci.address_space,
                       &s->pci.address_space_root,
                       "pcie-bus-address-space");
    pci_setup_iommu(pci->bus, designware_pcie_host_set_iommu, s);

    qdev_set_parent_bus(DEVICE(&s->root), BUS(pci->bus));
    qdev_init_nofail(DEVICE(&s->root));
}

static void designware_pcie_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = designware_pcie_host_root_bus_path;
    dc->realize = designware_pcie_host_realize;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "pci";
}

static void designware_pcie_host_init(Object *obj)
{
    DesignwarePCIEHost *s = DESIGNWARE_PCIE_HOST(obj);
    DesignwarePCIERoot *root = &s->root;

    object_initialize(root, sizeof(*root), TYPE_DESIGNWARE_PCIE_ROOT);
    object_property_add_child(obj, "root", OBJECT(root), NULL);
    qdev_prop_set_int32(DEVICE(root), "addr", PCI_DEVFN(0, 0));
    qdev_prop_set_bit(DEVICE(root), "multifunction", false);
}

static const TypeInfo designware_pcie_root_info = {
    .name = TYPE_DESIGNWARE_PCIE_ROOT,
    .parent = TYPE_PCI_BRIDGE,
    .instance_size = sizeof(DesignwarePCIERoot),
    .class_init = designware_pcie_root_class_init,
};

static const TypeInfo designware_pcie_host_info = {
    .name       = TYPE_DESIGNWARE_PCIE_HOST,
    .parent     = TYPE_PCI_HOST_BRIDGE,
    .instance_size = sizeof(DesignwarePCIEHost),
    .instance_init = designware_pcie_host_init,
    .class_init = designware_pcie_host_class_init,
};

static void designware_pcie_register(void)
{
    type_register_static(&designware_pcie_root_info);
    type_register_static(&designware_pcie_host_info);
}
type_init(designware_pcie_register)

/* 00:00.0 Class 0604: 16c3:abcd */
