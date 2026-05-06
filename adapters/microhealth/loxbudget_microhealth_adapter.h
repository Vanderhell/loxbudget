#ifndef LOXBUDGET_MICROHEALTH_ADAPTER_H
#define LOXBUDGET_MICROHEALTH_ADAPTER_H

#include "loxbudget.h"

typedef struct microhealth_t microhealth_t;

typedef struct {
  uint16_t elevated_pct;
  uint16_t critical_pct;
  uint16_t survival_pct;
} loxbudget_pressure_thresholds_t;

void loxbudget_microhealth_attach(loxbudget_t* budget, microhealth_t* health,
                                  const loxbudget_pressure_thresholds_t* t);

#endif
