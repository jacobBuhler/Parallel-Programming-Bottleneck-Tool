CXX = g++
CC  = gcc

CXXFLAGS = -O2 -std=c++17 -Wall -Wextra
CFLAGS   = -O2 -fopenmp -Wall -Wextra

BIN_DIR = bin
SRC_DIR = src
TST_DIR = tests

TARGET  = $(BIN_DIR)/ompcheck
GOOD    = $(BIN_DIR)/good_omp
BAD     = $(BIN_DIR)/bad_omp

all: $(TARGET) $(GOOD) $(BAD)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(SRC_DIR)/main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(GOOD): $(TST_DIR)/good_omp.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -lm -o $@

$(BAD): $(TST_DIR)/bad_omp.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -lm -o $@

run-good: all
	$(TARGET) --threads 1,2,4,8 -- $(GOOD) 80000000

run-bad: all
	$(TARGET) --threads 1,2,4,8 -- $(BAD) 20000000

run-good-avg: all
	$(TARGET) --threads 1,2,4,8 --runs 5 -- $(GOOD) 80000000

run-bad-avg: all
	$(TARGET) --threads 1,2,4,8 --runs 5 -- $(BAD) 20000000

run-good-csv: all
	$(TARGET) --threads 1,2,4,8 --runs 5 --csv good.csv -- $(GOOD) 80000000

run-bad-csv: all
	$(TARGET) --threads 1,2,4,8 --runs 5 --csv bad.csv -- $(BAD) 20000000

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean run-good run-bad run-good-avg run-bad-avg run-good-csv run-bad-csv
