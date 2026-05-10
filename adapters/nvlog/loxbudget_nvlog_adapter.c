#include "loxbudget_nvlog_adapter.h"

typedef struct {
  loxbudget_t* budget;
  nvlog_t* log;
  uint8_t min_severity;
} lb__nv_ctx_t;

static uint8_t lb__nv_severity_(loxbudget_action_t a) {
  switch (a) {
  case LOXBUDGET_ALLOW_FULL:
    return 0u;
  case LOXBUDGET_ALLOW_DEGRADED:
    return 1u;
  case LOXBUDGET_WAIT:
    return 2u;
  case LOXBUDGET_REJECT:
    return 3u;
  case LOXBUDGET_LOCKDOWN:
    return 4u;
  default:
    return 255u;
  }
}

static void lb__nv_on_decision_(void* user, const loxbudget_decision_t* d, loxbudget_op_id_t op) {
  lb__nv_ctx_t* ctx = (lb__nv_ctx_t*)user;
  if (ctx == NULL || ctx->budget == NULL || ctx->log == NULL || d == NULL) return;

  const uint8_t sev = lb__nv_severity_(d->action);
  if (sev < ctx->min_severity) return;

  loxbudget_snapshot_t snap;
  if (loxbudget_snapshot(ctx->budget, &snap) != LOXBUDGET_OK) return;

  loxbudget_nvlog_record_t r;
  r.uptime_ms = snap.uptime_ms;
  r.op_id = (uint8_t)op;
  r.action = (uint8_t)d->action;
  r.pressure = (uint8_t)d->pressure;
  r.reason = (uint8_t)d->reason;
  nvlog_write(ctx->log, sev, &r, sizeof(r));
}

void loxbudget_nvlog_attach(loxbudget_t* budget, nvlog_t* log, uint8_t min_severity) {
  static lb__nv_ctx_t ctx;
  ctx.budget = budget;
  ctx.log = log;
  ctx.min_severity = min_severity;
  (void)loxbudget_set_decision_hook(budget, &lb__nv_on_decision_, &ctx);
}
