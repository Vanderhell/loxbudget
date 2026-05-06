.PHONY: all test clean tiny format lint header-check

BUILD_DIR ?= build
CC ?= cc
AR ?= ar

CFLAGS ?= -std=c99 -Wall -Wextra -Werror -Iinclude

LOXBUDGET_OBJS := $(BUILD_DIR)/loxbudget.o $(BUILD_DIR)/loxbudget_hal.o

all: $(BUILD_DIR)/libloxbudget.a header-check

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/loxbudget.o: src/loxbudget.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/loxbudget_hal.o: src/loxbudget_hal.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libloxbudget.a: $(LOXBUDGET_OBJS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/test_header_compiles.o: tests/test_header_compiles.c include/loxbudget.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

header-check: $(BUILD_DIR)/test_header_compiles.o

test: header-check

clean:
	@rm -rf $(BUILD_DIR)

tiny:
	@echo "tiny profile build is implemented in Phase 7"

format:
	@echo "format is implemented in Phase 7"

lint:
	@echo "lint is implemented in Phase 7"
