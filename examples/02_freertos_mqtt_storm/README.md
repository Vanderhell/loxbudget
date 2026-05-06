# Example 02 — “MQTT storm” (host simulation)

This is a host-only compile/run demo that mimics an outage storm:

- Pressure ramps `NORMAL → ELEVATED → CRITICAL`.
- Operation profile maps pressure to `ALLOW_FULL / ALLOW_DEGRADED / WAIT / REJECT`.
- When audit is enabled (`LOXBUDGET_ENABLE_AUDIT_TRAIL=1`), you can pull recent decision records
  to see what happened and when.

