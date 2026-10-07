CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude

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
	$(CC) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

test1: $(TARGET)
	./$(TARGET) tests/test1.cfg

test2: $(TARGET)
	./$(TARGET) tests/test2.cfg

test3: $(TARGET)
	./$(TARGET) tests/test3.cfg

docs:
	$(DOXYGEN) $(DOXYFILE)

report:
	cd $(REPORT_DIR) && $(LATEX) report.tex && $(LATEX) report.tex

clean-docs:
	rm -rf $(DOCS_DIR)

clean-report:
	rm -f $(REPORT_DIR)/*.aux $(REPORT_DIR)/*.log $(REPORT_DIR)/*.out \
	      $(REPORT_DIR)/*.toc $(REPORT_PDF)

clean: clean-docs clean-report
	rm -rf $(BUILD_DIR)
	rm -f simulation.log

-include $(DEPS)

.PHONY: all run test1 test2 test3 docs report clean-docs clean-report clean
