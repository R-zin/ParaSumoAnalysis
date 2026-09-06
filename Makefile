# Convenience wrapper around CMake. The canonical build is CMake (used by
# CLion); this Makefile only forwards common tasks.
#
# On macOS the default Apple clang has no OpenMP support; pass the Homebrew
# LLVM compiler explicitly:
#   make CXX=/opt/homebrew/opt/llvm/bin/clang++
# or configure once and reuse the cached build directory.

BUILD_DIR ?= build
CMAKE ?= cmake
CXX ?=

ifdef CXX
CMAKE_CXX_ARG = -DCMAKE_CXX_COMPILER=$(CXX)
endif

.PHONY: all configure build test clean datasets benchmark

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_CXX_ARG)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

# Generate the three standard benchmark datasets (10K / 100K / 1M vehicles).
datasets: build
	mkdir -p data
	$(BUILD_DIR)/generate_dataset 10000 data/small.csv
	$(BUILD_DIR)/generate_dataset 100000 data/medium.csv
	$(BUILD_DIR)/generate_dataset 1000000 data/large.csv

# Full benchmark: datasets, runs, tables, graphs.
benchmark: build
	python3 benchmarks/run_benchmark.py --build-dir $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
