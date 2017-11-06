/*
 * PFUZE3000 PMIC emulation
 *
 * Copyright (c) 2017, Impinj, Inc.
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later. See the COPYING file in the top-level directory.
 */
#ifndef PFUZE3000_H
#define PFUZE3000_H

#include "hw/i2c/i2c.h"

#define TYPE_PFUZE3000 "pfuze3000"
#define PFUZE3000(obj) OBJECT_CHECK(PFuze3000State, (obj), TYPE_PFUZE3000)

/**
 * PFUZE3000State:
 */
typedef struct PFuze3000State {
    /*< private >*/
    I2CSlave i2c;

    uint8_t reg;

    uint8_t coinvol;
    uint8_t sw1abvol;
    uint8_t sw1aconf;
    uint8_t sw1cvol;
    uint8_t sw1bconf;
    uint8_t sw2vol;
    uint8_t sw3avol;
    uint8_t sw3bvol;
    uint8_t sw4vol;
    uint8_t swbstcon1;
    uint8_t vrefddrcon;
    uint8_t vsnvsvol;
    uint8_t vgen1vol;
    uint8_t vgen2vol;
    uint8_t vgen3vol;
    uint8_t vgen4vol;
    uint8_t vgen5vol;
    uint8_t vgen6vol;
} PFuze3000State;

#endif
