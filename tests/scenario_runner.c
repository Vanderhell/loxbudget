#include "loxbudget.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef LOXBUDGET_ENABLE_CAUSALITY
#define LOXBUDGET_ENABLE_CAUSALITY 1
#endif

static const char* skip_ws_(const char* s) {
  while (*s != '\0' && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) {
    s++;
  }
  return s;
}

static void rstrip_(char* s) {
  size_t n = strlen(s);
  while (n != 0u) {
    const char c = s[n - 1u];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      s[n - 1u] = '\0';
      n--;
    } else {
      break;
    }
  }
}

static int streq_(const char* a, const char* b) { return strcmp(a, b) == 0; }

static int parse_u8_(const char* s, uint8_t* out) {
  unsigned v = 0;
  if (sscanf(s, "%u", &v) != 1) return 0;
  if (v > 255u) return 0;
  *out = (uint8_t)v;
  return 1;
}

static int parse_u16_(const char* s, uint16_t* out) {
  unsigned v = 0;
  if (sscanf(s, "%u", &v) != 1) return 0;
  if (v > 65535u) return 0;
  *out = (uint16_t)v;
  return 1;
}

static int parse_pressure_(const char* s, loxbudget_pressure_t* out) {
  if (streq_(s, "NORMAL")) {
    *out = LOXBUDGET_PRESSURE_NORMAL;
    return 1;
  }
  if (streq_(s, "ELEVATED")) {
    *out = LOXBUDGET_PRESSURE_ELEVATED;
    return 1;
  }
  if (streq_(s, "CRITICAL")) {
    *out = LOXBUDGET_PRESSURE_CRITICAL;
    return 1;
  }
  if (streq_(s, "SURVIVAL")) {
    *out = LOXBUDGET_PRESSURE_SURVIVAL;
    return 1;
  }
  if (streq_(s, "LOCKDOWN")) {
    *out = LOXBUDGET_PRESSURE_LOCKDOWN;
    return 1;
  }
  return 0;
}

static int parse_action_(const char* s, loxbudget_action_t* out) {
  if (streq_(s, "ALLOW_FULL")) {
    *out = LOXBUDGET_ALLOW_FULL;
    return 1;
  }
  if (streq_(s, "ALLOW_DEGRADED")) {
    *out = LOXBUDGET_ALLOW_DEGRADED;
    return 1;
  }
  if (streq_(s, "WAIT")) {
    *out = LOXBUDGET_WAIT;
    return 1;
  }
  if (streq_(s, "REJECT")) {
    *out = LOXBUDGET_REJECT;
    return 1;
  }
  if (streq_(s, "LOCKDOWN")) {
    *out = LOXBUDGET_LOCKDOWN;
    return 1;
  }
  return 0;
}

