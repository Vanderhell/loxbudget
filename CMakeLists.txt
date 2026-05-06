cmake_minimum_required(VERSION 3.16)
project(loxbudget C)

add_library(loxbudget STATIC)
target_sources(loxbudget PRIVATE src/loxbudget_core.c src/loxbudget_audit.c
                                 src/loxbudget_hal.c src/loxbudget_strings.c)
target_include_directories(loxbudget PUBLIC include)

add_executable(test_header_compiles tests/test_header_compiles.c)
target_include_directories(test_header_compiles PRIVATE include)

add_executable(test_v0_1 tests/test_v0_1.c)
target_link_libraries(test_v0_1 PRIVATE loxbudget)
target_include_directories(test_v0_1 PRIVATE include)
