/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2019, Blue Origin.
 *
 * Authors: Andrey Smirnov <andrew.smirnov@gmail.com>
 *          Benjamin Kamath <kamath.ben@gmail.com>
 */
#ifndef HERCULES_ECAP_H
#define HERCULES_ECAP_H

#include "hercules_system.h"

enum {
    HERCULES_ECAP_NUM_CAPS = 4,
};

typedef struct HerculesECAPState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t cap[HERCULES_ECAP_NUM_CAPS];
    uint16_t ecflg;

} HerculesECAPState;

#define TYPE_HERCULES_ECAP "ti-hercules-ecap"
#define HERCULES_ECAP(obj) OBJECT_CHECK(HerculesECAPState, (obj), \
                                       TYPE_HERCULES_ECAP)
#endif
