# KokkosSCZMInterfaceKernelSmallStrain

!syntax description /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

This kernel assembles the small-strain SCZM traction-jump contribution on the Kokkos
backend. It consumes device-resident global traction and its derivative with respect
to the global shifted displacement jump. The properties must be produced by compatible
Kokkos materials; this kernel does not evaluate a cohesive traction law itself.

The kernel derives from the generic Kokkos shifted-interface base, which caches static
surrogate-to-true geometry from `sbm_distance_uo`. It assembles both residual rows and
all four interface Jacobian blocks, including cross-component displacement coupling.

## Material Properties

The `traction_global` property is a `MaterialProperty<Real, 1>` containing the global
traction vector. The `dtraction_djump_global` property is a `MaterialProperty<Real, 2>`
containing the global traction tangent. `base_name` prefixes these property names.
The number of displacement variables must match the mesh dimension.

## Directional correction

With `directional_correction = true` (the default) this kernel reproduces the residual
and Jacobian of the host `SCZMInterfaceKernelSmallStrainTempl<false>`. The bulk response
is read from four interface-face properties that
[`KokkosSCZMComputeLinearElasticBulkStress`](KokkosSCZMComputeLinearElasticBulkStress.md)
writes at the primary face quadrature point address, because a Kokkos interface material
is the single writer of an oriented face:

| Property | Type | Dimensions | Meaning |
|---|---|---|---|
| `sczm_bulk_stress` | `MaterialProperty<Real, 2>` | `{3, 3}` | primary side Cauchy stress |
| `sczm_bulk_stress_neighbor` | `MaterialProperty<Real, 2>` | `{3, 3}` | neighbor side Cauchy stress |
| `sczm_bulk_lame` | `MaterialProperty<Real, 1>` | `{2}` | primary side `(lambda, mu)` |
| `sczm_bulk_lame_neighbor` | `MaterialProperty<Real, 1>` | `{2}` | neighbor side `(lambda, mu)` |

`base_name` prefixes these names as well. They are retrieved only when the correction is
enabled, so a run with `directional_correction = false` does not declare the material.

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

Setting `no_shifted = true` makes this object a complete switch that reproduces the host
behavior. The base cohesive traction then drops the `areaCorrection()` factor and its
Jacobian uses the natural test functions rather than `shiftedPhi()`. Following the host,
`no_shifted` suppresses the directional correction entirely, so `directional_correction`
has no effect while it is enabled and the four bulk properties are not retrieved. The
paired displacement-jump material applies the matching change: it omits the
`grad_u dot distance` contribution to the jump and builds the reference rotation from the
surrogate face normal instead of the true normal.

## Limitations

Volumetric locking correction, curvilinear-coordinate constitutive strain (RZ and
RSPHERICAL), and general user-supplied `stress`, `tangent`, and `tangent_definition`
semantics are not implemented. The [ShiftedCohesiveZone
action](ShiftedCohesiveZoneAction.md) rejects each of these explicitly rather than silently
ignoring it. Do not read this kernel as full host parity while those items remain
outstanding.

The [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md) selects host objects by
default; the Kokkos path is opt-in through `use_kokkos = true`. Focused tests also
instantiate this kernel directly with a test-only linear traction law.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain
