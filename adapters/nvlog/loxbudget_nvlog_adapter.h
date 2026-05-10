#ifndef LOXBUDGET_NVLOG_ADAPTER_H
#define LOXBUDGET_NVLOG_ADAPTER_H

#include "loxbudget.h"

#include <stddef.h>

typedef struct nvlog_t nvlog_t;

/* Minimal external dependency: user must provide this function. */
void nvlog_write(nvlog_t* log, uint8_t severity, const void* bytes, size_t len);

typedef struct {
  uint32_t uptime_ms;
  uint8_t op_id;
  uint8_t action;
  uint8_t pressure;
  uint8_t reason;
} loxbudget_nvlog_record_t;

void loxbudget_nvlog_attach(loxbudget_t* budget, nvlog_t* log, uint8_t min_severity);

#endif