static int parse_reason_(const char* s, uint8_t* out) {
  if (streq_(s, "OK")) {
    *out = (uint8_t)LOXBUDGET_REASON_OK;
    return 1;
  }
  if (streq_(s, "INSUFFICIENT_RES")) {
    *out = (uint8_t)LOXBUDGET_REASON_INSUFFICIENT_RES;
    return 1;
  }
  if (streq_(s, "RATE_LIMIT")) {
    *out = (uint8_t)LOXBUDGET_REASON_RATE_LIMIT;
    return 1;
  }
  if (streq_(s, "LIFETIME_EXHAUSTED")) {
    *out = (uint8_t)LOXBUDGET_REASON_LIFETIME_EXHAUSTED;
    return 1;
  }
  if (streq_(s, "PRESSURE_BLOCK")) {
    *out = (uint8_t)LOXBUDGET_REASON_PRESSURE_BLOCK;
    return 1;
  }
  if (streq_(s, "LOCKDOWN_ACTIVE")) {
    *out = (uint8_t)LOXBUDGET_REASON_LOCKDOWN_ACTIVE;
    return 1;
  }
  if (streq_(s, "PRECONDITION_FAIL")) {
    *out = (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL;
    return 1;
  }
  if (streq_(s, "CAUSAL_CASCADE")) {
    *out = (uint8_t)LOXBUDGET_REASON_CAUSAL_CASCADE;
    return 1;
  }
  if (streq_(s, "UNKNOWN_OP")) {
    *out = (uint8_t)LOXBUDGET_REASON_UNKNOWN_OP;
    return 1;
  }
  if (streq_(s, "HAL_NOT_CONFIGURED")) {
    *out = (uint8_t)LOXBUDGET_REASON_HAL_NOT_CONFIGURED;
    return 1;
  }
  return 0;
}

static int parse_trigger_(const char* s, loxbudget_trigger_kind_t* out) {
  if (streq_(s, "NEVER")) {
    *out = LOXBUDGET_TRIGGER_NEVER;
    return 1;
  }
  if (streq_(s, "RARE")) {
    *out = LOXBUDGET_TRIGGER_RARE;
    return 1;
  }
  if (streq_(s, "MAYBE")) {
    *out = LOXBUDGET_TRIGGER_MAYBE;
    return 1;
  }
  if (streq_(s, "ALWAYS")) {
    *out = LOXBUDGET_TRIGGER_ALWAYS;
    return 1;
  }
  return 0;
}

static int parse_status_(const char* s, loxbudget_status_t* out) {
  if (streq_(s, "OK")) {
    *out = LOXBUDGET_OK;
    return 1;
  }
  if (streq_(s, "ERR_INVALID_ARG")) {
    *out = LOXBUDGET_ERR_INVALID_ARG;
    return 1;
  }
  if (streq_(s, "ERR_NOT_INITIALIZED")) {
    *out = LOXBUDGET_ERR_NOT_INITIALIZED;
    return 1;
  }
  if (streq_(s, "ERR_NO_SPACE")) {
    *out = LOXBUDGET_ERR_NO_SPACE;
    return 1;
  }
  if (streq_(s, "ERR_NOT_FOUND")) {
    *out = LOXBUDGET_ERR_NOT_FOUND;
    return 1;
  }
  if (streq_(s, "ERR_BAD_STATE")) {
    *out = LOXBUDGET_ERR_BAD_STATE;
    return 1;
  }
  if (streq_(s, "ERR_FEATURE_DISABLED")) {
    *out = LOXBUDGET_ERR_FEATURE_DISABLED;
    return 1;
  }
  return 0;
}

static int parse_kind_(const char* s, loxbudget_resource_kind_t* out) {
  if (streq_(s, "REUSABLE")) {
    *out = LOXBUDGET_RES_REUSABLE;
    return 1;
  }
  if (streq_(s, "CONSUMABLE")) {
    *out = LOXBUDGET_RES_CONSUMABLE;
    return 1;
  }
  if (streq_(s, "STATE")) {
    *out = LOXBUDGET_RES_STATE;
    return 1;
  }
  return 0;
}

static int fail_(unsigned line, const char* msg) {
  fprintf(stderr, "scenario_runner: line %u: %s\n", line, msg);
  return 2;
}

int main(void) {
  /* Use a max-sized buffer; scenario lines configure actual max_res/max_ops. */
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(LOXBUDGET_MAX_RESOURCES, LOXBUDGET_MAX_OPS, 0) +
                           3u) /
                          4u];
  loxbudget_t b;
  loxbudget_pressure_t pressure = LOXBUDGET_PRESSURE_NORMAL;

  /* Defaults (can be overridden by `init <max_res> <max_ops>` line). */
  uint8_t max_res = 2;
  uint8_t max_ops = 4;
  int did_init = 0;

  char line[512];
  unsigned lineno = 0;
  while (fgets(line, sizeof(line), stdin) != NULL) {
    lineno++;
    rstrip_(line);
    const char* s = skip_ws_(line);
    if (*s == '\0' || *s == '#') continue;

    char cmd[64] = {0};
    if (sscanf(s, "%63s", cmd) != 1) continue;

    if (streq_(cmd, "init")) {
      unsigned r = 0, o = 0;
      if (sscanf(s, "init %u %u", &r, &o) != 2) return fail_(lineno, "usage: init <max_res> <max_ops>");
      if (r == 0u || o == 0u || r > LOXBUDGET_MAX_RESOURCES || o > LOXBUDGET_MAX_OPS) {
        return fail_(lineno, "init out of range");
      }
      max_res = (uint8_t)r;
      max_ops = (uint8_t)o;
      if (loxbudget_init_simple(&b, storage, sizeof(storage), max_res, max_ops) != LOXBUDGET_OK) {
        return fail_(lineno, "loxbudget_init_simple failed");
      }
      did_init = 1;
      continue;
    }

    if (!did_init) {
      if (loxbudget_init_simple(&b, storage, sizeof(storage), max_res, max_ops) != LOXBUDGET_OK) {
        return fail_(lineno, "implicit init failed");
      }
      did_init = 1;
    }

    if (streq_(cmd, "resource")) {
      uint8_t id;
      uint16_t limit;
      char kind_s[32];
      if (sscanf(s, "resource %hhu %hu %31s", &id, &limit, kind_s) != 3) {
        return fail_(lineno, "usage: resource <id> <limit> <REUSABLE|CONSUMABLE|STATE>");
      }
      loxbudget_resource_kind_t kind;
      if (!parse_kind_(kind_s, &kind)) return fail_(lineno, "invalid resource kind");
      if (loxbudget_set_resource(&b, id, limit, kind) != LOXBUDGET_OK) {
        return fail_(lineno, "loxbudget_set_resource failed");
      }
      continue;
    }

    if (streq_(cmd, "op")) {
      uint8_t id;
      if (sscanf(s, "op %hhu", &id) != 1) return fail_(lineno, "usage: op <id>");
      loxbudget_op_profile_t p = loxbudget_op_profile_default(id);
      if (loxbudget_register_op(&b, &p) != LOXBUDGET_OK) return fail_(lineno, "loxbudget_register_op failed");
      continue;
    }

    if (streq_(cmd, "need")) {
      uint8_t op, res;
      uint16_t amt;
      if (sscanf(s, "need %hhu %hhu %hu", &op, &res, &amt) != 3) {
        return fail_(lineno, "usage: need <op> <res> <amount>");
      }
      if (loxbudget_op_set_need(&b, op, res, amt) != LOXBUDGET_OK) return fail_(lineno, "loxbudget_op_set_need failed");
      continue;
    }

    if (streq_(cmd, "edge")) {
      uint8_t parent, child;
      char kind_s[32];
      if (sscanf(s, "edge %hhu %hhu %31s", &parent, &child, kind_s) != 3) {
        return fail_(lineno, "usage: edge <parent> <child> <NEVER|RARE|MAYBE|ALWAYS>");
      }
      loxbudget_trigger_kind_t kind;
      if (!parse_trigger_(kind_s, &kind)) return fail_(lineno, "invalid trigger kind");
      const loxbudget_status_t st = loxbudget_op_may_trigger(&b, parent, child, kind);
      if (st != LOXBUDGET_OK) return fail_(lineno, "loxbudget_op_may_trigger failed");
      continue;
    }

    if (streq_(cmd, "expect_edge")) {
      uint8_t parent, child;
      char kind_s[32];
      char st_s[64];
      if (sscanf(s, "expect_edge %hhu %hhu %31s status=%63s", &parent, &child, kind_s, st_s) != 4) {
        return fail_(lineno, "usage: expect_edge <parent> <child> <KIND> status=<STATUS>");
      }
      loxbudget_trigger_kind_t kind;
      if (!parse_trigger_(kind_s, &kind)) return fail_(lineno, "invalid trigger kind");
      loxbudget_status_t exp;
      if (!parse_status_(st_s, &exp)) return fail_(lineno, "invalid expected status");
      const loxbudget_status_t got = loxbudget_op_may_trigger(&b, parent, child, kind);
      if (got != exp) {
        fprintf(stderr, "expected status=%d got=%d\n", (int)exp, (int)got);
        return 1;
      }
      continue;
    }

    if (streq_(cmd, "expect_edge_count")) {
      uint16_t n = 0;
      if (sscanf(s, "expect_edge_count %hu", &n) != 1) return fail_(lineno, "usage: expect_edge_count <n>");
      const uint16_t got = loxbudget_causality_edge_count(&b);
      if (got != n) {
        fprintf(stderr, "expected edge_count=%u got=%u\n", (unsigned)n, (unsigned)got);
        return 1;
      }
      continue;
    }

    if (streq_(cmd, "pressure")) {
      char p_s[32];
      if (sscanf(s, "pressure %31s", p_s) != 1) return fail_(lineno, "usage: pressure <NORMAL|ELEVATED|CRITICAL|SURVIVAL|LOCKDOWN>");
      if (!parse_pressure_(p_s, &pressure)) return fail_(lineno, "invalid pressure");
      if (loxbudget_set_pressure(&b, pressure) != LOXBUDGET_OK) return fail_(lineno, "loxbudget_set_pressure failed");
      continue;
    }

    if (streq_(cmd, "expect_check")) {
      uint8_t op;
      char action_s[32];
      char reason_s[64];
      int have_reason = 0;
      if (sscanf(s, "expect_check %hhu %31s reason=%63s", &op, action_s, reason_s) == 3) {
        have_reason = 1;
      } else if (sscanf(s, "expect_check %hhu %31s", &op, action_s) != 2) {
        return fail_(lineno, "usage: expect_check <op> <ACTION> [reason=<REASON>]");
      }

      loxbudget_action_t exp_action;
      if (!parse_action_(action_s, &exp_action)) return fail_(lineno, "invalid expected action");

      uint8_t exp_reason = 0;
      if (have_reason) {
        if (!parse_reason_(reason_s, &exp_reason)) return fail_(lineno, "invalid expected reason");
      }

      loxbudget_decision_t d;
      if (loxbudget_check(&b, op, &d) != LOXBUDGET_OK) return fail_(lineno, "loxbudget_check failed");
      if (d.action != exp_action) {
        fprintf(stderr, "expected action=%u got=%u\n", (unsigned)exp_action, (unsigned)d.action);
        return 1;
      }
      if (have_reason && d.reason != exp_reason) {
        fprintf(stderr, "expected reason=%u got=%u\n", (unsigned)exp_reason, (unsigned)d.reason);
        return 1;
      }
      continue;
    }

    return fail_(lineno, "unknown command");
  }

  if (did_init) { (void)loxbudget_deinit(&b); }
  return 0;
}
