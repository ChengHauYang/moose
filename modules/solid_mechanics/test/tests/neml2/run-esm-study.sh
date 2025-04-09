#!/bin/bash
for Ny in 16 32 64 128 256 512; do
    Nx=$(awk "BEGIN {print $Ny * 1.5}")
    number_elements=$(awk "BEGIN {print $Ny / 32}")

    output_dir="output_Ny${Ny}"  # Define a unique output folder based on Ny
    mkdir -p $output_dir          # Ensure the output directory exists

    mpirun -n 8 ../../../solid_mechanics-opt \
        -i esm.i \
        Outputs/file_base=$output_dir/results \
        nx=$Nx \
        ny=$Ny \
        number_elements=$number_elements
done
