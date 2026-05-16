#include "bench_cycles_dwt.h"

/* Minimal CMSIS-free DWT access. Works on Cortex-M3/M4/M7 where DWT exists. */

#define DEMCR (*(volatile uint32_t*)0xE000EDFCu)
#define DEMCR_TRCENA (1u << 24)

#define DWT_CTRL (*(volatile uint32_t*)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t*)0xE0001004u)
#define DWT_CTRL_CYCCNTENA (1u << 0)

void loxbench_cycles_init(void) {
  DEMCR |= DEMCR_TRCENA;
  DWT_CYCCNT = 0u;
  DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

uint32_t loxbench_cycles_now(void) { return DWT_CYCCNT; }

