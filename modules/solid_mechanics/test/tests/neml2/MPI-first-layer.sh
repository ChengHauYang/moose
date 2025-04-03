#!/bin/bash

# List of processor counts to try
for nproc in 1 2 4 8 16; do
  # Define an output directory for each processor count
  outdir="output_${nproc}proc"
  
  # Create the output directory
  mkdir -p "$outdir"

  # Run the simulation with the given number of processors
  mpirun -np $nproc ../../../solid_mechanics-opt -i first-layer.i Outputs/file_base=$outdir/solution
done

