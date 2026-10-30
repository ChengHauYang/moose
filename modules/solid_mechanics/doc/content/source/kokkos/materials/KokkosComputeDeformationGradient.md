# KokkosComputeDeformationGradient

!syntax description /Materials/KokkosComputeDeformationGradient

`KokkosComputeDeformationGradient` computes the total deformation gradient
$\mathbf{F} = \mathbf{I} + \nabla \mathbf{u}$ from Kokkos displacement gradients and stores it
as a Kokkos full rank-two material property. The property can be supplied directly to NEML2 through
the Kokkos input backend.

With [!param](/Materials/KokkosComputeDeformationGradient/stabilize_strain) set to `true`, which
requires three-dimensional displacements, the physical deformation gradient
$\mathbf{U} = \mathbf{I} + \nabla \mathbf{u}$ is stored under
[!param](/Materials/KokkosComputeDeformationGradient/physical_deformation_gradient), its average
$\mathbf{M}$ over the reference volume of the element is stored under
[!param](/Materials/KokkosComputeDeformationGradient/average_deformation_gradient), and the
total-mode F-bar deformation gradient

!equation
\mathbf{B} = \left(\frac{\det\mathbf{M}}{\det\mathbf{U}}\right)^{1/3}\mathbf{U}

is stored under [!param](/Materials/KokkosComputeDeformationGradient/deformation_gradient).
$\mathbf{M}$ is always integrated with the volume quadrature of the element that owns the
evaluation point, also when the material is evaluated on an element or neighbor face, so these
properties provide the F-bar kinematics for the
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) directional
correction. [ComputeDeformationGradient](ComputeDeformationGradient.md) is the host counterpart.

## Example Input File Syntax

In this example, the material supplies $\mathbf{U}$ and $\mathbf{B}$ on both sides of a shifted
cohesive-zone interface to the NEML2 model that computes the PK1 stress for the directional
correction. The `fbar` input variable switches the F-bar correction on and off.

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_tl_fbar_kokkos.i block=Materials/interface_deformation_gradient

!syntax parameters /Materials/KokkosComputeDeformationGradient

!syntax inputs /Materials/KokkosComputeDeformationGradient

!syntax children /Materials/KokkosComputeDeformationGradient
