#include "loxbudget.h"

#include <string.h>

/* Host-only compile demo: model flash writes as a CONSUMABLE resource. */
static loxbudget_op_profile_t profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_DEGRADED;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

int main(void) {
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(2, 2, 32)];
  loxbudget_t b;
  loxbudget_config_t cfg;
  loxbudget_op_profile_t ota = profile_(0);
  loxbudget_lease_t lease;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 32;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 1;

  (void)loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_CONSUMABLE); /* flash writes */
  (void)loxbudget_register_op(&b, &ota);
  (void)loxbudget_op_set_need(&b, 0, 0, 10);

  if (loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK) { (void)loxbudget_leave(&b, lease); }

  (void)loxbudget_deinit(&b);
  return 0;
}
