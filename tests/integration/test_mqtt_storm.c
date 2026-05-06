#include "loxbudget.h"

#include <assert.h>
#include <string.h>

static loxbudget_op_profile_t profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_DEGRADED;
  p.action_critical = LOXBUDGET_WAIT;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(2, 4, 0) / 4u];
  loxbudget_t b;
  loxbudget_config_t cfg;
  loxbudget_decision_t d;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 4;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  {
    loxbudget_op_profile_t telemetry = profile_(0);
    loxbudget_op_profile_t fault = profile_(1);
    fault.action_survival = LOXBUDGET_ALLOW_FULL;
    fault.action_lockdown = LOXBUDGET_ALLOW_FULL;
    fault.flags = LOXBUDGET_OPF_LOCKDOWN_PASS;
    assert(loxbudget_register_op(&b, &telemetry) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &fault) == LOXBUDGET_OK);
    assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);
    assert(loxbudget_op_set_need(&b, 1, 0, 1) == LOXBUDGET_OK);
  }

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_NORMAL) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_ELEVATED) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_DEGRADED);

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_CRITICAL) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_WAIT);

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_LOCKDOWN) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
