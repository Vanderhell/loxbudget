#include "loxbudget.h"

#include <assert.h>
#include <string.h>

static uint32_t now_ms_(void* user) { return *(uint32_t*)user; }

static loxbudget_op_profile_t profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_ALLOW_FULL;
  return p;
}

int main(void) {
#if !LOXBUDGET_ENABLE_RATE_WINDOWS || !LOXBUDGET_ENABLE_AUDIT_TRAIL
  return 0;
#else
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(2, 4, 64) / 4u];
  uint32_t t = 0;
  loxbudget_hal_callbacks_t hal = *loxbudget_hal_default_permissive();
  loxbudget_config_t cfg;
  loxbudget_t b;
  loxbudget_lease_t lease;
  loxbudget_decision_t d;
  loxbudget_decision_record_t rec[64];
  size_t n = 0;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 4;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 64;
  cfg.hal_strict = 0;
  hal.now_ms = &now_ms_;
  cfg.hal_callbacks = &hal;
  cfg.hal_user = &t;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_CONSUMABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_rate_limit(&b, 0, LOXBUDGET_WINDOW_MINUTE, 60) == LOXBUDGET_OK);

  {
    loxbudget_op_profile_t log_op = profile_(0);
    loxbudget_op_profile_t panic_op = profile_(1);
    panic_op.flags = LOXBUDGET_OPF_BYPASS_RATE_LIMIT;
    assert(loxbudget_register_op(&b, &log_op) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &panic_op) == LOXBUDGET_OK);
    assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);
    assert(loxbudget_op_set_need(&b, 1, 0, 1) == LOXBUDGET_OK);
  }

  /* 60/min succeed, 61st denied. */
  for (uint32_t i = 0; i < 60u; i++) {
    assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
    assert(d.action == LOXBUDGET_ALLOW_FULL);
    assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK);
    assert(loxbudget_leave(&b, lease) == LOXBUDGET_OK);
  }
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_REJECT);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_RATE_LIMIT);
  assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_ERR_BAD_STATE);

  /* Panic op bypasses. */
  assert(loxbudget_check(&b, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);
  assert(loxbudget_enter(&b, 1, &lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_OK);

  assert(loxbudget_audit_get_recent(&b, rec, 64, &n) == LOXBUDGET_OK);
  assert(n > 0u);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
#endif
}
