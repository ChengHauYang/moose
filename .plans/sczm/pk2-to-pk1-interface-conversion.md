# SCZM Kokkos interface PK2-to-PK1 conversion plan

## Goal

Allow the Kokkos shifted cohesive-zone formulation to consume interface NEML2 outputs

\[
S, \qquad \frac{\partial S}{\partial F}
\]

while supplying the directional-correction terms with

\[
P = FS, \qquad \frac{\partial P}{\partial F}.
\]

The user will configure this conversion once in the `ShiftedCohesiveZone` Physics block. The SCZM
Action will create the primary- and neighbor-side converter objects automatically.

The converters are **not removed**. Only the two manually written `[Materials]` blocks are removed
from the user-facing input.

## Intended data flow

```text
NEML2 interface evaluation
  |-- primary PK2 + dPK2/dF
  `-- neighbor PK2 + dPK2/dF
                 |
                 v
  primary and neighbor PK2-to-PK1 converter objects
                 |
                 |-- primary PK1 + dPK1/dF in boundary storage
                 `-- neighbor PK1 + dPK1/dF in neighbor storage
                 |
                 v
  KokkosSCZMInterfaceKernelSmallStrain directional correction
```

The bulk path remains separate:

```text
NEML2 bulk PK2 + dPK2/dF
                 |
                 v
KokkosComputeLagrangianStressCustomPK2
                 |
                 v
KokkosTotalLagrangianStressDivergence
```

The bulk and interface converters share their mathematical implementation, but they execute in
different quadrature-point contexts and write to different material-property storage.

## User-facing input

Add `pk2_wrt_deformation_gradient` as an Action-level `tangent_definition` option:

```ini
[Physics/SolidMechanics/ShiftedCohesiveZone/sczm]
  boundary = Block0_Block1
  displacements = 'disp_x disp_y disp_z'
  sbm_distance_uo = distance
  use_kokkos = true
  directional_correction = true

  stress = neml2_pk2
  tangent = dneml2_pk2_ddeformation_gradient
  tangent_definition = pk2_wrt_deformation_gradient
[]
```

This means:

- `stress` names a PK2 property;
- `tangent` names its derivative with respect to the deformation gradient;
- the Action must convert both interface sides to PK1 before configuring the kernel.

The user does not write separate `pk1_primary` and `pk1_neighbor` material blocks.

The existing direct-PK1 mode remains valid:

```ini
stress = my_pk1
tangent = my_dpk1_dF
tangent_definition = pk1_wrt_deformation_gradient
```

In direct-PK1 mode, the Action does not create converters.

## 1. Template the existing converter

Refactor:

```text
modules/solid_mechanics/include/kokkos/materials/
  KokkosComputeLagrangianStressCustomPK2.h
modules/solid_mechanics/src/kokkos/materials/
  KokkosComputeLagrangianStressCustomPK2.K
```

into a common implementation selected by compile-time interface and neighbor flags:

```cpp
template <bool interface, bool neighbor = false>
class KokkosComputeLagrangianStressCustomPK2Templ
  : public std::conditional_t<interface,
                              Moose::Kokkos::InterfaceMaterial,
                              Moose::Kokkos::Material>
{
  static_assert(interface || !neighbor);
};
```

Expose three factory-registerable aliases:

```cpp
using KokkosComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<false, false>;

using KokkosInterfaceComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<true, false>;

using KokkosNeighborComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<true, true>;
```

The existing bulk object name, parameters, defaults, and behavior must remain unchanged.

## 2. Share one device-side conversion implementation

Keep the PK2-to-PK1 calculation in one device function. The bulk and interface callbacks only select
the appropriate datum and quadrature point.

The unstabilized conversion is

\[
P_{iJ} = F_{iK} S_{KJ},
\]

\[
\frac{\partial P_{iJ}}{\partial F_{kL}}
= \delta_{ik} S_{LJ}
+ F_{iK}\frac{\partial S_{KJ}}{\partial F_{kL}}.
\]

The wrappers should have the following responsibilities:

```cpp
// Bulk
computeQpProperties(qp, datum)
{
  convertPK2ToPK1(datum, qp);
}
```

```cpp
// Interface
computeQpProperties(face, elem_datum, neighbor_datum, qp, neighbor_qp)
{
  if constexpr (neighbor)
    convertPK2ToPK1(neighbor_datum, neighbor_qp);
  else
    convertPK2ToPK1(elem_datum, qp);
}
```

