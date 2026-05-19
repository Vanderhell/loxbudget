#include "loxbudget.h"

#include <string.h>

#if LOXBUDGET_ENABLE_CALIBRATION

typedef struct {
  // cppcheck-suppress unusedStructMember
  uint32_t magic2;
  // cppcheck-suppress unusedStructMember
  uint8_t res_cfg[LOXBUDGET_MAX_RESOURCES];
  uint8_t op_cfg[LOXBUDGET_MAX_OPS];
  // cppcheck-suppress unusedStructMember
  uint8_t _pad[4];
} lb__storage_hdr_t;
LOXBUDGET_STATIC_ASSERT(sizeof(lb__storage_hdr_t) == 32, "storage header size (calib)");

typedef struct {
  uint32_t marker_n[5];
  int32_t marker_n_prime_q16[5];
  uint32_t marker_q_q16[5];
  uint32_t dn_q16[5];
  uint32_t count;
  uint32_t initial_buffer[5];
  uint8_t initialized;
} lb__p2_estimator_t;

typedef struct {
  uint8_t active;
  uint8_t op_id;
  uint16_t _pad0;
  uint32_t target_samples;
  uint32_t sample_count;
  lb__p2_estimator_t ram_p50;
  lb__p2_estimator_t ram_p95;
  lb__p2_estimator_t ram_p99;
  lb__p2_estimator_t dur_p95;
  lb__p2_estimator_t dur_p99;
  uint16_t ram_max;
  uint16_t ram_max_at_500;
  uint16_t outlier_count;
  uint32_t dur_max_us;
} lb__calib_state_t;

typedef struct {
  lb__calib_state_t s;
  // cppcheck-suppress unusedStructMember
  uint8_t _pad[LOXBUDGET_CALIB_STATE_SIZE - (uint32_t)sizeof(lb__calib_state_t)];
} lb__calib_state_slot_t;

LOXBUDGET_STATIC_ASSERT(sizeof(lb__calib_state_slot_t) == LOXBUDGET_CALIB_STATE_SIZE,
                        "calib slot size");

// cppcheck-suppress constParameterPointer
static lb__calib_state_slot_t* lb__calib_slots_(loxbudget_t* b) {
  return (lb__calib_state_slot_t*)(b->storage + b->calib_off);
}

static const lb__calib_state_slot_t* lb__calib_slots_c_(const loxbudget_t* b) {
  return (const lb__calib_state_slot_t*)(b->storage + b->calib_off);
}

static loxbudget_status_t lb__validate_budget_(const loxbudget_t* b) {
  if (b == NULL) return LOXBUDGET_ERR_INVALID_ARG;
  if (b->magic == 0u) return LOXBUDGET_ERR_NOT_INITIALIZED;
  return LOXBUDGET_OK;
}

static const lb__storage_hdr_t* lb__hdr_c_(const loxbudget_t* b) {
  return (const lb__storage_hdr_t*)b->storage;
}

