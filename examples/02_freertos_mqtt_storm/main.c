#include "loxbudget.h"

#include <stdint.h>
#include <string.h>

/* Host-only demo that mimics a "storm": pressure rises, decisions flip, audit shows why. */
static uint32_t demo_now_ms(void* user) {
  uint32_t* t = (uint32_t*)user;
  (*t) += 1u;
  return *t;
}

static const loxbudget_hal_callbacks_t* demo_hal_(uint32_t* t) {
  static loxbudget_hal_callbacks_t cb;
  cb.now_ms = &demo_now_ms;
  cb.critical_enter = 0;
  cb.critical_exit = 0;
  cb.boot_proven = 0;
  cb.voltage_ok = 0;
  cb.network_up = 0;
  return &cb;
}

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
  static uint8_t storage[LOXBUDGET_REQUIRED_SIZE(2, 2, 32)];
  uint32_t t = 0;
  loxbudget_t b;
  loxbudget_config_t cfg;
  loxbudget_op_profile_t mqtt_pub = profile_(0);
  loxbudget_decision_t d;

  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 32;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = demo_hal_(&t);
  cfg.hal_user = &t;

  if (loxbudget_init(&b, storage, sizeof(storage), &cfg) != LOXBUDGET_OK) return 1;

  (void)loxbudget_set_resource(&b, 0, 8, LOXBUDGET_RES_REUSABLE); /* queue slots */
  (void)loxbudget_register_op(&b, &mqtt_pub);
  (void)loxbudget_op_set_need(&b, 0, 0, 4);

  (void)loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_NORMAL);
  (void)loxbudget_check(&b, 0, &d);

  (void)loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_ELEVATED);
  (void)loxbudget_check(&b, 0, &d);

  (void)loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_CRITICAL);
  (void)loxbudget_check(&b, 0, &d);

#if LOXBUDGET_ENABLE_AUDIT_TRAIL
  {
    loxbudget_decision_record_t rec[8];
    size_t n = 0;
    (void)loxbudget_audit_get_recent(&b, rec, 8, &n);
    (void)n;
  }
#endif

  (void)loxbudget_deinit(&b);
  return 0;
}
