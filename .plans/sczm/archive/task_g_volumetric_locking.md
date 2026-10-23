# Task G - Kokkos SCZM volumetric locking correction

Status: **not started; framework gates unresolved**
Branch: `sczm_dev_gpu`
Baseline: `856c5d66e7` plus uncommitted work (G1 directional correction + `no_shifted` + the split-interface test-mesh fix)

This is the executable sub-plan for section 9.1 of
[task_g_directional_correction.md](task_g_directional_correction.md) and
[kokkos_sczm_plan.md](kokkos_sczm_plan.md). It replaces the short §9.1 sketch with the
investigation results gathered while the split-interface mesh fix was being built.

## 1. Outcome and definition of done

Implement the Kokkos SCZM volumetric locking correction with the same discrete residual and
Jacobian as `SCZMInterfaceKernelSmallStrainTempl<false>::calculateDirectionalCorrectionJacobian`
lines 293-316, using device-resident data only.

Done only when all of the following are true:

1. `volumetric_locking_correction = true` works with `use_kokkos = true` for shifted, non-AD,
   small-strain SCZM in 2D and 3D.
2. The correction residual and Jacobian match the host formulas in section 3, including the
   element-volume average of the trial gradient operator.
3. It works when the neighbor element is owned by another MPI rank.
4. Host and Kokkos agree on the corrected inputs at `abs_zero = 1e-10`, `rel_err = 1e-10`.
5. A `PetscJacobianTester` confirms the correction Jacobian is the exact derivative of the
   correction residual.
6. An explicit failure is caught if the element average is silently replaced by the local
   value, and if RZ/RSPHERICAL is requested before section 9.4 exists.
7. The focused CPU verification in section 7 passes.

CUDA/XPU device compilation and execution remain required before merge. CPU success is not
device validation.

## 2. Host behavior to reproduce exactly

Relevant source:

- `modules/shifted_boundary_method/src/interfacekernels/SCZMInterfaceKernelSmallStrain.C:293-316`
  - the correction Jacobian increment
- `modules/shifted_boundary_method/src/interfacekernels/SCZMInterfaceKernelSmallStrain.C:14-51`
  - `gradOpForCoord` (anonymous namespace, host only)
- `modules/solid_mechanics/include/utils/StabilizationUtils.h:16-29`
  - `elementAverage` (header only, host only)

The correction is added to the **diagonal blocks only** of the directional correction Jacobian,
inside `calculateDirectionalCorrectionJacobian(ivar, jvar, type)`. It reuses the normal
`n_tangent`, the natural (unshifted) trial gradient `grad_phi`, the row test, and the row-side
tangent `r4t` already computed there. No new block or sign rule is involved.

For residual component `i` and trial component `k`, with `C` the isotropic or row-side tangent:

```text
grad_op       = gradOpForCoord(coord_sys, k, grad_phi, phi, q_point)      # trial side
avg_grad_op   = elementAverage(qp -> gradOpForCoord(coord_sys, k,
                                                    grad_phi(qp), phi(qp), q_point(qp)),
                               JxW, coord)                                # element volume, trial side
increment     = (avg_grad_op.trace() - grad_op.trace()) / 3.0

correction_jacobian += sum_m ( C(i,0,m,m)*n0 + C(i,1,m,m)*n1 + C(i,2,m,m)*n2 ) * test * increment
```

`gradOpForCoord` builds a rank-two gradient operator for the coordinate system:

```text
XYZ:        grad_op(k, j) = grad(j)                        for j in 0..2
RZ:         grad_op(k, j) = grad(j)                        for j in 0..1
            grad_op(2, 2)  = value / point(0)
RSPHERICAL: grad_op(0, 0) = grad(0)
            grad_op(1, 1)  = value / point(0)
            grad_op(2, 2)  = value / point(0)
```

`elementAverage` is a JxW-and-`coord`-weighted mean over the **element volume**:

```text
avg = sum_qp f(qp) * JxW(qp) * coord(qp)  /  sum_qp JxW(qp) * coord(qp)
```

