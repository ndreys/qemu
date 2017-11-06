/*
 *
 * Copyright (c) 2017, Impinj, Inc.
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/i2c/i2c.h"
#include "hw/misc/pfuze3000.h"
#include "qapi/error.h"
#include "qapi/visitor.h"

#define PFUZE_NUMREGS       128
#define PFUZE100_VOL_OFFSET 0
#define PFUZE100_STANDBY_OFFSET 1
#define PFUZE100_MODE_OFFSET    3
#define PFUZE100_CONF_OFFSET    4

#define PFUZE100_DEVICEID   0x0
#define PFUZE100_REVID      0x3
#define PFUZE100_FABID      0x4

#define PFUZE100_COINVOL    0x1a
#define PFUZE100_SW1ABVOL   0x20
#define PFUZE100_SW1ACONF   0x24
#define PFUZE100_SW1CVOL    0x2e
#define PFUZE100_SW1BCONF   0x32
#define PFUZE100_SW2VOL     0x35
#define PFUZE100_SW3AVOL    0x3c
#define PFUZE100_SW3BVOL    0x43
#define PFUZE100_SW4VOL     0x4a
#define PFUZE100_SWBSTCON1  0x66
#define PFUZE100_VREFDDRCON 0x6a
#define PFUZE100_VSNVSVOL   0x6b
#define PFUZE100_VGEN1VOL   0x6c
#define PFUZE100_VGEN2VOL   0x6d
#define PFUZE100_VGEN3VOL   0x6e
#define PFUZE100_VGEN4VOL   0x6f
#define PFUZE100_VGEN5VOL   0x70
#define PFUZE100_VGEN6VOL   0x71

#define PFUZE100_INVAL      0xff

static int pfuze3000_recv(I2CSlave *i2c)
{
    PFuze3000State *s = PFUZE3000(i2c);

    const uint8_t reg = s->reg;

    s->reg = PFUZE100_INVAL;

    switch (reg) {
    case PFUZE100_DEVICEID:
        return 0x30;
    case PFUZE100_REVID:
        return 0x10;
    case PFUZE100_FABID:
        return 0x00;
    case PFUZE100_COINVOL:
        return s->coinvol;
    case PFUZE100_SW1ABVOL:
        return s->sw1abvol;
    case PFUZE100_SW1ACONF:
        return s->sw1aconf;
    case PFUZE100_SW1CVOL:
        return s->sw1cvol;
    case PFUZE100_SW1BCONF:
        return s->sw1bconf;
    case PFUZE100_SW2VOL:
        return s->sw2vol;
    case PFUZE100_SW3AVOL:
        return s->sw3avol;
    case PFUZE100_SW3BVOL:
        return s->sw3bvol;
    case PFUZE100_SW4VOL:
        return s->sw4vol;
    case PFUZE100_SWBSTCON1:
        return s->swbstcon1;
    case PFUZE100_VREFDDRCON:
        return s->vrefddrcon;
    case PFUZE100_VSNVSVOL:
        return s->vsnvsvol;
    case PFUZE100_VGEN1VOL:
        return s->vgen1vol;
    case PFUZE100_VGEN2VOL:
        return s->vgen2vol;
    case PFUZE100_VGEN3VOL:
        return s->vgen3vol;
    case PFUZE100_VGEN4VOL:
        return s->vgen4vol;
    case PFUZE100_VGEN5VOL:
        return s->vgen5vol;
    case PFUZE100_VGEN6VOL:
        return s->vgen6vol;
    }

    return -EINVAL;
}

static int pfuze3000_send(I2CSlave *i2c, uint8_t data)
{
    PFuze3000State *s = PFUZE3000(i2c);

    switch (s->reg) {
    case PFUZE100_INVAL:
        s->reg = data;
        return 0;

    case PFUZE100_COINVOL:
        s->coinvol = data;
        break;
    case PFUZE100_SW1ABVOL:
        s->sw1abvol = data;
        break;
    case PFUZE100_SW1ACONF:
        s->sw1aconf = data;
        break;
    case PFUZE100_SW1CVOL:
        s->sw1cvol = data;
        break;
    case PFUZE100_SW1BCONF:
        s->sw1bconf = data;
        break;
    case PFUZE100_SW2VOL:
        s->sw2vol = data;
        break;
    case PFUZE100_SW3AVOL:
        s->sw3avol = data;
        break;
    case PFUZE100_SW3BVOL:
        s->sw3bvol = data;
        break;
    case PFUZE100_SW4VOL:
        s->sw4vol = data;
        break;
    case PFUZE100_SWBSTCON1:
        s->swbstcon1 = data;
        break;
    case PFUZE100_VREFDDRCON:
        s->vrefddrcon = data;
        break;
    case PFUZE100_VSNVSVOL:
        s->vsnvsvol = data;
        break;
    case PFUZE100_VGEN1VOL:
        s->vgen1vol = data;
        break;
    case PFUZE100_VGEN2VOL:
        s->vgen2vol = data;
        break;
    case PFUZE100_VGEN3VOL:
        s->vgen3vol = data;
        break;
    case PFUZE100_VGEN4VOL:
        s->vgen4vol = data;
        break;
    case PFUZE100_VGEN5VOL:
        s->vgen5vol = data;
        break;
    case PFUZE100_VGEN6VOL:
        s->vgen6vol = data;
        break;
    }

    s->reg = PFUZE100_INVAL;
    return 0;
}

static void pfuze3000_reset(DeviceState *ds)
{
    PFuze3000State *s = PFUZE3000(ds);

    s->reg = PFUZE100_INVAL;
}

static void pfuze3000_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->reset = pfuze3000_reset;
    k->recv   = pfuze3000_recv;
    k->send   = pfuze3000_send;
}

static const TypeInfo pfuze3000_info = {
    .name          = TYPE_PFUZE3000,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(PFuze3000State),
    .class_init    = pfuze3000_class_init,
};

static void pfuze3000_register_types(void)
{
    type_register_static(&pfuze3000_info);
}
type_init(pfuze3000_register_types)
