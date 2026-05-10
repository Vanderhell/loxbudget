#include "loxbudget.h"

#include <string.h>

/* Host-only compile demo: model flash writes as a CONSUMABLE resource. */
static loxbudget_op_profile_t profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p = loxbudget_op_profile_default(id);
  p.action_critical = LOXBUDGET_ALLOW_DEGRADED;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

int main(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(2, 2, 32) + 3u) / 4u];
  loxbudget_t b;
  loxbudget_op_profile_t ota = profile_(0);
  loxbudget_lease_t lease;

  loxbudget_config_t cfg = loxbudget_config_simple(2, 2);
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 32;

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 1;

  (void)loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_CONSUMABLE); /* flash writes */
#if LOXBUDGET_ENABLE_RATE_WINDOWS
  (void)loxbudget_set_rate_limit(&b, 0, LOXBUDGET_WINDOW_MINUTE, 60);
  (void)loxbudget_set_lifetime_limit(&b, 0, 1000);
#endif
  (void)loxbudget_register_op(&b, &ota);
  (void)loxbudget_op_set_need(&b, 0, 0, 10);

  if (loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK) { (void)loxbudget_leave(&b, lease); }

  (void)loxbudget_deinit(&b);
  return 0;
}
