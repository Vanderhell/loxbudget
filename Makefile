.PHONY: all test test-adapters clean tiny format lint header-check example examples integration banned cross cross-tiny \
	cross-standard cross-full

BUILD_DIR ?= build
CC ?= cc
AR ?= ar

CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Iinclude
LDFLAGS ?=

LOXBUDGET_OBJS := $(BUILD_DIR)/loxbudget_core.o $(BUILD_DIR)/loxbudget_audit.o \
	$(BUILD_DIR)/loxbudget_hal.o $(BUILD_DIR)/loxbudget_strings.o $(BUILD_DIR)/loxbudget_calibration.o \
	$(BUILD_DIR)/loxbudget_causality.o

LOXBUDGET_ENABLE_CAUSALITY ?= 0

all: $(BUILD_DIR)/libloxbudget.a header-check

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/loxbudget_core.o: src/loxbudget_core.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_audit.o: src/loxbudget_audit.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_hal.o: src/loxbudget_hal.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_strings.o: src/loxbudget_strings.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_calibration.o: src/loxbudget_calibration.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_causality.o: src/loxbudget_causality.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libloxbudget.a: $(LOXBUDGET_OBJS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/test_header_compiles.o: tests/test_header_compiles.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

header-check: $(BUILD_DIR)/test_header_compiles.o

$(BUILD_DIR)/test_v0_1.o: tests/test_v0_1.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_v0_1: $(BUILD_DIR)/test_v0_1.o $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

banned: $(BUILD_DIR)/libloxbudget.a
	@chmod +x tools/check_banned_symbols.sh tools/footprint_check.sh || true
	@./tools/check_banned_symbols.sh $<

test: header-check $(BUILD_DIR)/test_v0_1 banned
	@$(BUILD_DIR)/test_v0_1

$(BUILD_DIR)/test_microlog_adapter: tests/test_microlog_adapter.c adapters/microlog/loxbudget_microlog_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/microlog $^ -o $@

$(BUILD_DIR)/test_microhealth_adapter: tests/test_microhealth_adapter.c adapters/microhealth/loxbudget_microhealth_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/microhealth $^ -o $@

$(BUILD_DIR)/test_microconf_adapter: tests/test_microconf_adapter.c adapters/microconf/loxbudget_microconf_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/microconf $^ -o $@

$(BUILD_DIR)/test_microbus_adapter: tests/test_microbus_adapter.c adapters/microbus/loxbudget_microbus_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/microbus $^ -o $@

$(BUILD_DIR)/test_nvlog_adapter: tests/test_nvlog_adapter.c adapters/nvlog/loxbudget_nvlog_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/nvlog $^ -o $@

$(BUILD_DIR)/test_loxguard_adapter: tests/test_loxguard_adapter.c adapters/loxguard/loxbudget_loxguard_adapter.c $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) -Iadapters/loxguard $^ -o $@

test-adapters: test $(BUILD_DIR)/test_microlog_adapter $(BUILD_DIR)/test_microhealth_adapter \
	$(BUILD_DIR)/test_microconf_adapter $(BUILD_DIR)/test_microbus_adapter $(BUILD_DIR)/test_nvlog_adapter \
	$(BUILD_DIR)/test_loxguard_adapter
	@$(BUILD_DIR)/test_microlog_adapter
	@$(BUILD_DIR)/test_microhealth_adapter
	@$(BUILD_DIR)/test_microconf_adapter
	@$(BUILD_DIR)/test_microbus_adapter
	@$(BUILD_DIR)/test_nvlog_adapter
	@$(BUILD_DIR)/test_loxguard_adapter

clean:
	@rm -rf $(BUILD_DIR)

tiny:
	@echo "tiny profile build is implemented in Phase 7"

format:
	@echo "format is implemented in CI via clang-format"

lint:
	@echo "lint is implemented in CI via clang-tidy"

example: $(BUILD_DIR)/libloxbudget.a
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude examples/01_bare_metal_minimal/main.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/example_minimal
	@$(BUILD_DIR)/example_minimal

