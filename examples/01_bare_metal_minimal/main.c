#include "loxbudget.h"

int main(void) {
  static uint32_t storage[(LOXBUDGET_REQUIRED_SIZE(2, 2, 0) + 3u) / 4u];
  loxbudget_t b;
  loxbudget_op_profile_t p = loxbudget_op_profile_default(0);
  loxbudget_decision_t d;

  if (loxbudget_init_simple(&b, storage, sizeof(storage), 2, 2) != LOXBUDGET_OK) return 2;
  (void)loxbudget_set_resource(&b, 0, 10, LOXBUDGET_RES_REUSABLE);
  (void)loxbudget_register_op(&b, &p);
  (void)loxbudget_op_set_need(&b, 0, 0, 5);
  (void)loxbudget_check(&b, 0, &d);
  return (d.action == LOXBUDGET_ALLOW_FULL) ? 0 : 1;
}
