#ifndef LOXBUDGET_MICROHEALTH_ADAPTER_H
#define LOXBUDGET_MICROHEALTH_ADAPTER_H

#include "loxbudget.h"

typedef struct microhealth_t microhealth_t;

/* Minimal external dependency: user must provide these functions.
 *
 * pressure_pct is in the range [0,100] (integer percent).
 * microhealth_subscribe registers a callback invoked when pressure changes.
 */
uint16_t microhealth_pressure_pct(microhealth_t* health);
void microhealth_subscribe(microhealth_t* health, void (*on_change)(void* user), void* user);

typedef struct {
  uint16_t elevated_pct;
  uint16_t critical_pct;
  uint16_t survival_pct;
} loxbudget_pressure_thresholds_t;

void loxbudget_microhealth_attach(loxbudget_t* budget, microhealth_t* health,
                                  const loxbudget_pressure_thresholds_t* t);

#endif
