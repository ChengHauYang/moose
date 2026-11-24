# KokkosHomogenizedTotalLagrangianStressDivergence

!if! function=hasCapability('kokkos')

`KokkosHomogenizedTotalLagrangianStressDivergence` is the Kokkos version of
[HomogenizedTotalLagrangianStressDivergence](HomogenizedTotalLagrangianStressDivergence.md). It adds
the homogenization constraints of a macro gradient scalar variable to
[KokkosTotalLagrangianStressDivergence](KokkosTotalLagrangianStressDivergence.md): for each
constraint $(i, j)$, in the order of the constraint map, the scalar residual is

\[
R_h = \int_{\Omega_0} \left(P_{ij} - P^{target}_{ij}\right) dV
\quad \text{(stress)},
\qquad
R_h = \int_{\Omega_0} \left(F_{ij} - \delta_{ij} - F^{target}_{ij}\right) dV
\quad \text{(strain)},
\]

where $F$ is replaced by its symmetric part with small kinematics. The kernel assembles all four
blocks of the coupled Jacobian. As on the host, only the `component = 0` kernel assembles the scalar
residual and the scalar-scalar Jacobian.

The macro gradient is added to the deformation gradient after the F-bar correction (see
[KokkosComputeDeformationGradient](KokkosComputeDeformationGradient.md)), so the derivatives with
respect to the macro gradient use the PK1 tangent without the F-bar factor,
[!param](/Kernels/KokkosHomogenizedTotalLagrangianStressDivergence/dpk1_bypass_fbar), published by
[KokkosComputeLagrangianStressCustomPK2](KokkosComputeLagrangianStressCustomPK2.md) or
[KokkosComputeLagrangianStressCustomSmall](KokkosComputeLagrangianStressCustomSmall.md) when
`stabilize_strain = true`. The strain constraints use the
[!param](/Kernels/KokkosHomogenizedTotalLagrangianStressDivergence/deformation_gradient) property,
which must include the macro gradient.

The targets are evaluated on the host at the origin once per residual evaluation, so they must be
functions of time only.

The [QuasiStatic](syntax/Physics/SolidMechanics/QuasiStatic/index.md) Physics with `use_kokkos = true`
adds this kernel and the kinematics materials.

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/small_2d.i block=Kernels

!syntax parameters /Kernels/KokkosHomogenizedTotalLagrangianStressDivergence

!syntax inputs /Kernels/KokkosHomogenizedTotalLagrangianStressDivergence

!syntax children /Kernels/KokkosHomogenizedTotalLagrangianStressDivergence

!if-end!

!else
!include kokkos/kokkos_warning.md
