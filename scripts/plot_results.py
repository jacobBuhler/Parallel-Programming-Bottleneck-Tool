import sys
import csv
from collections import defaultdict
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) != 4:
        print("Usage: python3 plot_results.py <input.csv> <output.png> <title>")
        sys.exit(1)

    input_csv = sys.argv[1] #read command line args
    output_png = sys.argv[2]
    title = sys.argv[3]

    times = defaultdict(list)#dict that maps each thread count to a list of run times

    with open(input_csv, "r") as f:
        reader = csv.reader(line for line in f if not line.startswith("#"))
        header = next(reader, None)
        if header != ["threads", "trial", "time"]: #validate csv has exp columns
            print("Unexpected CSV header:", header)
            sys.exit(1)

        for row in reader:#read
            threads = int(row[0])
            elapsed = float(row[2])
            times[threads].append(elapsed)

    thread_counts = sorted(times.keys())
    avg_times = []
    speedups = []
    efficiencies = []

    baseline = sum(times[thread_counts[0]]) / len(times[thread_counts[0]])#avg time of smallest thread count as baseline

    for t in thread_counts:#compute speedup avg runtime & efficiency for each thread count
        avg = sum(times[t]) / len(times[t])
        speedup = baseline / avg
        efficiency = speedup / t

        avg_times.append(avg)
        speedups.append(speedup)
        efficiencies.append(efficiency)
    
    #runtime vs threads
    plt.figure(figsize=(8, 5))
    plt.plot(thread_counts, avg_times, marker="o", label="Average Time")
    plt.xlabel("Threads")
    plt.ylabel("Time (seconds)")
    plt.title(f"{title} - Runtime")
    plt.grid(True)
    plt.savefig(output_png.replace(".png", "_runtime.png"), bbox_inches="tight")
    plt.close()

    #speedup vs threads
    plt.figure(figsize=(8, 5))
    plt.plot(thread_counts, speedups, marker="o", label="Measured Speedup")
    plt.plot(thread_counts, thread_counts, marker="x", linestyle="--", label="Ideal Speedup")
    plt.xlabel("Threads")
    plt.ylabel("Speedup")
    plt.title(f"{title} - Speedup")
    plt.grid(True)
    plt.legend()
    plt.savefig(output_png.replace(".png", "_speedup.png"), bbox_inches="tight")
    plt.close()

    #efficiency vs threads
    plt.figure(figsize=(8, 5))
    plt.plot(thread_counts, efficiencies, marker="o", label="Efficiency")
    plt.xlabel("Threads")
    plt.ylabel("Efficiency")
    plt.title(f"{title} - Efficiency")
    plt.grid(True)
    plt.savefig(output_png.replace(".png", "_efficiency.png"), bbox_inches="tight")
    plt.close()

    print("Saved plots based on", input_csv)

if __name__ == "__main__":
    main()
