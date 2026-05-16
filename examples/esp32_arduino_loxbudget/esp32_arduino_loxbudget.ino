#include <Arduino.h>

/*
  ESP32 Arduino demo for loxbudget.

  How to build:
  - Copy `single_header/loxbudget.h` from this repo into this sketch folder as `loxbudget.h`
  - Keep `loxbudget_impl.c` next to this `.ino` (it provides the library implementation)
  - Open this folder as a sketch in Arduino IDE (or PlatformIO's Arduino framework)

  Serial commands:
  - `help`                      show commands
  - `status`                    print current config
  - `run [passes]`              run N "mega bench" passes (default 1)
  - `soak <passes>`             run N passes with short pauses (default 10)
  - `stop`                      stop continuous bench
  - `set need <n>`              set op need (default 10)
  - `set limit <n>`             set resource limit (default 100)
  - `set rate <n>`              set per-minute rate limit (default 30, only if rate windows enabled)
  - `set lifetime <n>`          set lifetime limit (default 500, only if rate windows enabled)
*/

/* Enable optional features for this sketch. These must match `loxbudget_impl.c`. */
#define LOXBUDGET_ENABLE_RATE_WINDOWS 1
#define LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS 1

extern "C" {
#include "loxbudget.h"
}

static const uint32_t BENCH_CALLS_PER_PASS = 50000u;
static const uint32_t BENCH_YIELD_EVERY = 1000u;

static uint32_t hal_now_ms_(void* user) {
  (void)user;
  return (uint32_t)millis();
}
static void hal_critical_enter_(void* user) {
  (void)user;
  noInterrupts();
}
static void hal_critical_exit_(void* user) {
  (void)user;
  interrupts();
}
static loxbudget_bool_t hal_true_(void* user) {
  (void)user;
  return LOXBUDGET_TRUE;
}

static const loxbudget_hal_callbacks_t HAL_CB = {
    hal_now_ms_,
    hal_critical_enter_,
    hal_critical_exit_,
    hal_true_, /* boot_proven */
    hal_true_, /* voltage_ok */
    hal_true_, /* network_up */
};

static loxbudget_op_profile_t ota_profile_(loxbudget_op_id_t id) {
  loxbudget_op_profile_t p = loxbudget_op_profile_default(id);
  p.action_critical = LOXBUDGET_ALLOW_DEGRADED;
  p.action_survival = LOXBUDGET_REJECT;
  p.action_lockdown = LOXBUDGET_LOCKDOWN;
  return p;
}

/* 1 resource, 1 op, no audit ring in this demo. */
static uint32_t STORAGE[(LOXBUDGET_REQUIRED_SIZE(1, 1, 0) + 3u) / 4u];
static loxbudget_t B;
static loxbudget_op_profile_t OTA;

static uint16_t g_need = 10;
static uint16_t g_limit = 100;
static uint32_t g_rate_per_min = 30;
static uint32_t g_lifetime = 500;

typedef enum {
  MODE_IDLE = 0,
  MODE_RUN = 1,
  MODE_SOAK = 2
} mode_t;

static mode_t g_mode = MODE_IDLE;
static uint32_t g_passes_left = 0u;

static void apply_cfg_() {
  (void)loxbudget_set_resource(&B, 0, g_limit, LOXBUDGET_RES_CONSUMABLE);
#if LOXBUDGET_ENABLE_RATE_WINDOWS
  (void)loxbudget_set_rate_limit(&B, 0, LOXBUDGET_WINDOW_MINUTE, g_rate_per_min);
  (void)loxbudget_set_lifetime_limit(&B, 0, g_lifetime);
#endif
  (void)loxbudget_op_set_need(&B, 0, 0, g_need);
}

