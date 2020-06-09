/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_EMAC_H
#define HERCULES_EMAC_H

#include "net/net.h"
#include "hw/misc/unimp.h"

enum {
    HERCULES_EMAC_NUM_CHANNELS = 8,
    HERCULES_EMAC_MODULE_SIZE  = 2 * 1024,
    HERCULES_EMAC_CONTROL_SIZE = 256,
    HERCULES_EMAC_MDIO_SIZE    = 256,
};

typedef struct HerculesEmacState {
    SysBusDevice parent_obj;

    MemoryRegion module;
    MemoryRegion control;
    UnimplementedDeviceState mdio;
    MemoryRegion ram;

    NICState *nic;
    NICConf conf;

    uint32_t mac_lo[HERCULES_EMAC_NUM_CHANNELS];

    uint32_t txcontrol;
    uint32_t rxcontrol;
    uint32_t maccontrol;
    uint16_t rxbufferoffset;
    uint32_t machash[2];

    uint32_t mac_hi;
    uint32_t macindex;
    uint32_t rxmbpenable;
    uint32_t rxunicast;

    uint32_t txhdp[HERCULES_EMAC_NUM_CHANNELS];
    uint32_t rxhdp[HERCULES_EMAC_NUM_CHANNELS];
    uint32_t txcp[HERCULES_EMAC_NUM_CHANNELS];
    uint32_t rxcp[HERCULES_EMAC_NUM_CHANNELS];

    uint32_t active_channels;
} HerculesEmacState;

#define TYPE_HERCULES_EMAC "ti-hercules-emac"
#define HERCULES_EMAC(obj) OBJECT_CHECK(HerculesEmacState, (obj), \
                                        TYPE_HERCULES_EMAC)

#endif
