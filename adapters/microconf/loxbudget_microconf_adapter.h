#ifndef LOXBUDGET_MICROCONF_ADAPTER_H
#define LOXBUDGET_MICROCONF_ADAPTER_H

#include "loxbudget.h"

typedef struct microconf_t microconf_t;

loxbudget_status_t loxbudget_microconf_load(loxbudget_t* budget, microconf_t* conf);

#endif
