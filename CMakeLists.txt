cmake_minimum_required(VERSION 3.16)
project(loxbudget C)

add_library(loxbudget STATIC)
target_sources(loxbudget PRIVATE src/loxbudget_core.c src/loxbudget_audit.c
                                 src/loxbudget_hal.c src/loxbudget_strings.c
                                 src/loxbudget_calibration.c)
target_include_directories(loxbudget PUBLIC include)

add_executable(test_header_compiles tests/test_header_compiles.c)
target_include_directories(test_header_compiles PRIVATE include)

add_executable(test_v0_1 tests/test_v0_1.c)
target_link_libraries(test_v0_1 PRIVATE loxbudget)
target_include_directories(test_v0_1 PRIVATE include)

add_executable(test_microlog_adapter tests/test_microlog_adapter.c
                                     adapters/microlog/loxbudget_microlog_adapter.c)
target_include_directories(test_microlog_adapter PRIVATE include adapters/microlog)
target_link_libraries(test_microlog_adapter PRIVATE loxbudget)

add_executable(integration_flash_burnout tests/integration/test_flash_burnout.c)
target_link_libraries(integration_flash_burnout PRIVATE loxbudget)
target_include_directories(integration_flash_burnout PRIVATE include)

add_executable(integration_mqtt_storm tests/integration/test_mqtt_storm.c)
target_link_libraries(integration_mqtt_storm PRIVATE loxbudget)
target_include_directories(integration_mqtt_storm PRIVATE include)

add_executable(integration_ota_blocked tests/integration/test_ota_blocked.c)
target_link_libraries(integration_ota_blocked PRIVATE loxbudget)
target_include_directories(integration_ota_blocked PRIVATE include)
