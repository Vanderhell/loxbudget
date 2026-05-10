#include "loxbudget.h"
#include "loxbudget_microbus_adapter.h"

#include <assert.h>
#include <string.h>

struct microbus_t {
  uint32_t last_event;
  uint32_t last_a;
  uint32_t last_b;
  uint32_t last_c;
  uint32_t publishes;
};

void microbus_publish_event(microbus_t* bus, uint32_t event_id, uint32_t a, uint32_t b,
                            uint32_t c) {
  bus->publishes++;
  bus->last_event = event_id;
  bus->last_a = a;
  bus->last_b = b;
  bus->last_c = c;
}

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

static loxbudget_op_profile_t deny_profile_(loxbudget_op_id_t op_id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = op_id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_REJECT;
  p.action_elevated = LOXBUDGET_REJECT;
  p.action_critical = LOXBUDGET_REJECT;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_REJECT;
  return p;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(1, 1, 0) / 4u];
  loxbudget_t b;
  microbus_t bus;
  loxbudget_decision_t d;
  loxbudget_config_t cfg = cfg_();
  loxbudget_op_profile_t p = deny_profile_(0);

  memset(&bus, 0, sizeof(bus));
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);

  loxbudget_microbus_attach(&b, &bus);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_REJECT);
  assert(bus.publishes == 1u);
  assert(bus.last_event == (uint32_t)LOXBUDGET_MICROBUS_BUDGET_DENIED);

  assert(loxbudget_microbus_set_pressure(&b, &bus, LOXBUDGET_PRESSURE_CRITICAL) == LOXBUDGET_OK);
  assert(bus.last_event == (uint32_t)LOXBUDGET_MICROBUS_PRESSURE_CHANGED);
  assert(bus.last_a == (uint32_t)LOXBUDGET_PRESSURE_CRITICAL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