`coord` is the Assembly coordinate scale (1.0 in CARTESIAN, the radius in RZ). This is a
genuine element-volume reduction over quadrature points, not a face reduction.

## 3. What the spike already established

The §9.1 spike question was "can an element-volume average be computed from an oriented
interface face on the device?" The answer is yes, with caveats.

Confirmed:

1. A `Moose::Kokkos::Datum` constructed with `side = libMesh::invalid_uint` selects the
   **element-volume** quadrature count and offset. See
   `framework/include/kokkos/base/KokkosDatum.h:505-508`:

   ```cpp
   _n_qps(!isSide() ? assembly.getNumQps(_elem) : assembly.getNumFaceQps(_elem, side)),
   _qp_offset(!isSide() ? assembly.getQpOffset(_elem) : assembly.getQpFaceOffset(_elem, side)),
   _elem_property_idx(!isSide() ? _elem.id - _mesh.getStartingContiguousElementID(_elem.subdomain)
                                : assembly.getElemFacePropertyIndex(_elem, _side))
   ```

   A volume `Datum` therefore reaches `n_qps()`, `JxW(qp)`, `q_point(qp)` and volume
   properties. `KokkosMaterial.h:181` already constructs exactly this shape for element
   materials.
2. `Moose::Kokkos::Material` exposes `kokkosAssembly()` and `kokkosSystems()`
   (`framework/include/kokkos/materials/KokkosMaterial.h`), so a device volume datum can be
   constructed from inside a Kokkos material.
3. The interface face record already carries both elements:
   `face.elem` and `face.neighbor` (`framework/include/kokkos/base/KokkosInterfaceFaceCache.h:24-32`),
   so both physical sides of the average are reachable.
4. `elem->neighbor_ptr(side)` and `neighbor->which_neighbor_am_i(elem)` resolve correctly for
   both sides on a normally generated split-interface mesh, including across an MPI partition
   (`sczm_angled_kokkos.i` runs green under
   `-p2 --distributed-mesh --n-threads=2`).

Not established / unresolved:

5. `AssemblyDatum` cannot be constructed from a material. Its constructor requires a
   `Variable` (`framework/include/kokkos/base/KokkosDatum.h:638-644`), and a Kokkos material
   has no variable handle - only `_variable_value` from `kokkosCoupledValue("variable")`.
6. A test-only interface material failed to compile because `InterfaceMaterial` declares its
   own `operator()` overloads, which hide every `Moose::Kokkos::Material::operator()`
   overload by name. The inherited element/side dispatch paths then fail to resolve. The
   existing `KokkosSCZMComputeDisplacementJumpSmallStrain` compiles, so the base paths are
   evidently not instantiated for it - the mechanism is not yet understood and must be
   resolved before any new interface material is written. See section 4.1.
7. Derived Kokkos interface kernels do not receive the `InterfaceFaceRecord`. The base
   `Moose::Kokkos::InterfaceKernel::computeResidual()` has `face` in scope
   (`framework/include/kokkos/interfacekernels/KokkosInterfaceKernel.h:133-136`), but the
   derived `computeQpResidual` / `computeQpJacobian` / `computeQpOffDiagJacobian` hooks do
   not. Any kernel-side access to the average therefore needs new plumbing.

## 4. Framework gaps to close first

These are prerequisites. Do not write module code against assumptions about them.

### 4.1 The `operator()` name-hiding mechanism (blocking)

Establish exactly why `KokkosSCZMComputeDisplacementJumpSmallStrain` compiles while a newly
written interface material does not. Specifically:

- determine whether `Moose::Kokkos::Material::initStatefulProperties` and
  `computeProperties` are instantiated for a class deriving from `InterfaceMaterial`;
- determine whether the suppression is intentional, accidental, or an artifact of which
  base member functions are odr-used;
- then choose the smallest correct fix. Candidates, in order of preference:
  1. `InterfaceMaterial` re-exposes the base operators with a
     `using Moose::Kokkos::Material<...>::operator();` declaration;
  2. the derived class provides the missing `initQpStatefulProperties` /
     `computeQpProperties(qp, datum)` no-op overloads;
  3. make the inherited dispatch paths visible another way.

