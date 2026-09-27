import csv
import subprocess
import threading
import time
import argparse
import math

per_cluster_connections = 2500
deployments_per_compute_node = 50
node_physical_node_index = 0


def parse_job_file(filename):
    jobs = []

    with open(filename, "r") as file:
        reader = csv.DictReader(file, delimiter=";")
        for row in reader:
            job = {
                "Job Id": row["Job Id"],
                "Start Time": int(row["Start Time"]),
                "End Time": int(row["End Time"]),
                "Cluster Id": int(row["Cluster Id"]),
                "Compute Nodes": [
                    int(n.strip()) for n in row["Compute Nodes"].strip("[]").split(",")
                ],
                "Ports": [int(n.strip()) for n in row["Ports"].strip("[]").split(",")],
                "Locations": row["Locations"].strip(),
            }
            jobs.append(job)

    return jobs


def run_job_at_time(job, verbose):
    start_delay = job["Start Time"]
    # Here ideally, should remove time spent since the beginning.
    time.sleep(start_delay)
    if verbose:
        print(f"Starting job {job['Job Id']} at time {start_delay}s")

    # Simulate job execution
    duration = job["End Time"] - job["Start Time"]

    for index, compute_node_position in enumerate(job["Compute Nodes"]):
        if verbose:
            print(f"Executing job {job['Job Id']} on compute_node_position {compute_node_position}")

        port = job["Ports"][index]
        trace_compute_node = ((port - 1) % 4) + 1
        cmd = [
            "./data_job_single_timed_padll.sh",
            str(job["Job Id"]) + "+" + str(job["Cluster Id"]) + "+" + str(compute_node_position),
            str(duration),
            str(port),
            str(trace_compute_node),
        ]
        # time.sleep(1/1000.0)
        subprocess.Popen(cmd)


def main(filename, main_thread_max_time, verbose):
    if filename is None:
        raise ValueError("Filename must be provided")
    jobs = parse_job_file(filename)

    if verbose:
        print(f"All jobs scheduled. Waiting up to {main_thread_max_time} seconds...")

    for job in jobs:
        job["End Time"] = min(job["End Time"], main_thread_max_time)
        threading.Thread(target=run_job_at_time, args=(job, verbose), daemon=True).start()

    time.sleep(main_thread_max_time)
    if verbose:
        print("Exiting main thread.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run time_tester jobs from a job file.")
    parser.add_argument("--filename", help="Path to the job file")
    parser.add_argument(
        "--time", type=int, default=30, help="Max time to wait for jobs (default: 30 seconds)"
    )
    # parser.add_argument('--start_index', type=int, default=4, help='Start index for job execution (default: 4). Needed for ibrun execution to deploy in the compute node intended')
    parser.add_argument(
        "--deployments_per_compute_node",
        type=int,
        default=50,
        help="Number of deployments per compute node (default: 50)",
    )
    parser.add_argument(
        "--per_cluster_connections",
        type=int,
        default=2500,
        help="Number of connections per cluster (default: 2500)",
    )
    parser.add_argument(
        "--node_physical_node_index",
        type=int,
        default=0,
        help="Index of physical compute node that deploys local controllers (default: 0)",
    )
    parser.add_argument("--verbose", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    deployments_per_compute_node = args.deployments_per_compute_node
    node_physical_node_index = args.node_physical_node_index

    main(args.filename, args.time, args.verbose)
