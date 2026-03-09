# OMPCheck

**OMPCheck** is a command-line tool for analyzing the scalability of parallel programs.  
It automatically runs a target program with multiple thread counts, records runtime data, and computes scaling metrics such as **speedup**, **efficiency**, and **estimated serial fraction**.

The goal of this project is to help developers quickly determine whether their parallel programs are **scaling properly** and identify potential **performance bottlenecks**.


## Features

- **Automatic multi-thread benchmarking**
- Runs multiple trials per thread count for stable measurements
- Computes important scaling metrics:
  - **Average runtime**
  - **Speedup**
  - **Parallel efficiency**
  - **Serial fraction (Amdahl's Law estimate)**
- Generates a **scaling diagnosis report**
- **CSV export** of benchmark results
- **plot generation** using Python


## Requirements

- **C++17 compatible compiler**
- **Python 3**
- Python package:
  - `matplotlib`

Install matplotlib if needed:

```bash
pip install matplotlib

## Build

Compile the program using the provided **Makefile**:

    make

Clean the build files:

    make clean


## Usage

Basic usage:

    ./ompcheck --threads 1,2,4,8 -- ./target_program

Example with additional options:

    ./ompcheck --threads 1,2,4,8 --runs 5 --csv results.csv --plot -- ./target_program


## Command Line Options

| Option | Description |
|------|------|
| --threads | Comma-separated list of thread counts |
| --runs | Number of runs per thread configuration (default: 5) |
| --csv | Output benchmark data to CSV |
| --plot | Generate plots from the CSV results |
| --report | Output scaling diagnosis report |
