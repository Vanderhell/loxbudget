#ifndef LOXBUDGET_MICROLOG_ADAPTER_H
#define LOXBUDGET_MICROLOG_ADAPTER_H

#include "loxbudget.h"

typedef struct microlog_t microlog_t;

/* Minimal external dependency: user must provide this function. */
void microlog_write(microlog_t* log, const char* msg);

typedef struct {
  microlog_t* log;
  loxbudget_action_t min_action;
} loxbudget_microlog_ctx_t;

void loxbudget_microlog_attach(loxbudget_t* budget, microlog_t* log,
                               loxbudget_action_t min_action_to_log);

#endif /* LOXBUDGET_MICROLOG_ADAPTER_H */
