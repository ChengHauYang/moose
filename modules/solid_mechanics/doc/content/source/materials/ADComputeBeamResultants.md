# ADComputeBeamResultants

!syntax description /Materials/ADComputeBeamResultants

## Description

`ADComputeBeamResultants` is the automatic-differentiation counterpart of
[ComputeBeamResultants](/ComputeBeamResultants.md). It computes the forces and moments at time
$t$ in the global coordinate system from the displacement and rotational strain increments and
the constitutive relation. By consuming generic AD material properties from
[ADComputeIncrementalBeamStrain](/ADComputeIncrementalBeamStrain.md) and declaring generic AD
force and moment properties, it carries complete derivative information — including derivatives
with respect to temperature and prescribed eigenstrain variables — through to
[ADStressDivergenceBeam](/ADStressDivergenceBeam.md).

For the mathematical formulation and beam kinematics refer to
[C0 Timoshenko Beam](/C0TimoshenkoBeam.md).

!syntax parameters /Materials/ADComputeBeamResultants

!syntax inputs /Materials/ADComputeBeamResultants

!syntax children /Materials/ADComputeBeamResultants
