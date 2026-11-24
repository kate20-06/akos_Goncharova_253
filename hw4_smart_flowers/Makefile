CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Iinclude -pthread
COMMON = src/constants.c src/model.c src/flower_logic.c src/gardener_logic.c src/request_logic.c \
         src/input.c src/input_validation.c src/config_parser.c src/soft_assert.c \
         src/statistics.c src/logger.c src/network.c src/worker_protocol.c

all: build/server build/flower build/gardener build/observer

build:
	mkdir -p build

build/server: $(COMMON) src/server.c src/server_clients.c | build
	$(CC) $(CFLAGS) $^ -o $@

build/flower: src/client.c src/network.c src/worker_protocol.c src/model.c src/flower_logic.c src/gardener_logic.c src/request_logic.c src/logger.c src/constants.c src/soft_assert.c | build
	$(CC) $(CFLAGS) -DCLIENT_FLOWER $^ -o $@

build/gardener: src/client.c src/network.c src/worker_protocol.c src/model.c src/flower_logic.c src/gardener_logic.c src/request_logic.c src/logger.c src/constants.c src/soft_assert.c | build
	$(CC) $(CFLAGS) -DCLIENT_GARDENER $^ -o $@

build/observer: src/client.c src/network.c | build
	$(CC) $(CFLAGS) -DCLIENT_OBSERVER $^ -o $@

check: all
	python3 tests/check_tcp.py

run: all
	bash scripts/run_demo.sh tests/balanced.cfg

report:
	cd report && pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null && pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null

clean:
	rm -rf build simulation.log

.PHONY: all check run report clean