examples: example $(BUILD_DIR)/libloxbudget.a
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude examples/02_freertos_mqtt_storm/main.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/example_02
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude examples/03_esp_idf_flash_budget/main.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/example_03

integration: $(BUILD_DIR)/libloxbudget.a
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude tests/integration/test_flash_burnout.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/integration_flash_burnout
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude tests/integration/test_mqtt_storm.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/integration_mqtt_storm
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude tests/integration/test_ota_blocked.c \
		$(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/integration_ota_blocked
	@$(BUILD_DIR)/integration_flash_burnout
	@$(BUILD_DIR)/integration_mqtt_storm
	@$(BUILD_DIR)/integration_ota_blocked

cross: | $(BUILD_DIR)
	@arm-none-eabi-gcc --version >/dev/null
	@mkdir -p $(BUILD_DIR)/cross
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -DLOXBUDGET_ENABLE_AUDIT_TRAIL=$(LOXBUDGET_ENABLE_AUDIT_TRAIL) \
		-DLOXBUDGET_ENABLE_RATE_WINDOWS=$(LOXBUDGET_ENABLE_RATE_WINDOWS) \
		-DLOXBUDGET_ENABLE_CALIBRATION=$(LOXBUDGET_ENABLE_CALIBRATION) \
		-DLOXBUDGET_ENABLE_CAUSALITY=$(LOXBUDGET_ENABLE_CAUSALITY) -DLOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=$(LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS) \
		-c src/loxbudget_core.c -o $(BUILD_DIR)/cross/loxbudget_core_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -DLOXBUDGET_ENABLE_AUDIT_TRAIL=$(LOXBUDGET_ENABLE_AUDIT_TRAIL) -c src/loxbudget_audit.c -o $(BUILD_DIR)/cross/loxbudget_audit_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -DLOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=$(LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS) -c src/loxbudget_strings.c -o $(BUILD_DIR)/cross/loxbudget_strings_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -DLOXBUDGET_ENABLE_CALIBRATION=$(LOXBUDGET_ENABLE_CALIBRATION) -c src/loxbudget_calibration.c -o $(BUILD_DIR)/cross/loxbudget_calibration_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -DLOXBUDGET_ENABLE_CAUSALITY=$(LOXBUDGET_ENABLE_CAUSALITY) -c src/loxbudget_causality.c -o $(BUILD_DIR)/cross/loxbudget_causality_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb \
		-ffreestanding -c src/loxbudget_hal.c -o $(BUILD_DIR)/cross/loxbudget_hal_arm.o
	arm-none-eabi-ld -r $(BUILD_DIR)/cross/loxbudget_core_arm.o $(BUILD_DIR)/cross/loxbudget_audit_arm.o \
		$(BUILD_DIR)/cross/loxbudget_strings_arm.o $(BUILD_DIR)/cross/loxbudget_calibration_arm.o \
		$(BUILD_DIR)/cross/loxbudget_causality_arm.o $(BUILD_DIR)/cross/loxbudget_hal_arm.o -o $(BUILD_DIR)/loxbudget_arm_$(PROFILE).o

cross-tiny: PROFILE=tiny
cross-tiny: LOXBUDGET_ENABLE_AUDIT_TRAIL=0
cross-tiny: LOXBUDGET_ENABLE_RATE_WINDOWS=0
cross-tiny: LOXBUDGET_ENABLE_CALIBRATION=0
cross-tiny: LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=0
cross-tiny: cross

cross-standard: PROFILE=standard
cross-standard: LOXBUDGET_ENABLE_AUDIT_TRAIL=1
cross-standard: LOXBUDGET_ENABLE_RATE_WINDOWS=1
cross-standard: LOXBUDGET_ENABLE_CALIBRATION=0
cross-standard: LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=1
cross-standard: cross

cross-full: PROFILE=full
cross-full: LOXBUDGET_ENABLE_AUDIT_TRAIL=1
cross-full: LOXBUDGET_ENABLE_RATE_WINDOWS=1
cross-full: LOXBUDGET_ENABLE_CALIBRATION=1
cross-full: LOXBUDGET_ENABLE_DIAGNOSTIC_STRINGS=1
cross-full: cross
