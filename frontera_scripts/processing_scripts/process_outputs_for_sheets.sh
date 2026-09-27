#!/bin/bash

rm *.csv

for i in {1..4}; do grep -o 'collect_core;[0-9]*;[0-9]*;meta_op:[0-9]*;data_op:[0-9]*' cluster_output_app$i.out | sed 's/meta_op://; s/data_op://' >c$i.csv; done

grep -o 'collect_core;[0-9]*;[0-9]*;meta_op:[0-9]*;data_op:[0-9]*' global_output.out | sed 's/meta_op://; s/data_op://' >global.csv

awk -F';' 'BEGIN{OFS=";"} {$2=substr($2,1,length($2)-6); print}' c1.csv >tmp && mv tmp c1.csv
c1_top_time=$(awk -F';' 'NR==1 {print $2}' c1.csv)
input="c1.csv"

awk -F';' '
NR == 1 {
    prev = $3
    print
    next
}
{
    for (i = prev + 1; i < $3; i++) {
        printf "collect_core;0;%d;0;0\n", i
    }
    print
    prev = $3
}
' "$input" >c1_filled.csv

awk -F';' 'BEGIN{OFS=";"} {$2=substr($2,1,length($2)-6); print}' c2.csv >tmp && mv tmp c2.csv
c2_top_time=$(awk -F';' 'NR==1 {print $2}' c2.csv)
input="c2.csv"

awk -F';' '
NR == 1 {
    prev = $3
    print
    next
}
{
    for (i = prev + 1; i < $3; i++) {
        printf "collect_core;0;%d;0;0\n", i
    }
    print
    prev = $3
}
' "$input" >c2_filled.csv

awk -F';' 'BEGIN{OFS=";"} {$2=substr($2,1,length($2)-6); print}' c3.csv >tmp && mv tmp c3.csv
c3_top_time=$(awk -F';' 'NR==1 {print $2}' c3.csv)
input="c3.csv"

awk -F';' '
NR == 1 {
    prev = $3
    print
    next
}
{
    for (i = prev + 1; i < $3; i++) {
        printf "collect_core;0;%d;0;0\n", i
    }
    print
    prev = $3
}
' "$input" >c3_filled.csv

awk -F';' 'BEGIN{OFS=";"} {$2=substr($2,1,length($2)-6); print}' c4.csv >tmp && mv tmp c4.csv
c4_top_time=$(awk -F';' 'NR==1 {print $2}' c4.csv)
input="c4.csv"

awk -F';' '
NR == 1 {
    prev = $3
    print
    next
}
{
    for (i = prev + 1; i < $3; i++) {
        printf "collect_core;0;%d;0;0\n", i
    }
    print
    prev = $3
}
' "$input" >c4_filled.csv

awk -F';' 'BEGIN{OFS=";"} {$2=substr($2,1,length($2)-6); print}' global.csv >tmp && mv tmp global.csv
global_top_time=$(awk -F';' 'NR==1 {print $2}' global.csv)
input="global.csv"

awk -F';' '
NR == 1 {
    prev = $3
    print
    next
}
{
    for (i = prev + 1; i < $3; i++) {
        printf "collect_core;0;%d;0;0\n", i
    }
    print
    prev = $3
}
' "$input" >global_filled.csv

max=$(printf "%s\n" "$c1_top_time" "$c2_top_time" "$c3_top_time" "$c4_top_time" "$global_top_time" | sort -n | tail -1)

awk -F';' -v max="$max" '$2 >= max' c1_filled.csv >tmp && mv tmp c1_filled.csv
awk -F';' -v max="$max" '$2 >= max' c2_filled.csv >tmp && mv tmp c2_filled.csv
awk -F';' -v max="$max" '$2 >= max' c3_filled.csv >tmp && mv tmp c3_filled.csv
awk -F';' -v max="$max" '$2 >= max' c4_filled.csv >tmp && mv tmp c4_filled.csv
awk -F';' -v max="$max" '$2 >= max' global_filled.csv >tmp && mv tmp global_filled.csv

sed -i 's/$/;/' c1_filled.csv
sed -i 's/$/;/' c2_filled.csv
sed -i 's/$/;/' c3_filled.csv
sed -i 's/$/;/' c4_filled.csv
sed -i 's/$/;/' global_filled.csv

paste -d';' global_filled.csv c1_filled.csv c2_filled.csv c3_filled.csv c4_filled.csv >merged.csv
