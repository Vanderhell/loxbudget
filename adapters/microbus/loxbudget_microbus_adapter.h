#ifndef LOXBUDGET_MICROBUS_ADAPTER_H
#define LOXBUDGET_MICROBUS_ADAPTER_H

#include "loxbudget.h"

typedef struct microbus_t microbus_t;

void loxbudget_microbus_attach(loxbudget_t* budget, microbus_t* bus);

#endif
