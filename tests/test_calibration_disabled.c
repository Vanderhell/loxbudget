#include "loxbudget.h"

#include <assert.h>
#include <string.h>

int main(void) {
  loxbudget_t b;
  loxbudget_sample_t s;
  loxbudget_suggested_profile_t out;
  size_t n = 0;
  size_t written = 0;
  uint8_t buf[16];

  memset(&b, 0, sizeof(b));
  memset(&s, 0, sizeof(s));
  memset(&out, 0, sizeof(out));

  assert(loxbudget_calibrate_begin(&b, 0, 1) == LOXBUDGET_ERR_FEATURE_DISABLED);
  assert(loxbudget_calibrate_sample(&b, 0, &s) == LOXBUDGET_ERR_FEATURE_DISABLED);
  assert(loxbudget_calibrate_end(&b, 0, &out) == LOXBUDGET_ERR_FEATURE_DISABLED);
  assert(loxbudget_calibration_export_size(&b, &n) == LOXBUDGET_ERR_FEATURE_DISABLED);
  assert(loxbudget_calibration_export(&b, buf, sizeof(buf), &written) ==
         LOXBUDGET_ERR_FEATURE_DISABLED);
  return 0;
}
