# ADComputeExtraStressVDWGas

`ADComputeExtraStressVDWGas` computes the van der Waals gas stress described by
[ComputeExtraStressVDWGas](ComputeExtraStressVDWGas.md), preserving derivatives of the gas
concentration and the `b`, `Va`, and `T` material properties.

Pass the generated property to
[!param](/Materials/ADComputeLinearElasticStress/ad_extra_stress_names) to include those derivatives in
the elastic stress.

!syntax parameters /Materials/ADComputeExtraStressVDWGas

!syntax inputs /Materials/ADComputeExtraStressVDWGas

!syntax children /Materials/ADComputeExtraStressVDWGas
