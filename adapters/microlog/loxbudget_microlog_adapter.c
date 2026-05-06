#include "loxbudget_microlog_adapter.h"

#include <stddef.h>
#include <string.h>

typedef loxbudget_microlog_ctx_t lb_microlog_ctx_t;

static void lb__utoa_(char* out, size_t out_cap, uint32_t v) {
  char tmp[10];
  size_t n = 0;
  if (out_cap == 0u) { return; }
  if (v == 0u) {
    out[0] = '0';
    out[1 < out_cap ? 1 : 0] = 0;
    return;
  }
  while (v != 0u && n < sizeof(tmp)) {
    tmp[n++] = (char)('0' + (char)(v % 10u));
    v /= 10u;
  }
  {
    size_t i = 0;
    size_t j = n;
    while (i + 1u < out_cap && j > 0u) {
      out[i++] = tmp[--j];
    }
    out[i] = 0;
  }
}

static void lb__append_(char* buf, size_t cap, const char* s) {
  const size_t cur = strlen(buf);
  if (cur + 1u >= cap) { return; }
  strncat(buf, s, cap - cur - 1u);
}

static void lb__on_decision_(void* user, const loxbudget_decision_t* d, loxbudget_op_id_t op) {
  lb_microlog_ctx_t* ctx = (lb_microlog_ctx_t*)user;
  if (ctx == NULL || ctx->log == NULL || d == NULL) { return; }
  if (d->action < ctx->min_action) { return; }

  char msg[128];
  char num[16];
  msg[0] = 0;

  lb__append_(msg, sizeof(msg), "loxbudget op=");
  lb__utoa_(num, sizeof(num), (uint32_t)op);
  lb__append_(msg, sizeof(msg), num);

  lb__append_(msg, sizeof(msg), " action=");
  lb__utoa_(num, sizeof(num), (uint32_t)d->action);
  lb__append_(msg, sizeof(msg), num);

  lb__append_(msg, sizeof(msg), " pressure=");
  lb__utoa_(num, sizeof(num), (uint32_t)d->pressure);
  lb__append_(msg, sizeof(msg), num);

  lb__append_(msg, sizeof(msg), " reason=");
  lb__utoa_(num, sizeof(num), (uint32_t)d->reason);
  lb__append_(msg, sizeof(msg), num);

  microlog_write(ctx->log, msg);
}

void loxbudget_microlog_attach(loxbudget_t* budget, microlog_t* log,
                               loxbudget_action_t min_action_to_log) {
  static lb_microlog_ctx_t ctx;
  ctx.log = log;
  ctx.min_action = min_action_to_log;
  (void)loxbudget_set_decision_hook(budget, &lb__on_decision_, &ctx);
}
