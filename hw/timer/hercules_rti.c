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
#include "hw/ptimer.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qapi/error.h"

#include "hw/timer/hercules_rti.h"
#include "hw/arm/hercules.h"

enum HerculesRtiRegisters {
    RTIGCTRL       = 0x00,
    RTICOMPCTRL    = 0x0C,
    RTIFRC0        = 0x10,
    RTICPUC0       = 0x18,
    RTICPUC1       = 0x38,
    COMPSEL0       = BIT(0),
    COMPSEL1       = BIT(4),
    COMPSEL2       = BIT(8),
    COMPSEL3       = BIT(12),
    COMPSEL_ALL    = COMPSEL0 | COMPSEL1 | COMPSEL2 | COMPSEL3,
    RTICOMP0       = 0x50,
    RTIUDCP0       = 0x54,
    RTISETINTENA   = 0x80,
    RTICLEARINTENA = 0x84,
    RTIINTFLAG     = 0x88,
};

#define CNTnEN(n)    BIT(n)

static void hercules_rti_update_irq(HerculesRtiState *s,
                                    unsigned long changed)
{
    const uint32_t masked = s->intflag & s->intena;
    int i;

    for_each_set_bit(i, &changed, 32) {
        const int group = i / HERCULES_RTI_INT_PER_GROUP;
        const int line  = i % HERCULES_RTI_INT_PER_GROUP;

        qemu_set_irq(s->irq[group][line], masked & BIT(i));
    }
}

static bool hercules_rti_frc_is_enabled(const HerculesRtiFrc *frc)
{
    return frc->enabled && frc->gctrl_en & frc->rti->gctrl;
}

static void hercules_rti_set_frc(HerculesRtiFrc *frc, uint32_t value)
{
    frc->timestamp = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    frc->counter = value;
}

static void __hercules_rti_compare_event(HerculesRtiCompareModule *c,
                                         bool update_timer)
{
    c->rti->intflag |= c->mask;

    if (c->udcp) {
        const int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

        hercules_rti_set_frc(c->frc, c->comp);
        c->comp += c->udcp;

        if (update_timer) {
            timer_mod(c->timer, now + c->udcp_ns);
        }
    }

    hercules_rti_update_irq(c->rti, c->mask);
}

static void hercules_rti_compare_event(void *opaque)
{
    __hercules_rti_compare_event(opaque, true);
}

static uint32_t hercules_rti_get_frc(HerculesRtiFrc *frc)
{
    if (hercules_rti_frc_is_enabled(frc)) {
        const int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        const int64_t delta_ns = now - frc->timestamp;
        const uint32_t result = frc->counter + delta_ns / frc->period;

        hercules_rti_set_frc(frc, result);

        return result;
    }

    return frc->counter;
}

static uint64_t hercules_rti_read(void *opaque, hwaddr offset, unsigned size)
{
    HerculesRtiState *s = opaque;
    HerculesRtiCompareModule *c;
    uint32_t frc;

    switch (offset) {
    case RTIGCTRL:
        return s->gctrl;
    case RTIFRC0:
        frc = hercules_rti_get_frc(&s->frc[0]);
        return frc;
    case RTICPUC0:
        return s->frc[0].cpuc;
    case RTICOMP0:
        c = &s->compare[0];
        return c->comp;
    case RTIUDCP0:
        c = &s->compare[0];
        return c->udcp;
    case RTISETINTENA:
    case RTICLEARINTENA:
        return s->intena;
    case RTIINTFLAG:
        return s->intflag;
    default:
        return 0;
    }
}

static void __hercules_rti_update_capture(HerculesRtiState *s,
                                          unsigned long compctrl)
{
    int i;

    /*
     * We go through all of the capture and compare channels specified
     * by @compctrl and and either enable or disable corresponding
     * timer (based on the value passed in @on)
     */
    for_each_set_bit(i, &compctrl, 32) {
        /*
         * COMPSEL1,2,3,4 are bits 0, 4, 8 and 12, so we need to
         * divide bit number by 4 to get corresponding compare module
         */
        HerculesRtiCompareModule *c = &s->compare[i / 4];
        const uint32_t scale = c->frc->cpuc * c->frc->period;

        if (c->udcp) {
            /*
             * If RTIUDCPn is specified, let's cache its value in
             * nanoseconds for future use.
             */
            c->udcp_ns = c->udcp * scale;
        }

        if (hercules_rti_frc_is_enabled(c->frc)) {
            const int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
            uint32_t counter = hercules_rti_get_frc(c->frc);
            uint32_t delta;

            if (c->comp <= counter) {
                delta = UINT32_MAX - counter + c->comp;
                /*
                 * TODO: Emulate overflow interrupt
                 */
            } else {
                delta = c->comp - counter;
            }

            timer_mod(c->timer, now + delta * scale);
        } else {
            timer_del(c->timer);
        }
    }
}

