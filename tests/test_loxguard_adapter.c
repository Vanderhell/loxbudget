#include "loxbudget.h"
#include "loxbudget_loxguard_adapter.h"

#include <assert.h>
#include <string.h>

struct loxguard_t {
  loxbudget_loxguard_record_t last;
  uint32_t pushes;
  uint32_t flushes;
};

void loxguard_push(loxguard_t* guard, const void* bytes, size_t len) {
  assert(len == sizeof(loxbudget_loxguard_record_t));
  guard->pushes++;
  memcpy(&guard->last, bytes, sizeof(guard->last));
}

void loxguard_flush(loxguard_t* guard) { guard->flushes++; }

static loxbudget_config_t cfg_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 1;
  cfg.max_ops = 1;
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  return cfg;
}

static loxbudget_op_profile_t lockdown_profile_(loxbudget_op_id_t op_id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = op_id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_LOCKDOWN;
  p.action_elevated = LOXBUDGET_LOCKDOWN;
  p.action_critical = LOXBUDGET_LOCKDOWN;
  p.action_survival = LOXBUDGET_LOCKDOWN;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(1, 1, 0) / 4u];
  loxbudget_t b;
  loxguard_t guard;
  loxbudget_decision_t d;
  loxbudget_config_t cfg = cfg_();
  loxbudget_op_profile_t p = lockdown_profile_(0);

  memset(&guard, 0, sizeof(guard));
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);

  loxbudget_loxguard_attach(&b, &guard);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_LOCKDOWN);
  assert(guard.pushes == 1u);
  assert(guard.flushes == 1u);
  assert(guard.last.op_id == 0u);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
