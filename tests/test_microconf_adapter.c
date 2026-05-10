#include "loxbudget.h"
#include "loxbudget_microconf_adapter.h"

#include <assert.h>
#include <string.h>

struct microconf_t {
  const loxbudget_microconf_resource_t* rs;
  size_t rs_n;
  const loxbudget_op_profile_t* ops;
  size_t ops_n;
  const loxbudget_microconf_need_t* needs;
  size_t needs_n;
};

size_t microconf_loxbudget_resource_count(microconf_t* conf) { return conf->rs_n; }
const loxbudget_microconf_resource_t* microconf_loxbudget_resources(microconf_t* conf) {
  return conf->rs;
}
size_t microconf_loxbudget_op_count(microconf_t* conf) { return conf->ops_n; }
const loxbudget_op_profile_t* microconf_loxbudget_ops(microconf_t* conf) { return conf->ops; }
size_t microconf_loxbudget_need_count(microconf_t* conf) { return conf->needs_n; }
const loxbudget_microconf_need_t* microconf_loxbudget_needs(microconf_t* conf) {
  return conf->needs;
}

static loxbudget_config_t cfg_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 2;
  cfg.max_ops = 2;
  cfg.max_concurrent_leases = 1;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  return cfg;
}

static loxbudget_op_profile_t allow_profile_(loxbudget_op_id_t op_id) {
  loxbudget_op_profile_t p;
  memset(&p, 0, sizeof(p));
  p.op_id = op_id;
  p.priority = LOXBUDGET_PRIO_NORMAL;
  p.action_normal = LOXBUDGET_ALLOW_FULL;
  p.action_elevated = LOXBUDGET_ALLOW_FULL;
  p.action_critical = LOXBUDGET_ALLOW_FULL;
  p.action_survival = LOXBUDGET_ALLOW_FULL;
  p.action_lockdown = LOXBUDGET_ALLOW_FULL;
  return p;
}

int main(void) {
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(2, 2, 0) / 4u];
  loxbudget_t b;
  microconf_t conf;
  loxbudget_decision_t d;
  loxbudget_config_t cfg = cfg_();

  const loxbudget_microconf_resource_t rs[] = {
      {.id = 0, .limit = 1, .kind = (uint8_t)LOXBUDGET_RES_REUSABLE},
  };
  const loxbudget_op_profile_t ops[] = {
      allow_profile_(0),
  };
  const loxbudget_microconf_need_t needs[] = {
      {.op_id = 0, .resource_id = 0, .amount = 1},
  };

  memset(&conf, 0, sizeof(conf));
  conf.rs = rs;
  conf.rs_n = 1;
  conf.ops = ops;
  conf.ops_n = 1;
  conf.needs = needs;
  conf.needs_n = 1;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_microconf_load(&b, &conf) == LOXBUDGET_OK);

  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_ALLOW_FULL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
