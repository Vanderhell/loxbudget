#include "loxbudget.h"
#include "loxbudget_microhealth_adapter.h"

#include <assert.h>
#include <string.h>

struct microhealth_t {
  uint16_t pct;
  void (*cb)(void* user);
  void* user;
};

uint16_t microhealth_pressure_pct(microhealth_t* health) { return health->pct; }

void microhealth_subscribe(microhealth_t* health, void (*on_change)(void* user), void* user) {
  health->cb = on_change;
  health->user = user;
}

static loxbudget_config_t cfg_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 1;
  cfg.max_ops = 1;
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  return cfg;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(1, 1, 0) / 4u];
  loxbudget_t b;
  microhealth_t h;
  loxbudget_pressure_thresholds_t t;
  loxbudget_config_t cfg = cfg_();

  memset(&h, 0, sizeof(h));
  t.elevated_pct = 60;
  t.critical_pct = 80;
  t.survival_pct = 95;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);

  h.pct = 10;
  loxbudget_microhealth_attach(&b, &h, &t);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_NORMAL);

  h.pct = 70;
  h.cb(h.user);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_ELEVATED);

  h.pct = 90;
  h.cb(h.user);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_CRITICAL);

  h.pct = 99;
  h.cb(h.user);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_SURVIVAL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
