#ifndef LOXBUDGET_MICROCONF_ADAPTER_H
#define LOXBUDGET_MICROCONF_ADAPTER_H

#include "loxbudget.h"

#include <stddef.h>

typedef struct microconf_t microconf_t;

/* Minimal external dependency: user must provide these accessors.
 * The adapter reads arrays of definitions from the config object.
 */
typedef struct {
  loxbudget_resource_id_t id;
  uint16_t limit;
  uint8_t kind; /* loxbudget_resource_kind_t */
} loxbudget_microconf_resource_t;

typedef struct {
  loxbudget_op_id_t op_id;
  loxbudget_resource_id_t resource_id;
  uint16_t amount;
} loxbudget_microconf_need_t;

size_t microconf_loxbudget_resource_count(microconf_t* conf);
const loxbudget_microconf_resource_t* microconf_loxbudget_resources(microconf_t* conf);

size_t microconf_loxbudget_op_count(microconf_t* conf);
const loxbudget_op_profile_t* microconf_loxbudget_ops(microconf_t* conf);

size_t microconf_loxbudget_need_count(microconf_t* conf);
const loxbudget_microconf_need_t* microconf_loxbudget_needs(microconf_t* conf);

loxbudget_status_t loxbudget_microconf_load(loxbudget_t* budget, microconf_t* conf);

#endif
