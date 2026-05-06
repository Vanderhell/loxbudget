#include "loxbudget_microconf_adapter.h"

loxbudget_status_t loxbudget_microconf_load(loxbudget_t* budget, microconf_t* conf) {
  (void)budget;
  (void)conf;
  return LOXBUDGET_ERR_FEATURE_DISABLED;
}