Do not pick by intuition. The existing material compiling is not evidence that the paths are
correct - it may be evidence they are silently never instantiated.

### 4.2 Volume-datum construction helpers

`framework/include/kokkos/base/KokkosInterfaceDatum.h` currently provides only

```text
makePrimaryInterfaceDatum     -> DatumType(face.elem, face.side, face.neighbor, ...)
makeNeighborInterfaceDatum    -> DatumType(face.neighbor, face.neighbor_side, face.elem, ...)
```

Both hard-wire the face side. Add the smallest general pair that requests the element-volume
datum of either side, i.e. constructs `DatumType(elem, libMesh::invalid_uint, neighbor, ...)`.

Do not expose broad internal state, and do not cache duplicate element-QP arrays in the module.

### 4.3 Face-record access from derived kernels

The derived hooks do not receive `face`. Options, in order of preference:

1. Pass the `InterfaceFaceRecord` into the derived
   `computeQpResidual` / `computeQpJacobian` / `computeQpOffDiagJacobian` signatures, and
   update every existing caller;
2. provide a small accessor that returns the face record for the current thread, so the
   derived signature is unchanged.

Option 1 is more explicit but touches `KokkosShiftedPenaltyInterface` and the test-only
`KokkosLinearSCZMInterfaceKernel`. Option 2 is less invasive. Decide by which keeps the
`InterfaceKernel` contract clearer; do not add both.

### 4.4 Device versions of the two host helpers

Both are host-only today:

- `gradOpForCoord` (anonymous namespace in the host kernel, returns `RankTwoTensor`). Add a
  `KOKKOS_FUNCTION` device equivalent. Prefer a small namespace-scope
  `KOKKOS_INLINE_FUNCTION` that writes into a `Moose::Kokkos::Real33`. Keep the
  `mooseError` default branch out of device code.
- `StabilizationUtils::elementAverage` (header only, host only). Add a device equivalent that
  accumulates `f(qp) * JxW(qp) * coord(qp)` and divides by the accumulated weight. The
  caller must supply `datum.JxW(qp)` and the coordinate scale.

Do not port `StabilizationUtils` wholesale. The average is three lines; adding the whole
framework utility as a device dependency is out of proportion.

## 5. Module implementation

### 5.1 The average must be a material, not a kernel-local reduction

The element average is a genuine element-volume reduction that must be evaluated on both
physical sides of every interface face and stored before the kernel runs. It cannot be
computed inside the correction Jacobian functor without re-doing the reduction once per
Jacobian entry.

Add a `Moose::Kokkos::InterfaceMaterial` that, per interface face, computes the element
average of `gradOpForCoord` for each active displacement component over the element volume of
both `face.elem` and `face.neighbor`, and writes the results through `elem_datum, qp` as
scalar face properties.

Respect the single-writer rule from the G1 architecture: the interface material is the only
writer of an oriented face, and the kernel reads everything through `elem_datum`.

### 5.2 The kernel change

Extend `KokkosSCZMInterfaceKernelSmallStrain`:

- add `volumetric_locking_correction`, default `false`, matching the host;
- read the averaged properties and the row-side Lame pair already retrieved for the
  directional correction;
- add the section 2 increment to the **diagonal blocks only**, inside the existing
  `computeDirectionalCorrectionJacobian`, reusing `n_tangent`, `grad_phi`, `test` and the
  row-side Lame values that function already holds;
- natural, unshifted trial gradients and the row test, exactly as the rest of that function
  uses. Shifted kinematics enter the base cohesive Jacobian through
  `computeDResidualDDisplacement`; do not re-apply the shift here.

Coordinate systems: the increment is coordinate-system independent. The `gradOpForCoord`
lookup still depends on the coordinate system, so RZ and RSPHERICAL cannot work until
section 9.4 lands. Reject them explicitly until then, naming section 9.4.

### 5.3 Action

- Remove the `volumetric_locking_correction` rejection at
  `modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C:219` once sections
  4 and 5 work.
- Forward `volumetric_locking_correction` to the Kokkos kernel. Preserve the existing
  inheritance-from-physics behavior of the host path, including the mismatch warning.
