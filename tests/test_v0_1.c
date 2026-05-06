#include "loxbudget.h"

#include <assert.h>
#include <string.h>

static loxbudget_config_t cfg_default_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 4;
  cfg.max_ops = 8;
  cfg.max_concurrent_leases = 4;
  cfg.audit_size = 0;
  cfg.hal_strict = 1;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  cfg.hal_user = 0;
  return cfg;
}

static void test_init_invalid_args(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];

  assert(loxbudget_init(0, storage32, sizeof(storage32), &cfg) ==
         LOXBUDGET_ERR_INVALID_ARG);
  assert(loxbudget_init(&b, 0, sizeof(storage32), &cfg) ==
         LOXBUDGET_ERR_INVALID_ARG);
  assert(loxbudget_init(&b, storage32, 0, &cfg) == LOXBUDGET_ERR_INVALID_ARG);
  assert(loxbudget_init(&b, storage32, sizeof(storage32), 0) ==
         LOXBUDGET_ERR_INVALID_ARG);

  {
    static uint8_t storage8[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) + 1u];
    assert(loxbudget_init(&b, (void *)(storage8 + 1u),
                          LOXBUDGET_REQUIRED_SIZE(4, 8, 0), &cfg) ==
           LOXBUDGET_ERR_ALIGNMENT);
  }

  {
    loxbudget_config_t bad = cfg;
    bad.max_resources = 0;
    assert(loxbudget_init(&b, storage32, sizeof(storage32), &bad) ==
           LOXBUDGET_ERR_INVALID_ARG);
  }
  {
    loxbudget_config_t bad = cfg;
    bad.audit_size = 3;
    assert(loxbudget_init(&b, storage32, sizeof(storage32), &bad) ==
           LOXBUDGET_ERR_INVALID_ARG);
  }
}

static void test_init_valid(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_snapshot_t snap;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_snapshot(&b, &snap) == LOXBUDGET_OK);
  assert(snap.pressure == (uint8_t)LOXBUDGET_PRESSURE_NORMAL);
  assert(snap.total_decisions == 0u);
  assert(snap.total_grants == 0u);
  assert(snap.total_denials == 0u);
  assert(snap.total_degradations == 0u);
  assert(snap.resource_count == cfg.max_resources);
  assert(snap.op_count == cfg.max_ops);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_deinit_after_init(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  assert(loxbudget_deinit(&b) == LOXBUDGET_ERR_NOT_INITIALIZED);
}

static void test_pressure_set_get(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_NORMAL) == LOXBUDGET_OK);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_NORMAL);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_ELEVATED) == LOXBUDGET_OK);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_ELEVATED);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_CRITICAL) == LOXBUDGET_OK);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_CRITICAL);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_SURVIVAL) == LOXBUDGET_OK);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_SURVIVAL);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_LOCKDOWN) == LOXBUDGET_OK);
  assert(loxbudget_get_pressure(&b) == LOXBUDGET_PRESSURE_LOCKDOWN);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_set_resource_basic(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_op_profile_t p;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);

  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 1, 1, LOXBUDGET_RES_STATE) == LOXBUDGET_OK);

  memset(&p, 0, sizeof(p));
  p.op_id = 0;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = 0;
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 5) == LOXBUDGET_OK);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_register_op_duplicate(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_op_profile_t p;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);

  memset(&p, 0, sizeof(p));
  p.op_id = 1;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = 0;
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_ERR_DUPLICATE);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_op_set_need_limit(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_op_profile_t p;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 1, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 2, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 3, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  memset(&p, 0, sizeof(p));
  p.op_id = 2;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = 0;
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);

  assert(loxbudget_op_set_need(&b, 2, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 1, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 2, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 3, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 1, 1) == LOXBUDGET_OK);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_hal_strict_fail_closed(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_op_profile_t p;

  cfg.hal_strict = 1;
  cfg.hal_callbacks = 0; /* strict requires explicit callbacks */

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);

  memset(&p, 0, sizeof(p));
  p.op_id = 3;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN;

  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_ERR_HAL_NOT_CONFIGURED);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_hal_strict_disabled(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_op_profile_t p;

  cfg.hal_strict = 0;
  cfg.hal_callbacks = 0;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);

  memset(&p, 0, sizeof(p));
  p.op_id = 4;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_REJECT;
  p.flags = LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN;

  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static loxbudget_op_profile_t profile_allow_(loxbudget_op_id_t op_id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = op_id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_ALLOW_FULL;
  p.flags = 0;
  return p;
}

