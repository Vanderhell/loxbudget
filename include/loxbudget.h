#ifndef LOXBUDGET_H
#define LOXBUDGET_H

/*
 * loxbudget (V0.1)
 * Pre-flight checks for embedded C operations: "can this operation safely run right now?"
 *
 * License: MIT (see LICENSE).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Static assert compatibility (C99 toolchains, MSVC C mode). */
#ifndef LOXBUDGET_STATIC_ASSERT
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define LOXBUDGET_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
#define LOXBUDGET_STATIC_ASSERT(cond, msg) \
  typedef char loxbudget_static_assertion_##__LINE__[(cond) ? 1 : -1]
#endif
#endif

/* Compile-time feature switches (default off in V0.1). */
#ifndef LOXBUDGET_ENABLE_AUDIT_TRAIL
#define LOXBUDGET_ENABLE_AUDIT_TRAIL 0
#endif

#ifndef LOXBUDGET_ENABLE_RATE_WINDOWS
#define LOXBUDGET_ENABLE_RATE_WINDOWS 0
#endif

#ifndef LOXBUDGET_ENABLE_CALIBRATION
#define LOXBUDGET_ENABLE_CALIBRATION 0
#endif

#ifndef LOXBUDGET_ENABLE_CAUSALITY
#define LOXBUDGET_ENABLE_CAUSALITY 0
#endif

#ifndef LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS
#define LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS 0
#endif

#ifndef LOXBUDGET_MAX_RESOURCES
#define LOXBUDGET_MAX_RESOURCES 8
#endif

#ifndef LOXBUDGET_MAX_OPS
#define LOXBUDGET_MAX_OPS 16
#endif

#ifndef LOXBUDGET_MAX_NEEDS_PER_OP
#define LOXBUDGET_MAX_NEEDS_PER_OP 4
#endif

#ifndef LOXBUDGET_MAX_LEASES
#define LOXBUDGET_MAX_LEASES 4
#endif

#ifndef LOXBUDGET_AUDIT_SIZE
#define LOXBUDGET_AUDIT_SIZE 0
#endif

/* Boolean type (no <stdbool.h>). */
typedef uint8_t loxbudget_bool_t;
#define LOXBUDGET_TRUE 1u
#define LOXBUDGET_FALSE 0u

/* Identifiers. */
typedef uint8_t loxbudget_resource_id_t;
typedef uint8_t loxbudget_op_id_t;
typedef uint8_t loxbudget_lease_id_t;

typedef enum {
  LOXBUDGET_OK = 0,
  LOXBUDGET_ERR_INVALID_ARG = -1,
  LOXBUDGET_ERR_NOT_INITIALIZED = -2,
  LOXBUDGET_ERR_NO_SPACE = -3,
  LOXBUDGET_ERR_NOT_FOUND = -4,
  LOXBUDGET_ERR_DUPLICATE = -5,
  LOXBUDGET_ERR_OVERFLOW = -6,
  LOXBUDGET_ERR_BAD_STATE = -7,
  LOXBUDGET_ERR_FEATURE_DISABLED = -8,
  LOXBUDGET_ERR_HAL_NOT_CONFIGURED = -9,
  LOXBUDGET_ERR_ALIGNMENT = -10,
  LOXBUDGET_ERR_VERSION_MISMATCH = -11
} loxbudget_status_t;

typedef enum {
  LOXBUDGET_ALLOW_FULL = 0,
  LOXBUDGET_ALLOW_DEGRADED = 1,
  LOXBUDGET_WAIT = 2,
  LOXBUDGET_REJECT = 3,
  LOXBUDGET_LOCKDOWN = 4
} loxbudget_action_t;

typedef enum {
  LOXBUDGET_PRESSURE_NORMAL = 0,
  LOXBUDGET_PRESSURE_ELEVATED = 1,
  LOXBUDGET_PRESSURE_CRITICAL = 2,
  LOXBUDGET_PRESSURE_SURVIVAL = 3,
  LOXBUDGET_PRESSURE_LOCKDOWN = 4
} loxbudget_pressure_t;

