# KokkosComputeLagrangianStressCustomSmall

!if! function=hasCapability('kokkos')

`KokkosComputeLagrangianStressCustomSmall` is the small-kinematics counterpart of
[KokkosComputeLagrangianStressCustomPK2](KokkosComputeLagrangianStressCustomPK2.md). It publishes
the PK1 stress and its displacement-gradient derivative for
[KokkosTotalLagrangianStressDivergence](KokkosTotalLagrangianStressDivergence.md) with
`large_kinematics = false` from a small stress $\sigma$ and its strain derivative $C$, both full
tensors:

\[
P_{ij} = \sigma_{ij},
\qquad
\frac{\partial P_{ij}}{\partial u_{k,l}} = C_{ijmn} \frac{\partial F^{stab}_{mn}}{\partial F_{kl}}.
\]

With `stabilize_strain = false` the F-bar factor is the identity. With `stabilize_strain = true` it
is the additive (small kinematics) correction $\delta_{mk}\delta_{nl} - \delta_{mn}\delta_{kl} / 3$,
and the material also publishes $A = C : I$ and $\gamma / 3 = 1 / 3$ for the non-local Jacobian
term of the kernel, and $C$ as `dpk1_bypass_fbar` for
[KokkosHomogenizedTotalLagrangianStressDivergence](KokkosHomogenizedTotalLagrangianStressDivergence.md).

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/small_2d.i block=Materials/compute_stress

!syntax parameters /Materials/KokkosComputeLagrangianStressCustomSmall

!syntax inputs /Materials/KokkosComputeLagrangianStressCustomSmall

!syntax children /Materials/KokkosComputeLagrangianStressCustomSmall

!if-end!

!else
!include kokkos/kokkos_warning.md
