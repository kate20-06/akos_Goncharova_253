CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -pthread -Iinclude

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/smart_flowers

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

DOXYGEN := doxygen
DOXYFILE := Doxyfile
DOCS_DIR := docs
LATEX := pdflatex
REPORT_DIR := report
REPORT_PDF := $(REPORT_DIR)/report.pdf

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(OBJECTS) -pthread -o $@

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

check: $(TARGET)
	python3 tests/check_parallel.py

docs:
	$(DOXYGEN) $(DOXYFILE)

report:
	cd $(REPORT_DIR) && $(LATEX) -interaction=nonstopmode -halt-on-error report.tex && $(LATEX) -interaction=nonstopmode -halt-on-error report.tex

clean-docs:
	rm -rf $(DOCS_DIR)

clean-report:
	rm -f $(REPORT_DIR)/*.aux $(REPORT_DIR)/*.log $(REPORT_DIR)/*.out \
	      $(REPORT_DIR)/*.toc $(REPORT_DIR)/*.fls $(REPORT_DIR)/*.fdb_latexmk

clean: clean-docs
	rm -rf $(BUILD_DIR)
	rm -f simulation.log

-include $(DEPS)

.PHONY: all run check docs report clean-docs clean-report clean