static void print_status_() {
  Serial.printf("loxbudget ESP32 demo status:\n");
  Serial.printf("- mode: %s\n", (g_mode == MODE_IDLE) ? "idle" : (g_mode == MODE_RUN) ? "run" : "soak");
  Serial.printf("- need: %u\n", (unsigned)g_need);
  Serial.printf("- limit: %u\n", (unsigned)g_limit);
#if LOXBUDGET_ENABLE_RATE_WINDOWS
  Serial.printf("- rate/min: %lu\n", (unsigned long)g_rate_per_min);
  Serial.printf("- lifetime: %lu\n", (unsigned long)g_lifetime);
#else
  Serial.printf("- rate windows: disabled\n");
#endif
  Serial.printf("- bench calls/pass: %lu\n", (unsigned long)BENCH_CALLS_PER_PASS);
}

static void print_help_() {
  Serial.println("Commands:");
  Serial.println("  help");
  Serial.println("  status");
  Serial.println("  run [passes]");
  Serial.println("  soak <passes>");
  Serial.println("  stop");
  Serial.println("  set need <n>");
  Serial.println("  set limit <n>");
#if LOXBUDGET_ENABLE_RATE_WINDOWS
  Serial.println("  set rate <n>");
  Serial.println("  set lifetime <n>");
#endif
}

static bool read_line_(String& out) {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') return true;
    out += (char)c;
    if (out.length() > 200) { out.remove(0); }
  }
  return false;
}

static uint32_t parse_u32_(const String& s, int* ok) {
  char* endp = nullptr;
  const unsigned long v = strtoul(s.c_str(), &endp, 10);
  if (endp == s.c_str()) { *ok = 0; return 0; }
  *ok = 1;
  return (uint32_t)v;
}

static void handle_cmd_(const String& line) {
  String s = line;
  s.trim();
  if (s.length() == 0) return;
  s.toLowerCase();

  if (s == "help") { print_help_(); return; }
  if (s == "status") { print_status_(); return; }
  if (s == "stop") { g_mode = MODE_IDLE; g_passes_left = 0u; Serial.println("OK: stop"); return; }

  if (s == "run" || s.startsWith("run ")) {
    uint32_t passes = 1u;
    if (s.startsWith("run ")) {
      const String tail = s.substring(4);
      int ok = 0;
      passes = parse_u32_(tail, &ok);
      if (!ok || passes == 0u) { Serial.println("ERR: run [passes] (passes>0)"); return; }
    }
    g_mode = MODE_RUN;
    g_passes_left = passes;
    Serial.printf("OK: run passes=%lu\n", (unsigned long)passes);
    return;
  }

  if (s == "soak" || s.startsWith("soak ")) {
    uint32_t passes = 10u;
    if (s.startsWith("soak ")) {
      const String tail = s.substring(5);
      int ok = 0;
      passes = parse_u32_(tail, &ok);
      if (!ok || passes == 0u) { Serial.println("ERR: soak <passes> (passes>0)"); return; }
    }
    g_mode = MODE_SOAK;
    g_passes_left = passes;
    Serial.printf("OK: soak passes=%lu\n", (unsigned long)passes);
    return;
  }

  if (s.startsWith("set ")) {
    const int sp1 = s.indexOf(' ');
    const int sp2 = s.indexOf(' ', sp1 + 1);
    if (sp2 < 0) { Serial.println("ERR: expected 'set <key> <value>'"); return; }
    const String key = s.substring(sp1 + 1, sp2);
    const String val = s.substring(sp2 + 1);
    int ok = 0;
    const uint32_t v = parse_u32_(val, &ok);
    if (!ok) { Serial.println("ERR: invalid number"); return; }

    if (key == "need") {
      g_need = (uint16_t)v;
      apply_cfg_();
      Serial.println("OK: need");
      return;
    }
    if (key == "limit") {
      g_limit = (uint16_t)v;
      apply_cfg_();
      Serial.println("OK: limit");
      return;
    }
#if LOXBUDGET_ENABLE_RATE_WINDOWS
    if (key == "rate") {
      g_rate_per_min = v;
      apply_cfg_();
      Serial.println("OK: rate");
      return;
    }
    if (key == "lifetime") {
      g_lifetime = v;
      apply_cfg_();
      Serial.println("OK: lifetime");
      return;
    }
#endif
    Serial.println("ERR: unknown key");
    return;
  }

  Serial.println("ERR: unknown command (type 'help')");
}

