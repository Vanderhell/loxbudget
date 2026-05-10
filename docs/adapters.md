# Adapters

Adapters are optional integration modules. They are separate translation units and never required by core `loxbudget`.

## How to use an adapter

- Include the adapter header (for example `adapters/nvlog/loxbudget_nvlog_adapter.h`).
- Compile and link the corresponding `*_adapter.c` file into your firmware (opt-in).
- Provide the external symbols the adapter expects (documented in the header and summarized below).

## microlog

- Header: `adapters/microlog/loxbudget_microlog_adapter.h`
- Purpose: log decisions via a user-provided `microlog_write()`.

## microhealth

- Header: `adapters/microhealth/loxbudget_microhealth_adapter.h`
- Purpose: drive `loxbudget_set_pressure()` from a health/pressure percentage source.
- User must provide:
  - `microhealth_pressure_pct()`
  - `microhealth_subscribe()`

## microconf

- Header: `adapters/microconf/loxbudget_microconf_adapter.h`
- Purpose: load resources, op profiles, and needs from a configuration object.
- User must provide the microconf accessors declared in the header.

## microbus

- Header: `adapters/microbus/loxbudget_microbus_adapter.h`
- Purpose: emit events for denials/lockdown and pressure changes.
- User must provide:
  - `microbus_publish_event()`

## nvlog

- Header: `adapters/nvlog/loxbudget_nvlog_adapter.h`
- Purpose: persist reject/lockdown decisions as compact binary records.
- User must provide:
  - `nvlog_write()`

## loxguard

- Header: `adapters/loxguard/loxbudget_loxguard_adapter.h`
- Purpose: push reject/lockdown decisions into a blackbox buffer; flush on lockdown.
- User must provide:
  - `loxguard_push()`
  - `loxguard_flush()`
