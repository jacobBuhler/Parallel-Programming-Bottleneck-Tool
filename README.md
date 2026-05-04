# ParaCheck

A scaling-bottleneck diagnostic tool for parallel programs. ParaCheck benchmarks a target program across multiple thread or process counts, computes scaling metrics, and produces a plain-language diagnosis of *why* the program isn't scaling.

Supports **OpenMP**, **MPI**, and **pthreads** through a single C++17 driver. Includes a Django web interface for users who'd rather not use a terminal


## What it does

Point ParaCheck at any parallel executable and it will:

1. Sweep the program across a list of thread or process counts.
2. Run multiple trials per count with monotonic-clock timing.
3. Capture per-run resource usage (CPU time, RSS, context switches).
4. Compute speedup, efficiency, Karp–Flatt serial fraction, and parallel utilization.
5. Project performance at higher thread counts using Amdahl's Law.
6. Generate six PNG plots (three measured, three projected).
7. Produce a diagnosis report classifying the run as healthy, Amdahl-bound, sync-overhead-bound, false-sharing-bound, or SMT-saturated.


## Requirements

- Linux or WSL2
- `g++` (C++17)
- Python 3 with `matplotlib`
- Optional: `mpicc` / `mpirun` for MPI workloads
- For the web app: Django 5


## Quick start — CLI

```bash
pip install matplotlib
make

./bin/paracheck --threads 1,2,4,8 --runs 5 \
    --csv output/results.csv --plot \
    --report output/report.txt \
    --paradigm openmp \
    -- ./bin/good_omp 80000000
```

Results land in `output/`: a CSV, a diagnosis report, and six PNG plots.


## Quick start — Web app

```bash
cd webapp
python -m venv ../venv
source ../venv/bin/activate
pip install django

python manage.py migrate
python manage.py runserver
```

Open `http://localhost:8000/`. Sign up, click **New Analysis**, upload a `.c` or `.cpp` source file, pick a paradigm, and submit. ParaCheck compiles, runs, and shows you the report and plots when it's done. Past runs live on the **My Runs** page.


## Command-line options

| Flag | Description |
|---|---|
| `--threads` | Comma-separated thread/process counts (e.g. `1,2,4,8`). Required. |
| `--runs N` | Trials per count (default: 5). |
| `--paradigm P` | `openmp`, `mpi`, or `pthreads` (default: `openmp`). |
| `--csv FILE` | Write per-trial CSV. |
| `--plot` | Generate plots from the CSV. Requires `--csv`. |
| `--report FILE` | Write the diagnosis report. |
| `--` | End of ParaCheck flags. Everything after is the target program and its args. |


## Test programs

The `tests/` directory contains eight reference workloads — each one designed to trigger a different branch of the diagnosis classifier:

| File | Paradigm | Demonstrates |
|---|---|---|
| `good_omp.c` | OpenMP | Healthy CPU-bound scaling |
| `bad_critical_omp.c` | OpenMP | `critical` lock contention |
| `amdahl_omp.c` | OpenMP | Fixed 30% serial section |
| `false_sharing_omp.c` | OpenMP | Cache-line bouncing |
| `good_mpi.c` | MPI | Healthy distributed reduction |
| `chatty_mpi.c` | MPI | Per-iteration `MPI_Allreduce` overhead |
| `good_pthreads.c` | pthreads | Healthy independent threads |
| `mutex_pthreads.c` | pthreads | Global mutex contention |

Upload any of them through the web UI, or compile manually and run via the CLI.


## Project structure

```
Parallel-Programming-Bottleneck-Tool/
├── src/
│   ├── main.cpp           # CLI driver
│   ├── diagnosis.cpp      # scaling math & diagnosis rules
│   └── diagnosis.h
├── scripts/
│   └── plot_results.py    # plot generator
├── tests/                 # reference workloads
├── webapp/                # Django web interface
├── Makefile
└── README.md
```