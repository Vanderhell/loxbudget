#include "loxbudget.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

#if LOXBUDGET_ENABLE_RATE_WINDOWS
static uint32_t now_ms_(void* user) { return *(uint32_t*)user; }
#endif

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
#if !LOXBUDGET_ENABLE_RATE_WINDOWS
  (void)data;
  (void)size;
  return 0;
#else
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(4, 8, 0)];
  uint32_t t = 0;
  loxbudget_hal_callbacks_t hal = *loxbudget_hal_default_permissive();
  loxbudget_t b;
  loxbudget_config_t cfg;
  loxbudget_op_profile_t p;
  loxbudget_lease_t lease;

  if (size < 8) return 0;

  hal.now_ms = &now_ms_;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 4;
  cfg.max_ops = 8;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = &hal;
  cfg.hal_user = &t;

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 0;
  (void)loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_CONSUMABLE);

  (void)loxbudget_set_rate_limit(&b, 0, (loxbudget_window_t)(data[0] % 4u),
                                 (uint32_t)(data[1] + 1u));
  (void)loxbudget_set_lifetime_limit(&b, 0, (uint32_t)(data[2] + 1u));

  memset(&p, 0, sizeof(p));
  p.op_id = 0;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_ALLOW_FULL;
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, 0, (uint16_t)((data[3] % 3u) + 1u));

  for (size_t i = 4; i < size; i++) {
    t += (uint32_t)(data[i]);
    if (loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK) {
      (void)loxbudget_leave(&b, lease);
    }
  }

  (void)loxbudget_deinit(&b);
  return 0;
#endif
}

#endif

