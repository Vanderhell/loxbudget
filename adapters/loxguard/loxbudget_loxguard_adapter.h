#ifndef LOXBUDGET_LOXGUARD_ADAPTER_H
#define LOXBUDGET_LOXGUARD_ADAPTER_H

#include "loxbudget.h"

#include <stddef.h>

typedef struct loxguard_t loxguard_t;

/* Minimal external dependency: user must provide these functions. */
void loxguard_push(loxguard_t* guard, const void* bytes, size_t len);
void loxguard_flush(loxguard_t* guard);

typedef struct {
  uint32_t uptime_ms;
  uint8_t op_id;
  uint8_t action;
  uint8_t pressure;
  uint8_t reason;
} loxbudget_loxguard_record_t;

void loxbudget_loxguard_attach(loxbudget_t* budget, loxguard_t* guard);

#endif
