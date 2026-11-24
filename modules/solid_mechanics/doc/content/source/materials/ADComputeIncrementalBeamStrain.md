# ADComputeIncrementalBeamStrain

!syntax description /Materials/ADComputeIncrementalBeamStrain

## Description

`ADComputeIncrementalBeamStrain` is the automatic-differentiation counterpart of
[ComputeIncrementalBeamStrain](/ComputeIncrementalBeamStrain.md). It computes the small (and
optionally finite) displacement and rotational strain increments for beam elements. Nodal
displacement and rotation increments are read with `adCoupledDofValues()` so that derivatives
with respect to the displacement and rotation degrees of freedom are retained. Generic AD
material properties are declared for the mechanical strain increments so that downstream
materials such as [ADComputeBeamResultants](/ADComputeBeamResultants.md) carry complete
derivative information into the residual.

Eigenstrains are subtracted from the total strain using generic AD material properties, so
derivatives with respect to eigenstrain-driving variables (temperature, prescribed eigenstrain
variables) propagate automatically.

For the mathematical formulation and beam kinematics refer to
[C0 Timoshenko Beam](/C0TimoshenkoBeam.md).

!syntax parameters /Materials/ADComputeIncrementalBeamStrain

!syntax inputs /Materials/ADComputeIncrementalBeamStrain

!syntax children /Materials/ADComputeIncrementalBeamStrain
