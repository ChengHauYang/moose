# ADComputeExtraStressConstant

`ADComputeExtraStressConstant` computes the extra stress described by
[ComputeExtraStressConstant](ComputeExtraStressConstant.md) and preserves derivatives of its material
property prefactor.

Pass the generated property to
[!param](/Materials/ADComputeLinearElasticStress/ad_extra_stress_names) to include those derivatives in
the elastic stress.

!syntax parameters /Materials/ADComputeExtraStressConstant

!syntax inputs /Materials/ADComputeExtraStressConstant

!syntax children /Materials/ADComputeExtraStressConstant
