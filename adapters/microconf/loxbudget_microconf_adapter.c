#include "loxbudget_microconf_adapter.h"

loxbudget_status_t loxbudget_microconf_load(loxbudget_t* budget, microconf_t* conf) {
  if (budget == NULL || conf == NULL) return LOXBUDGET_ERR_INVALID_ARG;

  /* Resources */
  {
    const size_t n = microconf_loxbudget_resource_count(conf);
    const loxbudget_microconf_resource_t* rs = microconf_loxbudget_resources(conf);
    if (n != 0u && rs == NULL) return LOXBUDGET_ERR_INVALID_ARG;
    for (size_t i = 0; i < n; i++) {
      loxbudget_status_t st = loxbudget_set_resource(budget, rs[i].id, rs[i].limit,
                                                     (loxbudget_resource_kind_t)rs[i].kind);
      if (st != LOXBUDGET_OK) return st;
    }
  }

  /* Operation profiles */
  {
    const size_t n = microconf_loxbudget_op_count(conf);
    const loxbudget_op_profile_t* ops = microconf_loxbudget_ops(conf);
    if (n != 0u && ops == NULL) return LOXBUDGET_ERR_INVALID_ARG;
    for (size_t i = 0; i < n; i++) {
      loxbudget_status_t st = loxbudget_register_op(budget, &ops[i]);
      if (st != LOXBUDGET_OK) return st;
    }
  }

  /* Needs */
  {
    const size_t n = microconf_loxbudget_need_count(conf);
    const loxbudget_microconf_need_t* ns = microconf_loxbudget_needs(conf);
    if (n != 0u && ns == NULL) return LOXBUDGET_ERR_INVALID_ARG;
    for (size_t i = 0; i < n; i++) {
      loxbudget_status_t st =
          loxbudget_op_set_need(budget, ns[i].op_id, ns[i].resource_id, ns[i].amount);
      if (st != LOXBUDGET_OK) return st;
    }
  }

  return LOXBUDGET_OK;
}
