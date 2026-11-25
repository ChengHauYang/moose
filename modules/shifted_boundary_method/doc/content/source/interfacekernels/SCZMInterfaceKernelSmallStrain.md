# SCZMInterfaceKernelSmallStrain

!syntax description /InterfaceKernels/SCZMInterfaceKernelSmallStrain

The `SCZMInterfaceKernelSmallStrain` assembles cohesive traction on a surrogate interface for
small-strain problems. It evaluates the displacement jump at the true interface and can include a
directional correction based on the bulk stress and constitutive tangent. This object is normally
created by the [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md).

## Weighted-traction stabilization

Set [!param](/InterfaceKernels/SCZMInterfaceKernelSmallStrain/stabilization) to `weighted_traction` to use the host's legacy linear blend
$\widehat{\mathbf{t}} = \mathbf{S}\mathbf{t}_c + (\mathbf{I}-\mathbf{S})\mathbf{T}_h$, where
$\mathbf{t}_c$ is the cohesive traction at the shifted jump, $\mathbf{T}_h$ is the average
adjacent bulk Cauchy traction, and
$\mathbf{S} = \beta(\mathbf{K}_{\mathrm{ref}}+\beta\mathbf{I})^{-1}$.
Supply both [!param](/InterfaceKernels/SCZMInterfaceKernelSmallStrain/stabilization_beta) and
[!param](/InterfaceKernels/SCZMInterfaceKernelSmallStrain/stabilization_normal_stiffness). The optional
[!param](/InterfaceKernels/SCZMInterfaceKernelSmallStrain/stabilization_tangential_stiffness) defaults to the normal stiffness.
The reference stiffnesses define $\mathbf{K}_{\mathrm{ref}}$ in the interface frame.

This blend is the affine Robin solution with $\mathbf{K}_{\mathrm{ref}}$. It agrees with the
[Kokkos formulation](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md#weighted-traction-stabilization) only when the reference stiffness
tensor equals the actual cohesive tangent and $\beta$ matches.

!syntax parameters /InterfaceKernels/SCZMInterfaceKernelSmallStrain

!syntax inputs /InterfaceKernels/SCZMInterfaceKernelSmallStrain

!syntax children /InterfaceKernels/SCZMInterfaceKernelSmallStrain
