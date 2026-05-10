cmake_minimum_required(VERSION 3.16)
project(loxbudget C)

add_library(loxbudget STATIC)
target_sources(loxbudget PRIVATE src/loxbudget_core.c src/loxbudget_audit.c src/loxbudget_hal.c
                                 src/loxbudget_strings.c src/loxbudget_calibration.c
                                 src/loxbudget_causality.c)
target_include_directories(loxbudget PUBLIC include)

# Feature-on object library variants (used by tests that need specific feature flags).
add_library(loxbudget_causality_obj OBJECT)
target_sources(loxbudget_causality_obj PRIVATE src/loxbudget_core.c src/loxbudget_audit.c
                                               src/loxbudget_hal.c src/loxbudget_strings.c
                                               src/loxbudget_calibration.c src/loxbudget_causality.c)
target_include_directories(loxbudget_causality_obj PUBLIC include)
target_compile_definitions(loxbudget_causality_obj PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)
target_compile_definitions(loxbudget_causality_obj PRIVATE LOXBUDGET_CAUSALITY_MAX_EDGES=4)

add_executable(test_header_compiles tests/test_header_compiles.c)
target_include_directories(test_header_compiles PRIVATE include)

add_executable(test_v0_1 tests/test_v0_1.c)
target_link_libraries(test_v0_1 PRIVATE loxbudget)
target_include_directories(test_v0_1 PRIVATE include)

add_executable(test_microlog_adapter tests/test_microlog_adapter.c
                                     adapters/microlog/loxbudget_microlog_adapter.c)
target_include_directories(test_microlog_adapter PRIVATE include adapters/microlog)
target_link_libraries(test_microlog_adapter PRIVATE loxbudget)

add_executable(test_microhealth_adapter tests/test_microhealth_adapter.c
                                        adapters/microhealth/loxbudget_microhealth_adapter.c)
target_include_directories(test_microhealth_adapter PRIVATE include adapters/microhealth)
target_link_libraries(test_microhealth_adapter PRIVATE loxbudget)

add_executable(test_microconf_adapter tests/test_microconf_adapter.c
                                      adapters/microconf/loxbudget_microconf_adapter.c)
target_include_directories(test_microconf_adapter PRIVATE include adapters/microconf)
target_link_libraries(test_microconf_adapter PRIVATE loxbudget)

add_executable(test_microbus_adapter tests/test_microbus_adapter.c
                                     adapters/microbus/loxbudget_microbus_adapter.c)
target_include_directories(test_microbus_adapter PRIVATE include adapters/microbus)
target_link_libraries(test_microbus_adapter PRIVATE loxbudget)

add_executable(test_nvlog_adapter tests/test_nvlog_adapter.c adapters/nvlog/loxbudget_nvlog_adapter.c)
target_include_directories(test_nvlog_adapter PRIVATE include adapters/nvlog)
target_link_libraries(test_nvlog_adapter PRIVATE loxbudget)

add_executable(test_loxguard_adapter tests/test_loxguard_adapter.c
                                     adapters/loxguard/loxbudget_loxguard_adapter.c)
target_include_directories(test_loxguard_adapter PRIVATE include adapters/loxguard)
target_link_libraries(test_loxguard_adapter PRIVATE loxbudget)

add_executable(test_backward_compat_v0_3 tests/test_backward_compat_v0_3.c)
target_link_libraries(test_backward_compat_v0_3 PRIVATE loxbudget)
target_include_directories(test_backward_compat_v0_3 PRIVATE include)

add_executable(test_determinism tests/test_determinism.c)
target_link_libraries(test_determinism PRIVATE loxbudget)
target_include_directories(test_determinism PRIVATE include)

add_executable(test_calibration_disabled tests/test_calibration_disabled.c)
target_include_directories(test_calibration_disabled PRIVATE include)
target_compile_definitions(test_calibration_disabled PRIVATE LOXBUDGET_ENABLE_CALIBRATION=0)

add_executable(scenario_runner tests/scenario_runner.c)
target_sources(scenario_runner PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(scenario_runner PRIVATE include)
target_compile_definitions(scenario_runner PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)

add_executable(test_causality tests/test_causality.c)
target_sources(test_causality PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(test_causality PRIVATE include)
target_compile_definitions(test_causality PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)
target_compile_definitions(test_causality PRIVATE LOXBUDGET_CAUSALITY_MAX_EDGES=4)

add_executable(integration_flash_burnout tests/integration/test_flash_burnout.c)
target_link_libraries(integration_flash_burnout PRIVATE loxbudget)
target_include_directories(integration_flash_burnout PRIVATE include)

add_executable(integration_mqtt_storm tests/integration/test_mqtt_storm.c)
target_link_libraries(integration_mqtt_storm PRIVATE loxbudget)
target_include_directories(integration_mqtt_storm PRIVATE include)

add_executable(integration_ota_blocked tests/integration/test_ota_blocked.c)
target_link_libraries(integration_ota_blocked PRIVATE loxbudget)
target_include_directories(integration_ota_blocked PRIVATE include)

add_executable(bench_check_overhead benchmarks/bench_check_overhead.c)
target_sources(bench_check_overhead PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(bench_check_overhead PRIVATE include)
target_compile_definitions(bench_check_overhead PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)
