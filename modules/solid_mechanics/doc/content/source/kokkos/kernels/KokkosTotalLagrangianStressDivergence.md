# KokkosTotalLagrangianStressDivergence

!if! function=hasCapability('kokkos')

`KokkosTotalLagrangianStressDivergence` computes one Cartesian component of the
total-Lagrangian internal-force residual from the first Piola-Kirchhoff stress:

\[
R_i = \int_{\Omega_0} P_{iJ}\,\phi_{,J}\,dV.
\]

Its diagonal and off-diagonal Jacobian terms use the full derivative
$\partial P_{iJ}/\partial u_{k,L}$. Add one kernel for each displacement component.

With [!param](/Kernels/KokkosTotalLagrangianStressDivergence/large_kinematics) set to `false`, the
stress material is expected to be
[KokkosComputeLagrangianStressCustomSmall](KokkosComputeLagrangianStressCustomSmall.md), and the
non-local Jacobian term of `stabilize_strain = true` follows the additive (small kinematics) F-bar
correction.

The kernel requires one scalar Cartesian displacement variable per reference mesh dimension; a
two-dimensional mesh is treated in plane strain. It does not provide volumetric locking correction or curvilinear coordinate terms.

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/neml2_input_gatherer/exact_kinematics.i block=Kernels

!syntax parameters /Kernels/KokkosTotalLagrangianStressDivergence

!syntax inputs /Kernels/KokkosTotalLagrangianStressDivergence

!syntax children /Kernels/KokkosTotalLagrangianStressDivergence

!if-end!

!else
!include kokkos/kokkos_warning.md
