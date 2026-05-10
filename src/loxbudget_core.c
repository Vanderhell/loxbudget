#include "loxbudget.h" /* core */

#include <string.h>

/* Optional causality helper implemented in src/loxbudget_causality.c. */
#if LOXBUDGET_ENABLE_CAUSALITY
uint32_t lb__causality_add_scaled_needs_(loxbudget_t* budget, loxbudget_op_id_t root_op,
                                         uint16_t* io_need_per_resource,
                                         loxbudget_pressure_t pressure);
#endif

/* Internal constants */
#define LOXBUDGET_MAGIC_INIT 0x4C58424Du /* 'LXBM' */

#define LOXBUDGET_ALIGN_UP(v, a) (((v) + ((a) - 1u)) & ~((a) - 1u))

typedef struct {
  uint32_t magic2;
  uint8_t res_cfg[LOXBUDGET_MAX_RESOURCES];
  uint8_t op_cfg[LOXBUDGET_MAX_OPS];
  uint8_t _pad[4];
} loxbudget_storage_hdr_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_storage_hdr_t) == 32, "storage header size");

/* Internal storage structs. Sized to match SPEC.md required sizes. */
typedef struct {
  uint16_t limit;
  uint16_t used;
  uint16_t reserved;
  uint16_t high_watermark;
  uint8_t kind;
  uint8_t flags;
  uint16_t _pad;
} loxbudget_resource_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_resource_t) == 12, "resource size");

typedef struct {
  loxbudget_resource_id_t resource;
  uint8_t _pad;
  uint16_t amount;
} loxbudget_need_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_need_t) == 4, "need size");

typedef struct {
  uint16_t magic;
  uint8_t active;
  uint8_t acquired_pressure;
} loxbudget_lease_slot_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_lease_slot_t) == 4, "lease slot meta");

static loxbudget_resource_t* loxbudget_resources_(loxbudget_t* b) {
  return (loxbudget_resource_t*)(b->storage + b->resources_off);
}
static const loxbudget_resource_t* loxbudget_resources_c_(const loxbudget_t* b) {
  return (const loxbudget_resource_t*)(b->storage + b->resources_off);
}
static loxbudget_op_profile_t* loxbudget_ops_(loxbudget_t* b) {
  return (loxbudget_op_profile_t*)(b->storage + b->ops_off);
}
static const loxbudget_op_profile_t* loxbudget_ops_c_(const loxbudget_t* b) {
  return (const loxbudget_op_profile_t*)(b->storage + b->ops_off);
}
static loxbudget_need_t* loxbudget_needs_(loxbudget_t* b) {
  return (loxbudget_need_t*)(b->storage + b->needs_off);
}
static const loxbudget_need_t* loxbudget_needs_c_(const loxbudget_t* b) {
  return (const loxbudget_need_t*)(b->storage + b->needs_off);
}
static loxbudget_lease_slot_t* loxbudget_lease_slots_(loxbudget_t* b) {
  return (loxbudget_lease_slot_t*)(b->storage + b->lease_slots_off);
}
static const loxbudget_lease_slot_t* loxbudget_lease_slots_c_(const loxbudget_t* b) {
  return (const loxbudget_lease_slot_t*)(b->storage + b->lease_slots_off);
}

static loxbudget_decision_record_t* loxbudget_audit_buf_(loxbudget_t* b) {
  return (loxbudget_decision_record_t*)(b->storage + b->audit_off);
}
#if defined(__GNUC__) || defined(__clang__)
#define LB__UNUSED __attribute__((unused))
#else
#define LB__UNUSED
#endif
static LB__UNUSED const loxbudget_decision_record_t* loxbudget_audit_buf_c_(const loxbudget_t* b) {
  return (const loxbudget_decision_record_t*)(b->storage + b->audit_off);
}
#undef LB__UNUSED

#if LOXBUDGET_ENABLE_RATE_WINDOWS
typedef struct {
  uint32_t window_start_ms;
  uint32_t window_duration_ms;
  uint32_t consumed;
  uint32_t limit;
} lb__rate_window_t;
typedef struct {
  lb__rate_window_t windows[4];
  uint32_t consumed_lifetime;
  uint32_t limit_lifetime;
} lb__rate_state_t;
LOXBUDGET_STATIC_ASSERT(sizeof(lb__rate_state_t) == 72, "rate state size");

static lb__rate_state_t* lb__rate_state_(loxbudget_t* b) {
  return (lb__rate_state_t*)(b->storage + b->rate_off);
}
static const lb__rate_state_t* lb__rate_state_c_(const loxbudget_t* b) {
  return (const lb__rate_state_t*)(b->storage + b->rate_off);
}

static uint32_t lb__window_duration_ms_(loxbudget_window_t w) {
  switch (w) {
  case LOXBUDGET_WINDOW_SECOND:
    return 1000u;
  case LOXBUDGET_WINDOW_MINUTE:
    return 60000u;
  case LOXBUDGET_WINDOW_HOUR:
    return 3600000u;
  case LOXBUDGET_WINDOW_DAY:
    return 86400000u;
  default:
    return 0u;
  }
}
#endif

static loxbudget_storage_hdr_t* loxbudget_hdr_(loxbudget_t* b) {
  return (loxbudget_storage_hdr_t*)b->storage;
}
static const loxbudget_storage_hdr_t* loxbudget_hdr_c_(const loxbudget_t* b) {
  return (const loxbudget_storage_hdr_t*)b->storage;
}

