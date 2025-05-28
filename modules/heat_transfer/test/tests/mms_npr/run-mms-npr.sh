#!/bin/bash

EXE="../../../*opt"
INPUT="npr_mms.i"
NX_VALUES=(10 20 40 80 160 320)
ELEMENTS=("QUAD4" "QUAD9")
ORDERS=("FIRST" "SECOND")

echo "# nx elem order l2_error" > convergence_results.txt

for i in "${!ELEMENTS[@]}"; do
  elem=${ELEMENTS[$i]}
  order=${ORDERS[$i]}
  for nx in "${NX_VALUES[@]}"; do
    echo "Running nx=$nx elem=$elem order=$order"
    output=$($EXE -i $INPUT nx=$nx elem=$elem order=$order)

    l2_error=$(echo "$output" | awk '
      BEGIN { in_block=0; last_line="" }
      /Postprocessor Values:/ { in_block=1; count=0; next }
      in_block && /^\+/ { count++; if (count > 1) last_line = prev; next }
      in_block { prev=$0 }
      END { split(last_line, a, "|"); gsub(/ /, "", a[3]); print a[3] }
    ')

    echo "$nx $elem $order $l2_error" >> convergence_results.txt
  done
done
