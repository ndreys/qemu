/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_DMA_H
#define HERCULES_DMA_H

enum {
    HERCULES_DMA_CHANNEL_NUM = 32,
    HERCULES_DMA_REQUEST_NUM = 48,
};

typedef struct HerculesDMAChannel {
    struct {
        MemoryRegion io;

        uint32_t isaddr;
        uint32_t idaddr;
        uint16_t iftcount;
        uint16_t ietcount;
        uint32_t chctrl;
        uint16_t eidxd;
        uint16_t eidxs;
        uint16_t fidxd;
        uint16_t fidxs;
    } pcp;

    struct {
        MemoryRegion io;

        uint32_t csaddr;
        uint32_t cdaddr;
        uint16_t cftcount;
        uint16_t cetcount;
    } wcp;
} HerculesDMAChannel;

typedef struct HerculesDMAState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion ram;

    uint32_t hwchena;
    uint32_t dreqasi[8];
    uint32_t btcflag;
    uint32_t gctrl;

    uint32_t reqmap[HERCULES_DMA_REQUEST_NUM];
    HerculesDMAChannel channel[HERCULES_DMA_CHANNEL_NUM];
} HerculesDMAState;

#define TYPE_HERCULES_DMA "ti-hercules-dma"
#define HERCULES_DMA(obj) OBJECT_CHECK(HerculesDMAState, (obj), \
                                       TYPE_HERCULES_DMA)
#endif
