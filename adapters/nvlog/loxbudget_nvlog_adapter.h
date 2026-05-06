#ifndef LOXBUDGET_NVLOG_ADAPTER_H
#define LOXBUDGET_NVLOG_ADAPTER_H

#include "loxbudget.h"

typedef struct nvlog_t nvlog_t;

void loxbudget_nvlog_attach(loxbudget_t* budget, nvlog_t* log, uint8_t min_severity);

#endif
