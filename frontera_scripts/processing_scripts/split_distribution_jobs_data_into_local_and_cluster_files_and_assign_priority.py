import csv
import subprocess
import threading
import time
import argparse
import math

deployments_per_compute_node = 50
total_clusters = 4

existing_local_files = []


def parse_job_file(filename):

    # create files for use cases where it may be empty
    for cluster_id in range(1, total_clusters + 1):
        for local_id in range(1, deployments_per_compute_node + 1):
            target_file_name = (
                filename.replace(".csv", "")
                + "_c"
                + str(cluster_id)
                + "_l"
                + str(local_id)
                + ".csv"
            )
            open(target_file_name, "w")

        priorities_file_name = "cluster_jobs_config_c" + str(cluster_id) + "_file"
        open(priorities_file_name, "w")

    open("global_jobs_config_file", "w")

    jobs = []

    with open(filename, "r") as file:
        reader = csv.DictReader(file, delimiter=";")
        for row in reader:
            cluster_id = int(row["Cluster Id"]) + 1
            job_locations = {}
            job_ports = {}
            compute_nodes_positions = [
                int(n.strip()) for n in row["Compute Nodes"].strip("{}").split(",")
            ]
            for compute_node_position in compute_nodes_positions:
                physical_node_index = (
                    math.floor(compute_node_position / deployments_per_compute_node) + 1
                )
                physical_node_port = compute_node_position % deployments_per_compute_node + 1

                if physical_node_index not in job_locations:
                    job_locations[physical_node_index] = []
                    job_ports[physical_node_index] = []

                job_locations[physical_node_index].append(compute_node_position)
                job_ports[physical_node_index].append(physical_node_port)

            for physical_node_index, compute_nodes_position in job_locations.items():
                physical_node_port = job_ports[physical_node_index]

                target_file_name = (
                    filename.replace(".csv", "")
                    + "_c"
                    + str(cluster_id)
                    + "_l"
                    + str(physical_node_index)
                    + ".csv"
                )
                target_file = open(target_file_name, "a")
                writer = csv.writer(target_file, delimiter=";")

                file_number = str(cluster_id) + "_" + str(physical_node_index)
                # Create file if it does not exist already
                if file_number not in existing_local_files:

                    writer.writerow(
                        [
                            "Job Id",
                            "Start Time",
                            "End Time",
                            "Cluster Id",
                            "Compute Nodes",
                            "Ports",
                            "Locations",
                        ]
                    )
                    existing_local_files.append(file_number)

                writer.writerow(
                    [
                        row["Job Id"],
                        row["Start Time"],
                        row["End Time"],
                        row["Cluster Id"],
                        compute_nodes_position,
                        physical_node_port,
                        row["Locations"],
                    ]
                )

                if row["Locations"].strip() == "global":
                    priorities_file_name = "global_jobs_config_file"
                    file_number = "g"
                else:
                    priorities_file_name = "cluster_jobs_config_c" + str(cluster_id) + "_file"
                    file_number = "c" + str(cluster_id)

                priorities_file = open(priorities_file_name, "a")

                # Create file if it does not exist already
                if file_number not in existing_local_files:

                    priorities_file.write("total_jobs: 0\n")
                    priorities_file.write("jobs:\n")

                    existing_local_files.append(file_number)

                for index, compute_node_position in enumerate(compute_nodes_position):
                    job_full_name = (
                        row["Job Id"] + "+" + row["Cluster Id"] + "+" + str(compute_node_position)
                    )
                    priorities_file.write("    - name: " + job_full_name + "\n")
                    priorities_file.write("      priority: 1\n")
    return jobs


def main(filename, verbose):
    if filename is None:
        raise ValueError("Filename must be provided")
    jobs = parse_job_file(filename)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run time_tester jobs from a job file.")
    parser.add_argument("--filename", help="Path to the job file")
    # parser.add_argument('--start_index', type=int, default=4, help='Start index for job execution (default: 4). Needed for ibrun execution to deploy in the compute node intended')
    parser.add_argument(
        "--deployments_per_compute_node",
        type=int,
        default=50,
        help="Number of deployments per compute node (default: 50)",
    )
    parser.add_argument("--verbose", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    deployments_per_compute_node = args.deployments_per_compute_node

    main(args.filename, args.verbose)