- Validate that the new averaging material covers the interface the way
  `validateKokkosBulkStressMaterial` does, reusing that pattern rather than adding a second
  ad-hoc check.

## 6. Tests

Place tests under `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/` and
keep `compute_devices = 'cpu cuda xpu'`. All Exodiff inputs must keep the split-interface mesh
from the current test-mesh fix; a conforming mesh makes these tests vacuous.

1. **Enable the correction on the existing angled inputs.** Add
   `volumetric_locking_correction = true` to `sczm_angled_kokkos.i` and
   `sczm_angled_host.i`, regenerate the canonical host gold, keep the Kokkos name as a relative
   symlink, retain `abs_zero = 1e-10` and `rel_err = 1e-10`.
2. **Same for the 3D inputs.**
3. **A `PetscJacobianTester` on the corrected Kokkos input**, added next to
   `sczm_hetero_jacobian_blocks`.
4. **A heterogeneous corrected pair**, mirroring `sczm_hetero_*`, to prove the row-side
   selection of the average and the Lame pair is correct.
5. **A sensitivity probe**: an input with the correction on and an otherwise identical input
   with it off must produce measurably different solutions. A test that cannot fail is not a
   test; verify this explicitly rather than inferring it from a pass.
6. **Distributed**: run the corrected 2D Kokkos case with
   `-p2 --distributed-mesh --n-threads=<n>`, per section 7 of the G1 plan.
7. **Negative**: RZ or RSPHERICAL under `volumetric_locking_correction = true` must fail
   explicitly until section 9.4 lands.

## 7. Verification procedure

From `modules/shifted_boundary_method`, after confirming the environment:

```text
./run_tests -j2 --re 'kokkos_shifted_interface'
```

Then the distributed selection, then the narrow Jacobian and negative selections while
iterating, then the complete module subset affected by the changed Action/material/kernel.

Record: exact command; build type and whether a rebuild occurred after source changes;
pass/fail counts; serial and distributed results; whether CUDA/XPU were actually executed or only
declared in `compute_devices`.

## 8. Sequencing and stop gates

Follow this order. Do not start a phase while an earlier stop gate is unresolved.

1. **Resolve 4.1**, then **4.2** and **4.3** in one small framework change. Stop gate: an
   interface material that needs a volume datum compiles, and a derived kernel can obtain the
   face record. Nothing else changes yet.
2. **4.4** device helpers. Stop gate: a device unit-level sanity check of
   `gradOpForCoord` and the average against the host formulas in CARTESIAN.
3. **5.1** averaging material. Stop gate: both side averages are finite, independently
   responsive to their own displacement field, and correct when the neighbor is across an MPI
   partition.
4. **5.2** kernel correction. Stop gate: `PetscJacobianTester` passes and the heterogeneous
   block test confirms only the diagonal blocks changed.
5. **5.3** Action. Stop gate: rejection removed, parameter forwarded, inheritance preserved.
6. **Section 6 tests**, including the sensitivity probe.

## 9. Risks and open questions

- **Blast radius of the framework changes.** Sections 4.2 and 4.3 touch shared Kokkos
  infrastructure. Every existing Kokkos interface material and kernel must still compile and
  pass. Treat the existing `kokkos_shifted_interface` suite as the regression gate for the
  framework change, and run it before any module code lands.
- **Performance.** The averaging material adds an element-volume reduction per interface face
  per component. Measure the cost on the angled 2D case against the G1 baseline and record it.
  If the average is recomputed more than once per face per timestep, that is a design error.
- **Stale checkpoints.** Recovery leaves mesh artifacts in gitignored `*_cp/` directories. A
  stale mesh checkpoint makes recovery fail with a misleading neighbor error. Delete those
  directories when a mesh change invalidates them. This cost real debugging time during the
  split-interface migration.
- **Do not claim parity with §9.4 outstanding.** The correction increment is
  coordinate-system independent, but `gradOpForCoord` is not. Advertising RZ or RSPHERICAL
  support before section 9.4 is complete would be wrong.