static void test_check_allow_full(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_allow_(0);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 5) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_OK);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_pressure_degraded(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_allow_(1);
  p.action_elevated = LOXBUDGET_ALLOW_DEGRADED;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 1, 0, 5) == LOXBUDGET_OK);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_ELEVATED) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_DEGRADED);
  assert(d.pressure == LOXBUDGET_PRESSURE_ELEVATED);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_pressure_reject(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_allow_(2);
  p.action_survival = LOXBUDGET_REJECT;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 2, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_SURVIVAL) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 2, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_REJECT);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_PRESSURE_BLOCK);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_lockdown_passthrough(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p_ok = profile_allow_(3);
  loxbudget_op_profile_t p_no = profile_allow_(4);
  p_ok.flags = LOXBUDGET_OPF_LOCKDOWN_PASS;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p_ok) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p_no) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 3, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 4, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_LOCKDOWN) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 3, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_check(&b, 4, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_LOCKDOWN);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_LOCKDOWN_ACTIVE);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_unknown_op(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 99, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_REJECT);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_UNKNOWN_OP);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_insufficient_resource(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_allow_(5);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 4, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 5, 0, 5) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 5, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_WAIT);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_INSUFFICIENT_RES);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_check_state_resource_fail(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_allow_(6);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 0, LOXBUDGET_RES_STATE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 6, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 6, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_REJECT);
  assert(d.reason == (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_decision_is_pure(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d1, d2;
  loxbudget_op_profile_t p = profile_allow_(7);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 7, 0, 5) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 7, &d1) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 7, &d2) == LOXBUDGET_OK);
  assert(memcmp(&d1, &d2, sizeof(d1)) == 0);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_enter_leave_cycle(void) {
  loxbudget_t a;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_lease_t lease;
  loxbudget_op_profile_t p1 = profile_allow_(0);
  loxbudget_op_profile_t p2 = profile_allow_(1);

  assert(loxbudget_init(&a, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&a, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  assert(loxbudget_register_op(&a, &p1) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&a, &p2) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&a, 0, 0, 5) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&a, 1, 0, 6) == LOXBUDGET_OK);

  assert(loxbudget_check(&a, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_enter(&a, 0, &lease) == LOXBUDGET_OK);

  assert(loxbudget_check(&a, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_WAIT);

  assert(loxbudget_leave(&a, lease) == LOXBUDGET_OK);

  assert(loxbudget_check(&a, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_deinit(&a) == LOXBUDGET_OK);
}

static void test_enter_consumable_persists(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_lease_t lease;
  loxbudget_op_profile_t p = profile_allow_(0);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_CONSUMABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 3) == LOXBUDGET_OK);

  assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_no_partial_reservation(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_decision_t d;
  loxbudget_lease_t lease;
  loxbudget_op_profile_t p_fail = profile_allow_(0);
  loxbudget_op_profile_t p_ok = profile_allow_(1);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 1, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 1, 1, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 2, 0, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  assert(loxbudget_register_op(&b, &p_fail) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p_ok) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 1, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 2, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 1, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_ERR_BAD_STATE);

  assert(loxbudget_check(&b, 1, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_double_leave_detected(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_lease_t lease;
  loxbudget_op_profile_t p = profile_allow_(0);

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_ERR_BAD_STATE);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_lease_magic_per_instance(void) {
  loxbudget_t a, b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage_a[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  static uint32_t storage_b[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_lease_t lease;
  loxbudget_op_profile_t p = profile_allow_(0);

  assert(loxbudget_init(&a, storage_a, sizeof(storage_a), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_init(&b, storage_b, sizeof(storage_b), &cfg) == LOXBUDGET_OK);

  assert(loxbudget_set_resource(&a, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  assert(loxbudget_register_op(&a, &p) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&a, 0, 0, 1) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_enter(&a, 0, &lease) == LOXBUDGET_OK);
  assert(loxbudget_leave(&b, lease) == LOXBUDGET_ERR_BAD_STATE);
  assert(loxbudget_leave(&a, lease) == LOXBUDGET_OK);

  assert(loxbudget_deinit(&a) == LOXBUDGET_OK);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

static void test_max_concurrent_leases(void) {
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_default_();
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_lease_t l1, l2;
  loxbudget_op_profile_t p = profile_allow_(0);

  cfg.max_concurrent_leases = 1;
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  assert(loxbudget_enter(&b, 0, &l1) == LOXBUDGET_OK);
  assert(loxbudget_enter(&b, 0, &l2) == LOXBUDGET_ERR_NO_SPACE);
  assert(loxbudget_leave(&b, l1) == LOXBUDGET_OK);
  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
}

int main(void) {
  test_init_invalid_args();
  test_init_valid();
  test_deinit_after_init();
  test_pressure_set_get();
  test_set_resource_basic();
  test_register_op_duplicate();
  test_op_set_need_limit();
  test_hal_strict_fail_closed();
  test_hal_strict_disabled();
  test_check_allow_full();
  test_check_pressure_degraded();
  test_check_pressure_reject();
  test_check_lockdown_passthrough();
  test_check_unknown_op();
  test_check_insufficient_resource();
  test_check_state_resource_fail();
  test_decision_is_pure();
  test_enter_leave_cycle();
  test_enter_consumable_persists();
  test_no_partial_reservation();
  test_double_leave_detected();
  test_lease_magic_per_instance();
  test_max_concurrent_leases();
  return 0;
}
