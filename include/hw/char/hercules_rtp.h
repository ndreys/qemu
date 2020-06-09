/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HW_HERCULES_RTP_H
#define HW_HERCULES_RTP_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_HERCULES_RTP "hercules-rtp"
#define HERCULES_RTP(obj) OBJECT_CHECK(HerculesRTPState, (obj), \
                                       TYPE_HERCULES_RTP)

/* This shares the same struct (and cast macro) as the base pl011 device */

typedef struct HerculesRTPState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    CharBackend chr;
} HerculesRTPState;

#define HERCULES_RTP_SIZE   256

#endif
