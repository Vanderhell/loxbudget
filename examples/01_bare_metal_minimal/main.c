#include "loxbudget.h"

int main(void) {
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(2, 2, 0)];
  loxbudget_t b;
  loxbudget_config_t cfg = {0};
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 2;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();

  loxbudget_op_profile_t p = {0, LOXBUDGET_PRIO_NORMAL, LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL, LOXBUDGET_ALLOW_FULL,
                              LOXBUDGET_ALLOW_FULL, LOXBUDGET_ALLOW_FULL, 0};
  loxbudget_decision_t d;

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK)
    return 2;
  (void)loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, 0, 5);
  (void)loxbudget_check(&b, 0, &d);
  return (d.action == LOXBUDGET_ALLOW_FULL) ? 0 : 1;
}