static void bench_pass_() {
  uint32_t allow_count = 0;
  uint32_t deny_count = 0;
  uint32_t enter_ok = 0;
  uint32_t enter_fail = 0;

  uint32_t min_us = 0xFFFFFFFFu;
  uint32_t max_us = 0u;
  uint64_t sum_us = 0u;

  /* Warm-up */
  for (uint32_t i = 0; i < 2000u; i++) {
    loxbudget_decision_t d;
    (void)loxbudget_check(&B, 0, &d);
  }

  const uint32_t start_us = (uint32_t)micros();
  for (uint32_t i = 0; i < BENCH_CALLS_PER_PASS; i++) {
    const uint32_t t0 = (uint32_t)micros();

    loxbudget_decision_t d;
    const loxbudget_status_t st = loxbudget_check(&B, 0, &d);
    if (st != LOXBUDGET_OK) {
      deny_count++;
    } else if (d.action == LOXBUDGET_ALLOW_FULL || d.action == LOXBUDGET_ALLOW_DEGRADED) {
      allow_count++;
      loxbudget_lease_t lease;
      if (loxbudget_enter(&B, 0, &lease) == LOXBUDGET_OK) {
        enter_ok++;
        (void)loxbudget_leave(&B, lease);
      } else {
        enter_fail++;
      }
    } else {
      deny_count++;
    }

    const uint32_t dt = (uint32_t)(micros() - t0);
    if (dt < min_us) min_us = dt;
    if (dt > max_us) max_us = dt;
    sum_us += (uint64_t)dt;

    if ((i % BENCH_YIELD_EVERY) == 0u) { delay(0); }
  }
  const uint32_t total_us = (uint32_t)(micros() - start_us);
  const float avg_us = (float)sum_us / (float)BENCH_CALLS_PER_PASS;
  const float ops_per_s = (total_us > 0u) ? ((float)BENCH_CALLS_PER_PASS * 1000000.0f / (float)total_us) : 0.0f;

  Serial.printf("MEGA BENCH: calls=%lu total=%luus ops/s=%.1f avg=%.3fus min=%luus max=%luus allow=%lu deny=%lu enter_ok=%lu enter_fail=%lu\n",
                (unsigned long)BENCH_CALLS_PER_PASS,
                (unsigned long)total_us,
                ops_per_s,
                avg_us,
                (unsigned long)min_us,
                (unsigned long)max_us,
                (unsigned long)allow_count,
                (unsigned long)deny_count,
                (unsigned long)enter_ok,
                (unsigned long)enter_fail);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  loxbudget_config_t cfg = loxbudget_config_simple(1, 1);
  cfg.hal_callbacks = &HAL_CB;
  cfg.hal_user = NULL;
  cfg.hal_strict = 1u;
  cfg.audit_size = 0u;
  cfg.max_concurrent_leases = 1u;

  const loxbudget_status_t st = loxbudget_init(&B, STORAGE, sizeof(STORAGE), &cfg);
  if (st != LOXBUDGET_OK) {
    Serial.printf("loxbudget_init failed: %d\n", (int)st);
    for (;;) { delay(1000); }
  }

  OTA = ota_profile_(0);
  (void)loxbudget_register_op(&B, &OTA);
  apply_cfg_();

  Serial.println("loxbudget ESP32 demo ready.");
  print_help_();
}

void loop() {
  static String line;
  if (read_line_(line)) {
    handle_cmd_(line);
    line = "";
  }

  if (g_mode == MODE_RUN || g_mode == MODE_SOAK) {
    if (g_passes_left == 0u) { g_mode = MODE_IDLE; return; }
    bench_pass_();
    g_passes_left--;
    if (g_passes_left == 0u) { g_mode = MODE_IDLE; Serial.println("DONE"); return; }
    if (g_mode == MODE_SOAK) { delay(50); }
    return;
  }

  delay(10);
}
