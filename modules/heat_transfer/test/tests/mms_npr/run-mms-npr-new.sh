#!/bin/bash
EXE="../../../*opt"
# INPUT="npr_mms_same_threshold.i"
INPUT="npr_mms.i"

NX_VALUES=(10 20 40 80 160 320)
ELEMENTS=("QUAD4" "QUAD9")
ORDERS=("FIRST" "SECOND")
ORDER_NUMBERS=("1" "2")

echo "# nx elem order order_number l2_error_approx l2_error_exact" \
     > convergence_results.txt

for i in "${!ELEMENTS[@]}"; do
  elem=${ELEMENTS[$i]}
  order=${ORDERS[$i]}
  order_number=${ORDER_NUMBERS[$i]}

  for nx in "${NX_VALUES[@]}"; do
    echo "Running nx=$nx elem=$elem order=$order order_number=$order_number"
    output=$($EXE -i $INPUT nx=$nx elem=$elem order=$order order_number=$order_number)

    read l2_approx l2_exact <<< "$(
      printf '%s\n' "$output" | awk '
        /Postprocessor Values:/ {block = ""; keep = 1; next}
        keep && /^\s*\+/      {next}
        keep && /^\s*\|/      {block = block $0 "\n"; next}
        keep && !/^\s*(\+|\|)/ {keep = 0}

        END{
          n = split(block, L, "\n")
          for (i=n; i>=1; --i)
            if (L[i] ~ /^\s*\|/) {line=L[i]; break}

          sub(/^\s*\|[^|]*\|/, "", line)

          split(line, f, "|")          # f[1]≈, f[2]=exact
          gsub(/[[:space:]]+/, "", f[1])
          gsub(/[[:space:]]+/, "", f[2])
          print (f[1]==""?"NaN":f[1]), (f[2]==""?"NaN":f[2])
        }'
    )"

    echo "$nx $elem $order $order_number $l2_approx $l2_exact" \
         >> convergence_results.txt
  done
done
