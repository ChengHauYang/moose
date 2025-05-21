#!/bin/bash
for Ny in 32; do
    Nx=$(awk "BEGIN {print $Ny * 1.5}")
    output_dir="MPI_NewIC_first_layer_Ny${Ny}"  # Define a unique output folder based on Ny
    mkdir -p "$output_dir"

    # Copy input file into output folder
    cp esm-debug-faster-newIC-first-layer.i "$output_dir/"

    # Enter the folder and run from there
    pushd "$output_dir" > /dev/null

    mpirun -np 8 ../../../../solid_mechanics-opt \
        -i esm-debug-faster-newIC-first-layer.i --no-color \
        Outputs/file_base="results" \
        nx="$Nx" \
        ny="$Ny" > log_extrapolation_first_layer.txt 2>&1

    popd > /dev/null
done