typedef enum {
  LOXBUDGET_RES_REUSABLE = 0,
  LOXBUDGET_RES_CONSUMABLE = 1,
  LOXBUDGET_RES_STATE = 2
} loxbudget_resource_kind_t;

typedef enum {
  LOXBUDGET_PRIO_LOW = 0,
  LOXBUDGET_PRIO_NORMAL = 1,
  LOXBUDGET_PRIO_HIGH = 2,
  LOXBUDGET_PRIO_CRITICAL = 3
} loxbudget_priority_t;

typedef enum {
  LOXBUDGET_REASON_OK = 0,
  LOXBUDGET_REASON_INSUFFICIENT_RES = 1,
  LOXBUDGET_REASON_RATE_LIMIT = 2,
  LOXBUDGET_REASON_LIFETIME_EXHAUSTED = 3,
  LOXBUDGET_REASON_PRESSURE_BLOCK = 4,
  LOXBUDGET_REASON_LOCKDOWN_ACTIVE = 5,
  LOXBUDGET_REASON_PRECONDITION_FAIL = 6,
  LOXBUDGET_REASON_CAUSAL_CASCADE = 7,
  LOXBUDGET_REASON_UNKNOWN_OP = 8,
  LOXBUDGET_REASON_HAL_NOT_CONFIGURED = 9
} loxbudget_reason_t;

/* Operation flags. */
#define LOXBUDGET_OPF_REQUIRES_BOOT_PROVEN (1u << 0)
#define LOXBUDGET_OPF_REQUIRES_VOLTAGE_OK (1u << 1)
#define LOXBUDGET_OPF_REQUIRES_NETWORK_UP (1u << 2)
#define LOXBUDGET_OPF_PERSIST_AUDIT (1u << 3)
#define LOXBUDGET_OPF_BYPASS_RATE_LIMIT (1u << 4)
#define LOXBUDGET_OPF_CALIBRATABLE (1u << 5)
#define LOXBUDGET_OPF_LOCKDOWN_PASS (1u << 6)

typedef struct {
  loxbudget_action_t action;
  loxbudget_pressure_t pressure;
  loxbudget_resource_id_t denied_resource;
  uint16_t requested;
  uint16_t available;
  uint8_t reason;
} loxbudget_decision_t;

typedef struct {
  uint16_t limit;
  uint16_t used;
  uint16_t reserved;
  uint16_t available;
  uint16_t high_watermark;
  uint8_t kind;
} loxbudget_resource_view_t;

typedef struct {
  loxbudget_op_id_t op_id;
  uint8_t priority;
  uint8_t action_normal;
  uint8_t action_elevated;
  uint8_t action_critical;
  uint8_t action_survival;
  uint8_t action_lockdown;
  uint8_t flags;
} loxbudget_op_profile_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_op_profile_t) == 8, "loxbudget_op_profile_t size");

typedef struct {
  uint32_t acquired_at_ms;
  uint16_t magic;
  uint8_t id;
  uint8_t op;
} loxbudget_lease_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_lease_t) == 8, "loxbudget_lease_t size");

typedef struct {
  uint32_t timestamp_ms;
  loxbudget_op_id_t op_id;
  loxbudget_resource_id_t denied_resource;
  uint16_t requested;
  uint16_t available;
  uint8_t action;
  uint8_t pressure;
  uint8_t reason;
  uint8_t _pad;
} loxbudget_decision_record_t;
LOXBUDGET_STATIC_ASSERT(sizeof(loxbudget_decision_record_t) == 16,
                        "loxbudget_decision_record_t size");

typedef struct {
  uint8_t pressure;
  uint8_t active_lease_count;
  uint8_t resource_count;
  uint8_t op_count;
  uint32_t total_decisions;
  uint32_t total_grants;
  uint32_t total_denials;
  uint32_t total_degradations;
  uint32_t uptime_ms;
} loxbudget_snapshot_t;

typedef struct loxbudget_hal_callbacks_t {
  uint32_t (*now_ms)(void *user);
  void (*critical_enter)(void *user);
  void (*critical_exit)(void *user);
  loxbudget_bool_t (*boot_proven)(void *user);
  loxbudget_bool_t (*voltage_ok)(void *user);
  loxbudget_bool_t (*network_up)(void *user);
} loxbudget_hal_callbacks_t;

