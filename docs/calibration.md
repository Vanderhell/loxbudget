# Calibration

Enable compilation:

- `-DLOXBUDGET_ENABLE_CALIBRATION=1`

## Workflow

1. Begin calibration for an operation:
   - `loxbudget_calibrate_begin(budget, op_id, target_samples)`
2. During the operation, collect a `loxbudget_sample_t` and call:
   - `loxbudget_calibrate_sample(budget, op_id, &sample)`
3. End calibration and read suggestions:
   - `loxbudget_calibrate_end(budget, op_id, &suggested)`

## Export to host

The library does not provide transport (UART, BLE, filesystem, ...). It provides a binary snapshot you can transmit:

- `loxbudget_calibration_export_size(budget, &nbytes)`
- `loxbudget_calibration_export(budget, buf, buf_cap, &written)`

## Host report tool

- `python3 tools/calibration_report.py calibration.bin`
- JSON output: `python3 tools/calibration_report.py --json calibration.bin`

The tool supports both export formats:

- v1 (legacy scaffold)
- v2 (current)

Text mode includes extra hints for v2 exports:

- `Confidence`: how close `sample_count` is to `target_samples`.
- `Recommendation`: e.g. "collect more samples" or "inspect outliers".
- `rule`: which `SPEC.md` §14 term dominated the suggested limit (`p99+...` vs `max*...`).
