#include "loxbudget.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(4, 8, 0)];
  loxbudget_t b;
  loxbudget_config_t cfg;
  loxbudget_op_profile_t p;
  loxbudget_decision_t d;

  if (size < 4) return 0;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 4;
  cfg.max_ops = 8;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 0;
  (void)loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_set_resource(&b, 1, 10, LOXBUDGET_RES_CONSUMABLE);

  memset(&p, 0, sizeof(p));
  p.op_id = 0;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_DEGRADED;
  p.action_critical = LOXBUDGET_WAIT;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, (loxbudget_resource_id_t)(data[0] & 1u),
                              (uint16_t)(data[1] & 7u));

  (void)loxbudget_set_pressure(&b, (loxbudget_pressure_t)(data[2] % 5u));
  (void)loxbudget_check(&b, 0, &d);

  (void)loxbudget_deinit(&b);
  return 0;
}

#endif

