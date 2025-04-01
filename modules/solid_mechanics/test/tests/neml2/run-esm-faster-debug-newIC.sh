#    mpirun -n 8
### /Users/chenghau.yang/Documents/Package/moose-chenghau/modules/solid_mechanics/solid_mechanics-opt -i esm.i ##--no-color ##--dump > log_dump

export DYLD_LIBRARY_PATH=/Users/chenghau.yang/Documents/Package/moose-chenghau/framework:$DYLD_LIBRARY_PATH

####export DYLD_LIBRARY_PATH=/Users/chenghau.yang/Documents/Package/moose-chenghau/modules/solid_mechanics/test/lib:/Users/chenghau.yang/Documents/Package/moose-chenghau/framework:$DYLD_LIBRARY_PATH

#!/bin/bash
for Ny in 32; do
    Nx=$(awk "BEGIN {print $Ny * 1.5}")
    ####number_elements=$(awk "BEGIN {print $Ny / 32}")

    output_dir="NewIC_Debug_output_Ny${Ny}"  # Define a unique output folder based on Ny
    mkdir -p $output_dir          # Ensure the output directory exists

    #mpirun -n 1 
       /Users/chenghau.yang/Documents/Package/moose-chenghau/modules/solid_mechanics/solid_mechanics-opt \
        -i esm-debug-faster.i --no-color \
        Outputs/file_base=$output_dir/results \
        nx=$Nx \
        ny=$Ny 
done
