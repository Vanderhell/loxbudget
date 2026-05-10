#include "loxbudget.h"

#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
static uint64_t now_ns_(void) {
  LARGE_INTEGER freq;
  LARGE_INTEGER t;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&t);
  return (uint64_t)((t.QuadPart * 1000000000ull) / (uint64_t)freq.QuadPart);
}
#else
#include <time.h>
static uint64_t now_ns_(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
#endif

static void run_case_(const char* name, loxbudget_t* b, loxbudget_op_id_t op, uint32_t iters) {
  loxbudget_decision_t d;
  /* Warmup */
  for (uint32_t i = 0; i < 1000u; i++) {
    (void)loxbudget_check(b, op, &d);
  }

  const uint64_t t0 = now_ns_();
  for (uint32_t i = 0; i < iters; i++) {
    (void)loxbudget_check(b, op, &d);
  }
  const uint64_t t1 = now_ns_();
  const uint64_t dt = (t1 - t0);
  const double per = (iters == 0u) ? 0.0 : (double)dt / (double)iters;

  printf("%s: total_ns=%llu per_check_ns=%.1f\n", name, (unsigned long long)dt, per);
}

int main(void) {
  /* Baseline budget (causality off at compile time). */
  {
    static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 3, 0) + 3u) / 4u];
    loxbudget_t b;
    loxbudget_op_profile_t p0 = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t p1 = loxbudget_op_profile_default(1);

    if (loxbudget_init_simple(&b, storage, sizeof(storage), 1, 3) != LOXBUDGET_OK) return 2;
    (void)loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_REUSABLE);
    (void)loxbudget_register_op(&b, &p0);
    (void)loxbudget_register_op(&b, &p1);
    (void)loxbudget_op_set_need(&b, 0, 0, 10);
    (void)loxbudget_op_set_need(&b, 1, 0, 10);

    run_case_("baseline(no causality)", &b, 0, 200000u);
    (void)loxbudget_deinit(&b);
  }

  /* Causality-enabled variant (this file is compiled with LOXBUDGET_ENABLE_CAUSALITY=1). */
#if LOXBUDGET_ENABLE_CAUSALITY
  {
    static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(1, 3, 0) + 3u) / 4u];
    loxbudget_t b;
    loxbudget_op_profile_t p0 = loxbudget_op_profile_default(0);
    loxbudget_op_profile_t p1 = loxbudget_op_profile_default(1);
    loxbudget_op_profile_t p2 = loxbudget_op_profile_default(2);

    if (loxbudget_init_simple(&b, storage, sizeof(storage), 1, 3) != LOXBUDGET_OK) return 3;
    (void)loxbudget_set_resource(&b, 0, 100, LOXBUDGET_RES_REUSABLE);
    (void)loxbudget_register_op(&b, &p0);
    (void)loxbudget_register_op(&b, &p1);
    (void)loxbudget_register_op(&b, &p2);

    (void)loxbudget_op_set_need(&b, 0, 0, 10);
    (void)loxbudget_op_set_need(&b, 1, 0, 10);
    (void)loxbudget_op_set_need(&b, 2, 0, 10);

    (void)loxbudget_op_may_trigger(&b, 0, 1, LOXBUDGET_TRIGGER_ALWAYS);
    (void)loxbudget_op_may_trigger(&b, 0, 2, LOXBUDGET_TRIGGER_MAYBE);

    run_case_("with_causality(2 edges)", &b, 0, 200000u);
    (void)loxbudget_deinit(&b);
  }
#endif

  return 0;
}

