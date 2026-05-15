#include "loxbudget.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
static uint64_t ticks_now_(void) { return __rdtsc(); }
#elif defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
static uint64_t ticks_now_(void) { return __rdtsc(); }
#else
/* Fallback: no cycle counter available; keep the test as a functional smoke. */
static uint64_t ticks_now_(void) { return 0u; }
#endif

static int cmp_u64_(const void* a, const void* b) {
  const uint64_t x = *(const uint64_t*)a;
  const uint64_t y = *(const uint64_t*)b;
  return (x > y) - (x < y);
}

static loxbudget_config_t cfg_(void) {
  loxbudget_config_t cfg = loxbudget_config_simple(2, 1);
  cfg.max_concurrent_leases = 1;
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
  static uint32_t storage32[LOXBUDGET_REQUIRED_SIZE(2, 1, 0) / 4u];
  loxbudget_t b;
  loxbudget_config_t cfg = cfg_();
  loxbudget_op_profile_t p = profile_(0);
  loxbudget_decision_t d;
  loxbudget_lease_t lease;

  assert(loxbudget_init(&b, storage32, sizeof(storage32), &cfg) == LOXBUDGET_OK);
  assert(loxbudget_set_resource(&b, 0, 1, LOXBUDGET_RES_REUSABLE) == LOXBUDGET_OK);
  assert(loxbudget_register_op(&b, &p) == LOXBUDGET_OK);
  assert(loxbudget_op_set_need(&b, 0, 0, 1) == LOXBUDGET_OK);

  /* Warm-up */
  for (uint32_t i = 0; i < 64u; i++) {
    (void)loxbudget_check(&b, 0, &d);
    if (loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK) { (void)loxbudget_leave(&b, lease); }
  }

  /* Measure: check */
  {
    enum { N = 1000 };
    uint64_t samples[N];
    for (uint32_t i = 0; i < N; i++) {
      const uint64_t t0 = ticks_now_();
      (void)loxbudget_check(&b, 0, &d);
      const uint64_t t1 = ticks_now_();
      samples[i] = (t1 >= t0) ? (t1 - t0) : 0u;
    }

    if (samples[0] != 0u || samples[N - 1] != 0u) {
      qsort(samples, N, sizeof(samples[0]), &cmp_u64_);
      const uint64_t median = samples[N / 2u];
      const uint64_t p99 = samples[(N * 99u) / 100u];
      /* CI/desktop noise can produce rare outliers; enforce percentile bound. */
      if (median != 0u) {
#if defined(_WIN32)
        const uint64_t max_ratio = 20u;
        const uint64_t slack = 0u;
#else
        const uint64_t max_ratio = 4u;
        const uint64_t slack = 256u;
#endif
        if (median <= ((UINT64_MAX - slack) / max_ratio)) {
          assert(p99 <= (median * max_ratio) + slack);
        }
      }
    }
  }

  /* Measure: enter+leave pair */
  {
    enum { N = 1000 };
    uint64_t samples[N];
    for (uint32_t i = 0; i < N; i++) {
      const uint64_t t0 = ticks_now_();
      if (loxbudget_enter(&b, 0, &lease) == LOXBUDGET_OK) { (void)loxbudget_leave(&b, lease); }
      const uint64_t t1 = ticks_now_();
      samples[i] = (t1 >= t0) ? (t1 - t0) : 0u;
    }

    if (samples[0] != 0u || samples[N - 1] != 0u) {
      qsort(samples, N, sizeof(samples[0]), &cmp_u64_);
      const uint64_t median = samples[N / 2u];
      const uint64_t p99 = samples[(N * 99u) / 100u];
      if (median != 0u) {
#if defined(_WIN32)
        const uint64_t max_ratio = 20u;
        const uint64_t slack = 0u;
#else
        const uint64_t max_ratio = 4u;
        const uint64_t slack = 256u;
#endif
        if (median <= ((UINT64_MAX - slack) / max_ratio)) {
          assert(p99 <= (median * max_ratio) + slack);
        }
      }
    }
  }

  assert(loxbudget_deinit(&b) == LOXBUDGET_OK);
  return 0;
}
