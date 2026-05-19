#include "loxbudget.h"

#include <string.h>

#if LOXBUDGET_ENABLE_CAUSALITY

typedef struct {
  loxbudget_op_id_t parent;
  loxbudget_op_id_t child;
  uint8_t weight_q8;
  uint8_t _pad;
} lb__causality_edge_t;
LOXBUDGET_STATIC_ASSERT(sizeof(lb__causality_edge_t) == 4, "lb__causality_edge_t size");

typedef struct {
  uint16_t edge_count;
  uint16_t _pad;
} lb__causality_hdr_t;
LOXBUDGET_STATIC_ASSERT(sizeof(lb__causality_hdr_t) == 4, "lb__causality_hdr_t size");

typedef struct {
  // cppcheck-suppress unusedStructMember
  uint32_t magic2;
  // cppcheck-suppress unusedStructMember
  uint8_t res_cfg[LOXBUDGET_MAX_RESOURCES];
  uint8_t op_cfg[LOXBUDGET_MAX_OPS];
  // cppcheck-suppress unusedStructMember
  uint8_t _pad[4];
} lb__storage_hdr_t;
LOXBUDGET_STATIC_ASSERT(sizeof(lb__storage_hdr_t) == 32, "lb__storage_hdr_t size");

static uint32_t lb__align_up_(uint32_t v, uint32_t a) { return (v + (a - 1u)) & ~(a - 1u); }

static uint32_t lb__causality_off_(const loxbudget_t* b) {
  /* Must match the storage layout in loxbudget_init (src/loxbudget_core.c). */
  const uint32_t align = 4u;
  uint32_t off = 0u;

  off += 32u; /* storage header reservation */
  off = lb__align_up_(off, align);
  off += (uint32_t)b->max_resources * 12u;
  off = lb__align_up_(off, align);
  off += (uint32_t)b->max_ops * 8u;
  off = lb__align_up_(off, align);
  off += (uint32_t)b->max_ops * (uint32_t)LOXBUDGET_MAX_NEEDS_PER_OP * 4u;
  off = lb__align_up_(off, align);
  off += (uint32_t)b->max_leases * 4u; /* loxbudget_lease_slot_t */
  off = lb__align_up_(off, align);
  off += (uint32_t)b->audit_size * 16u;
  off = lb__align_up_(off, align);

#if LOXBUDGET_ENABLE_RATE_WINDOWS
  off += (uint32_t)b->max_resources * 72u;
  off = lb__align_up_(off, align);
#endif

#if LOXBUDGET_ENABLE_CALIBRATION
  off += (uint32_t)b->max_ops * (uint32_t)LOXBUDGET_CALIB_STATE_SIZE;
  off = lb__align_up_(off, align);
#endif

  return off;
}

// cppcheck-suppress constParameterPointer
static lb__causality_hdr_t* lb__causality_hdr_(loxbudget_t* b) {
  return (lb__causality_hdr_t*)(b->storage + lb__causality_off_(b));
}
static const lb__causality_hdr_t* lb__causality_hdr_c_(const loxbudget_t* b) {
  return (const lb__causality_hdr_t*)(b->storage + lb__causality_off_(b));
}

// cppcheck-suppress constParameterPointer
static lb__causality_edge_t* lb__causality_edges_(loxbudget_t* b) {
  uint8_t* base = b->storage + lb__causality_off_(b);
  return (lb__causality_edge_t*)(base + sizeof(lb__causality_hdr_t));
}
static const lb__causality_edge_t* lb__causality_edges_c_(const loxbudget_t* b) {
  const uint8_t* base = b->storage + lb__causality_off_(b);
  return (const lb__causality_edge_t*)(base + sizeof(lb__causality_hdr_t));
}

// cppcheck-suppress constParameterPointer
static uint8_t* lb__causality_visited_(loxbudget_t* b) {
  uint8_t* base = b->storage + lb__causality_off_(b);
  base += sizeof(lb__causality_hdr_t);
  base += (uint32_t)LOXBUDGET_CAUSALITY_MAX_EDGES * (uint32_t)sizeof(lb__causality_edge_t);
  return base;
}

static loxbudget_bool_t lb__causality_has_storage_(const loxbudget_t* b) {
  const uint32_t off = lb__causality_off_(b);
  const uint32_t need = off + (uint32_t)sizeof(lb__causality_hdr_t) +
                        (uint32_t)LOXBUDGET_CAUSALITY_MAX_EDGES *
                            (uint32_t)sizeof(lb__causality_edge_t) +
                        (((uint32_t)b->max_ops + 7u) / 8u);
  return (need <= b->storage_size) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}

static void lb__visit_clear_(uint8_t* bits, uint32_t nbits) {
  const uint32_t nbytes = (nbits + 7u) / 8u;
  memset(bits, 0, (size_t)nbytes);
}
static loxbudget_bool_t lb__visit_test_(const uint8_t* bits, uint32_t bit) {
  return (bits[bit >> 3] & (uint8_t)(1u << (bit & 7u))) ? LOXBUDGET_TRUE : LOXBUDGET_FALSE;
}
static void lb__visit_set_(uint8_t* bits, uint32_t bit) { bits[bit >> 3] |= (uint8_t)(1u << (bit & 7u)); }

static loxbudget_bool_t lb__path_exists_(loxbudget_t* b, loxbudget_op_id_t start,
                                        loxbudget_op_id_t goal) {
  if (start >= b->max_ops || goal >= b->max_ops) return LOXBUDGET_FALSE;
  if (start == goal) return LOXBUDGET_TRUE;

  const lb__causality_hdr_t* hdr = lb__causality_hdr_c_(b);
  const lb__causality_edge_t* edges = lb__causality_edges_c_(b);
  uint8_t* visited = lb__causality_visited_(b);

  lb__visit_clear_(visited, b->max_ops);

  loxbudget_op_id_t stack[LOXBUDGET_MAX_OPS];
  uint16_t sp = 0;
  stack[sp++] = start;

  while (sp != 0u) {
    const loxbudget_op_id_t cur = stack[--sp];
    if (lb__visit_test_(visited, cur) == LOXBUDGET_TRUE) continue;
    lb__visit_set_(visited, cur);

    for (uint16_t ei = 0; ei < hdr->edge_count; ei++) {
      if (edges[ei].parent != cur) continue;
      const loxbudget_op_id_t child = edges[ei].child;
      if (child == goal) return LOXBUDGET_TRUE;
      if (child >= b->max_ops) continue;
      if (lb__visit_test_(visited, child) == LOXBUDGET_TRUE) continue;
      if (sp < (uint16_t)LOXBUDGET_MAX_OPS) { stack[sp++] = child; }
    }
  }
  return LOXBUDGET_FALSE;
}

uint16_t loxbudget_causality_edge_count(const loxbudget_t* budget) {
  if (budget == NULL || budget->magic != 0x4C58424Du) return 0u; /* LOXBUDGET_MAGIC_INIT */
  if (lb__causality_has_storage_(budget) == LOXBUDGET_FALSE) return 0u;
  return lb__causality_hdr_c_(budget)->edge_count;
}

loxbudget_status_t loxbudget_op_may_trigger(loxbudget_t* budget, loxbudget_op_id_t parent,
                                            loxbudget_op_id_t child,
                                            loxbudget_trigger_kind_t kind) {
  if (budget == NULL || budget->magic != 0x4C58424Du) return LOXBUDGET_ERR_NOT_INITIALIZED;
  if (lb__causality_has_storage_(budget) == LOXBUDGET_FALSE) return LOXBUDGET_ERR_NO_SPACE;
  if (parent >= budget->max_ops || child >= budget->max_ops) return LOXBUDGET_ERR_INVALID_ARG;
  if (parent == child) return LOXBUDGET_ERR_INVALID_ARG;
  switch (kind) {
  case LOXBUDGET_TRIGGER_NEVER:
  case LOXBUDGET_TRIGGER_RARE:
  case LOXBUDGET_TRIGGER_MAYBE:
  case LOXBUDGET_TRIGGER_ALWAYS:
    break;
  default:
    return LOXBUDGET_ERR_INVALID_ARG;
  }

  {
    const lb__storage_hdr_t* hdr = (const lb__storage_hdr_t*)budget->storage;
    if (hdr->op_cfg[parent] == 0u || hdr->op_cfg[child] == 0u) return LOXBUDGET_ERR_NOT_FOUND;
  }

  lb__causality_hdr_t* hdr = lb__causality_hdr_(budget);
  lb__causality_edge_t* edges = lb__causality_edges_(budget);

  /* Update if present. */
  for (uint16_t i = 0; i < hdr->edge_count; i++) {
    if (edges[i].parent == parent && edges[i].child == child) {
      edges[i].weight_q8 = (uint8_t)kind;
      return LOXBUDGET_OK;
    }
  }

  if (hdr->edge_count >= (uint16_t)LOXBUDGET_CAUSALITY_MAX_EDGES) return LOXBUDGET_ERR_NO_SPACE;
  edges[hdr->edge_count].parent = parent;
  edges[hdr->edge_count].child = child;
  edges[hdr->edge_count].weight_q8 = (uint8_t)kind;
  edges[hdr->edge_count]._pad = 0u;
  hdr->edge_count++;

  /* Cycle detection at registration time (fail loud at boot). */
  if (lb__path_exists_(budget, child, parent) == LOXBUDGET_TRUE) {
    hdr->edge_count--;
    memset(&edges[hdr->edge_count], 0, sizeof(edges[hdr->edge_count]));
    return LOXBUDGET_ERR_BAD_STATE;
  }

  return LOXBUDGET_OK;
}

/* Internal helper for core decision engine (defined in src/loxbudget_core.c). */
uint32_t lb__causality_add_scaled_needs_(loxbudget_t* budget, loxbudget_op_id_t root_op,
                                        uint16_t* io_need_per_resource,
                                        loxbudget_pressure_t pressure);

uint32_t lb__causality_add_scaled_needs_(loxbudget_t* budget, loxbudget_op_id_t root_op,
                                        uint16_t* io_need_per_resource,
                                        loxbudget_pressure_t pressure) {
  if (lb__causality_has_storage_(budget) == LOXBUDGET_FALSE) return 0u;
  if (root_op >= budget->max_ops) return 0u;

  const lb__causality_hdr_t* hdr = lb__causality_hdr_c_(budget);
  const lb__causality_edge_t* edges = lb__causality_edges_c_(budget);
  uint8_t* visited = lb__causality_visited_(budget);

  lb__visit_clear_(visited, budget->max_ops);

  loxbudget_op_id_t stack_op[LOXBUDGET_MAX_OPS];
  uint8_t stack_depth[LOXBUDGET_MAX_OPS];
  uint16_t sp = 0;
  stack_op[sp] = root_op;
  stack_depth[sp] = 0u;
  sp++;

  while (sp != 0u) {
    sp--;
    const loxbudget_op_id_t cur = stack_op[sp];
    const uint8_t depth = stack_depth[sp];

    if (lb__visit_test_(visited, cur) == LOXBUDGET_TRUE) continue;
    lb__visit_set_(visited, cur);

    if (depth >= (uint8_t)LOXBUDGET_CAUSALITY_MAX_DEPTH) continue;

    for (uint16_t ei = 0; ei < hdr->edge_count; ei++) {
      if (edges[ei].parent != cur) continue;
      const loxbudget_op_id_t child = edges[ei].child;
      const uint8_t w = edges[ei].weight_q8;
      if (w == 0u) continue;
      if (child >= budget->max_ops) continue;
      if (lb__visit_test_(visited, child) == LOXBUDGET_TRUE) continue;
      if (w == (uint8_t)LOXBUDGET_TRIGGER_RARE && pressure < LOXBUDGET_PRESSURE_CRITICAL) {
        continue;
      }

      /* Add scaled child needs into the per-resource accumulator. */
      for (uint8_t ni = 0; ni < (uint8_t)LOXBUDGET_MAX_NEEDS_PER_OP; ni++) {
        const uint32_t idx = (uint32_t)child * (uint32_t)LOXBUDGET_MAX_NEEDS_PER_OP + (uint32_t)ni;
        /* needs table layout: {u8 resource, u8 pad, u16 amount} */
        const uint8_t res = budget->storage[budget->needs_off + idx * 4u + 0u];
        const uint16_t amt =
            (uint16_t)budget->storage[budget->needs_off + idx * 4u + 2u] |
            (uint16_t)((uint16_t)budget->storage[budget->needs_off + idx * 4u + 3u] << 8);
        if (amt == 0u) continue;
        if (res >= budget->max_resources) continue;

        const uint32_t scaled = ((uint32_t)amt * (uint32_t)w + 128u) >> 8;
        uint32_t sum = (uint32_t)io_need_per_resource[res] + scaled;
        if (sum > 0xFFFFu) sum = 0xFFFFu;
        io_need_per_resource[res] = (uint16_t)sum;
      }

      if (sp < (uint16_t)LOXBUDGET_MAX_OPS) {
        stack_op[sp] = child;
        stack_depth[sp] = (uint8_t)(depth + 1u);
        sp++;
      }
    }
  }
  return 1u;
}

#else
/* C file intentionally empty when causality is disabled. */
#endif
