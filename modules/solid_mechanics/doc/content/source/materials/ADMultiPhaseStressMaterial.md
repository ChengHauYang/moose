# ADMultiPhaseStressMaterial

`ADMultiPhaseStressMaterial` computes the switching-function-weighted mixture of phase stresses
described by [MultiPhaseStressMaterial](MultiPhaseStressMaterial.md). Supply AD switching functions
through [!param](/Materials/ADMultiPhaseStressMaterial/h) and AD phase stresses identified by
[!param](/Materials/ADMultiPhaseStressMaterial/phase_base). The AD mixture does not require the
non-AD phase tangent properties.

In this example, two distinct elastic phases are blended using nonlinear-variable-dependent weights.
The mixed stress feeds [ADStressDivergenceTensors](ADStressDivergenceTensors.md), preserving both
displacement and switching-function couplings.

!listing modules/solid_mechanics/test/tests/ad_migration/multiphase_stress.i block=Materials

!syntax parameters /Materials/ADMultiPhaseStressMaterial

!syntax inputs /Materials/ADMultiPhaseStressMaterial

!syntax children /Materials/ADMultiPhaseStressMaterial
