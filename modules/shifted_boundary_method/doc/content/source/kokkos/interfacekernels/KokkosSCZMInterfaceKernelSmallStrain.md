# KokkosSCZMInterfaceKernelSmallStrain

!syntax description /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

This kernel assembles the small-strain SCZM traction-jump contribution on the Kokkos
backend. It consumes device-resident global traction and its derivative with respect
to the global shifted displacement jump from compatible Kokkos materials. With weighted
traction, it reads the stabilized traction and tangent from a shared user object.

The kernel derives from the generic Kokkos shifted-interface base, which caches static
surrogate-to-true geometry from `sbm_distance_uo`. It assembles both residual rows and
all four interface Jacobian blocks, including cross-component displacement coupling.

## Material Properties

The `traction_global` property is a `MaterialProperty<Real, 1>` containing the global
traction vector. The `dtraction_djump_global` property is a `MaterialProperty<Real, 2>`
containing the global traction tangent. `base_name` prefixes these property names.
The number of displacement variables must match the mesh dimension.

## Weighted-traction stabilization

Set [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/stabilization) to `weighted_traction` to use the generalized Robin
formulation described in [KokkosSCZMInterfaceKernelTotalLagrangian](KokkosSCZMInterfaceKernelTotalLagrangian.md#weighted-traction-stabilization).
The small-strain bulk traction is
$\mathbf{T}_h = \frac{1}{2}(\boldsymbol{\sigma}^-+\boldsymbol{\sigma}^+)\mathbf{N}$,
where $\boldsymbol{\sigma}^\pm$ are the adjacent Cauchy stresses. The shared evaluator uses
$\mathbf{F} = \mathbf{I}$ and identity deformation rotations, so the Nanson area ratio is one
and the cohesive frame remains the fixed reference rotation $\mathbf{Q}_0$.

The action supplies a shared [KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md)
user object through [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/stabilization_data).
The user object derives and freezes $\beta$ on its first evaluation from the normal stiffness of the
face [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/tangent) property, typically
`Jacobian_mult`, scaled by
[!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/stabilization_gamma).

## Directional correction

With [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/directional_correction) enabled (the default), the correction reproduces
the corresponding contribution of the host `SCZMInterfaceKernelSmallStrainTempl<false>`.
The bulk response is read from the `stress` and `tangent` Kokkos material properties named by the `stress`
and `tangent` parameters, `base_name`-prefixed; the primary side is read from the element
storage and the neighbor side from the neighbor storage of the same property name. A
heterogeneous interface is represented by producing these properties independently on the
primary and neighbor sides, for example with two `[NEML2]` blocks using
`interface_output_side = primary` and `interface_output_side = neighbor`. They are
required for directional correction and weighted-traction stabilization.

The correction adds `-test * sum_j stress_primary(i,j) * n_tangent(j)` to the element
residual row and `+test_neighbor * sum_j stress_neighbor(i,j) * n_tangent(j)` to the
neighbor row, where `n_tangent = normals - dot(true_normal, normals) * true_normal`. This
term is deliberately *not* multiplied by `areaCorrection()`; that factor applies only to
the base cohesive traction.

In the Jacobian the correction contributes `-correction_jacobian` to `ElementElement`
using the primary Lame constants and `+correction_jacobian` to `NeighborNeighbor` using
the neighbor constants. Both cross blocks receive exactly zero. The correction Jacobian
uses the natural, unshifted test and trial gradient, because the shift already enters
the base cohesive traction Jacobian. The two tensor contractions of the host tangent
definition are both evaluated, so a future non-isotropic tangent cannot silently change
the formula.

## `no_shifted`

Setting [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/no_shifted) to `true` disables shifted terms. The base cohesive
traction then drops the `areaCorrection()` factor and its
Jacobian uses the natural test functions rather than `shiftedPhi()`. Following the host,
`no_shifted` suppresses the directional correction entirely, so `directional_correction`
has no effect while it is enabled. Weighted traction still uses the bulk stress and tangent.
The paired displacement-jump material applies the matching change: it omits the
`grad_u dot distance` contribution to the jump and builds the reference rotation from the
surrogate face normal instead of the true normal.

## Supported configurations

The [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md) requires Cartesian coordinates
and a stress-strain tangent for small-strain Kokkos kernels. Volumetric locking correction
and element-linear stress recovery are available only for total-Lagrangian kinematics.

The [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md) selects host objects by
default; the Kokkos path is opt-in through `use_kokkos = true`. Focused tests also
instantiate this kernel directly with a test-only linear traction law.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain
