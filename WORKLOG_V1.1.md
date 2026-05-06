# WORKLOG — V1.1 (Causality Tracking)

> **Prerequisite**: V1.0 tagged, in production use, and feedback collected.
> **Goal**: causality / cascading-cost detection. Operations declare what other operations they may trigger; the decision engine accounts for the cascade.
> **Estimated effort**: ~10 days.

---

## Why This Phase Comes Last

Causality is the most technically interesting feature in the loxbudget roadmap. It is also the **highest-risk** feature, because:

- Its correctness depends on cascade weighting that needs real workload data to calibrate.
- Cycle handling has subtle edge cases.
- Wrong implementation produces false denials worse than no causality at all.

For these reasons, V1.0 ships **without** causality. Once V1.0 is in production and we have real reports of *"each op was fine, but together they crashed us"*, V1.1 addresses those reports with data, not speculation.

If V1.0 users never report cascading-load problems, **V1.1 may be deprioritized indefinitely**. That's a feature, not a bug — many users won't need this complexity.

---

## Pre-Work

- [ ] V1.0 tagged and in production for ≥ 3 months.
- [ ] At least 3 distinct reports of cascading-load problems from real users (not from your imagination).
- [ ] Read `SPEC.md §17` (Causality Tracking).
- [ ] Confirm Q8 weight semantics are still correct given real workload data.

If you cannot tick all four pre-work boxes, **stop**. Causality without real demand is over-engineering.

---

## Scope

### In scope (V1.1)

- `loxbudget_op_may_trigger` API.
- Causality graph (directed, weighted with Q8).
- Transitive budget check up to depth limit.
- Cycle detection with explicit visited bitmap.
- `RARE` edge handling (only counted under CRITICAL+).
- Configurable depth and max-edges limits.
- Scenario replay tool for testing cascades.
- Documentation of when causality helps and when it doesn't.

### Out of scope

- Auto-discovery of triggers (e.g., from runtime tracing).
- Probabilistic execution sampling.
- Anything that introduces non-determinism.

---

## Phase 1 — Graph Storage

### P1.1 — Edge structure

```c
typedef struct {
    loxbudget_op_id_t parent;
    loxbudget_op_id_t child;
    uint8_t weight_q8;     /* trigger kind: NEVER=0, RARE=32, MAYBE=128, ALWAYS=255 */
    uint8_t flags;
} lb__causality_edge_t;
```

`_Static_assert(sizeof(lb__causality_edge_t) == 4, ...)`.

### P1.2 — Graph storage

- [ ] Fixed-size edge array in user buffer.
- [ ] Default `LOXBUDGET_CAUSALITY_MAX_EDGES = 32`.
- [ ] Storage gated by `LOXBUDGET_ENABLE_CAUSALITY`.

### P1.3 — Update `LOXBUDGET_REQUIRED_SIZE`

- [ ] Add edge array size when enabled.
- [ ] Add visited bitmap: `(LOXBUDGET_MAX_OPS + 7) / 8` bytes.
- [ ] Zero contribution when disabled.

---

## Phase 2 — Edge Registration

### P2.1 — `loxbudget_op_may_trigger`

```c
loxbudget_status_t
loxbudget_op_may_trigger(loxbudget_t *budget,
                         loxbudget_op_id_t parent,
                         loxbudget_op_id_t child,
                         loxbudget_trigger_kind_t kind);
```

- [ ] Validate both ops are registered.
- [ ] Validate kind is in enum range.
- [ ] If parent == child → ERR_INVALID_ARG (no self-loops).
- [ ] Add edge to graph.
- [ ] If edge already exists → update weight (last write wins).
- [ ] If edge array full → ERR_NO_SPACE.

### P2.2 — Cycle detection at registration time

- [ ] After adding edge, run cycle check from parent.
- [ ] If cycle detected → remove edge, return ERR_BAD_STATE.
- [ ] Document: cycles are detected at registration, not at check time, to make application boot fail loud rather than runtime fail silent.

### P2.3 — Edge enumeration helper (internal)

- [ ] `lb__causality_outgoing_edges(graph, op_id, out_edges, max)` — returns edges where `parent == op_id`.

### P2.4 — Tests

