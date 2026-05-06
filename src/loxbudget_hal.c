#include "loxbudget.h"

/* Weak symbol support (GCC/Clang). MSVC uses callback HAL. */
#if defined(__GNUC__) || defined(__clang__)
#define LOXBUDGET_WEAK __attribute__((weak))
#else
#define LOXBUDGET_WEAK
#endif

uint32_t LOXBUDGET_WEAK loxbudget_hal_now_ms(void) { return 0u; }

void LOXBUDGET_WEAK loxbudget_hal_critical_enter(void) {}
void LOXBUDGET_WEAK loxbudget_hal_critical_exit(void) {}

loxbudget_bool_t LOXBUDGET_WEAK loxbudget_hal_boot_proven(void) { return LOXBUDGET_FALSE; }
loxbudget_bool_t LOXBUDGET_WEAK loxbudget_hal_voltage_ok(void) { return LOXBUDGET_FALSE; }
loxbudget_bool_t LOXBUDGET_WEAK loxbudget_hal_network_up(void) { return LOXBUDGET_FALSE; }

static uint32_t loxbudget_hal_perm_now_ms_(void* user) {
  (void)user;
  return 0u;
}
static void loxbudget_hal_perm_crit_enter_(void* user) { (void)user; }
static void loxbudget_hal_perm_crit_exit_(void* user) { (void)user; }
static loxbudget_bool_t loxbudget_hal_perm_true_(void* user) {
  (void)user;
  return LOXBUDGET_TRUE;
}

const loxbudget_hal_callbacks_t* loxbudget_hal_default_permissive(void) {
  static const loxbudget_hal_callbacks_t cb = {
      &loxbudget_hal_perm_now_ms_, &loxbudget_hal_perm_crit_enter_, &loxbudget_hal_perm_crit_exit_,
      &loxbudget_hal_perm_true_,   &loxbudget_hal_perm_true_,       &loxbudget_hal_perm_true_};
  return &cb;
}