static uint32_t hercules_rti_compctrl(HerculesRtiState *s, unsigned int idx)
{
    /*
     * All compare channels that are running off of free-running
     * counter 0 will have their COMPSELn bits set to 0, so we need to
     * invert COMPCTRL before we mask it by compsel to flip those
     * zeros to ones
     */
    return idx ? s->compctrl : ~s->compctrl;
}

static void
hercules_rti_update_capture_cnt(HerculesRtiState *s, unsigned int idx,
                                uint32_t compsel)
{
    __hercules_rti_update_capture(s, hercules_rti_compctrl(s, idx) & compsel);
}

static void hercules_rti_update_capture(HerculesRtiState *s, uint32_t gctrl,
                                        uint32_t compsel)
{
    if (gctrl & CNTnEN(0)) {
        hercules_rti_update_capture_cnt(s, 0, compsel);
    }

    if (gctrl & CNTnEN(1)) {
        hercules_rti_update_capture_cnt(s, 1, compsel);
    }
}

static void hercules_rti_write(void *opaque, hwaddr offset, uint64_t val64,
                               unsigned size)
{
    HerculesRtiState *s = opaque;
    HerculesRtiCompareModule *c;
    const uint32_t val = val64;
    uint32_t gctrl;
    uint32_t intflag;
    uint32_t intena;

    switch (offset) {
    case RTIGCTRL:
        /*
         * We only update timer settings for free-running counter
         * whose state was changed by this write. "on" -> "on" and
         * "off" -> "off" transitions should be a no-op
         */
        gctrl = s->gctrl ^ val;
        s->gctrl = val;
        hercules_rti_update_capture(s, gctrl, COMPSEL_ALL);
        break;
    case RTIFRC0:
        hercules_rti_set_frc(&s->frc[0], val);
        break;
    case RTICPUC0:
        hercules_rti_get_frc(&s->frc[0]);
        s->frc[0].cpuc = val;
        hercules_rti_update_capture_cnt(s, 0, COMPSEL_ALL);
        break;
    case RTICPUC1:
        s->frc[1].cpuc = val;
        hercules_rti_update_capture_cnt(s, 1, COMPSEL_ALL);
        break;
    case RTICOMP0:
        c = &s->compare[0];
        c->comp = val;
        hercules_rti_update_capture(s,  s->gctrl, COMPSEL0);
        break;
    case RTIUDCP0:
        c = &s->compare[0];
        c->udcp = val;
        hercules_rti_update_capture(s,  s->gctrl, COMPSEL0);
        break;
    case RTISETINTENA:
        intena = s->intena;
        s->intena |= val;
        hercules_rti_update_irq(s, intena ^ s->intena);
        break;
    case RTICLEARINTENA:
        intena = s->intena;
        s->intena &= ~val;
        hercules_rti_update_irq(s, intena ^ s->intena);
        break;
    case RTIINTFLAG:
        intflag = s->intflag;
        s->intflag &= ~val;
        hercules_rti_update_irq(s, intflag ^ s->intflag);
        break;
    default:
        return;
    }
}

static void hercules_rti_init_irq_group(HerculesRtiState *s, int group,
                                        int line_num)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    int i;

    s->irq[group] = g_new(qemu_irq, line_num);
    for (i = 0; i < line_num; i++) {
        sysbus_init_irq(sbd, &s->irq[group][i]);
    }
}

static void hercules_rti_reset_irq_group(HerculesRtiState *s, int group,
                                        int line_num)
{
    int i;
    for (i = 0; i < line_num; i++) {
        qemu_irq_lower(s->irq[group][i]);
    }
}

static void hercules_rti_realize(DeviceState *dev, Error **errp)
{
    HerculesRtiState *s = HERCULES_RTI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Object *obj = OBJECT(dev);
    HerculesState *parent = HERCULES_SOC(obj->parent);
    int i;

    static MemoryRegionOps hercules_rti_ops = {
        .read = hercules_rti_read,
        .write = hercules_rti_write,
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
        hercules_rti_ops.endianness = DEVICE_BIG_ENDIAN;
    }

    memory_region_init_io(&s->iomem, obj, &hercules_rti_ops, s,
                          TYPE_HERCULES_RTI ".io",
                          HERCULES_RTI_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    for (i = 0; i < HERCULES_RTI_INT_LINE_COMPARE_NUM; i++) {
        HerculesRtiCompareModule *c = &s->compare[i];

        c->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                hercules_rti_compare_event,
                                c);
        c->mask  = BIT(i);
        c->rti   = s;
    }

    for (i = 0; i < HERCULES_RTI_FRC_NUM; i++) {
        HerculesRtiFrc *frc = &s->frc[i];
        /*
         * Hardcode to 75 Mhz clock ~13ns per tick
         */
        frc->period = 13;
        frc->rti = s;
        frc->gctrl_en = CNTnEN(i);
    }

    hercules_rti_init_irq_group(s, HERCULES_RTI_INT_GROUP_COMPARE,
                                HERCULES_RTI_INT_LINE_COMPARE_NUM);

    hercules_rti_init_irq_group(s, HERCULES_RTI_INT_GROUP_DMA,
                                HERCULES_RTI_INT_LINE_DMA_NUM);

    hercules_rti_init_irq_group(s, HERCULES_RTI_INT_GROUP_TBOVL,
                                HERCULES_RTI_INT_LINE_TBOVL_NUM);
}

