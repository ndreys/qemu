#ifndef XILINX_SLAVE_SERIAL_H
#define XILINX_SLAVE_SERIAL_H

#include "hw/ssi/ssi.h"

typedef struct XilinxSlaveSerialState {
    /*< private >*/
    SSISlave parent_obj;

    qemu_irq done;
    int state;
} XilinxSlaveSerialState;

#define TYPE_XILINX_SLAVE_SERIAL "xilinx:slave-serial"
#define XILINX_SLAVE_SERIAL(obj) \
    OBJECT_CHECK(XilinxSlaveSerialState, (obj), TYPE_XILINX_SLAVE_SERIAL)

#define XILINX_SLAVE_SERIAL_GPIO_DONE   "xilinx:slave-serial:done"
#define XILINX_SLAVE_SERIAL_GPIO_PROG_B "xilinx:slave-serial:prog-b"

#endif /* XILINX_SLAVE_SERIAL_H */
