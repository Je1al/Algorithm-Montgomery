# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)
project(montx LANGUAGES CXX
        DESCRIPTION "Constant-time Montgomery arithmetic toolkit with a side-channel lab")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(MONTX_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(MONTX_WERROR   "Treat warnings as errors" OFF)
option(MONTX_FUZZ     "Build the libFuzzer target (requires clang)" OFF)

add_library(montx STATIC
  src/bigint.cpp
  src/montgomery.cpp
  src/modexp.cpp
  src/rng.cpp
  src/primes.cpp
  src/rsa.cpp)
target_include_directories(montx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(montx PRIVATE -Wall -Wextra)
if(MONTX_WERROR)
  target_compile_options(montx PRIVATE -Werror)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 12/13 emit false-positive -Warray-bounds / -Wstringop-* inside libstdc++
    # std::vector code paths at -O2/-O3. The code is clean under Clang, ASan and
    # UBSan, so keep these as warnings rather than hard errors.
    target_compile_options(montx PRIVATE -Wno-error=array-bounds -Wno-error=stringop-overflow)
  endif()
endif()
if(MONTX_SANITIZE)
  target_compile_options(montx PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(montx PUBLIC -fsanitize=address,undefined)
endif()

# --- executables -------------------------------------------------------------
add_executable(montx_cli apps/montx_cli.cpp)
target_link_libraries(montx_cli PRIVATE montx)

add_executable(bench_modexp bench/bench_modexp.cpp)
target_link_libraries(bench_modexp PRIVATE montx)

add_executable(timing_attack lab/timing_attack.cpp)
target_link_libraries(timing_attack PRIVATE montx)

# --- tests -------------------------------------------------------------------
enable_testing()
foreach(t bigint montgomery primes rsa constant_time)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE montx)
  target_include_directories(test_${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

# --- fuzzing (clang only) ----------------------------------------------------
if(MONTX_FUZZ)
  add_executable(fuzz_montmul fuzz/fuzz_montmul.cpp
    src/bigint.cpp src/montgomery.cpp src/modexp.cpp)
  target_include_directories(fuzz_montmul PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_options(fuzz_montmul PRIVATE -g -O1 -fsanitize=fuzzer,address,undefined)
  target_link_options(fuzz_montmul PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
