cmake_minimum_required(VERSION 3.16)
project(SumoPara CXX)

# The project template used C++20; the analyzers only require C++17, so we
# pin the standard explicitly to keep the code portable across toolchains.
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# OpenMP detection.
#
# On macOS the default compiler (/usr/bin/c++, Apple clang) ships without
# OpenMP support, while Homebrew LLVM (/opt/homebrew/opt/llvm) provides both
# the compiler and libomp. We therefore:
#   1. try the standard find_package(OpenMP) — works with LLVM, GCC, MSVC;
#   2. fall back to compiling a probe file with -fopenmp (works when the
#      compiler supports the flag even though CMake's finder does not);
#   3. as a last resort build only the serial/tools targets and warn loudly.
# ---------------------------------------------------------------------------
set(SUMOPARA_HAVE_OPENMP FALSE)

find_package(OpenMP QUIET COMPONENTS CXX)
if(OpenMP_CXX_FOUND)
    set(SUMOPARA_HAVE_OPENMP TRUE)
    set(SUMOPARA_OMP_LINK OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found via find_package: ${OpenMP_CXX_FLAGS}")
else()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fopenmp")
    check_cxx_source_compiles("
        #include <omp.h>
        int main() {
            double s = 0.0;
            #pragma omp parallel for reduction(+:s)
            for (int i = 0; i < 16; ++i) s += i;
            return static_cast<int>(s) == 120 ? 0 : 1;
        }" SUMOPARA_FOPENMP_WORKS)
    unset(CMAKE_REQUIRED_FLAGS)
    if(SUMOPARA_FOPENMP_WORKS)
        set(SUMOPARA_HAVE_OPENMP TRUE)
        set(SUMOPARA_OMP_LINK "")
        set(SUMOPARA_OMP_MANUAL_FLAGS "-fopenmp")
        message(STATUS "OpenMP usable via manual -fopenmp flags")
    else()
        message(WARNING
            "OpenMP was NOT found. The OpenMP targets will be skipped. "
            "On macOS, install Homebrew LLVM ('brew install llvm') and run: "
            "cmake -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ ..")
    endif()
endif()

# ---------------------------------------------------------------------------
# Shared library: data model, CSV loader, serial reference kernel, compare.
# ---------------------------------------------------------------------------
add_library(traffic_common STATIC src/common/traffic_analysis.cpp)

# ---------------------------------------------------------------------------
# Module I — serial analyzer.
# ---------------------------------------------------------------------------
add_executable(traffic_analysis_serial src/serial/traffic_analysis_serial.cpp)
target_link_libraries(traffic_analysis_serial PRIVATE traffic_common)
if(SUMOPARA_HAVE_OPENMP)
    # Link OpenMP only so the serial timer can use omp_get_wtime() — the
    # exact same wall clock as the OpenMP module (no parallel regions are
    # created). When OpenMP is unavailable the code falls back to
    # std::chrono::high_resolution_clock.
    target_compile_definitions(traffic_analysis_serial
        PRIVATE SUMOPARA_USE_OMP_WTIME)
    target_link_libraries(traffic_analysis_serial PRIVATE ${SUMOPARA_OMP_LINK})
    if(SUMOPARA_OMP_MANUAL_FLAGS)
        target_compile_options(traffic_analysis_serial PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
        target_link_options(traffic_analysis_serial PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
    endif()
endif()

# ---------------------------------------------------------------------------
# Module II — OpenMP analyzer (only when OpenMP is available).
# ---------------------------------------------------------------------------
if(SUMOPARA_HAVE_OPENMP)
    add_executable(traffic_analysis_openmp
        src/openmp/traffic_analysis_openmp.cpp
        src/openmp/traffic_analysis_openmp_kernel.cpp)
    target_link_libraries(traffic_analysis_openmp PRIVATE
        traffic_common ${SUMOPARA_OMP_LINK})
    if(SUMOPARA_OMP_MANUAL_FLAGS)
        target_compile_options(traffic_analysis_openmp PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
        target_link_options(traffic_analysis_openmp PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
    endif()

    # Correctness tests compare serial and OpenMP kernels, so they need
    # OpenMP as well.
    add_executable(test_correctness
        tests/test_correctness.cpp
        src/openmp/traffic_analysis_openmp_kernel.cpp)
    target_link_libraries(test_correctness PRIVATE
        traffic_common ${SUMOPARA_OMP_LINK})
    if(SUMOPARA_OMP_MANUAL_FLAGS)
        target_compile_options(test_correctness PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
        target_link_options(test_correctness PRIVATE
            ${SUMOPARA_OMP_MANUAL_FLAGS})
    endif()

    enable_testing()
    add_test(NAME correctness COMMAND test_correctness)
endif()

# ---------------------------------------------------------------------------
# Deterministic synthetic dataset generator (no OpenMP needed).
# ---------------------------------------------------------------------------
add_executable(generate_dataset tools/generate_dataset.cpp)
