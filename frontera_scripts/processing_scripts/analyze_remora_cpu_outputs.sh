#!/bin/bash

echo "All Collect Latency:"
grep -nr "All Collect Latency:" $1/local_output_app* | sed 's/.* Latency: \([0-9]\{1,\}\) \[µs\]/\1/g' | paste -s -d+ - | bc
echo "Average Collect Latency:"
grep -nr "All Collect Latency:" $1/local_output_app* | sed 's/.* Latency: \([0-9]\{1,\}\) \[µs\]/\1/g' | awk '{for (i=1;i<=NF;++i) {sum+=$i; ++n}} END {printf "%d\n", sum/n}'

echo "All Enforce Latency:"
grep -nr "All Enforce Latency:" $1/local_output_app* | sed 's/.* Latency: \([0-9]\{1,\}\) \[µs\]/\1/g' | paste -s -d+ - | bc
echo "Average Enforce Lantency:"
grep -nr "All Enforce Latency" $1/local_output_app* | sed 's/.* Latency: \([0-9]\{1,\}\) \[µs\]/\1/g' | awk '{for (i=1;i<=NF;++i) {sum+=$i; ++n}} END {printf "%d\n", sum/n}'
