#ifndef HERCULES_SCM_H
#define HERCULES_SCM_H

#include "hercules_system.h"

typedef struct HerculesSCMState {
    SysBusDevice parent_obj;

    struct {
        MemoryRegion scm;
        MemoryRegion sdr_mmr;
    } io;

    uint32_t scmcntrl;

    uint32_t sdc_status;

    QEMUBH *self_test;
    qemu_irq icrst;
} HerculesSCMState;

#define TYPE_HERCULES_SCM "ti-hercules-scm"
#define HERCULES_SCM(obj) OBJECT_CHECK(HerculesSCMState, (obj), \
                                       TYPE_HERCULES_SCM)
#endif
