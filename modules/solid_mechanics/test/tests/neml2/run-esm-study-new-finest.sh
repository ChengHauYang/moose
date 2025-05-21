#!/bin/bash
#for Ny in 8 16 32; do
for Ny in 512; do
    Nx=$(awk "BEGIN {print $Ny * 1.5}")
    number_elements=$(awk "BEGIN {print $Ny/8}")
    beginning_activation_of_row=$(awk "BEGIN {print $Ny/8}")

    output_dir="new_output_Ny${Ny}"  # Define a unique output folder based on Ny
    mkdir -p $output_dir          # Ensure the output directory exists

    mpirun -np 12 ../../../solid_mechanics-opt \
        -i esm-new.i  --no-color \
        Outputs/file_base=$output_dir/results \
        nx=$Nx \
        ny=$Ny \
        number_elements=$number_elements \
        beginning_activation_of_row=$beginning_activation_of_row > log_new_${Ny} 2>&1
done