- [ ] `test_may_trigger_basic`: add edge, verify it's in graph.
- [ ] `test_may_trigger_self_loop_rejected`: parent == child → error.
- [ ] `test_may_trigger_cycle_rejected`: A → B → C → A; the third edge fails.
- [ ] `test_may_trigger_capacity`: fill MAX_EDGES; next add fails.
- [ ] `test_may_trigger_update_weight`: same edge twice with different kind; weight updated.

---

## Phase 3 — Cascade Computation

### P3.1 — Cascade walk algorithm

For `check(op)`:

1. Start with op's direct needs.
2. Initialize visited bitmap, push op onto explicit DFS stack with depth=0.
3. Loop until stack empty:
   - Pop (current_op, depth).
   - Mark visited.
   - If depth ≥ MAX_DEPTH, continue.
   - For each outgoing edge:
     - If child already visited (cycle prevention), continue.
     - If edge is `RARE` and pressure < CRITICAL, continue.
     - Compute scaled needs: `scaled = (need * weight_q8 + 128) >> 8`.
     - Add scaled needs to total request.
     - Push (child, depth+1).
4. Final: check if total scaled request fits available resources.

### P3.2 — Q8 fixed-point arithmetic

```c
static inline uint16_t lb__scale_q8(uint16_t need, uint8_t weight_q8)
{
    /* Round to nearest. */
    uint32_t product = (uint32_t)need * (uint32_t)weight_q8 + 128u;
    uint32_t result = product >> 8;
    if (result > UINT16_MAX) result = UINT16_MAX;  /* saturate */
    return (uint16_t)result;
}
```

### P3.3 — Multiple cascades to same resource

- [ ] If two ops in cascade need the same resource, sum their scaled needs (with saturation).
- [ ] Maintain a small per-resource accumulator during the walk.

### P3.4 — Integration with `check`

- [ ] When causality is enabled and graph non-empty for this op:
  - Run cascade walk.
  - Use cascade-augmented needs in availability check.
- [ ] When disabled or graph empty:
  - Direct needs only (V1.0 behavior).
- [ ] Reason code `CAUSAL_CASCADE` returned if denial was due to cascade rather than direct need.

### P3.5 — Tests

- [ ] `test_cascade_always_edge`: parent + child both need 100 of resource (limit 150). Edge ALWAYS. Without causality: parent alone needs 100, available, allowed. With causality: parent needs 100 + scaled child = 100 + 100 = 200, denied.
- [ ] `test_cascade_maybe_edge`: same but MAYBE → 100 + 50 = 150. Available. Allowed.
- [ ] `test_cascade_rare_under_normal`: RARE edge ignored under NORMAL pressure.
- [ ] `test_cascade_rare_under_critical`: RARE edge counted under CRITICAL.
- [ ] `test_cascade_depth_limit`: chain longer than MAX_DEPTH; deeper ops not counted.
- [ ] `test_cascade_diamond`: A → B and A → C; both B and C need same resource. Verified accumulation works.
- [ ] `test_cascade_reason_code`: denial due to cascade has reason `CAUSAL_CASCADE`, not `INSUFFICIENT_RES`.

---

## Phase 4 — Configuration Knobs

### P4.1 — Compile-time tunables

```c
#ifndef LOXBUDGET_CAUSALITY_MAX_EDGES
  #define LOXBUDGET_CAUSALITY_MAX_EDGES 32
#endif

#ifndef LOXBUDGET_CAUSALITY_MAX_DEPTH
  #define LOXBUDGET_CAUSALITY_MAX_DEPTH 3
#endif
```

### P4.2 — Runtime introspection

- [ ] `loxbudget_causality_edge_count(budget)` returns current edge count.
- [ ] Useful for diagnostics: "is my graph filling up?"

---

## Phase 5 — Scenario Replay Tool

### P5.1 — Replay format

- [ ] Define a simple text or binary format for scenarios:
  ```
  # scenario: parser cascade
  register MQTT_PUBLISH normal=ALLOW_FULL ...
  register NVLOG_WRITE normal=ALLOW_FULL ...
  may_trigger MQTT_PUBLISH NVLOG_WRITE ALWAYS
  set_resource RAM 4096 REUSABLE
  set_pressure NORMAL
  expect check(MQTT_PUBLISH) -> ALLOW_FULL
  set_pressure CRITICAL
  expect check(MQTT_PUBLISH) -> REJECT reason=CAUSAL_CASCADE
  ```

