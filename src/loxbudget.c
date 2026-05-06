#include "loxbudget.h"

#include <string.h>

/* Internal constants */
#define LOXBUDGET_MAGIC_INIT 0x4C58424Du /* 'LXBM' */

#define LOXBUDGET_ALIGN_UP(v, a) (((v) + ((a)-1u)) & ~((a)-1u))

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
  uint8_t _pad;
} loxbudget_lease_slot_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_lease_slot_t) == 4, "lease slot meta");

static loxbudget_resource_t *loxbudget_resources_(loxbudget_t *b) {
  return (loxbudget_resource_t *)(b->storage + b->resources_off);
}
static const loxbudget_resource_t *loxbudget_resources_c_(const loxbudget_t *b) {
  return (const loxbudget_resource_t *)(b->storage + b->resources_off);
}
static loxbudget_op_profile_t *loxbudget_ops_(loxbudget_t *b) {
  return (loxbudget_op_profile_t *)(b->storage + b->ops_off);
}
static const loxbudget_op_profile_t *loxbudget_ops_c_(const loxbudget_t *b) {
  return (const loxbudget_op_profile_t *)(b->storage + b->ops_off);
}
static loxbudget_need_t *loxbudget_needs_(loxbudget_t *b) {
  return (loxbudget_need_t *)(b->storage + b->needs_off);
}
static const loxbudget_need_t *loxbudget_needs_c_(const loxbudget_t *b) {
  return (const loxbudget_need_t *)(b->storage + b->needs_off);
}
static loxbudget_lease_slot_t *loxbudget_lease_slots_(loxbudget_t *b) {
  return (loxbudget_lease_slot_t *)(b->storage + b->lease_slots_off);
}
static const loxbudget_lease_slot_t *loxbudget_lease_slots_c_(const loxbudget_t *b) {
  return (const loxbudget_lease_slot_t *)(b->storage + b->lease_slots_off);
}

static loxbudget_storage_hdr_t *loxbudget_hdr_(loxbudget_t *b) {
  return (loxbudget_storage_hdr_t *)b->storage;
}
static const loxbudget_storage_hdr_t *loxbudget_hdr_c_(const loxbudget_t *b) {
  return (const loxbudget_storage_hdr_t *)b->storage;
}

static loxbudget_bool_t loxbudget_is_aligned_u32_(const void *p) {
  return (((uintptr_t)p) & 3u) == 0u ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static loxbudget_bool_t loxbudget_is_pow2_or_zero_u8_(uint8_t v) {
  if (v == 0u) {
    return LOXBUDGET_TRUE;
  }
  return ((v & (uint8_t)(v - 1u)) == 0u) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static loxbudget_status_t loxbudget_validate_init_args_(loxbudget_t *budget,
                                                       void *storage,
                                                       size_t storage_size,
                                                       const loxbudget_config_t *cfg) {
  if (budget == NULL || storage == NULL || cfg == NULL) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (storage_size == 0u) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (loxbudget_is_aligned_u32_(storage) == LOXBUDGET_FALSE) {
    return LOXBUDGET_ERR_ALIGNMENT;
  }
  if (cfg->max_resources == 0u || cfg->max_ops == 0u ||
      cfg->max_concurrent_leases == 0u) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (cfg->max_resources > LOXBUDGET_MAX_RESOURCES ||
      cfg->max_ops > LOXBUDGET_MAX_OPS ||
      cfg->max_concurrent_leases > LOXBUDGET_MAX_LEASES) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (loxbudget_is_pow2_or_zero_u8_(cfg->audit_size) == LOXBUDGET_FALSE) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  return LOXBUDGET_OK;
}

static loxbudget_status_t loxbudget_validate_budget_(const loxbudget_t *budget) {
  if (budget == NULL) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (budget->magic != LOXBUDGET_MAGIC_INIT) {
    return LOXBUDGET_ERR_NOT_INITIALIZED;
  }
  return LOXBUDGET_OK;
}

static uint16_t loxbudget_lease_magic_for_id_(const loxbudget_t *budget,
                                             uint8_t id) {
  return (uint16_t)(budget->lease_magic_base ^ (uint16_t)id);
}

static uint32_t loxbudget_hal_now_ms_(const loxbudget_t *budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->now_ms != NULL) {
    return budget->hal_cb->now_ms(budget->hal_user);
  }
  return loxbudget_hal_now_ms();
}

static void loxbudget_critical_enter_(const loxbudget_t *budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->critical_enter != NULL) {
    budget->hal_cb->critical_enter(budget->hal_user);
    return;
  }
  loxbudget_hal_critical_enter();
}

static void loxbudget_critical_exit_(const loxbudget_t *budget) {
  if (budget->hal_cb != NULL && budget->hal_cb->critical_exit != NULL) {
    budget->hal_cb->critical_exit(budget->hal_user);
    return;
  }
  loxbudget_hal_critical_exit();
}

