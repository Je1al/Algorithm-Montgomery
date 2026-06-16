# SPDX-License-Identifier: MIT
# Plain-Make build for systems without CMake. `make help` lists targets.

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
INCLUDES := -Iinclude
BUILD    := build

LIB_SRC  := src/bigint.cpp src/montgomery.cpp src/modexp.cpp \
            src/rng.cpp src/primes.cpp src/rsa.cpp
TESTS    := bigint montgomery primes rsa constant_time

.PHONY: all cli bench lab test clean help fuzz fuzz-replay asan

all: cli bench lab ## Build the CLI, benchmark and side-channel lab

$(BUILD):
	@mkdir -p $(BUILD)

cli: | $(BUILD) ## Build the montx command-line tool
	$(CXX) $(CXXFLAGS) $(INCLUDES) apps/montx_cli.cpp $(LIB_SRC) -o $(BUILD)/montx

bench: | $(BUILD) ## Build the benchmark
	$(CXX) $(CXXFLAGS) -O3 $(INCLUDES) bench/bench_modexp.cpp $(LIB_SRC) -o $(BUILD)/bench_modexp

lab: | $(BUILD) ## Build the side-channel lab
	$(CXX) $(CXXFLAGS) $(INCLUDES) lab/timing_attack.cpp $(LIB_SRC) -o $(BUILD)/timing_attack

test: | $(BUILD) ## Build and run the unit tests
	@set -e; for t in $(TESTS); do \
		echo "building test_$$t"; \
		$(CXX) $(CXXFLAGS) $(INCLUDES) -Itests tests/test_$$t.cpp $(LIB_SRC) -o $(BUILD)/test_$$t; \
		$(BUILD)/test_$$t; \
	done

asan: | $(BUILD) ## Run the tests under ASan + UBSan
	@set -e; for t in $(TESTS); do \
		$(CXX) $(CXXFLAGS) -g -fsanitize=address,undefined $(INCLUDES) -Itests \
			tests/test_$$t.cpp $(LIB_SRC) -o $(BUILD)/asan_$$t; \
		$(BUILD)/asan_$$t; \
	done

fuzz: | $(BUILD) ## Build the libFuzzer target (clang only)
	$(CXX) $(CXXFLAGS) -g -O1 -fsanitize=fuzzer,address,undefined $(INCLUDES) \
		fuzz/fuzz_montmul.cpp src/bigint.cpp src/montgomery.cpp src/modexp.cpp \
		-o $(BUILD)/fuzz_montmul

fuzz-replay: | $(BUILD) ## Build the corpus replayer (any compiler) and run it on fuzz/corpus
	$(CXX) $(CXXFLAGS) -DMONTX_FUZZ_STANDALONE $(INCLUDES) \
		fuzz/fuzz_montmul.cpp src/bigint.cpp src/montgomery.cpp src/modexp.cpp \
		-o $(BUILD)/fuzz_replay
	$(BUILD)/fuzz_replay fuzz/corpus/*

clean: ## Remove build artifacts
	rm -rf $(BUILD)

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-12s\033[0m %s\n", $$1, $$2}'
