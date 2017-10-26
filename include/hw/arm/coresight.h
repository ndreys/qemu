#ifndef CORESIGHT_H
#define CORESIGHT_H

#include "hw/sysbus.h"

typedef struct A7MPCoreDAPState {
    /*< private >*/
    SysBusDevice parent_obj;

    MemoryRegion container;

    MemoryRegion ca7_atb_funnel;
    MemoryRegion cpu0_etm;
    MemoryRegion atb_funnel;
    MemoryRegion tmc_etb;
    MemoryRegion tmc_etr;
    MemoryRegion tpiu;

} A7MPCoreDAPState;

#define TYPE_A7MPCORE_DAP "a7mpcore-dap"
#define A7MPCORE_DAP(obj) OBJECT_CHECK(A7MPCoreDAPState, (obj), TYPE_A7MPCORE_DAP)

#endif /* CORESIGHT_H */