This keeps the tensor contractions and index ordering identical for all three objects.

## 3. Preserve the two material-property storage domains

A single material instance must not be made responsible for declaring the same property into both
boundary and neighbor storage. The existing Kokkos material-property abstraction selects its storage
through `_material_data_type` at construction.

The primary converter will:

- derive from `Moose::Kokkos::InterfaceMaterial`;
- read PK2 and its tangent from primary boundary storage;
- construct `F` from primary displacement gradients;
- write PK1 and its tangent to primary boundary storage.

The neighbor converter will set

```cpp
params.set<Moose::MaterialDataType>("_material_data_type") =
    Moose::NEIGHBOR_MATERIAL_DATA;
```

and will:

- read PK2 and its tangent from neighbor storage;
- use `neighbor_datum` and the mapped `neighbor_qp`;
- construct `F` from neighbor displacement gradients;
- write PK1 and its tangent to neighbor storage.

Both converters declare identical material-property names. They do not use a `_neighbor` name
suffix. The SCZM kernel already distinguishes the two storage domains with
`getKokkosMaterialPropertyByName` and `getKokkosNeighborMaterialPropertyByName`.

## 4. Register the converter types

Register the bulk alias as a Kokkos material and both interface aliases as Kokkos interface
materials:

```cpp
registerKokkosMaterial(
    "SolidMechanicsApp", KokkosComputeLagrangianStressCustomPK2);

registerKokkosInterfaceMaterial(
    "SolidMechanicsApp", KokkosInterfaceComputeLagrangianStressCustomPK2);

registerKokkosInterfaceMaterial(
    "SolidMechanicsApp", KokkosNeighborComputeLagrangianStressCustomPK2);
```

The interface aliases remain independently constructible for focused testing and advanced use, even
though normal SCZM inputs will rely on Action-generated instances.

## 5. Make the SCZM Action create the pair

Extend `ShiftedCohesiveZoneAction` so that all of the following conditions require PK2-to-PK1
conversion:

```text
directional_correction = true
no_shifted = false
tangent_definition = pk2_wrt_deformation_gradient
```

These conditions are independent of the execution backend. `use_kokkos` selects the backend;
it does not determine whether PK2 input is mathematically valid.

This plan provides Kokkos converters. Until a CPU interface converter is available, report a
parameter error explaining that automatic PK2-to-PK1 conversion is not yet supported on the CPU
path only when `use_kokkos = false` and all of the above conditions hold. Do not reject CPU PK2
input when directional correction is disabled or `no_shifted = true`, because no conversion is
needed.

For `use_kokkos = true`, the Action will create two internal objects when conversion is required:

```text
<sczm-name>_pk2_to_pk1_primary
<sczm-name>_pk2_to_pk1_neighbor
```

Both receive the Action's:

- interface boundary;
- displacement variables;
- `stress` property as `custom_pk2_stress`;
- `tangent` property as `custom_pk2_jacobian`.

The Action will generate collision-resistant output names such as:

```text
<sczm-name>_pk1_stress
<sczm-name>_pk1_jacobian
```

It will then configure each generated Kokkos SCZM interface kernel with:

```text
stress = <sczm-name>_pk1_stress
tangent = <sczm-name>_pk1_jacobian
tangent_definition = pk1_wrt_deformation_gradient
```

`pk2_wrt_deformation_gradient` is therefore an Action-level input contract. The kernel continues to
consume only its already supported stress/tangent definitions; it never contracts PK2 as though it
were PK1.

No converter pair should be created when:

- directional correction is disabled;
- the user supplies PK1 directly;
- shifted terms are disabled and therefore omit the directional correction.

Add the following comment beside the Action's converter-pair creation logic:

```cpp
// For large-deformation models, NEML2 provides PK2 stress and dPK2/dF on both sides
// of the interface. Convert them to PK1 stress and dPK1/dF for the SCZM directional correction.
```

When directional correction is inactive, the PK2 option requires no conversion. Leave the kernel's
`stress`, `tangent`, and `tangent_definition` parameters at their defaults because they are unused;
do not forward the Action's PK2 inputs. This applies to both CPU and Kokkos kernels.

## 6. F-bar scope

Preserve the existing bulk converter's `stabilize_strain = true` implementation and its nonlocal
F-bar auxiliary properties. The current bulk Step 5 input must continue to work without changes.

