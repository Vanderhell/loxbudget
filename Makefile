.PHONY: all test clean tiny format lint header-check example banned cross

BUILD_DIR ?= build
CC ?= cc
AR ?= ar

CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Iinclude
LDFLAGS ?=

LOXBUDGET_OBJS := $(BUILD_DIR)/loxbudget_core.o $(BUILD_DIR)/loxbudget_audit.o $(BUILD_DIR)/loxbudget_hal.o

all: $(BUILD_DIR)/libloxbudget.a header-check

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/loxbudget_core.o: src/loxbudget_core.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_audit.o: src/loxbudget_audit.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_hal.o: src/loxbudget_hal.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libloxbudget.a: $(LOXBUDGET_OBJS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/test_header_compiles.o: tests/test_header_compiles.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

header-check: $(BUILD_DIR)/test_header_compiles.o

$(BUILD_DIR)/test_v0_1.o: tests/test_v0_1.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(BUILD_DIR)/test_v0_1: $(BUILD_DIR)/test_v0_1.o $(BUILD_DIR)/libloxbudget.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

banned: $(BUILD_DIR)/libloxbudget.a
	@chmod +x tools/check_banned_symbols.sh tools/footprint_check.sh || true
	@./tools/check_banned_symbols.sh $<

test: header-check $(BUILD_DIR)/test_v0_1 banned
	@$(BUILD_DIR)/test_v0_1

clean:
	@rm -rf $(BUILD_DIR)

tiny:
	@echo "tiny profile build is implemented in Phase 7"

format:
	@echo "format is implemented in Phase 7"

lint:
	@echo "lint is implemented in Phase 7"

example:
	@chmod +x tools/check_banned_symbols.sh tools/footprint_check.sh || true
	@$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude examples/01_bare_metal_minimal/main.c $(BUILD_DIR)/libloxbudget.a -o $(BUILD_DIR)/example_minimal
	@$(BUILD_DIR)/example_minimal

cross: | $(BUILD_DIR)
	@arm-none-eabi-gcc --version >/dev/null
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb -ffreestanding -c src/loxbudget_core.c -o $(BUILD_DIR)/loxbudget_arm.o
	arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -Iinclude -Os -mcpu=cortex-m0 -mthumb -ffreestanding -c src/loxbudget_hal.c -o $(BUILD_DIR)/loxbudget_hal_arm.o
