# KokkosComputeIsotropicLinearElasticStress

!if! function=hasCapability('kokkos')

`KokkosComputeIsotropicLinearElasticStress` computes the isotropic linear elastic small stress

\[
\sigma = \lambda \operatorname{tr}(\varepsilon) I + 2 \mu \varepsilon,
\qquad
\varepsilon = \frac{1}{2}\left(F + F^T\right) - I,
\]

and its strain derivative as full $3 \times 3$ and $3 \times 3 \times 3 \times 3$ Kokkos material
properties from a deformation gradient property, typically the output of
[KokkosComputeDeformationGradient](KokkosComputeDeformationGradient.md), which includes the F-bar
correction and the homogenization gradient. On a two-dimensional mesh the out-of-plane row of $F$ is
the identity, so the stress is the plane-strain stress. The outputs are inputs for
[KokkosComputeLagrangianStressCustomSmall](KokkosComputeLagrangianStressCustomSmall.md).

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/small_2d.i block=Materials/elastic_1

!syntax parameters /Materials/KokkosComputeIsotropicLinearElasticStress

!syntax inputs /Materials/KokkosComputeIsotropicLinearElasticStress

!syntax children /Materials/KokkosComputeIsotropicLinearElasticStress

!if-end!

!else
!include kokkos/kokkos_warning.md
