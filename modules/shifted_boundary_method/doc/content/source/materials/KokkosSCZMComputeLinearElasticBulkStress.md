# KokkosSCZMComputeLinearElasticBulkStress

!syntax description /Materials/KokkosSCZMComputeLinearElasticBulkStress

This material evaluates the isotropic linear-elastic bulk Cauchy stress and Lame constants
on both physical sides of a shifted cohesive-zone interface. It supplies the bulk response
consumed by the directional correction of
[`KokkosSCZMInterfaceKernelSmallStrain`](../interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.md).

The action does not create this material. Declare it explicitly in a `[Materials]` block
on the SCZM boundary, because a Kokkos interface material is the single writer of an
oriented face and must be restricted to the same interface as the kernel:

```
[Physics/SolidMechanics/ShiftedCohesiveZone]
  [sczm]
    boundary = interface
    displacements = 'ux uy'
    use_kokkos = true
    directional_correction = true
  []
[]

[Materials]
  [bulk_stress]
    type = KokkosSCZMComputeLinearElasticBulkStress
    boundary = interface
    displacements = 'ux uy'
    youngs_modulus = 1
    poissons_ratio = 0.3
    neighbor_youngs_modulus = 3
    neighbor_poissons_ratio = 0.2
  []
[]
```

## Declared properties

The following properties are declared by this material and read by the directional
correction. An optional `base_name` prefixes each name.

| Name | Type | Dimensions | Meaning |
|---|---|---|---|
| `sczm_bulk_stress` | `MaterialProperty<Real, 2>` | `{3, 3}` | primary side Cauchy stress |
| `sczm_bulk_stress_neighbor` | `MaterialProperty<Real, 2>` | `{3, 3}` | neighbor side Cauchy stress |
| `sczm_bulk_lame` | `MaterialProperty<Real, 1>` | `{2}` | primary side `(lambda, mu)` |
| `sczm_bulk_lame_neighbor` | `MaterialProperty<Real, 1>` | `{2}` | neighbor side `(lambda, mu)` |

All four are written through the primary face quadrature point address, including the two
`_neighbor` properties, because the interface material evaluates both oriented sides at
that single address. All entries are initialized, including the entries that a 2D or 1D
problem does not use, so that contractions against fixed three-component normals never
read uninitialized storage.

## Heterogeneous interfaces

`neighbor_youngs_modulus` and `neighbor_poissons_ratio` default to `youngs_modulus` and
`poissons_ratio` respectively. Set them explicitly to give the two sides of the interface
different elastic constants; the element residual row and the `ElementElement` Jacobian
block then consume the primary values, and the neighbor residual row and the
`NeighborNeighbor` block consume the neighbor values.

Function-valued elastic constants are not supported. A device function evaluated at an
interface position cannot distinguish the two material sides, and the Kokkos function API
has no subdomain argument.

## Limitations

CARTESIAN, RZ, and RSPHERICAL coordinate systems are supported. The number of displacement
variables must equal the mesh dimension (1 for RSPHERICAL, 2 for RZ, and 1, 2, or 3 for CARTESIAN).

This material provides the strain-based isotropic response only. It is the replacement for
a user-supplied `stress`, `tangent`, or `tangent_definition`, which the
[ShiftedCohesiveZone action](../actions/ShiftedCohesiveZoneAction.md) rejects when
`use_kokkos = true`.

!syntax parameters /Materials/KokkosSCZMComputeLinearElasticBulkStress

!syntax inputs /Materials/KokkosSCZMComputeLinearElasticBulkStress

!syntax children /Materials/KokkosSCZMComputeLinearElasticBulkStress
