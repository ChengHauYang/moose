# ComputeDeformationGradient

!syntax description /Materials/ComputeDeformationGradient

`ComputeDeformationGradient` computes the total deformation gradient
$\mathbf{F} = \mathbf{I} + \nabla \mathbf{u}$ in Cartesian coordinates and stores it under
[!param](/Materials/ComputeDeformationGradient/deformation_gradient).

With [!param](/Materials/ComputeDeformationGradient/stabilize_strain) set to `true`, which requires
three-dimensional displacements, the physical deformation gradient
$\mathbf{U} = \mathbf{I} + \nabla \mathbf{u}$ is stored under
[!param](/Materials/ComputeDeformationGradient/physical_deformation_gradient), its average
$\mathbf{M}$ over the reference volume of the element is stored under
[!param](/Materials/ComputeDeformationGradient/average_deformation_gradient), and the total-mode
F-bar deformation gradient

!equation
\mathbf{B} = \left(\frac{\det\mathbf{M}}{\det\mathbf{U}}\right)^{1/3}\mathbf{U}

is stored under [!param](/Materials/ComputeDeformationGradient/deformation_gradient). $\mathbf{M}$
is integrated with a separate volume finite element reinitialized on the element that owns the
evaluation point, so it is the element-volume average also when the material is evaluated on an
element or neighbor face. It is read from the current solution, so it is available on ghosted
neighbor elements. The volume quadrature rule of the current assembly context is used, so the
elements on both sides of an interface must share one volume quadrature rule.

These properties provide the F-bar kinematics for the
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) directional
correction. [KokkosComputeDeformationGradient](KokkosComputeDeformationGradient.md) is the Kokkos
counterpart.

## Example Input File Syntax

In this example, the material supplies $\mathbf{U}$ and $\mathbf{B}$ on both sides of a shifted
cohesive-zone interface to the NEML2 model that computes the PK1 stress for the directional
correction. The `fbar` input variable switches the F-bar correction on and off.

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_tl_fbar_host.i block=Materials/interface_deformation_gradient

!syntax parameters /Materials/ComputeDeformationGradient

!syntax inputs /Materials/ComputeDeformationGradient

!syntax children /Materials/ComputeDeformationGradient
