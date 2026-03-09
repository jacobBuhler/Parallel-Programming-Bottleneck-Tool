CXX = g++
CC  = gcc
PYTHON = python3

CXXFLAGS = -O2 -std=c++17 -Wall -Wextra
CFLAGS   = -O2 -fopenmp -Wall -Wextra

BIN_DIR = bin
SRC_DIR = src
TST_DIR = tests
OUT_DIR = output
PLT_DIR = plots

TARGET  = $(BIN_DIR)/ompcheck
GOOD    = $(BIN_DIR)/good_omp
BAD     = $(BIN_DIR)/bad_omp

GOOD_CSV = $(OUT_DIR)/good.csv
BAD_CSV  = $(OUT_DIR)/bad.csv

all: $(TARGET) $(GOOD) $(BAD)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(PLT_DIR):
	mkdir -p $(PLT_DIR)

$(TARGET): $(SRC_DIR)/main.cpp $(SRC_DIR)/diagnosis.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/main.cpp $(SRC_DIR)/diagnosis.cpp -o $@

$(GOOD): $(TST_DIR)/good_omp.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -lm -o $@

$(BAD): $(TST_DIR)/bad_omp.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -lm -o $@

run-good: all | $(OUT_DIR)
	$(TARGET) --threads 1,2,4,8 --runs 5 --csv $(GOOD_CSV) --plot --report $(OUT_DIR)/good_report.txt -- $(GOOD) 80000000

run-bad: all | $(OUT_DIR)
	$(TARGET) --threads 1,2,4,8 --runs 5 --csv $(BAD_CSV) --plot --report $(OUT_DIR)/bad_report.txt -- $(BAD) 20000000

clean:
	rm -rf $(BIN_DIR) $(OUT_DIR) $(PLT_DIR)

.PHONY: all clean run-good run-bad
