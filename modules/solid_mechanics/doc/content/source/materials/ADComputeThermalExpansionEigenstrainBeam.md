# ADComputeThermalExpansionEigenstrainBeam

!syntax description /Materials/ADComputeThermalExpansionEigenstrainBeam

## Description

`ADComputeThermalExpansionEigenstrainBeam` is the automatic-differentiation counterpart of
[ComputeThermalExpansionEigenstrainBeam](/ComputeThermalExpansionEigenstrainBeam.md). It
computes a beam thermal-expansion eigenstrain from a temperature field using a constant thermal
expansion coefficient. Because the temperature is read as a generic AD variable value, its
derivative propagates through [ADComputeIncrementalBeamStrain](/ADComputeIncrementalBeamStrain.md)
and [ADComputeBeamResultants](/ADComputeBeamResultants.md) into the residual assembled by
[ADStressDivergenceBeam](/ADStressDivergenceBeam.md).

!syntax parameters /Materials/ADComputeThermalExpansionEigenstrainBeam

!syntax inputs /Materials/ADComputeThermalExpansionEigenstrainBeam

!syntax children /Materials/ADComputeThermalExpansionEigenstrainBeam
