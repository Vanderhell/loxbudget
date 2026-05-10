#ifndef LOXBUDGET_MICROBUS_ADAPTER_H
#define LOXBUDGET_MICROBUS_ADAPTER_H

#include "loxbudget.h"

typedef struct microbus_t microbus_t;

/* Minimal external dependency: user must provide this function. */
void microbus_publish_event(microbus_t* bus, uint32_t event_id, uint32_t a, uint32_t b, uint32_t c);

typedef enum {
  LOXBUDGET_MICROBUS_PRESSURE_CHANGED = 1,
  LOXBUDGET_MICROBUS_BUDGET_DENIED = 2,
  LOXBUDGET_MICROBUS_LOCKDOWN_ENTERED = 3
} loxbudget_microbus_event_t;

void loxbudget_microbus_attach(loxbudget_t* budget, microbus_t* bus);

/* Convenience wrapper for emitting PRESSURE_CHANGED via microbus. */
loxbudget_status_t loxbudget_microbus_set_pressure(loxbudget_t* budget, microbus_t* bus,
                                                   loxbudget_pressure_t pressure);

#endif
