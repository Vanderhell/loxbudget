#include "loxbudget.h"

#if LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS

static const char* const k_action_names[] = {"ALLOW_FULL", "ALLOW_DEGRADED", "WAIT", "REJECT",
                                             "LOCKDOWN"};

static const char* const k_pressure_names[] = {"NORMAL", "ELEVATED", "CRITICAL", "SURVIVAL",
                                               "LOCKDOWN"};

static const char* const k_reason_names[] = {"OK",
                                             "INSUFFICIENT_RES",
                                             "RATE_LIMIT",
                                             "LIFETIME_EXHAUSTED",
                                             "PRESSURE_BLOCK",
                                             "LOCKDOWN_ACTIVE",
                                             "PRECONDITION_FAIL",
                                             "CAUSAL_CASCADE",
                                             "UNKNOWN_OP",
                                             "HAL_NOT_CONFIGURED"};

static const char* const k_status_names[] = {
    "OK",
    "ERR_INVALID_ARG",
    "ERR_NOT_INITIALIZED",
    "ERR_NO_SPACE",
    "ERR_NOT_FOUND",
    "ERR_DUPLICATE",
    "ERR_OVERFLOW",
    "ERR_BAD_STATE",
    "ERR_FEATURE_DISABLED",
    "ERR_HAL_NOT_CONFIGURED",
    "ERR_ALIGNMENT",
    "ERR_VERSION_MISMATCH",
};

static const char* lb__name_in_range_(const char* const* table, size_t n, int idx) {
  if (idx < 0) { return NULL; }
  if ((size_t)idx >= n) { return NULL; }
  return table[(size_t)idx];
}

const char* loxbudget_action_name(loxbudget_action_t a) {
  return lb__name_in_range_(k_action_names, sizeof(k_action_names) / sizeof(k_action_names[0]),
                            (int)a);
}

const char* loxbudget_pressure_name(loxbudget_pressure_t p) {
  return lb__name_in_range_(k_pressure_names,
                            sizeof(k_pressure_names) / sizeof(k_pressure_names[0]), (int)p);
}

const char* loxbudget_reason_name(loxbudget_reason_t r) {
  return lb__name_in_range_(k_reason_names, sizeof(k_reason_names) / sizeof(k_reason_names[0]),
                            (int)r);
}

const char* loxbudget_status_name(loxbudget_status_t s) {
  const int idx = (s <= 0) ? (int)(-s) : 999999;
  return lb__name_in_range_(k_status_names, sizeof(k_status_names) / sizeof(k_status_names[0]),
                            idx);
}

#endif /* LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS */
