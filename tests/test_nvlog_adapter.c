#include "loxbudget.h"
#include "loxbudget_nvlog_adapter.h"

#include <assert.h>
#include <string.h>

struct nvlog_t {
  uint8_t last_sev;
  loxbudget_nvlog_record_t last;
  uint32_t writes;
};

void nvlog_write(nvlog_t* log, uint8_t severity, const void* bytes, size_t len) {
  assert(len == sizeof(loxbudget_nvlog_record_t));
  log->writes++;
  log->last_sev = severity;
  memcpy(&log->last, bytes, sizeof(log->last));
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
  nvlog_t log;
  loxbudget_decision_t d;
  loxbudget_config_t cfg = cfg_();
  loxbudget_op_profile_t p = deny_profile_(0);

  memset(&log, 0, sizeof(log));
  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);

  loxbudget_nvlog_attach(&b, &log, 3u);
  assert(loxbudget_check(&b, 0, &d) == LOXBUDGET_OK);
  assert(log.writes == 1u);
  assert(log.last_sev == 3u);
  assert(log.last.op_id == 0u);
  assert(log.last.action == (uint8_t)LOXBUDGET_REJECT);

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