typedef struct {
  uint8_t max_resources;
  uint8_t max_ops;
  uint8_t max_concurrent_leases;
  uint8_t audit_size;
  uint8_t hal_strict;
  uint8_t _reserved[3];
  uint16_t flags;
  const loxbudget_hal_callbacks_t *hal_callbacks;
  void *hal_user;
} loxbudget_config_t;

typedef struct loxbudget_t loxbudget_t;

/* Compile-time buffer sizing helper. */
#define LOXBUDGET_REQUIRED_SIZE(n_res, n_ops, audit_n) \
  (32u + (uint32_t)(n_res) * 12u + (uint32_t)(n_ops) * 8u + \
   (uint32_t)(n_ops) * (uint32_t)LOXBUDGET_MAX_NEEDS_PER_OP * 4u + \
   (uint32_t)LOXBUDGET_MAX_LEASES * 8u + (uint32_t)(audit_n) * 16u + 16u)

/* Core API (V0.1). */
/* Initialize a budget instance in caller-provided storage. Storage must be uint32_t-aligned. */
loxbudget_status_t loxbudget_init(loxbudget_t *budget, void *storage,
                                 size_t storage_size,
                                 const loxbudget_config_t *cfg);
/* Deinitialize an instance and invalidate existing leases. */
loxbudget_status_t loxbudget_deinit(loxbudget_t *budget);

/* Define or replace a resource limit and kind. */
loxbudget_status_t loxbudget_set_resource(loxbudget_t *budget,
                                         loxbudget_resource_id_t id,
                                         uint16_t limit,
                                         loxbudget_resource_kind_t kind);

/* Register an operation profile. */
loxbudget_status_t loxbudget_register_op(loxbudget_t *budget,
                                        const loxbudget_op_profile_t *profile);

/* Declare that an operation needs a given resource amount. */
loxbudget_status_t loxbudget_op_set_need(loxbudget_t *budget,
                                        loxbudget_op_id_t op,
                                        loxbudget_resource_id_t resource,
                                        uint16_t amount);

/* Non-mutating decision query (does not reserve resources). */
loxbudget_status_t loxbudget_check(loxbudget_t *budget, loxbudget_op_id_t op,
                                  loxbudget_decision_t *out);

/* Atomically reserve all needed resources and return a lease token. */
loxbudget_status_t loxbudget_enter(loxbudget_t *budget, loxbudget_op_id_t op,
                                  loxbudget_lease_t *out_lease);

/* Release a previously acquired lease and return reusable resources. */
loxbudget_status_t loxbudget_leave(loxbudget_t *budget, loxbudget_lease_t lease);

/* Set the current global pressure state (application-controlled). */
loxbudget_status_t loxbudget_set_pressure(loxbudget_t *budget,
                                         loxbudget_pressure_t pressure);

/* Get current global pressure state. */
loxbudget_pressure_t loxbudget_get_pressure(const loxbudget_t *budget);

/* Get a read-only snapshot of instance counters and state. */
loxbudget_status_t loxbudget_snapshot(const loxbudget_t *budget,
                                     loxbudget_snapshot_t *out);

/* HAL API (weak symbols provided in src/loxbudget_hal.c, overridable via callbacks). */
/* Time since boot in milliseconds (monotonic). */
uint32_t loxbudget_hal_now_ms(void);
/* Critical section for atomic reservations (may be no-op). */
void loxbudget_hal_critical_enter(void);
void loxbudget_hal_critical_exit(void);
/* Optional system preconditions. Return LOXBUDGET_TRUE only when satisfied. */
loxbudget_bool_t loxbudget_hal_boot_proven(void);
loxbudget_bool_t loxbudget_hal_voltage_ok(void);
loxbudget_bool_t loxbudget_hal_network_up(void);
/* Convenience callback table for permissive defaults (tests/minimal example). */
const loxbudget_hal_callbacks_t *loxbudget_hal_default_permissive(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LOXBUDGET_H */