static void hercules_compare_adjust_frc(HerculesRtiState *s)
{
    int i;

    for (i = 0; i < HERCULES_RTI_INT_LINE_COMPARE_NUM; i++) {
        s->compare[i].frc = &s->frc[!!(s->compctrl & BIT(4 * i))];
    }
}

static void hercules_rti_reset(DeviceState *d)
{
    HerculesRtiState *s = HERCULES_RTI(d);
    int i;

    s->gctrl    = 0;
    s->intflag  = 0;
    s->intena   = 0;
    s->compctrl = 0;

    for (i = 0; i < HERCULES_RTI_FRC_NUM; i++) {
        HerculesRtiFrc *frc = &s->frc[i];

        frc->counter = 0;
        frc->cpuc = 0;
        frc->timestamp = 0;

        frc->enabled = true;
    }

    hercules_compare_adjust_frc(s);

    for (i = 0; i < HERCULES_RTI_INT_LINE_COMPARE_NUM; i++) {
        HerculesRtiCompareModule *c = &s->compare[i];

        timer_del(c->timer);
        c->comp = 0;
        c->udcp = 0;
        c->udcp_ns = 0;
    }

    hercules_rti_reset_irq_group(s, HERCULES_RTI_INT_GROUP_COMPARE,
                                 HERCULES_RTI_INT_LINE_COMPARE_NUM);

    hercules_rti_reset_irq_group(s, HERCULES_RTI_INT_GROUP_DMA,
                                 HERCULES_RTI_INT_LINE_DMA_NUM);

    hercules_rti_reset_irq_group(s, HERCULES_RTI_INT_GROUP_TBOVL,
                                 HERCULES_RTI_INT_LINE_TBOVL_NUM);
}

static void hercules_rti_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = hercules_rti_reset;
    dc->realize = hercules_rti_realize;
}

static const TypeInfo hercules_rti_info = {
    .name          = TYPE_HERCULES_RTI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HerculesRtiState),
    .class_init    = hercules_rti_class_init,
};

static void hercules_rti_register_types(void)
{
    type_register_static(&hercules_rti_info);
}

type_init(hercules_rti_register_types)

void hercules_rti_counter_enable(HerculesRtiState *s, uint32_t idx,
                                 bool enable)
{
    if (s->frc[idx].enabled != enable) {
        uint32_t compctrl = hercules_rti_compctrl(s, idx) & COMPSEL_ALL;
        s->frc[idx].enabled = enable;
        __hercules_rti_update_capture(s, compctrl);
    }
}

static HerculesRtiCompareModule *
hercules_rti_next_active_compare(HerculesRtiState *s, unsigned long compctrl,
                                 uint32_t now, uint32_t dest)
{
    HerculesRtiCompareModule *active = NULL;
    uint32_t deadline = dest;
    int i;

    for_each_set_bit(i, &compctrl, 32) {
        HerculesRtiCompareModule *c = &s->compare[i / 4];

        if (c->comp < now) {
            /*
             * skip compare channels that are in the "past"
             */
            continue;
        }

        if (c->comp < deadline) {
            /*
             * for the ones that aren't - find the soonest one to
             * expire
             */
            deadline = c->comp;
            active   = c;
        }
    }

    return active;
}

void hercules_rti_counter_advance(HerculesRtiState *s, uint32_t idx,
                                  uint32_t delta)
{
    HerculesRtiCompareModule *c;
    bool needs_disabling = s->frc[idx].enabled;
    uint32_t now  = s->frc[idx].counter;
    uint32_t dest = now + delta;
    uint32_t compctrl = hercules_rti_compctrl(s, idx) & COMPSEL_ALL;

    if (needs_disabling) {
        hercules_rti_counter_enable(s, idx, false);
    }

    while ((c = hercules_rti_next_active_compare(s, compctrl, now, dest))) {
        now = c->comp;
        __hercules_rti_compare_event(c, false);
    }

    s->frc[idx].counter = dest;

    if (needs_disabling) {
        hercules_rti_counter_enable(s, idx, true);
    }
}
