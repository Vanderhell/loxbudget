#include "loxbudget.h"

#include <string.h>

#if LOXBUDGET_ENABLE_CALIBRATION

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
  uint16_t outlier_count;
  uint32_t dur_max_us;
} lb__calib_state_t;

typedef struct {
  lb__calib_state_t s;
  uint8_t _pad[LOXBUDGET_CALIB_STATE_SIZE - (uint32_t)sizeof(lb__calib_state_t)];
} lb__calib_state_slot_t;

LOXBUDGET_STATIC_ASSERT(sizeof(lb__calib_state_slot_t) == LOXBUDGET_CALIB_STATE_SIZE,
                        "calib slot size");

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
  const int32_t n_i_d = n_i + d_i;

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

loxbudget_status_t loxbudget_calibrate_begin(loxbudget_t* budget, loxbudget_op_id_t op,
                                             uint32_t target_samples) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK) return st;
  if (target_samples == 0u) return LOXBUDGET_ERR_INVALID_ARG;
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;
  if (op >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;

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
  lb__calib_state_t* s = &lb__calib_slots_(budget)[op].s;
  if (s->active == 0u || s->op_id != op) return LOXBUDGET_ERR_BAD_STATE;

  s->sample_count++;
  if (sample->ram_used > s->ram_max) s->ram_max = sample->ram_used;
  if (sample->duration_us > s->dur_max_us) s->dur_max_us = sample->duration_us;

  lb__p2_add_sample_(&s->ram_p50, 0x8000u, (uint32_t)sample->ram_used);
  lb__p2_add_sample_(&s->ram_p95, 0xF333u, (uint32_t)sample->ram_used);
  lb__p2_add_sample_(&s->ram_p99, 0xFD70u, (uint32_t)sample->ram_used);

  if (s->sample_count == 500u) {
    s->outlier_count = 0u;
    /* reuse outlier_count slot to store max@500? keep simple */
  }

  /* outlier detection: sample > p99 * 1.5 */
  {
    const uint32_t p99 = lb__p2_get_q_q16_(&s->ram_p99);
    if (((uint64_t)sample->ram_used << 16) > (uint64_t)p99 + (p99 >> 1)) { s->outlier_count++; }
  }

  /* Duration estimator expects <= 65535 range; saturate for V1.0 minimal impl. */
  const uint32_t dur_sat = (sample->duration_us > 65535u) ? 65535u : sample->duration_us;
  lb__p2_add_sample_(&s->dur_p95, 0xF333u, dur_sat);
  lb__p2_add_sample_(&s->dur_p99, 0xFD70u, dur_sat);

  return LOXBUDGET_OK;
}

loxbudget_status_t loxbudget_calibrate_end(loxbudget_t* budget, loxbudget_op_id_t op,
                                           loxbudget_suggested_profile_t* out) {
  loxbudget_status_t st = lb__validate_budget_(budget);
  if (st != LOXBUDGET_OK || out == NULL) { return (out == NULL) ? LOXBUDGET_ERR_INVALID_ARG : st; }
  if (budget->calib_off == 0u) return LOXBUDGET_ERR_NO_SPACE;
  if (op >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;
  lb__calib_state_t* s = &lb__calib_slots_(budget)[op].s;
  if (s->active == 0u || s->op_id != op) return LOXBUDGET_ERR_BAD_STATE;

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

  /* Suggested limits (conservative, integer-only). */
  {
    uint16_t ram_suggest = out->ram_p99 + 32u;
    if (ram_suggest < out->ram_max) ram_suggest = out->ram_max;
    out->suggested_ram_limit = ram_suggest;
  }
  {
    uint32_t time_suggest = out->duration_p99_us + 1000u;
    if (time_suggest < out->duration_max_us) time_suggest = out->duration_max_us;
    out->suggested_time_limit_us = time_suggest;
  }

  s->active = 0u;
  return LOXBUDGET_OK;
}

#else
/* Calibration disabled: this translation unit compiles to nothing. */
#endif /* LOXBUDGET_ENABLE_CALIBRATION */
