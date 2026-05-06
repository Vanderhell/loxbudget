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

int main(void) {
  test_init_invalid_args();
  test_init_valid();
  test_deinit_after_init();
  test_pressure_set_get();
  return 0;
}

