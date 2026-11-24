# ADComputeEigenstrainBeamFromVariable

!syntax description /Materials/ADComputeEigenstrainBeamFromVariable

## Description

`ADComputeEigenstrainBeamFromVariable` is the automatic-differentiation counterpart of
[ComputeEigenstrainBeamFromVariable](/ComputeEigenstrainBeamFromVariable.md). It reads up to
three displacement and three rotational eigenstrain variables and stores their values in generic
AD beam eigenstrain material properties. Because the variables are read as AD quantities, their
derivatives propagate through [ADComputeIncrementalBeamStrain](/ADComputeIncrementalBeamStrain.md)
and [ADComputeBeamResultants](/ADComputeBeamResultants.md) into the residual assembled by
[ADStressDivergenceBeam](/ADStressDivergenceBeam.md).

!syntax parameters /Materials/ADComputeEigenstrainBeamFromVariable

!syntax inputs /Materials/ADComputeEigenstrainBeamFromVariable

!syntax children /Materials/ADComputeEigenstrainBeamFromVariable
