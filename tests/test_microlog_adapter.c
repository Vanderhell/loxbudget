#include "loxbudget.h"
#include "loxbudget_microlog_adapter.h"

#include <assert.h>
#include <string.h>

struct microlog_t {
  char last[128];
  unsigned writes;
};

void microlog_write(microlog_t* log, const char* msg) {
  log->writes++;
  strncpy(log->last, msg, sizeof(log->last) - 1u);
  log->last[sizeof(log->last) - 1u] = 0;
}

static loxbudget_config_t cfg_(void) {
  loxbudget_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_resources = 4;
  cfg.max_ops = 8;
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = 0;
  cfg.hal_strict = 0;
  cfg.hal_callbacks = loxbudget_hal_default_permissive();
  return cfg;
}

static loxbudget_op_profile_t profile_(loxbudget_op_id_t op_id) {
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
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(4, 8, 0) / 4u];
  loxbudget_t b;
  microlog_t log;
  loxbudget_decision_t d;
  loxbudget_op_profile_t p = profile_(0);
  loxbudget_config_t cfg = cfg_();

  memset(&log, 0, sizeof(log));
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 0, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  loxbudget_microlog_attach(&b, &log, LOXBUDGET_WAIT);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(d.action == LOXBUDGET_WAIT);
  assert(log.writes == 1u);
  assert(strstr(log.last, "loxbudget op=") != NULL);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
