# Shifted Cohesive Zone Physics System

!syntax description /Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction

The Shifted Cohesive Zone Physics creates the interface kernels, displacement-jump and
global-traction materials, and optional geometry objects needed by the shifted cohesive-zone method
(SCZM). Small-strain and total-Lagrangian kinematics are supported on the host and, with non-AD
evaluation in Cartesian coordinates, on the Kokkos path (see [#kokkos-implementation-status]).

For `strain = FINITE`, SCZM Taylor-shifts the displacement jump to the true interface and uses the
true reference normal for the cohesive frame and Nanson transformation. The deformation gradient
is averaged across the surrogate interface but is not Taylor-shifted; displacement basis orders
above `SECOND` therefore produce a warning about the accuracy limitation of this approximation.

Setting up an SCZM problem involves two independent choices:

1. Select how the Physics obtains the distance and true normal for each interface.
2. Define a constitutive traction material on the same surrogate boundaries.

## Select the interface geometry

The `boundary` parameter always lists the surrogate boundaries on which SCZM is applied. Exactly
one of the following three approaches supplies their corresponding true-interface geometry.

### Use an existing distance user object

Set `sbm_distance_uo` to an existing `BoundaryShortestDistanceToSurface` for full control over the
distance calculation. The following example defines the object from a level-set function:

!listing modules/shifted_boundary_method/test/tests/shifted_cohesive_zone/angled_sczm_nonad_salehani.i start=[UserObjects] end=[Functions] include-end=false

The Physics can receive the object name directly or inherit it from `[GlobalParams]`, as this example
does:

!listing modules/shifted_boundary_method/test/tests/shifted_cohesive_zone/angled_sczm_nonad_salehani.i start=[GlobalParams] end=[Problem] include-end=false

This approach is the most flexible, but the user is responsible for constructing the distance user
object and any functions or surface data it needs.

### Use one surface mesh per interface

Set `generate_sbm_distance = true` when a separate saved surface mesh is available for every
interface. The Physics creates the surface-mesh builders, distance functions, and aggregate boundary
distance user object. If each saved mesh has the same name as its corresponding surrogate boundary,
only the following Physics configuration is needed:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_separate_interfaces.i start=[Physics/SolidMechanics/ShiftedCohesiveZone] end=[Materials] include-end=false

Set `surface_meshes` only when the saved mesh names differ from the boundary names. Entries in
`surface_meshes` and `boundary` correspond by position. For example:

```
boundary = 'grain1_grain2 grain1_grain4'
surface_meshes = 'surface_12 surface_14'
```

Here, `surface_12` supplies the geometry for `grain1_grain2`, and `surface_14` supplies the geometry
for `grain1_grain4`.

### Use one complete interface mesh

Set `complete_interface_mesh` to one saved mesh containing the complete outward-oriented boundary
of every subdomain. The Physics creates one
[`SBMInterfaceManager`](userobjects/SBMInterfaceManager.md) and one boundary distance user object:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_coarse_lambda1.i start=[Physics/SolidMechanics/ShiftedCohesiveZone] end=[Materials] include-end=false

The mesh generator that supplies `complete_interface_mesh` must retain the mesh with
`save_with_name`. Alternatively, set `complete_interface_mesh_file` to read the same surface mesh
directly from a file. Mesh generators do not run with a pre-split mesh (`--use-split`), so the
file form is the one to use for large distributed runs:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_mesh_file.i start=[Physics/SolidMechanics/ShiftedCohesiveZone] end=[Materials] include-end=false

Boundary names of the form `grainX_grainY` or `blockX_blockY` (in any letter case, so including
the `BlockX_BlockY` names [BreakMeshByBlockGenerator](BreakMeshByBlockGenerator.md) gives unnamed
subdomains) identify the two interface subdomains automatically. For other boundary names, set
`interface_subdomain_pairs` with one pair per boundary in the same order. The pair selects an
interface; its order does not control the final true-normal orientation.

Shifted integration is enabled by default and is the intended mode for SBM simulations. The
`no_shifted = true` option is retained primarily for verification and comparison: it disables the
shifted correction terms while using the same interface kernel and cohesive-zone model, providing a
non-shifted baseline that isolates their effect. This option does not require a separate geometry
source.

## Define the traction law

The Physics does not create the constitutive traction law. Define an interface material such as
`BiLinearMixedModeTraction` or `SalehaniIrani3DCTraction` and restrict it to the same surrogate
interfaces/ boundaries as the SCZM Physics. The Action also creates the displacement-jump and
global-traction materials. The displacement-jump material computes the local displacement jump
used by the constitutive material to determine the local traction. The global-traction material
then uses the local traction to compute the traction required by the equilibrium equations.

For multiple interfaces, define the boundary list once at the top of the input file and use `${interfaces}`
wherever it is needed. The `${interfaces}` expression is replaced with the boundary list assigned to
interfaces. Reusing this list in both the Physics and the traction material keeps their boundary
specifications consistent. The complete-interface example uses this pattern:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_coarse_lambda1.i start=interfaces end=ny include-end=false

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_coarse_lambda1.i start=[Physics/SolidMechanics/ShiftedCohesiveZone] end=[Executioner] include-end=false

Keeping the traction material explicit allows different constitutive laws to be assigned to
different subsets of the SCZM boundaries. The Physics sets `boundary`, `displacements`, and
`base_name` only on the kernels and materials that it creates; it does not modify user-defined
materials in `[Materials]`.

## Non-AD tangent definition

The non-AD interface kernel uses `stress` and `tangent` material properties for its directional
correction and Jacobian. When `tangent_definition = auto`, the standard `Jacobian_mult` property is
interpreted as $\partial\boldsymbol{\sigma}/\partial\boldsymbol{\varepsilon}$ and
`pk1_jacobian` is interpreted as $\partial\mathbf{P}/\partial\mathbf{F}$. A custom `tangent`
property name requires an explicit `tangent_definition`:

- Use `stress_wrt_strain` only for a derivative of stress with respect to symmetric strain ($\partial\boldsymbol{\sigma}/\partial\boldsymbol{\varepsilon}$).
- Use `pk1_wrt_deformation_gradient` only for a derivative of first Piola-Kirchhoff stress with respect to the full deformation gradient ($\partial\mathbf{P}/\partial\mathbf{F}$).

The AD interface kernel differentiates its residual automatically and does not use `tangent` or
`tangent_definition`; setting either parameter with AD enabled is an error.

## Weighted-traction stabilization id=weighted-traction-stabilization

Set [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization) to `weighted_traction` to combine the shifted cohesive traction
with the average adjacent bulk traction. In both Kokkos formulations,
[KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md) solves a generalized Robin equation
at each paired interface quadrature point once per assembly. The
[small-strain kernel](KokkosSCZMInterfaceKernelSmallStrain.md) uses Cauchy stress and the
[total-Lagrangian kernel](KokkosSCZMInterfaceKernelTotalLagrangian.md) uses PK1 stress.

Supply [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization_beta) for a fixed $\beta$. Otherwise the first
stabilization evaluation derives $\beta$ from the adjacent elements' normal directional volume
tangents and normal sizes, scales it by [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization_gamma) (default 5), and freezes it
for subsequent assemblies. The [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/volume_tangent) property defaults to the
[!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/tangent) property (typically `Jacobian_mult`) for small strain and to
`dpk1_d_grad_u` for total Lagrangian. Setting [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stress_recovery) to
`element_linear` is available only for the total-Lagrangian formulation.

The host kernels use the legacy linear weighted traction
$\widehat{\mathbf{t}} = \mathbf{S}\mathbf{t}_c + (\mathbf{I}-\mathbf{S})\mathbf{T}_h$,
with $\mathbf{S} = \beta(\mathbf{K}_{\mathrm{ref}}+\beta\mathbf{I})^{-1}$.
Here $\mathbf{t}_c$ is the cohesive traction at the shifted jump and $\mathbf{T}_h$ is the
average adjacent bulk traction. The host requires [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization_beta) and
[!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization_normal_stiffness). The optional
[!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization_tangential_stiffness) defaults to the normal stiffness. Kokkos accepts
both reference stiffness parameters for input compatibility and ignores their values.
The legacy blend is the affine Robin solution with $\mathbf{K}_{\mathrm{ref}}$.
The host and Kokkos formulations agree only when $\mathbf{K}_{\mathrm{ref}}$ equals the actual
cohesive tangent and $\beta$ matches.

### Interface normal under large deformation id=weighted-traction-normal

Both sides of the Robin equation of
[KokkosSCZMInterfaceKernelTotalLagrangian](KokkosSCZMInterfaceKernelTotalLagrangian.md) use one
fixed reference unit normal $\mathbf{N}$: the true-interface reference normal when shifted, and the
surrogate face reference normal with `no_shifted = true`. Hence $\partial\mathbf{N}/\partial\mathbf{u} = \mathbf{0}$. The average bulk traction is

!equation
\mathbf{T}_h = \frac{1}{2}\left(\mathbf{P}^- + \mathbf{P}^+\right)\mathbf{N},

where $\mathbf{P}^\pm$ is the first Piola-Kirchhoff stress of the adjacent element on each side. No
current normal is needed, because Nanson's relation gives on each side

!equation
\boldsymbol{\sigma}^\pm\mathbf{n}^\pm\,da^\pm = \mathbf{P}^\pm\mathbf{N}\,dA, \qquad
\mathbf{n}^\pm = \frac{\mathbf{F}^{\pm,-T}\mathbf{N}}{\lVert\mathbf{F}^{\pm,-T}\mathbf{N}\rVert}.

Thus $\mathbf{P}^\pm\mathbf{N}$ is already the traction per unit reference area, including the
current normal and area change of its own side. The two current normals generally differ under large
deformation, and $\mathbf{P}^\pm = \mathbf{F}^\pm\mathbf{S}^\pm$, with $\mathbf{S}^\pm$ the second
Piola-Kirchhoff stress, carries that difference exactly; rescaling $\mathbf{P}^\pm\mathbf{N}$ by a
separately computed current normal would count it twice. The cohesive traction is mapped to the same
measure,

!equation
\mathbf{f}(\mathbf{j}) = a_N\,\mathbf{Q}\,\mathbf{f}_\mathrm{loc}(\mathbf{Q}^T\mathbf{j}), \qquad
\mathbf{Q} = \bar{\mathbf{R}}\mathbf{Q}_0, \qquad
a_N = \det\bar{\mathbf{F}}\,\lVert\bar{\mathbf{F}}^{-T}\mathbf{N}\rVert,

where $\bar{\mathbf{F}} = \frac{1}{2}(\mathbf{F}^- + \mathbf{F}^+) = \bar{\mathbf{R}}\bar{\mathbf{U}}$
is the polar decomposition of the average deformation gradient, $\mathbf{Q}_0$ is the reference
rotation built from $\mathbf{N}$, and $\mathbf{f}_\mathrm{loc}$ is the local cohesive traction per
unit current area. Both sides of the Robin equation are therefore tractions per unit reference area.

The linearization keeps $\mathbf{N}$ fixed. The bulk side gives
$\delta\mathbf{T}_h = \frac{1}{2}(\delta\mathbf{P}^- + \delta\mathbf{P}^+)\mathbf{N}$, with the
deformation entering through $\partial\mathbf{P}/\partial\mathbf{F}$ (`tangent`, or
`volume_tangent` with `stress_recovery = element_linear`). The cohesive side includes $\delta a_N$,
$\delta\mathbf{Q} = \delta\bar{\mathbf{R}}\,\mathbf{Q}_0$, and the $\delta\mathbf{Q}^T\mathbf{j}$
term of the local jump. The automatic beta uses the same normal,
$\kappa_N^\pm = N_i N_J C^\pm_{iJkL} N_k N_L$, with
$\mathbf{C}^\pm = \partial\mathbf{P}^\pm/\partial\mathbf{F}$ taken from each adjacent element's
`volume_tangent`.

## Coupling with NEML2 material models

When using NEML2 constitutive models alongside shifted cohesive zone physics, the continuum
stresses and material tangents required for directional correction (`directional_correction = true`)
are evaluated at quadrature points on the surrogate cohesive interface.

Unlike native MOOSE materials which reinitialize on interface faces automatically, NEML2
models evaluate only on explicitly declared domains. Therefore, if a `[NEML2]` block produces
the continuum stress or tangent consumed by `[Physics/SolidMechanics/ShiftedCohesiveZone]`, its
`interface` parameter must include the SCZM `boundary`. If `interface` is omitted or does
not overlap the cohesive boundary, the action reports a validation error instructing the user to
either configure `interface` on the NEML2 block or set `directional_correction = false`.

## Total-Lagrangian F-bar id=total-lagrangian-f-bar

For `strain = FINITE` with `directional_correction = true`, `volumetric_locking_correction = true`
applies the total-mode F-bar correction to the bulk PK1 stress used by the directional correction,
on both the host non-AD and the Kokkos paths. It is limited to Cartesian coordinates and requires
`tangent_definition = pk1_wrt_deformation_gradient`. Two-dimensional meshes are treated in plane
strain: all tensors remain $3 \times 3$ with $U_{zz} = 1$, so the formulation below is the same in
two and three dimensions. On each side of the interface, the stress is

!equation
\mathbf{P} = \mathbf{U}\,\mathbf{S}(\mathbf{B}), \qquad
\mathbf{B} = \left(\frac{\det\mathbf{M}}{\det\mathbf{U}}\right)^{1/3}\mathbf{U},

where $\mathbf{U} = \mathbf{I} + \nabla_X\mathbf{u}$ is the physical deformation gradient,
$\mathbf{S}$ is the second Piola-Kirchhoff stress, and $\mathbf{M}$ is the average of $\mathbf{U}$
over the reference volume of the bulk element on that side. The cohesive kinematics and the Nanson
geometry continue to use $\mathbf{U}$.

[ComputeDeformationGradient](ComputeDeformationGradient.md) or
[KokkosComputeDeformationGradient](KokkosComputeDeformationGradient.md) with `stabilize_strain = true`
provides $\mathbf{U}$, $\mathbf{M}$, and $\mathbf{B}$ as the `physical_deformation_gradient`,
`average_deformation_gradient`, and `deformation_gradient` properties. The constitutive model must
provide the stress $\mathbf{P}$ as `stress`, its derivative with respect to $\mathbf{U}$ at fixed
$\mathbf{B}$ as `tangent`, and its derivative with respect to $\mathbf{B}$ at fixed $\mathbf{U}$
as `stabilized_tangent`. The Jacobian adds the derivative of $\mathbf{B}$ with respect to the
displacements, including the contribution of $\mathbf{M}$ that couples each interface residual to
every displacement degree of freedom of the bulk element on the same side. With NEML2, both
derivatives are requested from a single interface model:

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_tl_fbar_common.i block=NEML2

## Kokkos Implementation Status

The [KokkosSCZMInterfaceKernelSmallStrain](KokkosSCZMInterfaceKernelSmallStrain.md)
implements the traction-jump residual and Jacobian contribution using device material
properties. It can be instantiated directly with compatible Kokkos property producers.
The focused tests exercise assembly with a test-only linear traction law.

Set `use_kokkos = true` to create the Kokkos implementation objects. For `strain = SMALL`,
this instantiates `KokkosSCZMComputeDisplacementJumpSmallStrain`,
`KokkosCZMComputeGlobalTractionSmallStrain`, and `KokkosSCZMInterfaceKernelSmallStrain`.
For `strain = FINITE`, it instantiates `KokkosSCZMComputeDisplacementJumpTotalLagrangian`,
`KokkosSCZMComputeGlobalTractionTotalLagrangian`, and `KokkosSCZMInterfaceKernelTotalLagrangian`.
The constitutive traction law remains an explicit material; use a compatible Kokkos material
such as `KokkosPureElasticTractionSeparation` or `KokkosSalehaniIraniTractionSeparation`. Kokkos
SCZM supports non-AD evaluation with both small-strain and total-Lagrangian finite-strain
kinematics.

### Directional correction

`directional_correction = true` is supported on the Kokkos path and reproduces the host
residual and Jacobian, including the zero contribution to the two cross blocks. As on the
host path, the correction reads the `stress` and `tangent` material properties (see
"Non-AD tangent definition" above); the action no longer creates or requires a dedicated
bulk-stress material.

When a `[NEML2]` block produces `stress` or `tangent` for a Kokkos directional correction, it
must set `output_backend = kokkos`, `volume = false`, and an `interface` that overlaps the SCZM
`boundary`; the
action validates this and reports an error on the offending NEML2 block parameter otherwise.
Both symmetric stress-strain tangents and full $\partial\mathbf{P}/\partial\mathbf{F}$ tangents
are transferred without changing their tensor layout. For a heterogeneous interface, use two
`[NEML2]` sub-blocks producing the same property name, each restricted with `block` to the bulk
block on its side of the interface (see
[different models across an interface](syntax/NEML2/index.md#heterogeneous-interface)).

Non-Cartesian coordinate systems are rejected when `use_kokkos = true` because their
out-of-plane kinematics are not yet verified for NEML2.

`no_shifted = true` is supported on the Kokkos path as a complete switch. The base cohesive
traction drops its area correction, its Jacobian uses the natural test functions, the paired
displacement-jump material omits the gradient contribution to the jump and builds the
reference rotation from the surrogate face normal, and the directional correction is
suppressed entirely. Setting `directional_correction = true` alongside it is harmless: the
correction is simply not applied, matching the host.

These outstanding items mean the Kokkos path is not yet full host parity. Volumetric locking
correction is available only for total-Lagrangian kinematics, as described in
[#total-lagrangian-f-bar]. General tangent semantics are still to be implemented.

!syntax parameters /Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction

## Associated Actions

!syntax list /Physics/SolidMechanics/ShiftedCohesiveZone objects=True actions=False subsystems=False

!syntax list /Physics/SolidMechanics/ShiftedCohesiveZone objects=False actions=False subsystems=True

!syntax list /Physics/SolidMechanics/ShiftedCohesiveZone objects=False actions=True subsystems=False