loxbudget_status_t loxbudget_init(loxbudget_t *budget, void *storage,
                                 size_t storage_size,
                                 const loxbudget_config_t *cfg) {
  loxbudget_status_t st = loxbudget_validate_init_args_(budget, storage, storage_size, cfg);
  if (st != LOXBUDGET_OK) {
    return st;
  }

  /* Zero the entire instance and storage tables area we manage. */
  memset(budget, 0, sizeof(*budget));

  budget->magic = LOXBUDGET_MAGIC_INIT;
  budget->max_resources = cfg->max_resources;
  budget->max_ops = cfg->max_ops;
  budget->max_leases = cfg->max_concurrent_leases;
  budget->pressure = (uint8_t)LOXBUDGET_PRESSURE_NORMAL;
  budget->storage = (uint8_t *)storage;
  budget->storage_size = (uint32_t)storage_size;
  budget->hal_cb = cfg->hal_callbacks;
  budget->hal_user = cfg->hal_user;
  budget->hal_strict = cfg->hal_strict ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;

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

    /* No audit in V0.1; still reserve size if user asked via cfg->audit_size. */
    off += (uint32_t)cfg->audit_size * (uint32_t)sizeof(loxbudget_decision_record_t);
    off = LOXBUDGET_ALIGN_UP(off, align);

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
  memset(loxbudget_ops_(budget), 0,
         (size_t)budget->max_ops * sizeof(loxbudget_op_profile_t));
  memset(loxbudget_needs_(budget), 0,
         (size_t)budget->max_ops * LOXBUDGET_MAX_NEEDS_PER_OP * sizeof(loxbudget_need_t));
  memset(loxbudget_lease_slots_(budget), 0,
         (size_t)budget->max_leases * sizeof(loxbudget_lease_slot_t));

  /* Initialize per-instance lease magic base. */
  {
    uintptr_t addr = (uintptr_t)budget;
    uint32_t t = loxbudget_hal_now_ms_(budget);
    budget->lease_magic_base = (uint16_t)((addr ^ (uintptr_t)t) & 0xFFFFu);
    if (budget->lease_magic_base == 0u) {
      budget->lease_magic_base = 0xA5C3u;
    }
  }

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_deinit(loxbudget_t *budget) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) {
    return st;
  }
  budget->magic = 0u;
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_snapshot(const loxbudget_t *budget,
                                     loxbudget_snapshot_t *out) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) {
    return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }

  out->pressure = budget->pressure;

  /* Count active leases on demand to remain consistent even if drifted. */
  {
    uint8_t i;
    uint8_t cnt = 0u;
    for (i = 0u; i < budget->max_leases; i++) {
      if (loxbudget_lease_slots_c_(budget)[i].active != 0u) {
        cnt++;
      }
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

loxbudget_status_t loxbudget_set_pressure(loxbudget_t *budget,
                                         loxbudget_pressure_t pressure) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) {
    return st;
  }
  if ((uint8_t)pressure > (uint8_t)LOXBUDGET_PRESSURE_LOCKDOWN) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  loxbudget_critical_enter_(budget);
  budget->pressure = (uint8_t)pressure;
  loxbudget_critical_exit_(budget);
  return LOXBUDGET_OK;
}

loxbudget_pressure_t loxbudget_get_pressure(const loxbudget_t *budget) {
  if (loxbudget_validate_budget_(budget) != LOXBUDGET_OK) {
    return LOXBUDGET_PRESSURE_LOCKDOWN;
  }
  return (loxbudget_pressure_t)budget->pressure;
}

/* Phase 4/5 functions are implemented later. */
loxbudget_status_t loxbudget_set_resource(loxbudget_t *budget,
                                         loxbudget_resource_id_t id,
                                         uint16_t limit,
                                         loxbudget_resource_kind_t kind) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) {
    return st;
  }
  if (id >= budget->max_resources) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if ((uint8_t)kind > (uint8_t)LOXBUDGET_RES_STATE) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }

  loxbudget_resource_t *r = &loxbudget_resources_(budget)[id];
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

loxbudget_status_t loxbudget_register_op(loxbudget_t *budget,
                                        const loxbudget_op_profile_t *profile) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK || profile == NULL) {
    return (profile == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }
  if (profile->op_id >= budget->max_ops) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (profile->priority > (uint8_t)LOXBUDGET_PRIO_CRITICAL) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (profile->action_normal > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_elevated > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_critical > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_survival > (uint8_t)LOXBUDGET_LOCKDOWN ||
      profile->action_lockdown > (uint8_t)LOXBUDGET_LOCKDOWN) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }

  if (loxbudget_hdr_(budget)->op_cfg[profile->op_id] != 0u) {
    return LOXBUDGET_ERR_DUPLICATE;
  }

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

loxbudget_status_t loxbudget_op_set_need(loxbudget_t *budget,
                                        loxbudget_op_id_t op,
                                        loxbudget_resource_id_t resource,
                                        uint16_t amount) {
  loxbudget_status_t st = loxbudget_validate_budget_(budget);
  if (st != LOXBUDGET_OK) {
    return st;
  }
  if (op >= budget->max_ops || resource >= budget->max_resources) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (amount == 0u) {
    return LOXBUDGET_ERR_INVALID_ARG;
  }
  if (loxbudget_hdr_(budget)->op_cfg[op] == 0u) {
    return LOXBUDGET_ERR_NOT_FOUND;
  }
  if (loxbudget_hdr_(budget)->res_cfg[resource] == 0u) {
    return LOXBUDGET_ERR_NOT_FOUND;
  }

  loxbudget_need_t *list =
      &loxbudget_needs_(budget)[(uint32_t)op * LOXBUDGET_MAX_NEEDS_PER_OP];
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

loxbudget_status_t loxbudget_check(loxbudget_t *budget, loxbudget_op_id_t op,
                                  loxbudget_decision_t *out) {
  (void)budget;
  (void)op;
  (void)out;
  return LOXBUDGET_ERR_FEATURE_DISABLED;
}

loxbudget_status_t loxbudget_enter(loxbudget_t *budget, loxbudget_op_id_t op,
                                  loxbudget_lease_t *out_lease) {
  (void)budget;
  (void)op;
  (void)out_lease;
  return LOXBUDGET_ERR_FEATURE_DISABLED;
}

loxbudget_status_t loxbudget_leave(loxbudget_t *budget, loxbudget_lease_t lease) {
  (void)budget;
  (void)lease;
  return LOXBUDGET_ERR_FEATURE_DISABLED;
}