### P5.2 — Python scenario runner

- [ ] `tools/scenario_replay.py`.
- [ ] Compiles a host-mode loxbudget binary that reads scenario from stdin and produces results.
- [ ] Used in CI: each scenario file becomes a regression test.

### P5.3 — Library of scenarios

- [ ] `tests/scenarios/` directory.
- [ ] At least 5 scenarios from real-world causality patterns:
  - MQTT publish triggers offline queue write
  - Parser response triggers MQTT publish
  - OTA verify triggers flash read storm
  - Diamond cascade
  - Long chain (depth 3+)

---

## Phase 6 — Documentation

### P6.1 — Causality guide

- [ ] When to use causality (signs you need it).
- [ ] When not to (most cases — V1.0 is enough).
- [ ] How to declare edges.
- [ ] How weights work; how to choose ALWAYS vs MAYBE vs RARE.
- [ ] How to interpret CAUSAL_CASCADE denials.

### P6.2 — Performance notes

- [ ] Cascade walk is O(edges × depth).
- [ ] Worst case: ~96 operations.
- [ ] Document overhead in measured cycles on Cortex-M4F.

### P6.3 — Update FAQ

- [ ] "Should I use causality?" → "Probably not, unless you've seen cascading-load problems in production. V1.0 covers most cases via STATE resources and PRECONDITION flags."

---

## Phase 7 — Footprint and Performance

### P7.1 — Footprint

- [ ] EXPERIMENTAL profile (causality enabled): document new flash/RAM cost.
- [ ] FULL profile (causality disabled): unchanged.

### P7.2 — Cycles benchmark

- [ ] Measure `check()` with cascade vs without.
- [ ] Realistic graph (5-10 edges, depth 2): expected overhead < 50%.
- [ ] Document in `benchmarks/v1.1_cycles.md`.

---

## Phase 8 — Release

### P8.1 — Beta period

- [ ] Tag `v1.1.0-beta1`.
- [ ] Solicit feedback specifically from V1.0 users who reported cascading-load problems.
- [ ] Iterate based on whether the feature actually helps.

### P8.2 — Final release

- [ ] Tag `v1.1.0`.
- [ ] Release notes emphasize: this is for users who experienced cascading-load problems in V1.0. Most users don't need it.

---

## V1.1 Done Criteria

- [ ] Causality graph registration works.
- [ ] Cascade walk correctly accounts for direct + transitive needs.
- [ ] Cycles detected at registration time.
- [ ] RARE edges only counted under CRITICAL+.
- [ ] All cascade tests pass.
- [ ] Scenario replay tool runs library of test scenarios.
- [ ] Documentation explains *when not to use this*.
- [ ] Real users with cascading-load problems report the feature solves their problem.
- [ ] V1.0 tests still pass.
- [ ] FULL profile (causality off) footprint unchanged.
- [ ] `v1.1.0` tagged.

---

## Common Mistakes to Avoid

1. **Floats sneaking into Q8 math.** `(need * weight + 128) >> 8` is integer; keep it that way.
2. **Recursion in cascade walk.** Use explicit stack; avoid blowing host stack.
3. **Cycle detection at check time instead of registration.** Cycles must fail loud at boot, not silent at runtime.
4. **MAYBE edges treated as ALWAYS for "safety."** That's not safety; that's false denials. Respect the weight.
5. **Causality on by default.** It must be opt-in. Most users don't need it.
6. **Adding causality without real user demand.** If pre-work boxes can't be ticked, this phase shouldn't happen yet.
7. **Auto-discovering triggers from runtime tracing.** Out of scope; introduces non-determinism.

---

## Final Note

After V1.1, the roadmap is **deliberately empty**. Future versions are reactive — driven by real user reports, not anticipated features.

The library is **done** when:
- No new feature would meaningfully serve "can this operation safely run right now?"
- Adding any new feature would compromise one of the core promises.

V1.1 is the last planned major addition. Beyond that, the project enters maintenance: bug fixes, footprint optimizations, new platform support, and reactive features only.

This is not pessimism. This is discipline. A small, complete library that does one thing well outlasts a large, growing library that does many things badly.

---

*End of V1.1 worklog.*
