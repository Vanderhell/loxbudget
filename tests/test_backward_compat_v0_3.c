#include "loxbudget.h"

#include <assert.h>
#include <string.h>

/* This test intentionally uses only V0.3-era API surface (no adapters, no calibration export),
 * to validate that V1.0+ changes stay source-compatible for common integrations.
 */

static loxbudget_config_t cfg_default_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  return cfg;
}

static loxbudget_op_profile_t allow_profile_(loxbudget_op_id_t op_id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = op_id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = 0;
  return p;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(2, 2, 0) / 4u];
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  loxbudget_op_profile_t p = allow_profile_(0);
  loxbudget_decision_t d;
  loxbudget_lease_t lease;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 1, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_OK);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
