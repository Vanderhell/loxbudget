#include "loxbudget_loxguard_adapter.h"

typedef struct {
  loxbudget_t* budget;
  loxguard_t* guard;
} lb__lg_ctx_t;

static void lb__lg_on_decision_(void* user, const loxbudget_decision_t* d, loxbudget_op_id_t op) {
  lb__lg_ctx_t* ctx = (lb__lg_ctx_t*)user;
  if (ctx == NULL || ctx->budget == NULL || ctx->guard == NULL || d == NULL) return;

  if (d->action != LOXBUDGET_REJECT && d->action != LOXBUDGET_LOCKDOWN) return;

  loxbudget_snapshot_t snap;
  if (loxbudget_snapshot(ctx->budget, &snap) != LOXBUDGET_OK) return;

  loxbudget_loxguard_record_t r;
  r.uptime_ms = snap.uptime_ms;
  r.op_id = (uint8_t)op;
  r.action = (uint8_t)d->action;
  r.pressure = (uint8_t)d->pressure;
  r.reason = (uint8_t)d->reason;
  loxguard_push(ctx->guard, &r, sizeof(r));

  if (d->action == LOXBUDGET_LOCKDOWN) { loxguard_flush(ctx->guard); }
}

void loxbudget_loxguard_attach(loxbudget_t* budget, loxguard_t* guard) {
  static lb__lg_ctx_t ctx;
  ctx.budget = budget;
  ctx.guard = guard;
  (void)loxbudget_set_decision_hook(budget, &lb__lg_on_decision_, &ctx);
}
