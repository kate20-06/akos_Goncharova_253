CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Iinclude
BUILD_DIR := build
COMMON := src/constants.c src/model.c src/flower_logic.c src/gardener_logic.c \
          src/request_logic.c src/input.c src/input_validation.c src/config_parser.c \
          src/soft_assert.c src/statistics.c src/logger.c src/simulation_ipc.c \
          src/simulation_processes.c

all: $(BUILD_DIR)/garden $(BUILD_DIR)/flower $(BUILD_DIR)/gardener

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/garden: $(COMMON) src/simulation.c src/controller.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/flower: $(COMMON) src/worker_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/gardener: $(COMMON) src/worker_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DWORKER_GARDENER=1 $^ -o $@

run: all
	bash scripts/run_demo.sh tests/balanced.cfg

check: all
	python3 tests/check_independent.py

docs:
	doxygen Doxyfile

clean:
	rm -rf $(BUILD_DIR) docs simulation_*.log

.PHONY: all run check docs clean
