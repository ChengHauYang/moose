# SCZMInterfaceKernelTotalLagrangian

!syntax description /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

This interface kernel assembles cohesive PK1 traction on a surrogate interface. It applies the
true-to-surrogate reference-area projection and the directional correction based on bulk PK1
stress. Its non-AD Jacobian includes derivatives through both the shifted displacement jump and the
surrogate-interface deformation gradient.

With `volumetric_locking_correction = true`, the directional correction uses the total-mode F-bar
PK1 stress and its exact Jacobian, as described in
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md). The trial-gradient
average over each bulk element is integrated with a separate volume finite element reinitialized on
that element.

## Weighted-traction stabilization

Set [!param](/InterfaceKernels/SCZMInterfaceKernelTotalLagrangian/stabilization) to `weighted_traction` to use the host's legacy linear blend
$\widehat{\mathbf{t}} = \mathbf{S}\mathbf{t}_c + (\mathbf{I}-\mathbf{S})\mathbf{T}_h$, where
$\mathbf{t}_c$ is the cohesive traction at the shifted jump, $\mathbf{T}_h$ is the average
adjacent bulk PK1 traction, and
$\mathbf{S} = \beta(\mathbf{K}_{\mathrm{ref}}+\beta\mathbf{I})^{-1}$.
Supply both [!param](/InterfaceKernels/SCZMInterfaceKernelTotalLagrangian/stabilization_beta) and
[!param](/InterfaceKernels/SCZMInterfaceKernelTotalLagrangian/stabilization_normal_stiffness). The optional
[!param](/InterfaceKernels/SCZMInterfaceKernelTotalLagrangian/stabilization_tangential_stiffness) defaults to the normal stiffness.
The reference stiffnesses define $\mathbf{K}_{\mathrm{ref}}$ in the interface frame.

This blend is the affine Robin solution with $\mathbf{K}_{\mathrm{ref}}$. It agrees with the
[Kokkos formulation](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md#weighted-traction-stabilization) only when the reference stiffness
tensor equals the actual cohesive tangent and $\beta$ matches.

!syntax parameters /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian
