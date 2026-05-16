#include "loxbudget.h"

#include <stddef.h>
#include <stdint.h>

/* Provide these from your platform:
   - For M3/M4/M7 you can use `bench_cycles_dwt.c`
   - For M0 use a hardware timer implementation */
extern void loxbench_cycles_init(void);
extern uint32_t loxbench_cycles_now(void);

/* Optional: route results somewhere (UART/log). */
__attribute__((weak)) void loxbench_report_u32(const char* key, uint32_t v) {
  (void)key;
  (void)v;
}

static loxbudget_op_profile_t profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p = loxbudget_op_profile_default(id);
  p.action_elevated = LOXBUDGET_ALLOW_DEGRADED;
  p.action_critical = LOXBUDGET_WAIT;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

/* Configure something non-trivial but bounded: a few resources + ops + audit ring. */
enum {
  RES = 2,
  OPS = 2,
  AUDIT = 32,
};

static uint32_t storage_[(LOXBUDGET_REQUIRED_SIZE(RES, OPS, AUDIT) + 3u) / 4u];

static void setup_budget_(loxbudget_t* b) {
  loxbudget_config_t cfg = loxbudget_config_simple(RES, OPS);
  cfg.max_concurrent_leases = 2;
  cfg.audit_size = AUDIT;
  cfg.hal_callbacks = 0;
  cfg.hal_user = 0;
  cfg.hal_strict = 0u;

  (void)loxbudget_init(b, storage_, sizeof(storage_), &cfg);

  (void)loxbudget_set_resource(b, 0, 8, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_set_resource(b, 1, 100, LOXBUDGET_RES_CONSUMABLE);

  loxbudget_op_profile_t op0 = profile_(0);
  loxbudget_op_profile_t op1 = profile_(1);
  (void)loxbudget_register_op(b, &op0);
  (void)loxbudget_register_op(b, &op1);

  (void)loxbudget_op_set_need(b, 0, 0, 4);
  (void)loxbudget_op_set_need(b, 0, 1, 10);
  (void)loxbudget_op_set_need(b, 1, 0, 2);
  (void)loxbudget_op_set_need(b, 1, 1, 5);

  (void)loxbudget_set_pressure(b, LOXBUDGET_PRESSURE_CRITICAL);
}

uint32_t loxbench_wcet_check_cycles(uint32_t iters) {
  loxbudget_t b;
  loxbudget_decision_t d;

  setup_budget_(&b);
  loxbench_cycles_init();

  /* Warm-up */
  for (uint32_t i = 0; i < 100u; i++) {
    (void)loxbudget_check(&b, 0, &d);
  }

  uint32_t worst = 0u;
  for (uint32_t i = 0; i < iters; i++) {
    const uint32_t t0 = loxbench_cycles_now();
    (void)loxbudget_check(&b, 0, &d);
    const uint32_t t1 = loxbench_cycles_now();
    const uint32_t dt = t1 - t0;
    if (dt > worst) worst = dt;
  }

  (void)loxbudget_deinit(&b);
  return worst;
}

/* Example entrypoint; call from your board's main(). */
void loxbench_run(void) {
  const uint32_t wcet = loxbench_wcet_check_cycles(2000u);
  loxbench_report_u32("loxbudget_check_wcet_cycles", wcet);
}