static loxbudget_bool_t lb__op_registered_(const loxbudget_t* b, loxbudget_op_id_t op) {
  if (b == NULL || b->storage == NULL) return LOXBUDGET_FALSE;
  if (op >= b->max_ops) return LOXBUDGET_FALSE;
  return (lb__hdr_c_(b)->op_cfg[op] != 0u) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static void lb__sort5_u32_(uint32_t* a) {
  /* Simple sorting network for 5 elements. */
  uint32_t t;
#define SWAP(i, j)                                                                                 \
  do {                                                                                             \
    if (a[(i)] > a[(j)]) {                                                                         \
      t = a[(i)];                                                                                  \
      a[(i)] = a[(j)];                                                                             \
      a[(j)] = t;                                                                                  \
    }                                                                                              \
  } while (0)
  SWAP(0, 1);
  SWAP(3, 4);
  SWAP(2, 4);
  SWAP(2, 3);
  SWAP(1, 4);
  SWAP(0, 3);
  SWAP(0, 2);
  SWAP(1, 3);
  SWAP(1, 2);
#undef SWAP
}

static void lb__p2_init_(lb__p2_estimator_t* e, uint32_t p_q16) {
  memset(e, 0, sizeof(*e));
  /* dn increments: [0, p/2, p, (1+p)/2, 1] */
  e->dn_q16[0] = 0u;
  e->dn_q16[1] = p_q16 / 2u;
  e->dn_q16[2] = p_q16;
  e->dn_q16[3] = (0x10000u + p_q16) / 2u;
  e->dn_q16[4] = 0x10000u;
}

static void lb__p2_seed5_(lb__p2_estimator_t* e, uint32_t p_q16) {
  uint32_t s[5];
  memcpy(s, e->initial_buffer, sizeof(s));
  lb__sort5_u32_(s);

  for (uint32_t i = 0; i < 5u; i++) {
    e->marker_n[i] = i + 1u;
    e->marker_q_q16[i] = s[i] << 16;
  }

  e->marker_n_prime_q16[0] = 1 << 16;
  e->marker_n_prime_q16[1] = (int32_t)((1u << 16) + 2u * p_q16);
  e->marker_n_prime_q16[2] = (int32_t)((1u << 16) + 4u * p_q16);
  e->marker_n_prime_q16[3] = (int32_t)((3u << 16) + 2u * p_q16);
  e->marker_n_prime_q16[4] = 5 << 16;

  e->initialized = 1u;
  e->count = 5u;
}

static uint32_t lb__p2_linear_(uint32_t qi_q16, int d, uint32_t qid_q16, uint32_t ni_d,
                               uint32_t ni) {
  const int64_t num = (int64_t)qi_q16 + (int64_t)d * ((int64_t)qid_q16 - (int64_t)qi_q16) /
                                            (int64_t)((int32_t)ni_d - (int32_t)ni);
  return (uint32_t)num;
}

static uint32_t lb__p2_parabolic_(const lb__p2_estimator_t* e, uint32_t i, int d) {
  const int32_t n_im1 = (int32_t)e->marker_n[i - 1u];
  const int32_t n_i = (int32_t)e->marker_n[i];
  const int32_t n_ip1 = (int32_t)e->marker_n[i + 1u];

  const int64_t qi = (int64_t)e->marker_q_q16[i];
  const int64_t qim1 = (int64_t)e->marker_q_q16[i - 1u];
  const int64_t qip1 = (int64_t)e->marker_q_q16[i + 1u];

  const int32_t d_i = (int32_t)d;

  /* term1 = (n_i - n_im1 + d) * (qip1 - qi) / (n_ip1 - n_i) */
  const int64_t term1_num = (int64_t)(n_i - n_im1 + d_i) * (qip1 - qi);
  const int64_t term1 = term1_num / (int64_t)(n_ip1 - n_i);

  /* term2 = (n_ip1 - n_i - d) * (qi - qim1) / (n_i - n_im1) */
  const int64_t term2_num = (int64_t)(n_ip1 - n_i - d_i) * (qi - qim1);
  const int64_t term2 = term2_num / (int64_t)(n_i - n_im1);

  const int64_t num = qi + (int64_t)d_i * (term1 + term2) / (int64_t)(n_ip1 - n_im1);
  return (uint32_t)num;
}

static void lb__p2_add_sample_(lb__p2_estimator_t* e, uint32_t p_q16, uint32_t x) {
  if (e->initialized == 0u) {
    e->initial_buffer[e->count++] = x;
    if (e->count == 5u) { lb__p2_seed5_(e, p_q16); }
    return;
  }

  const uint32_t x_q16 = x << 16;

  /* Find cell k. */
  uint32_t k = 0u;
  if (x_q16 < e->marker_q_q16[0]) {
    e->marker_q_q16[0] = x_q16;
    k = 0u;
  } else if (x_q16 >= e->marker_q_q16[4]) {
    e->marker_q_q16[4] = x_q16;
    k = 3u;
  } else {
    for (uint32_t i = 0u; i < 4u; i++) {
      if (e->marker_q_q16[i] <= x_q16 && x_q16 < e->marker_q_q16[i + 1u]) {
        k = i;
        break;
      }
    }
  }

  for (uint32_t i = k + 1u; i < 5u; i++) {
    e->marker_n[i]++;
  }
  for (uint32_t i = 0u; i < 5u; i++) {
    e->marker_n_prime_q16[i] += (int32_t)e->dn_q16[i];
  }
  e->count++;

  for (uint32_t i = 1u; i <= 3u; i++) {
    const int32_t n_q16 = (int32_t)(e->marker_n[i] << 16);
    const int32_t delta_q16 = e->marker_n_prime_q16[i] - n_q16;
    int d = 0;
    if (delta_q16 >= (1 << 16) && (e->marker_n[i + 1u] - e->marker_n[i]) > 1u) {
      d = 1;
    } else if (delta_q16 <= -(1 << 16) && (e->marker_n[i] - e->marker_n[i - 1u]) > 1u) {
      d = -1;
    }
    if (d == 0) { continue; }

    const uint32_t q_new = lb__p2_parabolic_(e, i, d);
    const uint32_t q_lo = e->marker_q_q16[i - 1u];
    const uint32_t q_hi = e->marker_q_q16[i + 1u];
    if (q_new > q_lo && q_new < q_hi) {
      e->marker_q_q16[i] = q_new;
    } else {
      /* Linear fallback. */
      const uint32_t qid = e->marker_q_q16[(uint32_t)((int)i + d)];
      const uint32_t nid = e->marker_n[(uint32_t)((int)i + d)];
      e->marker_q_q16[i] = lb__p2_linear_(e->marker_q_q16[i], d, qid, nid, e->marker_n[i]);
    }
    e->marker_n[i] = (uint32_t)((int32_t)e->marker_n[i] + (int32_t)d);
  }
}

static uint32_t lb__p2_get_q_q16_(const lb__p2_estimator_t* e) {
  if (e->initialized == 0u) {
    if (e->count == 0u) { return 0u; }
    /* Median of buffered samples as a rough fallback. */
    uint32_t s[5] = {0};
    memcpy(s, e->initial_buffer, e->count * sizeof(uint32_t));
    lb__sort5_u32_(s);
    return s[e->count / 2u] << 16;
  }
  return e->marker_q_q16[2];
}

static void lb__calib_compute_suggested_(const lb__calib_state_t* s,
                                         loxbudget_suggested_profile_t* out) {
  memset(out, 0, sizeof(*out));
  out->ram_p50 = (uint16_t)(lb__p2_get_q_q16_(&s->ram_p50) >> 16);
  out->ram_p95 = (uint16_t)(lb__p2_get_q_q16_(&s->ram_p95) >> 16);
  out->ram_p99 = (uint16_t)(lb__p2_get_q_q16_(&s->ram_p99) >> 16);
  out->ram_max = s->ram_max;
  out->duration_p95_us = (lb__p2_get_q_q16_(&s->dur_p95) >> 16);
  out->duration_p99_us = (lb__p2_get_q_q16_(&s->dur_p99) >> 16);
  out->duration_max_us = s->dur_max_us;
  out->outlier_count = s->outlier_count;
  out->sample_count = s->sample_count;

  /* Suggested limits (SPEC.md §14, integer-only). */
  {
    const uint16_t p99_plus = (uint16_t)(out->ram_p99 + 32u);
    const uint16_t max_plus5pct = (uint16_t)(((uint32_t)out->ram_max * 105u) / 100u);
    out->suggested_ram_limit = (p99_plus > max_plus5pct) ? p99_plus : max_plus5pct;
  }
  {
    const uint32_t p99_plus = out->duration_p99_us + 500u;
    const uint32_t max_plus10pct = (out->duration_max_us * 110u) / 100u;
    out->suggested_time_limit_us = (p99_plus > max_plus10pct) ? p99_plus : max_plus10pct;
  }
}

loxbudget_status_t loxbudget_calibrate_begin(loxbudget_t* budget, loxbudget_op_id_t op,
                                             uint32_t target_samples) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK) return st;
  if (target_samples == 0u) return LOXBUDGET_ERR_INVALID_ARG;
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;
  if (op >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;
  if (lb__op_registered_(budget, op) == LOXBUDGET_FALSE) return LOXBUDGET_ERR_NOT_FOUND;

  lb__calib_state_slot_t* slot = &lb__calib_slots_(budget)[op];
  lb__calib_state_t* s = &slot->s;
  if (s->active != 0u) return LOXBUDGET_ERR_BAD_STATE;

  memset(slot, 0, sizeof(*slot));
  s->active = 1u;
  s->op_id = op;
  s->target_samples = target_samples;
  lb__p2_init_(&s->ram_p50, 0x8000u);
  lb__p2_init_(&s->ram_p95, 0xF333u);
  lb__p2_init_(&s->ram_p99, 0xFD70u);
  lb__p2_init_(&s->dur_p95, 0xF333u);
  lb__p2_init_(&s->dur_p99, 0xFD70u);
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_calibrate_sample(loxbudget_t* budget, loxbudget_op_id_t op,
                                              const loxbudget_sample_t* sample) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK || sample == NULL) {
    return (sample == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;
  if (op >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;
  if (lb__op_registered_(budget, op) == LOXBUDGET_FALSE) return LOXBUDGET_ERR_NOT_FOUND;
  lb__calib_state_t* s = &lb__calib_slots_(budget)[op].s;
  if (s->active == 0u || s->op_id != op) return LOXBUDGET_ERR_BAD_STATE;

  s->sample_count++;
  if (sample->ram_used > s->ram_max) s->ram_max = sample->ram_used;
  if (sample->duration_us > s->dur_max_us) s->dur_max_us = sample->duration_us;

  /* Capture a stable baseline after the first 500 samples (SPEC.md §14). */
  if (s->sample_count == 500u) { s->ram_max_at_500 = s->ram_max; }

  lb__p2_add_sample_(&s->ram_p50, 0x8000u, (uint32_t)sample->ram_used);
  lb__p2_add_sample_(&s->ram_p95, 0xF333u, (uint32_t)sample->ram_used);
  lb__p2_add_sample_(&s->ram_p99, 0xFD70u, (uint32_t)sample->ram_used);

  /* Outlier detection (SPEC.md §14):
   *   sample > p99 * 1.5
   *   OR (after sample 500) sample > max_at_500 * 1.2
   */
  {
    const uint32_t p99 = lb__p2_get_q_q16_(&s->ram_p99);
    const uint64_t x_q16 = (uint64_t)sample->ram_used << 16;
    if (x_q16 > (uint64_t)p99 + (p99 >> 1)) {
      s->outlier_count++;
    } else if (s->sample_count > 500u && s->ram_max_at_500 != 0u) {
      const uint32_t thr = (uint32_t)(((uint32_t)s->ram_max_at_500 * 12u) / 10u);
      if (sample->ram_used > thr) { s->outlier_count++; }
    }
  }

  lb__p2_add_sample_(&s->dur_p95, 0xF333u, sample->duration_us);
  lb__p2_add_sample_(&s->dur_p99, 0xFD70u, sample->duration_us);

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_calibrate_end(loxbudget_t* budget, loxbudget_op_id_t op,
                                           loxbudget_suggested_profile_t* out) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;
  if (op >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;
  if (lb__op_registered_(budget, op) == LOXBUDGET_FALSE) return LOXBUDGET_ERR_NOT_FOUND;
  lb__calib_state_t* s = &lb__calib_slots_(budget)[op].s;
  if (s->active == 0u || s->op_id != op) return LOXBUDGET_ERR_BAD_STATE;

  lb__calib_compute_suggested_(s, out);

  s->active = 0u;
  return LOXBUDGET_OK;
}

static void lb__write_u16_le_(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void lb__write_u32_le_(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

loxbudget_status_t loxbudget_calibration_export_size(const loxbudget_t* budget, size_t* out_size) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK || out_size == NULL) {
    return (out_size == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st;
  }
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;

  const lb__calib_state_slot_t* slots = lb__calib_slots_c_(budget);
  uint32_t rec_count = 0u;
  for (uint32_t op = 0; op < budget->max_ops; op++) {
    const lb__calib_state_t* s = &slots[op].s;
    if (s->sample_count != 0u || s->active != 0u) { rec_count++; }
  }

  /* header(4) + records(rec_count * 40) */
  *out_size = 4u + (size_t)rec_count * 40u;
  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_calibration_export(const loxbudget_t* budget, void* out,
                                                size_t out_size, size_t* out_written) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL || out_written == NULL) {
    if (out == NULL || out_written == NULL) return LOXBUDGET_ERR_INVALID_ARG;
    return st;
  }
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;

  size_t need = 0u;
  st = loxbudget_calibration_export_size(budget, &need);
  if (st != LOXBUDGET_OK) return st;
  if (out_size < need) return LOXBUDGET_ERR_NO_SPACE;

  uint8_t* p = (uint8_t*)out;

  const lb__calib_state_slot_t* slots = lb__calib_slots_c_(budget);
  uint32_t rec_count = 0u;
  for (uint32_t op = 0; op < budget->max_ops; op++) {
    const lb__calib_state_t* s = &slots[op].s;
    if (s->sample_count != 0u || s->active != 0u) { rec_count++; }
  }

  /* Header */
  p[0] = 2u; /* version */
  p[1] = (uint8_t)rec_count;
  p[2] = 0u;
  p[3] = 0u;
  size_t off = 4u;

  for (uint32_t op = 0; op < budget->max_ops; op++) {
    const lb__calib_state_t* s = &slots[op].s;
    if (s->sample_count == 0u && s->active == 0u) continue;

    loxbudget_suggested_profile_t sugg;
    lb__calib_compute_suggested_(s, &sugg);

    uint8_t flags = 0u;
    if (s->active != 0u) flags |= 1u;

    /* Record (40 bytes) */
    p[off + 0u] = (uint8_t)op;
    p[off + 1u] = flags;
    lb__write_u16_le_(&p[off + 2u], 0u);

    lb__write_u16_le_(&p[off + 4u], sugg.ram_p50);
    lb__write_u16_le_(&p[off + 6u], sugg.ram_p95);
    lb__write_u16_le_(&p[off + 8u], sugg.ram_p99);
    lb__write_u16_le_(&p[off + 10u], sugg.ram_max);

    lb__write_u32_le_(&p[off + 12u], sugg.duration_p95_us);
    lb__write_u32_le_(&p[off + 16u], sugg.duration_p99_us);
    lb__write_u32_le_(&p[off + 20u], sugg.duration_max_us);

    lb__write_u16_le_(&p[off + 24u], sugg.suggested_ram_limit);
    lb__write_u16_le_(&p[off + 26u], sugg.outlier_count);
    lb__write_u32_le_(&p[off + 28u], sugg.sample_count);
    lb__write_u32_le_(&p[off + 32u], sugg.suggested_time_limit_us);
    lb__write_u32_le_(&p[off + 36u], s->target_samples);

    off += 40u;
  }

  *out_written = off;
  return LOXBUDGET_OK;
}

#else
/* Calibration disabled: this translation unit compiles to nothing. */
#endif /* LOXBUDGET_ENABLE_CALIBRATION */
