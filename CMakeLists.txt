cmake_minimum_required(VERSION 3.16)
project(loxbudget VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

enable_testing()

add_library(loxbudget STATIC)
target_sources(loxbudget PRIVATE src/loxbudget_core.c src/loxbudget_audit.c src/loxbudget_hal.c
                                 src/loxbudget_strings.c src/loxbudget_calibration.c
                                 src/loxbudget_causality.c)
target_include_directories(loxbudget PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(loxbudget PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME loxbudget
)

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
add_test(NAME test_header_compiles COMMAND test_header_compiles)

add_executable(test_v0_1 tests/test_v0_1.c)
target_link_libraries(test_v0_1 PRIVATE loxbudget)
target_include_directories(test_v0_1 PRIVATE include)
add_test(NAME test_v0_1 COMMAND test_v0_1)

add_executable(test_microlog_adapter tests/test_microlog_adapter.c
                                     adapters/microlog/loxbudget_microlog_adapter.c)
target_include_directories(test_microlog_adapter PRIVATE include adapters/microlog)
target_link_libraries(test_microlog_adapter PRIVATE loxbudget)
add_test(NAME test_microlog_adapter COMMAND test_microlog_adapter)

add_executable(test_microhealth_adapter tests/test_microhealth_adapter.c
                                        adapters/microhealth/loxbudget_microhealth_adapter.c)
target_include_directories(test_microhealth_adapter PRIVATE include adapters/microhealth)
target_link_libraries(test_microhealth_adapter PRIVATE loxbudget)
add_test(NAME test_microhealth_adapter COMMAND test_microhealth_adapter)

add_executable(test_microconf_adapter tests/test_microconf_adapter.c
                                      adapters/microconf/loxbudget_microconf_adapter.c)
target_include_directories(test_microconf_adapter PRIVATE include adapters/microconf)
target_link_libraries(test_microconf_adapter PRIVATE loxbudget)
add_test(NAME test_microconf_adapter COMMAND test_microconf_adapter)

add_executable(test_microbus_adapter tests/test_microbus_adapter.c
                                     adapters/microbus/loxbudget_microbus_adapter.c)
target_include_directories(test_microbus_adapter PRIVATE include adapters/microbus)
target_link_libraries(test_microbus_adapter PRIVATE loxbudget)
add_test(NAME test_microbus_adapter COMMAND test_microbus_adapter)

add_executable(test_nvlog_adapter tests/test_nvlog_adapter.c adapters/nvlog/loxbudget_nvlog_adapter.c)
target_include_directories(test_nvlog_adapter PRIVATE include adapters/nvlog)
target_link_libraries(test_nvlog_adapter PRIVATE loxbudget)
add_test(NAME test_nvlog_adapter COMMAND test_nvlog_adapter)

add_executable(test_loxguard_adapter tests/test_loxguard_adapter.c
                                     adapters/loxguard/loxbudget_loxguard_adapter.c)
target_include_directories(test_loxguard_adapter PRIVATE include adapters/loxguard)
target_link_libraries(test_loxguard_adapter PRIVATE loxbudget)
add_test(NAME test_loxguard_adapter COMMAND test_loxguard_adapter)

add_executable(test_backward_compat_v0_3 tests/test_backward_compat_v0_3.c)
target_link_libraries(test_backward_compat_v0_3 PRIVATE loxbudget)
target_include_directories(test_backward_compat_v0_3 PRIVATE include)
add_test(NAME test_backward_compat_v0_3 COMMAND test_backward_compat_v0_3)

add_executable(test_determinism tests/test_determinism.c)
target_link_libraries(test_determinism PRIVATE loxbudget)
target_include_directories(test_determinism PRIVATE include)
add_test(NAME test_determinism COMMAND test_determinism)

add_executable(test_calibration_disabled tests/test_calibration_disabled.c)
target_include_directories(test_calibration_disabled PRIVATE include)
target_compile_definitions(test_calibration_disabled PRIVATE LOXBUDGET_ENABLE_CALIBRATION=0)
add_test(NAME test_calibration_disabled COMMAND test_calibration_disabled)

add_executable(scenario_runner tests/scenario_runner.c)
target_sources(scenario_runner PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(scenario_runner PRIVATE include)
target_compile_definitions(scenario_runner PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)

add_executable(test_causality tests/test_causality.c)
target_sources(test_causality PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(test_causality PRIVATE include)
target_compile_definitions(test_causality PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)
target_compile_definitions(test_causality PRIVATE LOXBUDGET_CAUSALITY_MAX_EDGES=4)
add_test(NAME test_causality COMMAND test_causality)

add_executable(integration_flash_burnout tests/integration/test_flash_burnout.c)
target_link_libraries(integration_flash_burnout PRIVATE loxbudget)
target_include_directories(integration_flash_burnout PRIVATE include)
add_test(NAME integration_flash_burnout COMMAND integration_flash_burnout)

add_executable(integration_mqtt_storm tests/integration/test_mqtt_storm.c)
target_link_libraries(integration_mqtt_storm PRIVATE loxbudget)
target_include_directories(integration_mqtt_storm PRIVATE include)
add_test(NAME integration_mqtt_storm COMMAND integration_mqtt_storm)

add_executable(integration_ota_blocked tests/integration/test_ota_blocked.c)
target_link_libraries(integration_ota_blocked PRIVATE loxbudget)
target_include_directories(integration_ota_blocked PRIVATE include)
add_test(NAME integration_ota_blocked COMMAND integration_ota_blocked)

add_executable(bench_check_overhead benchmarks/bench_check_overhead.c)
target_sources(bench_check_overhead PRIVATE $<TARGET_OBJECTS:loxbudget_causality_obj>)
target_include_directories(bench_check_overhead PRIVATE include)
target_compile_definitions(bench_check_overhead PRIVATE LOXBUDGET_ENABLE_CAUSALITY=1)

install(TARGETS loxbudget
  EXPORT loxbudgetTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES include/loxbudget.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT loxbudgetTargets
  FILE loxbudgetTargets.cmake
  NAMESPACE loxbudget::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loxbudget
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/loxbudgetConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/loxbudgetConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/loxbudgetConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loxbudget
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/loxbudgetConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/loxbudgetConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loxbudget
)