For the first interface implementation, require:

```ini
stabilize_strain = false
```

and reject `true` with a parameter error. The bulk F-bar implementation publishes nonlocal Jacobian
terms that are assembled by `KokkosTotalLagrangianStressDivergence`; the SCZM interface kernel does
not yet have an equivalent nonlocal F-bar coupling contract. Silently applying only the local part
would produce an inconsistent interface Jacobian.

F-bar-consistent interface conversion is separate future work and is not part of this plan.

## 7. Verification

### Slice 1: bulk template regression

Refactor the bulk converter into the template without adding interface behavior.

Verify that:

- the existing object name and input parameters are unchanged;
- unstabilized PK1 and tangent values are unchanged;
- the existing F-bar path and auxiliary properties are unchanged;
- the LD Step 5 input requires no edits.

### Slice 2: primary and neighbor converters

Add the two interface aliases and a focused 3D interface fixture. Use distinct, nonsymmetric values
on the two sides:

```text
F_primary != F_neighbor
PK2_primary != PK2_neighbor
dPK2/dF_primary != dPK2/dF_neighbor
```

Verify that:

- each converter reads the intended side;
- neighbor quadrature-point mapping is correct;
- each result is written to the correct storage domain;
- PK1 and every tangent index agree with the analytical conversion;
- interface `stabilize_strain = true` produces the intended parameter error.

### Slice 3: SCZM Action automation

Add `pk2_wrt_deformation_gradient` and have the Action create the converter pair.

Verify that:

- one Physics block creates exactly two internal converter objects;
- generated property names do not collide between multiple SCZM blocks;
- direct-PK1 mode creates no converters;
- PK2 mode with directional correction disabled creates no converters and produces no
  conversion-related error on either CPU or Kokkos;
- PK2 mode with `no_shifted = true` creates no converters and produces no conversion-related error
  on either CPU or Kokkos;
- until a CPU interface converter is available, PK2 mode with `use_kokkos = false`,
  `directional_correction = true`, and `no_shifted = false` reports that automatic conversion is
  not yet supported on the CPU path;
- inactive directional correction does not forward the Action-only PK2 option to the kernel;
- the kernel receives generated PK1 properties and the PK1 tangent definition.

### Slice 4: SCZM Jacobian coverage

Add or update a `PetscJacobianTester` case with:

```ini
directional_correction = true
tangent_definition = pk2_wrt_deformation_gradient
```

Use a 3D angled shifted interface and a response with nonzero cross-component couplings. Exercise
all affected displacement couplings and all four interface Jacobian blocks:

```text
ElementElement
ElementNeighbor
NeighborElement
NeighborNeighbor
```

Run the case on its supported Kokkos compute devices.

### Slice 5: LD + SCZM full-GPU integration

Connect the interface conversion path to the existing LD full-GPU backbone:

```text
NEML2/CUDA -> Kokkos interface PK2-to-PK1 -> Kokkos SCZM -> PETSc GPU
```

Verify:

- LD state advances only after converged timesteps;
- SCZM traction and directional correction are active;
- the nonlinear solve converges with the consistent Jacobian;
- CPU-Kokkos and CUDA solutions agree within justified tolerances;
- profiling shows no unintended host transfer in the constitutive and interface assembly hot path.

## Files expected to change

```text
modules/solid_mechanics/include/kokkos/materials/
  KokkosComputeLagrangianStressCustomPK2.h
modules/solid_mechanics/src/kokkos/materials/
  KokkosComputeLagrangianStressCustomPK2.K
modules/solid_mechanics/doc/content/source/kokkos/materials/
  KokkosComputeLagrangianStressCustomPK2.md

modules/shifted_boundary_method/include/actions/
  ShiftedCohesiveZoneAction.h
modules/shifted_boundary_method/src/actions/
  ShiftedCohesiveZoneAction.C
modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/
  tests
  <new focused input files>
```

The SCZM kernel should require no new PK2 contraction logic. Any kernel edit should be limited to
parameter validation or documentation needed to preserve the distinction between Action-level PK2
input and kernel-level PK1 input.

## Non-goals

This work does not:

- remove PK2-to-PK1 conversion;
- merge boundary and neighbor material-property storage;
- add finite-deformation cohesive-zone kinematics;
- add interface F-bar nonlocal Jacobian terms;
- change the existing bulk LD stress-divergence formulation;
- require NEML2 to output PK1 directly.