static loxbudget_bool_t loxbudget_is_aligned_u32_(const void* p) {
  return (((uintptr_t)p) & 3u) == 0u ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static loxbudget_bool_t loxbudget_is_pow2_or_zero_u8_(uint8_t v) {
  if (v == 0u) { return LOXBUDGET_TRUE; }
  return ((v & (uint8_t)(v - 1u)) == 0u) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static loxbudget_status_t loxbudget_validate_init_args_(loxbudget_t* budget, void* storage,
                                                        size_t storage_size,
                                                        const loxbudget_config_t* cfg) {
  if (budget == NULL || storage == NULL || cfg == NULL) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (storage_size == 0u) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (loxbudget_is_aligned_u32_(storage) == LOXBUDGET_FALSE) { return LOXBUDGET_ERR_ALIGNMENT; }
  if (cfg->max_resources == 0u || cfg->max_ops == 0u || cfg->max_concurrent_leases == 0u) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (cfg->max_resources > LOXBUDGET_MAX_RESOURCES || cfg->max_ops > LOXBUDGET_MAX_OPS ||
      cfg->max_concurrent_leases > LOXBUDGET_MAX_LEASES) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (loxbudget_is_pow2_or_zero_u8_(cfg->audit_size) == LOXBUDGET_FALSE) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  return LOXBUDGET_OK;
}

static loxbudget_status_t loxbudget_validate_budget_(const loxbudget_t* budget) {
  if (budget == NULL) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (budget->magic != LOXBUDGET_MAGIC_INIT) { return LOXBUDGET_ERR_NOT_INITIALIZED; }
  return LOXBUDGET_OK;
}

static uint16_t loxbudget_lease_magic_for_id_(const loxbudget_t* budget, uint8_t id) {
  return (uint16_t)(budget->lease_magic_base ^ (uint16_t)id);
}

static uint16_t loxbudget_available_u16_(uint16_t limit, uint16_t used, uint16_t reserved) {
  uint32_t v = (uint32_t)limit;
  if ((uint32_t)used > v) {
    v = 0u;
  } else {
    v -= (uint32_t)used;
  }
  if ((uint32_t)reserved > v) {
    v = 0u;
  } else {
    v -= (uint32_t)reserved;
  }
  return (uint16_t)v;
}

static const loxbudget_op_profile_t* loxbudget_profile_c_(const loxbudget_t* b,
                                                          loxbudget_op_id_t op) {
  if (op >= b->max_ops) { return NULL; }
  if (loxbudget_hdr_c_(b)->op_cfg[op] == 0u) { return NULL; }
  return &loxbudget_ops_c_(b)[op];
}

static uint8_t loxbudget_mapped_action_(const loxbudget_op_profile_t* p, uint8_t pressure) {
  switch ((loxbudget_pressure_t)pressure) {
  case LOXBUDGET_PRESSURE_NORMAL:
    return p->action_normal;
  case LOXBUDGET_PRESSURE_ELEVATED:
    return p->action_elevated;
  case LOXBUDGET_PRESSURE_CRITICAL:
    return p->action_critical;
  case LOXBUDGET_PRESSURE_SURVIVAL:
    return p->action_survival;
  case LOXBUDGET_PRESSURE_LOCKDOWN:
  default:
    return p->action_lockdown;
  }
}

static loxbudget_bool_t loxbudget_hal_bool_(const loxbudget_t* b, loxbudget_bool_t (*cb)(void*),
                                            loxbudget_bool_t (*weak)(void)) {
  if (b->hal_cb != NULL && cb != NULL) { return cb(b->hal_user); }
  if (b->hal_strict != 0u) { return 2u; /* sentinel: not configured */ }
  return weak();
}

static loxbudget_bool_t loxbudget_hal_boot_proven_(const loxbudget_t* b) {
  return loxbudget_hal_bool_(b, (b->hal_cb != NULL) ? b->hal_cb->boot_proven : NULL,
                             &loxbudget_hal_boot_proven);
}
static loxbudget_bool_t loxbudget_hal_voltage_ok_(const loxbudget_t* b) {
  return loxbudget_hal_bool_(b, (b->hal_cb != NULL) ? b->hal_cb->voltage_ok : NULL,
                             &loxbudget_hal_voltage_ok);
}
static loxbudget_bool_t loxbudget_hal_network_up_(const loxbudget_t* b) {
  return loxbudget_hal_bool_(b, (b->hal_cb != NULL) ? b->hal_cb->network_up : NULL,
                             &loxbudget_hal_network_up);
}

static uint32_t loxbudget_hal_now_ms_(const loxbudget_t* budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->now_ms != NULL) {
    return budget->hal_cb->now_ms(budget->hal_user);
  }
  return loxbudget_hal_now_ms();
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

static void loxbudget_audit_record_(loxbudget_t* b, loxbudget_op_id_t op,
                                    const loxbudget_decision_t* d) {
#if LOXBUDGET_ENABLE_AUDIT_TRAIL
  if (b->audit_size == 0u) { return; }
  loxbudget_critical_enter_(b);
  {
    loxbudget_decision_record_t* ring = loxbudget_audit_buf_(b);
    const uint8_t idx = b->audit_head;
    ring[idx].timestamp_ms = loxbudget_hal_now_ms_(b);
    ring[idx].op_id = op;
    ring[idx].denied_resource = d->denied_resource;
    ring[idx].requested = d->requested;
    ring[idx].available = d->available;
    ring[idx].action = (uint8_t)d->action;
    ring[idx].pressure = (uint8_t)d->pressure;
    ring[idx].reason = (uint8_t)d->reason;
    ring[idx]._pad = 0u;

    b->audit_head = (uint8_t)((idx + 1u) & (uint8_t)(b->audit_size - 1u));
    if (b->audit_count < b->audit_size) { b->audit_count++; }
  }
  loxbudget_critical_exit_(b);
#else
  (void)b;
  (void)op;
  (void)d;
#endif
}

loxbudget_status_t loxbudget_init(loxbudget_t* budget, void* storage, size_t storage_size,
                                  const loxbudget_config_t* cfg) {
  loxbudget_status_t st = loxbudget_validate_init_args_(budget, storage, storage_size, cfg);
  if (st != LOXBUDGET_OK) { return st; }

  /* Zero the entire instance and storage tables area we manage. */
  memset(budget, 0, sizeof(*budget));

  budget->magic = LOXBUDGET_MAGIC_INIT;
  budget->max_resources = cfg->max_resources;
  budget->max_ops = cfg->max_ops;
  budget->max_leases = cfg->max_concurrent_leases;
  budget->pressure = (uint8_t)LOXBUDGET_PRESSURE_NORMAL;
  budget->storage = (uint8_t*)storage;
  budget->storage_size = (uint32_t)storage_size;
  budget->hal_cb = cfg->hal_callbacks;
  budget->hal_user = cfg->hal_user;
  budget->hal_strict = cfg->hal_strict ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
  budget->audit_size = cfg->audit_size;
  budget->audit_head = 0u;
  budget->audit_count = 0u;
  budget->decision_hook = NULL;
  budget->decision_hook_user = NULL;
  budget->rate_off = 0u;
  budget->calib_off = 0u;

  /* Compute table layout within user storage. */
  {
    uint32_t off = 0u;
    const uint32_t align = 4u;

    /* Storage header (fits into spec's 32-byte header reservation). */
    off += (uint32_t)sizeof(loxbudget_storage_hdr_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

    budget->resources_off = off;
    off += (uint32_t)budget->max_resources * (uint32_t)sizeof(loxbudget_resource_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

    budget->ops_off = off;
    off += (uint32_t)budget->max_ops * (uint32_t)sizeof(loxbudget_op_profile_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

    budget->needs_off = off;
    off += (uint32_t)budget->max_ops * (uint32_t)LOXBUDGET_MAX_NEEDS_PER_OP *
           (uint32_t)sizeof(loxbudget_need_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

    budget->lease_slots_off = off;
    off += (uint32_t)budget->max_leases * (uint32_t)sizeof(loxbudget_lease_slot_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

    /* Audit ring. */
    budget->audit_off = off;
    off += (uint32_t)cfg->audit_size * (uint32_t)sizeof(loxbudget_decision_record_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

#if LOXBUDGET_ENABLE_RATE_WINDOWS
    budget->rate_off = off;
    off += (uint32_t)budget->max_resources * (uint32_t)sizeof(lb__rate_state_t);
    off = LOXBUDGET_ALIGN_UP(off, align);
#endif

#if LOXBUDGET_ENABLE_CALIBRATION
    budget->calib_off = off;
    off += (uint32_t)budget->max_ops * (uint32_t)LOXBUDGET_CALIB_STATE_SIZE;
    off = LOXBUDGET_ALIGN_UP(off, align);
#endif

#if LOXBUDGET_ENABLE_CAUSALITY
    /* Causality tables: header + fixed edge array + visited bitmap. */
    off += 4u; /* lb__causality_hdr_t */
    off += (uint32_t)LOXBUDGET_CAUSALITY_MAX_EDGES * 4u;
    off += ((uint32_t)budget->max_ops + 7u) / 8u;
    off = LOXBUDGET_ALIGN_UP(off, align);
#endif

    /* Alignment slack. */
    off += 16u;

    if (off > (uint32_t)storage_size) {
      memset(budget, 0, sizeof(*budget));
      return LOXBUDGET_ERR_NO_SPACE;
    }
  }

  /* Clear tables. */
  memset(loxbudget_hdr_(budget), 0, sizeof(loxbudget_storage_hdr_t));
  loxbudget_hdr_(budget)->magic2 = 0x4C584248u; /* 'LXBH' */
  memset(loxbudget_resources_(budget), 0,
         (size_t)budget->max_resources * sizeof(loxbudget_resource_t));
  memset(loxbudget_ops_(budget), 0, (size_t)budget->max_ops * sizeof(loxbudget_op_profile_t));
  memset(loxbudget_needs_(budget), 0,
         (size_t)budget->max_ops * LOXBUDGET_MAX_NEEDS_PER_OP * sizeof(loxbudget_need_t));
  memset(loxbudget_lease_slots_(budget), 0,
         (size_t)budget->max_leases * sizeof(loxbudget_lease_slot_t));
  if (budget->audit_size != 0u) {
    memset(loxbudget_audit_buf_(budget), 0,
           (size_t)budget->audit_size * sizeof(loxbudget_decision_record_t));
  }

#if LOXBUDGET_ENABLE_RATE_WINDOWS
  memset(lb__rate_state_(budget), 0, (size_t)budget->max_resources * sizeof(lb__rate_state_t));
#endif

#if LOXBUDGET_ENABLE_CALIBRATION
  if (budget->calib_off != 0u) {
    memset((budget->storage + budget->calib_off), 0,
           (size_t)budget->max_ops * (size_t)LOXBUDGET_CALIB_STATE_SIZE);
  }
#endif

#if LOXBUDGET_ENABLE_CAUSALITY
  {
    /* Clear causality block (layout must match src/loxbudget_causality.c). */
    uint32_t off = 0u;
    off += (uint32_t)sizeof(loxbudget_storage_hdr_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
    off += (uint32_t)budget->max_resources * (uint32_t)sizeof(loxbudget_resource_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
    off += (uint32_t)budget->max_ops * (uint32_t)sizeof(loxbudget_op_profile_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
    off += (uint32_t)budget->max_ops * (uint32_t)LOXBUDGET_MAX_NEEDS_PER_OP *
           (uint32_t)sizeof(loxbudget_need_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
    off += (uint32_t)budget->max_leases * (uint32_t)sizeof(loxbudget_lease_slot_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
    off += (uint32_t)budget->audit_size * (uint32_t)sizeof(loxbudget_decision_record_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
#if LOXBUDGET_ENABLE_RATE_WINDOWS
    off += (uint32_t)budget->max_resources * (uint32_t)sizeof(lb__rate_state_t);
    off = LOXBUDGET_ALIGN_UP(off, 4u);
#endif
#if LOXBUDGET_ENABLE_CALIBRATION
    off += (uint32_t)budget->max_ops * (uint32_t)LOXBUDGET_CALIB_STATE_SIZE;
    off = LOXBUDGET_ALIGN_UP(off, 4u);
#endif

    {
      const uint32_t nbytes = 4u + (uint32_t)LOXBUDGET_CAUSALITY_MAX_EDGES * 4u +
                              (((uint32_t)budget->max_ops + 7u) / 8u);
      memset(budget->storage + off, 0, (size_t)nbytes);
    }
  }
#endif

  /* Initialize per-instance lease magic base. */
  {
    uintptr_t addr = (uintptr_t)budget;
    uint32_t t = loxbudget_hal_now_ms_(budget);
    budget->lease_magic_base = (uint16_t)((addr ^ (uintptr_t)t) & 0xFFFFu);
    if (budget->lease_magic_base == 0u) { budget->lease_magic_base = 0xA5C3u; }
  }

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_deinit(loxbudget_t* budget) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  budget->magic = 0u;
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_snapshot(const loxbudget_t* budget, loxbudget_snapshot_t* out) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }

  out->pressure = budget->pressure;

  /* Count active leases on demand to remain consistent even if drifted. */
  {
    uint8_t i;
    uint8_t cnt = 0u;
    for (i = 0u; i < budget->max_leases; i++) {
      if (loxbudget_lease_slots_c_(budget)[i].active != 0u) { cnt++; }
    }
    out->active_lease_count = cnt;
  }

  out->resource_count = budget->max_resources;
  out->op_count = budget->max_ops;
  out->total_decisions = budget->total_decisions;
  out->total_grants = budget->total_grants;
  out->total_denials = budget->total_denials;
  out->total_degradations = budget->total_degradations;
  out->uptime_ms = loxbudget_hal_now_ms_(budget);

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_set_decision_hook(loxbudget_t* budget, loxbudget_decision_hook_fn fn,
                                               void* user) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  budget->decision_hook = (void (*)(void*, const loxbudget_decision_t*, loxbudget_op_id_t))fn;
  budget->decision_hook_user = user;
  return LOXBUDGET_OK;
}

#if LOXBUDGET_ENABLE_RATE_WINDOWS
loxbudget_status_t loxbudget_set_rate_limit(loxbudget_t* budget, loxbudget_resource_id_t res,
                                            loxbudget_window_t window, uint32_t limit) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (res >= budget->max_resources) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (window > LOXBUDGET_WINDOW_DAY) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (loxbudget_hdr_c_(budget)->res_cfg[res] == 0u) { return LOXBUDGET_ERR_NOT_FOUND; }
  if ((loxbudget_resource_kind_t)loxbudget_resources_c_(budget)[res].kind !=
      LOXBUDGET_RES_CONSUMABLE) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }

  const uint32_t dur = lb__window_duration_ms_(window);
  if (dur == 0u) { return LOXBUDGET_ERR_INVALID_ARG; }

  lb__rate_state_t* rs = &lb__rate_state_(budget)[res];
  lb__rate_window_t* w = &rs->windows[(uint8_t)window];
  w->window_duration_ms = dur;
  w->window_start_ms = loxbudget_hal_now_ms_(budget);
  w->limit = limit;
#if LOXBUDGET_RATE_IMPL == LOXBUDGET_RATE_IMPL_TOKEN_BUCKET
  w->consumed = (uint32_t)(((uint64_t)limit) << 16);
#else
  w->consumed = 0u;
#endif
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_set_lifetime_limit(loxbudget_t* budget, loxbudget_resource_id_t res,
                                                uint32_t lifetime_max) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (res >= budget->max_resources) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (loxbudget_hdr_c_(budget)->res_cfg[res] == 0u) { return LOXBUDGET_ERR_NOT_FOUND; }
  if ((loxbudget_resource_kind_t)loxbudget_resources_c_(budget)[res].kind !=
      LOXBUDGET_RES_CONSUMABLE) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  lb__rate_state_t* rs = &lb__rate_state_(budget)[res];
  rs->limit_lifetime = lifetime_max;
  /* consumed_lifetime intentionally not reset to support .noinit reuse. */
  return LOXBUDGET_OK;
}

static loxbudget_bool_t lb__rate_roll_and_check_(lb__rate_window_t* w, uint32_t now_ms,
                                                 uint32_t amount) {
#if LOXBUDGET_RATE_IMPL == LOXBUDGET_RATE_IMPL_TOKEN_BUCKET
  if (w->limit == 0u || w->window_duration_ms == 0u) { return LOXBUDGET_TRUE; }
  /* consumed is tokens in Q16.16, window_start_ms is last refill timestamp. */
  {
    const uint32_t dt = now_ms - w->window_start_ms;
    if (dt != 0u) {
      const uint64_t cap = ((uint64_t)w->limit) << 16;
      const uint64_t add = (cap * (uint64_t)dt) / (uint64_t)w->window_duration_ms;
      uint64_t tok = (uint64_t)w->consumed + add;
      if (tok > cap) { tok = cap; }
      w->consumed = (uint32_t)tok;
      w->window_start_ms = now_ms;
    }
    const uint64_t need = ((uint64_t)amount) << 16;
    return ((uint64_t)w->consumed >= need) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
  }
#else
  if (w->limit == 0u || w->window_duration_ms == 0u) { return LOXBUDGET_TRUE; }
  if ((uint32_t)(now_ms - w->window_start_ms) >= w->window_duration_ms) {
    w->window_start_ms = now_ms;
    w->consumed = 0u;
  }
  if (w->consumed > w->limit) { return LOXBUDGET_FALSE; }
  if (amount > (w->limit - w->consumed)) { return LOXBUDGET_FALSE; }
  return LOXBUDGET_TRUE;
#endif
}

static void lb__rate_commit_(lb__rate_window_t* w, uint32_t now_ms, uint32_t amount) {
#if LOXBUDGET_RATE_IMPL == LOXBUDGET_RATE_IMPL_TOKEN_BUCKET
  if (w->limit == 0u || w->window_duration_ms == 0u) { return; }
  /* Refill as in check, then consume tokens. */
  (void)lb__rate_roll_and_check_(w, now_ms, 0u);
  {
    const uint64_t need = ((uint64_t)amount) << 16;
    if ((uint64_t)w->consumed >= need) {
      w->consumed = (uint32_t)((uint64_t)w->consumed - need);
    } else {
      w->consumed = 0u;
    }
  }
#else
  if (w->limit == 0u || w->window_duration_ms == 0u) { return; }
  if ((uint32_t)(now_ms - w->window_start_ms) >= w->window_duration_ms) {
    w->window_start_ms = now_ms;
    w->consumed = 0u;
  }
  if (0xFFFFFFFFu - w->consumed < amount) {
    w->consumed = 0xFFFFFFFFu;
  } else {
    w->consumed += amount;
  }
#endif
}

loxbudget_status_t loxbudget_get_burn_rate(const loxbudget_t* budget, loxbudget_resource_id_t res,
                                           loxbudget_burn_rate_t* out) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }
  if (res >= budget->max_resources) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (loxbudget_hdr_c_(budget)->res_cfg[res] == 0u) { return LOXBUDGET_ERR_NOT_FOUND; }
  const lb__rate_state_t* rs = &lb__rate_state_c_(budget)[res];

  memset(out, 0, sizeof(*out));
#if LOXBUDGET_RATE_IMPL == LOXBUDGET_RATE_IMPL_TOKEN_BUCKET
  out->per_minute = 0u;
  out->per_hour = 0u;
#else
  out->per_minute = rs->windows[(uint8_t)LOXBUDGET_WINDOW_MINUTE].consumed;
  out->per_hour = rs->windows[(uint8_t)LOXBUDGET_WINDOW_HOUR].consumed;
#endif

  if (rs->limit_lifetime == 0u) { return LOXBUDGET_OK; }
  if (rs->consumed_lifetime >= rs->limit_lifetime) { return LOXBUDGET_OK; }

  /* Use minute window rate as estimator; avoid division by zero. */
  if (out->per_minute == 0u) { return LOXBUDGET_OK; }

  {
    const uint32_t remaining = rs->limit_lifetime - rs->consumed_lifetime;
    /* remaining / (per_minute per 60000ms) => ms */
    const uint64_t num = (uint64_t)remaining * 60000ull;
    out->estimated_exhaustion_ms = (uint32_t)(num / (uint64_t)out->per_minute);
  }
  return LOXBUDGET_OK;
}
#endif /* LOXBUDGET_ENABLE_RATE_WINDOWS */

loxbudget_status_t loxbudget_set_pressure(loxbudget_t* budget, loxbudget_pressure_t pressure) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if ((uint8_t)pressure > (uint8_t)LOXBUDGET_PRESSURE_LOCKDOWN) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  loxbudget_critical_enter_(budget);
  budget->pressure = (uint8_t)pressure;
  loxbudget_critical_exit_(budget);
  return LOXBUDGET_OK;
}

loxbudget_pressure_t loxbudget_get_pressure(const loxbudget_t* budget) {
  if (loxbudget_validate_budget_(budget) != LOXBUDGET_OK) { return LOXBUDGET_PRESSURE_LOCKDOWN; }
  return (loxbudget_pressure_t)budget->pressure;
}

/* Phase 4/5 functions are implemented later. */
loxbudget_status_t loxbudget_set_resource(loxbudget_t* budget, loxbudget_resource_id_t id,
                                          uint16_t limit, loxbudget_resource_kind_t kind) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (id >= budget->max_resources) { return LOXBUDGET_ERR_INVALID_ARG; }
  if ((uint8_t)kind > (uint8_t)LOXBUDGET_RES_STATE) { return LOXBUDGET_ERR_INVALID_ARG; }

  loxbudget_resource_t* r = &loxbudget_resources_(budget)[id];
  r->limit = limit;
  r->used = 0u;
  r->reserved = 0u;
  r->high_watermark = 0u;
  r->kind = (uint8_t)kind;
  r->flags = 1u; /* configured */
  r->_pad = 0u;

  loxbudget_hdr_(budget)->res_cfg[id] = 1u;
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_register_op(loxbudget_t* budget,
                                         const loxbudget_op_profile_t* profile) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || profile == NULL) {
    return (profile == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }
  if (profile->op_id >= budget->max_ops) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (profile->priority > (uint8_t)LOXBUDGET_PRIO_CRITICAL) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (profile->action_normal > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_elevated > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_critical > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_survival > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_lockdown > (uint8_t)LOXBUDGET_LOCKDOWN) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }

  if (loxbudget_hdr_(budget)->op_cfg[profile->op_id] != 0u) { return LOXBUDGET_ERR_DUPLICATE; }

  if (budget->hal_strict != 0u) {
    const uint8_t f = profile->flags;
    if ((f & LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN) != 0u) {
      if (budget->hal_cb == NULL || budget->hal_cb->boot_proven == NULL) {
        return LOXBUDGET_ERR_HAL_NOT_CONFIGURED;
      }
    }
    if ((f & LOXBUDGET_OPF_REQUIRES_VOLTAGE_OK) != 0u) {
      if (budget->hal_cb == NULL || budget->hal_cb->voltage_ok == NULL) {
        return LOXBUDGET_ERR_HAL_NOT_CONFIGURED;
      }
    }
    if ((f & LOXBUDGET_OPF_REQUIRES_NETWORK_UP) != 0u) {
      if (budget->hal_cb == NULL || budget->hal_cb->network_up == NULL) {
        return LOXBUDGET_ERR_HAL_NOT_CONFIGURED;
      }
    }
  }

  loxbudget_ops_(budget)[profile->op_id] = *profile;
  loxbudget_hdr_(budget)->op_cfg[profile->op_id] = 1u;

  /* Clear needs list for this op. */
  memset(&loxbudget_needs_(budget)[(uint32_t)profile->op_id * LOXBUDGET_MAX_NEEDS_PER_OP], 0,
         (size_t)LOXBUDGET_MAX_NEEDS_PER_OP * sizeof(loxbudget_need_t));

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_op_set_need(loxbudget_t* budget, loxbudget_op_id_t op,
                                         loxbudget_resource_id_t resource, uint16_t amount) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (op >= budget->max_ops || resource >= budget->max_resources) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (amount == 0u) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (loxbudget_hdr_(budget)->op_cfg[op] == 0u) { return LOXBUDGET_ERR_NOT_FOUND; }
  if (loxbudget_hdr_(budget)->res_cfg[resource] == 0u) { return LOXBUDGET_ERR_NOT_FOUND; }

  loxbudget_need_t* list = &loxbudget_needs_(budget)[(uint32_t)op * LOXBUDGET_MAX_NEEDS_PER_OP];
  uint8_t i;
  for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
    if (list[i].amount != 0u && list[i].resource == resource) {
      list[i].amount = amount;
      return LOXBUDGET_OK;
    }
  }
  for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
    if (list[i].amount == 0u) {
      list[i].resource = resource;
      list[i].amount = amount;
      list[i]._pad = 0u;
      return LOXBUDGET_OK;
    }
  }
  return LOXBUDGET_ERR_NO_SPACE;
}

loxbudget_status_t loxbudget_check(loxbudget_t* budget, loxbudget_op_id_t op,
                                   loxbudget_decision_t* out) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }

  memset(out, 0, sizeof(*out));
  out->pressure = (loxbudget_pressure_t)budget->pressure;
  out->denied_resource = 0xFFu;

  const loxbudget_op_profile_t* p = loxbudget_profile_c_(budget, op);
  if (p == NULL) {
    out->action = LOXBUDGET_REJECT;
    out->reason = (uint8_t)LOXBUDGET_REASON_UNKNOWN_OP;
    budget->total_decisions++;
    budget->total_denials++;
    loxbudget_audit_record_(budget, op, out);
    if (budget->decision_hook != NULL) {
      budget->decision_hook(budget->decision_hook_user, out, op);
    }
    return LOXBUDGET_OK;
  }

  {
    const uint8_t mapped = loxbudget_mapped_action_(p, budget->pressure);
    out->action = (loxbudget_action_t)mapped;
  }

  if (budget->pressure == (uint8_t)LOXBUDGET_PRESSURE_LOCKDOWN) {
    if ((p->flags & LOXBUDGET_OPF_LOCKDOWN_PASS) == 0u) {
      out->action = LOXBUDGET_LOCKDOWN;
      out->reason = (uint8_t)LOXBUDGET_REASON_LOCKDOWN_ACTIVE;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
  }

  if (out->action == LOXBUDGET_WAIT || out->action == LOXBUDGET_REJECT ||
      out->action == LOXBUDGET_LOCKDOWN) {
    out->reason = (out->action == LOXBUDGET_LOCKDOWN) ? (uint8_t)LOXBUDGET_REASON_LOCKDOWN_ACTIVE
                                                      : (uint8_t)LOXBUDGET_REASON_PRESSURE_BLOCK;
    budget->total_decisions++;
    budget->total_denials++;
    loxbudget_audit_record_(budget, op, out);
    if (budget->decision_hook != NULL) {
      budget->decision_hook(budget->decision_hook_user, out, op);
    }
    return LOXBUDGET_OK;
  }

  /* HAL preconditions. */
  if ((p->flags & LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN) != 0u) {
    const loxbudget_bool_t v = loxbudget_hal_boot_proven_(budget);
    if (v == 2u) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_HAL_NOT_CONFIGURED;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
    if (v == LOXBUDGET_FALSE) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
  }
  if ((p->flags & LOXBUDGET_OPF_REQUIRES_VOLTAGE_OK) != 0u) {
    const loxbudget_bool_t v = loxbudget_hal_voltage_ok_(budget);
    if (v == 2u) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_HAL_NOT_CONFIGURED;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
    if (v == LOXBUDGET_FALSE) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
  }
  if ((p->flags & LOXBUDGET_OPF_REQUIRES_NETWORK_UP) != 0u) {
    const loxbudget_bool_t v = loxbudget_hal_network_up_(budget);
    if (v == 2u) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_HAL_NOT_CONFIGURED;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
    if (v == LOXBUDGET_FALSE) {
      out->action = LOXBUDGET_REJECT;
      out->reason = (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL;
      budget->total_decisions++;
      budget->total_denials++;
      loxbudget_audit_record_(budget, op, out);
      if (budget->decision_hook != NULL) {
        budget->decision_hook(budget->decision_hook_user, out, op);
      }
      return LOXBUDGET_OK;
    }
  }

  /* Resource needs. */
  {
    const loxbudget_need_t* list =
        &loxbudget_needs_c_(budget)[(uint32_t)op * LOXBUDGET_MAX_NEEDS_PER_OP];
#if LOXBUDGET_ENABLE_RATE_WINDOWS
    const uint32_t now_ms = loxbudget_hal_now_ms_(budget);
#endif
    uint16_t need_per_res[LOXBUDGET_MAX_RESOURCES];
    memset(need_per_res, 0, sizeof(need_per_res));
    uint8_t i;
    for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
      const loxbudget_need_t n = list[i];
      if (n.amount == 0u) { continue; }
      if (n.resource >= budget->max_resources ||
          loxbudget_hdr_c_(budget)->res_cfg[n.resource] == 0u) {
        out->action = LOXBUDGET_REJECT;
        out->reason = (uint8_t)LOXBUDGET_REASON_INSUFFICIENT_RES;
        out->denied_resource = n.resource;
        out->requested = n.amount;
        out->available = 0u;
        budget->total_decisions++;
        budget->total_denials++;
        loxbudget_audit_record_(budget, op, out);
        if (budget->decision_hook != NULL) {
          budget->decision_hook(budget->decision_hook_user, out, op);
        }
        return LOXBUDGET_OK;
      }

      const loxbudget_resource_t* r = &loxbudget_resources_c_(budget)[n.resource];
      if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_STATE) {
        if (r->limit == 0u) {
          out->action = LOXBUDGET_REJECT;
          out->reason = (uint8_t)LOXBUDGET_REASON_PRECONDITION_FAIL;
          out->denied_resource = n.resource;
          out->requested = n.amount;
          out->available = 0u;
          budget->total_decisions++;
          budget->total_denials++;
          loxbudget_audit_record_(budget, op, out);
          if (budget->decision_hook != NULL) {
            budget->decision_hook(budget->decision_hook_user, out, op);
          }
          return LOXBUDGET_OK;
        }
        continue;
      }

      {
        uint32_t sum = (uint32_t)need_per_res[n.resource] + (uint32_t)n.amount;
        if (sum > 0xFFFFu) sum = 0xFFFFu;
        need_per_res[n.resource] = (uint16_t)sum;
      }

      const uint16_t avail = loxbudget_available_u16_(r->limit, r->used, r->reserved);
      if (avail < n.amount) {
        out->denied_resource = n.resource;
        out->requested = n.amount;
        out->available = avail;
        out->reason = (uint8_t)LOXBUDGET_REASON_INSUFFICIENT_RES;

        if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_REUSABLE) {
          out->action = LOXBUDGET_WAIT;
        } else {
          out->action = LOXBUDGET_REJECT;
        }

        budget->total_decisions++;
        budget->total_denials++;
        loxbudget_audit_record_(budget, op, out);
        if (budget->decision_hook != NULL) {
          budget->decision_hook(budget->decision_hook_user, out, op);
        }
        return LOXBUDGET_OK;
      }

#if LOXBUDGET_ENABLE_RATE_WINDOWS
      if ((p->flags & LOXBUDGET_OPF_BYPASS_RATE_LIMIT) == 0u &&
          (loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_CONSUMABLE) {
        lb__rate_state_t* rs = &lb__rate_state_(budget)[n.resource];

        if (rs->limit_lifetime != 0u) {
          if (rs->consumed_lifetime > rs->limit_lifetime ||
              (uint32_t)n.amount > (rs->limit_lifetime - rs->consumed_lifetime)) {
            out->action = LOXBUDGET_REJECT;
            out->reason = (uint8_t)LOXBUDGET_REASON_LIFETIME_EXHAUSTED;
            out->denied_resource = n.resource;
            out->requested = n.amount;
            out->available = 0u;
            budget->total_decisions++;
            budget->total_denials++;
            loxbudget_audit_record_(budget, op, out);
            if (budget->decision_hook != NULL) {
              budget->decision_hook(budget->decision_hook_user, out, op);
            }
            return LOXBUDGET_OK;
          }
        }

        {
          uint8_t wi;
          for (wi = 0u; wi < 4u; wi++) {
            if (lb__rate_roll_and_check_(&rs->windows[wi], now_ms, (uint32_t)n.amount) ==
                LOXBUDGET_FALSE) {
              out->action = LOXBUDGET_REJECT;
              out->reason = (uint8_t)LOXBUDGET_REASON_RATE_LIMIT;
              out->denied_resource = n.resource;
              out->requested = n.amount;
              out->available = 0u;
              budget->total_decisions++;
              budget->total_denials++;
              loxbudget_audit_record_(budget, op, out);
              if (budget->decision_hook != NULL) {
                budget->decision_hook(budget->decision_hook_user, out, op);
              }
              return LOXBUDGET_OK;
            }
          }
        }
      }
#endif
    }

#if LOXBUDGET_ENABLE_CAUSALITY
    /* Transitive cascade check (SPEC.md §17). */
    if (lb__causality_add_scaled_needs_(budget, op, need_per_res,
                                        (loxbudget_pressure_t)budget->pressure) != 0u) {
      uint8_t rid;
      for (rid = 0u; rid < budget->max_resources; rid++) {
        const uint16_t req = need_per_res[rid];
        if (req == 0u) continue;
        if (loxbudget_hdr_c_(budget)->res_cfg[rid] == 0u) continue;
        const loxbudget_resource_t* r = &loxbudget_resources_c_(budget)[rid];
        if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_STATE) {
          if (r->limit == 0u) {
            out->action = LOXBUDGET_REJECT;
            out->reason = (uint8_t)LOXBUDGET_REASON_CAUSAL_CASCADE;
            out->denied_resource = (loxbudget_resource_id_t)rid;
            out->requested = req;
            out->available = 0u;
            budget->total_decisions++;
            budget->total_denials++;
            loxbudget_audit_record_(budget, op, out);
            if (budget->decision_hook != NULL) {
              budget->decision_hook(budget->decision_hook_user, out, op);
            }
            return LOXBUDGET_OK;
          }
          continue;
        }

        {
          const uint16_t avail = loxbudget_available_u16_(r->limit, r->used, r->reserved);
          if (avail < req) {
            out->denied_resource = (loxbudget_resource_id_t)rid;
            out->requested = req;
            out->available = avail;
            out->reason = (uint8_t)LOXBUDGET_REASON_CAUSAL_CASCADE;
            out->action = ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_REUSABLE)
                              ? LOXBUDGET_WAIT
                              : LOXBUDGET_REJECT;
            budget->total_decisions++;
            budget->total_denials++;
            loxbudget_audit_record_(budget, op, out);
            if (budget->decision_hook != NULL) {
              budget->decision_hook(budget->decision_hook_user, out, op);
            }
            return LOXBUDGET_OK;
          }
        }
      }
    }
#endif
  }

  out->reason = (uint8_t)LOXBUDGET_REASON_OK;
  budget->total_decisions++;
  if (out->action == LOXBUDGET_ALLOW_DEGRADED) {
    budget->total_degradations++;
    budget->total_grants++;
  } else {
    budget->total_grants++;
  }
  loxbudget_audit_record_(budget, op, out);
  if (budget->decision_hook != NULL) { budget->decision_hook(budget->decision_hook_user, out, op); }
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_enter(loxbudget_t* budget, loxbudget_op_id_t op,
                                   loxbudget_lease_t* out_lease) {
  loxbudget_decision_t d;
  loxbudget_status_t st = loxbudget_check(budget, op, &d);
  if (st != LOXBUDGET_OK) { return st; }
  if (out_lease == NULL) { return LOXBUDGET_ERR_INVALID_ARG; }

  const loxbudget_op_profile_t* p = loxbudget_profile_c_(budget, op);
  if (p == NULL) { return LOXBUDGET_ERR_BAD_STATE; }

  if (d.action != LOXBUDGET_ALLOW_FULL && d.action != LOXBUDGET_ALLOW_DEGRADED) {
    memset(out_lease, 0, sizeof(*out_lease));
    return LOXBUDGET_ERR_BAD_STATE;
  }

  /* Phase A: validate needs again (check is read-only for resources). */
  const loxbudget_need_t* list =
      &loxbudget_needs_c_(budget)[(uint32_t)op * LOXBUDGET_MAX_NEEDS_PER_OP];
  uint8_t i;
  for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
    const loxbudget_need_t n = list[i];
    if (n.amount == 0u) { continue; }
    const loxbudget_resource_t* r = &loxbudget_resources_c_(budget)[n.resource];
    if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_STATE) {
      if (r->limit == 0u) { return LOXBUDGET_ERR_BAD_STATE; }
      continue;
    }
    if (loxbudget_available_u16_(r->limit, r->used, r->reserved) < n.amount) {
      return LOXBUDGET_ERR_BAD_STATE;
    }
  }

  /* Lease slot allocation + reservation commit under critical section. */
  loxbudget_critical_enter_(budget);
  {
    loxbudget_lease_slot_t* slots = loxbudget_lease_slots_(budget);
    uint8_t slot_id = 0xFFu;
    for (i = 0u; i < budget->max_leases; i++) {
      if (slots[i].active == 0u) {
        slot_id = i;
        break;
      }
    }
    if (slot_id == 0xFFu) {
      loxbudget_critical_exit_(budget);
      return LOXBUDGET_ERR_NO_SPACE;
    }

    /* Apply all mutations. */
    for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
      const loxbudget_need_t n = list[i];
      if (n.amount == 0u) { continue; }
      loxbudget_resource_t* r = &loxbudget_resources_(budget)[n.resource];
      if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_REUSABLE) {
        r->reserved = (uint16_t)(r->reserved + n.amount);
        if (r->reserved > r->high_watermark) { r->high_watermark = r->reserved; }
      } else if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_CONSUMABLE) {
        r->used = (uint16_t)(r->used + n.amount);
        if (r->used > r->high_watermark) { r->high_watermark = r->used; }

#if LOXBUDGET_ENABLE_RATE_WINDOWS
        if ((p->flags & LOXBUDGET_OPF_BYPASS_RATE_LIMIT) == 0u) {
          lb__rate_state_t* rs = &lb__rate_state_(budget)[n.resource];
          const uint32_t now_ms = loxbudget_hal_now_ms_(budget);
          uint8_t wi;
          for (wi = 0u; wi < 4u; wi++) {
            lb__rate_commit_(&rs->windows[wi], now_ms, (uint32_t)n.amount);
          }
          if (0xFFFFFFFFu - rs->consumed_lifetime < (uint32_t)n.amount) {
            rs->consumed_lifetime = 0xFFFFFFFFu;
          } else {
            rs->consumed_lifetime += (uint32_t)n.amount;
          }
        }
#endif
      }
    }

    slots[slot_id].active = 1u;
    slots[slot_id].magic = loxbudget_lease_magic_for_id_(budget, slot_id);
    slots[slot_id].acquired_pressure = budget->pressure;

    out_lease->acquired_at_ms = loxbudget_hal_now_ms_(budget);
    out_lease->magic = slots[slot_id].magic;
    out_lease->id = slot_id;
    out_lease->op = op;
  }
  loxbudget_critical_exit_(budget);
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_leave(loxbudget_t* budget, loxbudget_lease_t lease) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) { return st; }
  if (lease.id >= budget->max_leases) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (lease.magic != loxbudget_lease_magic_for_id_(budget, lease.id)) {
    return LOXBUDGET_ERR_BAD_STATE;
  }

  const loxbudget_op_profile_t* p = loxbudget_profile_c_(budget, lease.op);
  if (p == NULL) { return LOXBUDGET_ERR_BAD_STATE; }

  loxbudget_critical_enter_(budget);
  {
    loxbudget_lease_slot_t* slots = loxbudget_lease_slots_(budget);
    if (slots[lease.id].active == 0u || slots[lease.id].magic != lease.magic) {
      loxbudget_critical_exit_(budget);
      return LOXBUDGET_ERR_BAD_STATE;
    }

    const loxbudget_need_t* list =
        &loxbudget_needs_c_(budget)[(uint32_t)lease.op * LOXBUDGET_MAX_NEEDS_PER_OP];
    uint8_t i;
    for (i = 0u; i < LOXBUDGET_MAX_NEEDS_PER_OP; i++) {
      const loxbudget_need_t n = list[i];
      if (n.amount == 0u) { continue; }
      loxbudget_resource_t* r = &loxbudget_resources_(budget)[n.resource];
      if ((loxbudget_resource_kind_t)r->kind == LOXBUDGET_RES_REUSABLE) {
        if (r->reserved >= n.amount) {
          r->reserved = (uint16_t)(r->reserved - n.amount);
        } else {
          r->reserved = 0u;
        }
      }
    }

    slots[lease.id].active = 0u;
    slots[lease.id].magic = 0u;
  }
  loxbudget_critical_exit_(budget);
  (void)p;
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_yield_check(loxbudget_t* budget, loxbudget_lease_t lease,
                                         loxbudget_pressure_hint_t* out) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }
  if (lease.id >= budget->max_leases) { return LOXBUDGET_ERR_INVALID_ARG; }
  if (lease.magic != loxbudget_lease_magic_for_id_(budget, lease.id)) {
    return LOXBUDGET_ERR_BAD_STATE;
  }
  if (loxbudget_profile_c_(budget, lease.op) == NULL) { return LOXBUDGET_ERR_BAD_STATE; }

  {
    const loxbudget_lease_slot_t* slots = loxbudget_lease_slots_c_(budget);
    if (slots[lease.id].active == 0u || slots[lease.id].magic != lease.magic) {
      return LOXBUDGET_ERR_BAD_STATE;
    }

    const uint8_t prev = slots[lease.id].acquired_pressure;
    const uint8_t cur = budget->pressure;
    if (cur == prev) {
      *out = LOXBUDGET_PRESSURE_HOLDING;
    } else if (cur > prev) {
      *out = LOXBUDGET_PRESSURE_RISING;
    } else {
      *out = LOXBUDGET_PRESSURE_FALLING;
    }

    /* Abort hint: if pressure-mapped action is no longer an allow. */
    {
      const loxbudget_op_profile_t* p = loxbudget_profile_c_(budget, lease.op);
      const uint8_t a = loxbudget_mapped_action_(p, cur);
      if (cur == (uint8_t)LOXBUDGET_PRESSURE_LOCKDOWN &&
          (p->flags & LOXBUDGET_OPF_LOCKDOWN_PASS) == 0u) {
        *out = LOXBUDGET_SHOULD_ABORT;
      } else if (a == (uint8_t)LOXBUDGET_WAIT || a == (uint8_t)LOXBUDGET_REJECT ||
                 a == (uint8_t)LOXBUDGET_LOCKDOWN) {
        *out = LOXBUDGET_SHOULD_ABORT;
      }
    }
  }

  return LOXBUDGET_OK;
}
