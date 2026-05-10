#include "loxbudget.h"

#include <assert.h>
#include <string.h>

static void basic_cascade_denies(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 3, 0) + 3u) / 4u];
  loxbudget_t b;
  loxbudget_decision_t d;

  assert(loxbudget_init_simple(&b, storage, sizeof(storage), 1, 3) == LOXBUDGET_OK);

  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  {
    loxbudget_op_profile_t a = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t b2 = loxbudget_op_profile_default(1);
    assert(loxbudget_register_op(&b, &a) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &b2) == LOXBUDGET_OK);
  }

  assert(loxbudget_op_set_need(&b, 0, 0, 6) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 1, 0, 6) == LOXBUDGET_OK);

  /* Without edges, op0 fits. */
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);
  assert(d.reason == LOXBUDGET_REASON_OK);

  /* Declare: op0 may trigger op1 always => 6 + 6 > 10 => denied by causality. */
  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_causality_edge_count(&b) == 1u);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.reason == LOXBUDGET_REASON_CAUSAL_CASCADE);
  assert(d.denied_resource == 0);
  assert(d.action == LOXBUDGET_WAIT);

  (void)loxbudget_deinit(&b);
}

static void rare_edges_only_at_critical(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 3, 0) + 3u) / 4u];
  loxbudget_t b;
  loxbudget_decision_t d;

  assert(loxbudget_init_simple(&b, storage, sizeof(storage), 1, 3) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);

  {
    loxbudget_op_profile_t a = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t b2 = loxbudget_op_profile_default(1);
    assert(loxbudget_register_op(&b, &a) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &b2) == LOXBUDGET_OK);
  }
  assert(loxbudget_op_set_need(&b, 0, 0, 6) == LOXBUDGET_OK);
  /* Make op1 expensive so a RARE (weight=32) edge still has a visible impact. */
  assert(loxbudget_op_set_need(&b, 1, 0, 50) == LOXBUDGET_OK);
  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_RARE) == LOXBUDGET_OK);

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_NORMAL) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_set_pressure(&b, LOXBUDGET_PRESSURE_CRITICAL) == LOXBUDGET_OK);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.reason == LOXBUDGET_REASON_CAUSAL_CASCADE);

  (void)loxbudget_deinit(&b);
}

static void edge_update_and_count(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 4, 0) + 3u) / 4u];
  loxbudget_t b;

  assert(loxbudget_init_simple(&b, storage, sizeof(storage), 1, 4) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  {
    loxbudget_op_profile_t a = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t b2 = loxbudget_op_profile_default(1);
    assert(loxbudget_register_op(&b, &a) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &b2) == LOXBUDGET_OK);
  }

  assert(loxbudget_causality_edge_count(&b) == 0u);
  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_MAYBE) == LOXBUDGET_OK);
  assert(loxbudget_causality_edge_count(&b) == 1u);
  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_causality_edge_count(&b) == 1u);

  (void)loxbudget_deinit(&b);
}

static void cycle_rejected_at_registration(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 4, 0) + 3u) / 4u];
  loxbudget_t b;

  assert(loxbudget_init_simple(&b, storage, sizeof(storage), 1, 4) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  {
    loxbudget_op_profile_t a = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t b2 = loxbudget_op_profile_default(1);
    loxbudget_op_profile_t c = loxbudget_op_profile_default(2);
    assert(loxbudget_register_op(&b, &a) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &b2) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &c) == LOXBUDGET_OK);
  }

  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_op_may_trigger(&b, 1, 2, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_causality_edge_count(&b) == 2u);

  /* 2 -> 0 would create a cycle: 0 -> 1 -> 2 -> 0 */
  assert(loxbudget_op_may_trigger(&b, 2, 0, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_ERR_BAD_STATE);
  assert(loxbudget_causality_edge_count(&b) == 2u);

  (void)loxbudget_deinit(&b);
}

static void capacity_limit_enforced(void) {
  /* test_causality builds with LOXBUDGET_CAUSALITY_MAX_EDGES=4 (see CMakeLists.txt). */
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 6, 0) + 3u) / 4u];
  loxbudget_t b;

  assert(loxbudget_init_simple(&b, storage, sizeof(storage), 1, 6) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  {
    loxbudget_op_profile_t p0 = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t p1 = loxbudget_op_profile_default(1);
    loxbudget_op_profile_t p2 = loxbudget_op_profile_default(2);
    loxbudget_op_profile_t p3 = loxbudget_op_profile_default(3);
    loxbudget_op_profile_t p4 = loxbudget_op_profile_default(4);
    assert(loxbudget_register_op(&b, &p0) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &p1) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &p2) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &p3) == LOXBUDGET_OK);
    assert(loxbudget_register_op(&b, &p4) == LOXBUDGET_OK);
  }

  assert(loxbudget_causality_edge_count(&b) == 0u);
  assert(loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_op_may_trigger(&b, 0, 2, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_op_may_trigger(&b, 0, 3, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_op_may_trigger(&b, 0, 4, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_OK);
  assert(loxbudget_causality_edge_count(&b) == 4u);

  /* One more unique edge should fail with ERR_NO_SPACE. */
  assert(loxbudget_op_may_trigger(&b, 1, 2, LOXBUDGET_TRIGGER_ALWAYS) == LOXBUDGET_ERR_NO_SPACE);
  assert(loxbudget_causality_edge_count(&b) == 4u);

  (void)loxbudget_deinit(&b);
}

int main(void) {
  basic_cascade_denies();
  rare_edges_only_at_critical();
  edge_update_and_count();
  cycle_rejected_at_registration();
  capacity_limit_enforced();
  return 0;
}
