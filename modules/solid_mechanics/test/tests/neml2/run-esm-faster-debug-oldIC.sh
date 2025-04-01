#!/bin/bash
for Ny in 32; do
    Nx=$(awk "BEGIN {print $Ny * 1.5}")

    output_dir="OldIC_Ny${Ny}"  # Define a unique output folder based on Ny
    mkdir -p "$output_dir"      # Ensure the output directory exists

    # Copy the input file into the output folder
    cp esm-debug-faster-oldIC.i "$output_dir/"

    # Change into the folder and run the simulation
    pushd "$output_dir" > /dev/null

    ../../../../solid_mechanics-opt \
        -i esm-debug-faster-oldIC.i --no-color \
        Outputs/file_base="results" \
        nx="$Nx" \
        ny="$Ny" > log_default.txt 2>&1

    popd > /dev/null
done
