#include "loxbudget_microbus_adapter.h"

typedef struct {
  microbus_t* bus;
} lb__mb_ctx_t;

static void lb__mb_on_decision_(void* user, const loxbudget_decision_t* d, loxbudget_op_id_t op) {
  lb__mb_ctx_t* ctx = (lb__mb_ctx_t*)user;
  if (ctx == NULL || ctx->bus == NULL || d == NULL) return;

  if (d->action == LOXBUDGET_REJECT) {
    microbus_publish_event(ctx->bus, (uint32_t)LOXBUDGET_MICROBUS_BUDGET_DENIED, (uint32_t)op,
                           (uint32_t)d->reason, (uint32_t)d->pressure);
  } else if (d->action == LOXBUDGET_LOCKDOWN) {
    microbus_publish_event(ctx->bus, (uint32_t)LOXBUDGET_MICROBUS_LOCKDOWN_ENTERED, (uint32_t)op,
                           (uint32_t)d->reason, (uint32_t)d->pressure);
  }
}

void loxbudget_microbus_attach(loxbudget_t* budget, microbus_t* bus) {
  static lb__mb_ctx_t ctx;
  ctx.bus = bus;
  (void)loxbudget_set_decision_hook(budget, &lb__mb_on_decision_, &ctx);
}

loxbudget_status_t loxbudget_microbus_set_pressure(loxbudget_t* budget, microbus_t* bus,
                                                   loxbudget_pressure_t pressure) {
  if (budget == NULL || bus == NULL) return LOXBUDGET_ERR_INVALID_ARG;
  loxbudget_status_t st = loxbudget_set_pressure(budget, pressure);
  if (st == LOXBUDGET_OK) {
    microbus_publish_event(bus, (uint32_t)LOXBUDGET_MICROBUS_PRESSURE_CHANGED, (uint32_t)pressure,
                           0u, 0u);
  }
  return st;
}
