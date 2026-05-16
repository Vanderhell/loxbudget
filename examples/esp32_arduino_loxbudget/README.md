# ESP32 (Arduino) end-to-end demo

This is a real-MCU integration demo for `loxbudget` on ESP32 using the Arduino framework.

## Build (Arduino IDE)

1. Generate the single-header distribution:
   - Run `python3 tools/amalgamate.py` in the repo root.
2. Copy `single_header/loxbudget.h` into this folder as `loxbudget.h`.
3. Keep `loxbudget_impl.c` next to the `.ino` (it provides the implementation).
4. Open `esp32_arduino_loxbudget.ino` in Arduino IDE, select your ESP32 board, and upload.

## Serial commands

- `help` / `status`
- `run [passes]` runs a tight-loop stress pass (`check()` + `enter()`/`leave()` when allowed)
- `soak <passes>` repeats passes with short pauses
- `set need <n>` / `set limit <n>` (and rate-window knobs if enabled)

The sketch prints a single-line summary per pass (throughput, average latency, min/max observed).

