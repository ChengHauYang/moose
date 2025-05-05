COUNT=1
for RES in 8 16 32 64 128; do
  FORMATTED_COUNT=$(printf "%05d" $COUNT)
  ~/Documents/Packages/moose/examples/ex02_kernel/ex02-opt -i 3D_lambda1.i N=$RES Outputs/file_base=removeElement-meshOnly-3D_N_$FORMATTED_COUNT
  ((COUNT++))  # Increment the counter
done
