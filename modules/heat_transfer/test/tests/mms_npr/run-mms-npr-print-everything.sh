#!/bin/bash

# Path to your executable and input file
EXEC_PATH="../../../*opt"
INPUT_FILE="npr_mms_whole.i"

# Fixed parameters
ELEM="QUAD4"
ORDER="FIRST"
ORDER_NUMBER=1

# List of nx values to sweep through
for NX in 10 20 40 80 160 320
do
  echo "Running simulation with nx=$NX"
  $EXEC_PATH -i $INPUT_FILE elem=$ELEM order=$ORDER order_number=$ORDER_NUMBER nx=$NX
done

