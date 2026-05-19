#include "loxbudget.h"

#include <string.h>

#if LOXBUDGET_ENABLE_AUDIT_TRAIL

/* Must match core init magic. */
#define LOXBUDGET_MAGIC_INIT 0x4C58424Du /* 'LXBM' */

static loxbudget_status_t loxbudget_validate_budget_(const loxbudget_t* budget) {
  if (budget == NULL) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (budget->magic != LOXBUDGET_MAGIC_INIT) { return LOXBUDGET_ERR_NOT_INITIALIZED; }
  return LOXBUDGET_OK;
}

static void loxbudget_critical_enter_(const loxbudget_t* budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->critical_enter != NULL) {
    budget->hal_cb->critical_enter(budget->hal_user);
    return;
  }
  loxbudget_hal_critical_enter();
}
static void loxbudget_critical_exit_(const loxbudget_t* budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->critical_exit != NULL) {
    budget->hal_cb->critical_exit(budget->hal_user);
    return;
  }
  loxbudget_hal_critical_exit();
}

static const loxbudget_decision_record_t* loxbudget_audit_buf_c_(const loxbudget_t* b) {
  return (const loxbudget_decision_record_t*)(b->storage + b->audit_off);
}
// cppcheck-suppress constParameterPointer
static loxbudget_decision_record_t* loxbudget_audit_buf_(loxbudget_t* b) {
  return (loxbudget_decision_record_t*)(b->storage + b->audit_off);
}

loxbudget_status_t loxbudget_audit_clear(loxbudget_t* budget) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (budget->audit_size == 0u) { return LOXBUDGET_OK; }

  loxbudget_critical_enter_(budget);
  budget->audit_head = 0u;
  budget->audit_count = 0u;
  memset(loxbudget_audit_buf_(budget), 0,
         (size_t)budget->audit_size * sizeof(loxbudget_decision_record_t));
  loxbudget_critical_exit_(budget);
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_audit_get_recent(const loxbudget_t* budget,
                                              loxbudget_decision_record_t* out, size_t max_records,
                                              size_t* out_count) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out_count == NULL) {
    return (out_count == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }
  *out_count = 0u;
  if (budget->audit_size == 0u || max_records == 0u || out == NULL) { return LOXBUDGET_OK; }

  loxbudget_critical_enter_(budget);
  {
    const uint8_t count = budget->audit_count;
    const uint8_t to_copy = (uint8_t)((max_records < (size_t)count) ? max_records : (size_t)count);
    const loxbudget_decision_record_t* ring = loxbudget_audit_buf_c_(budget);
    uint8_t i;
    for (i = 0u; i < to_copy; i++) {
      const uint8_t idx =
          (uint8_t)((budget->audit_head - 1u - i) & (uint8_t)(budget->audit_size - 1u));
      out[i] = ring[idx];
    }
    *out_count = (size_t)to_copy;
  }
  loxbudget_critical_exit_(budget);
  return LOXBUDGET_OK;
}

#endif /* LOXBUDGET_ENABLE_AUDIT_TRAIL */
