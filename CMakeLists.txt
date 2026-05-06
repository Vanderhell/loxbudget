cmake_minimum_required(VERSION 3.16)
project(loxbudget C)

add_library(loxbudget STATIC)
target_sources(loxbudget PRIVATE src/loxbudget.c src/loxbudget_hal.c)
target_include_directories(loxbudget PUBLIC include)

add_executable(test_header_compiles tests/test_header_compiles.c)
target_include_directories(test_header_compiles PRIVATE include)
