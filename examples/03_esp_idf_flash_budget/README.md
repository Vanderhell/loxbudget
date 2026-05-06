# Example 03 — “ESP-IDF flash budget” (host compile demo)

This example models flash writes as a `CONSUMABLE` resource:

- Each `enter()` permanently increments the resource’s `used` counter.
- `leave()` does not restore consumables.
- With audit enabled you can retrieve records to see which operations consumed flash.

