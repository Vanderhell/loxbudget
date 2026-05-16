#ifndef LOXBENCH_CYCLES_DWT_H
#define LOXBENCH_CYCLES_DWT_H

#include <stdint.h>

/* Cortex-M3/M4/M7 DWT cycle counter helpers.
   Not available on Cortex-M0/M0+. */

void loxbench_cycles_init(void);
uint32_t loxbench_cycles_now(void);

#endif

