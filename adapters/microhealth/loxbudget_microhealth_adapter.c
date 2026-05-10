#include "loxbudget_microhealth_adapter.h"

typedef struct {
  loxbudget_t* budget;
  microhealth_t* health;
  loxbudget_pressure_thresholds_t t;
} lb__mh_ctx_t;

static loxbudget_pressure_t lb__mh_map_(const loxbudget_pressure_thresholds_t* t, uint16_t pct) {
  if (pct >= t->survival_pct) return LOXBUDGET_PRESSURE_SURVIVAL;
  if (pct >= t->critical_pct) return LOXBUDGET_PRESSURE_CRITICAL;
  if (pct >= t->elevated_pct) return LOXBUDGET_PRESSURE_ELEVATED;
  return LOXBUDGET_PRESSURE_NORMAL;
}

static void lb__mh_on_change_(void* user) {
  lb__mh_ctx_t* ctx = (lb__mh_ctx_t*)user;
  if (ctx == NULL || ctx->budget == NULL || ctx->health == NULL) return;
  const uint16_t pct = microhealth_pressure_pct(ctx->health);
  (void)loxbudget_set_pressure(ctx->budget, lb__mh_map_(&ctx->t, pct));
}

void loxbudget_microhealth_attach(loxbudget_t* budget, microhealth_t* health,
                                  const loxbudget_pressure_thresholds_t* t) {
  static lb__mh_ctx_t ctx;
  if (budget == NULL || health == NULL || t == NULL) return;
  ctx.budget = budget;
  ctx.health = health;
  ctx.t = *t;

  microhealth_subscribe(health, &lb__mh_on_change_, &ctx);
  lb__mh_on_change_(&ctx);
}
