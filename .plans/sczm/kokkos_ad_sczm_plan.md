# Design & Implementation Plan: Kokkos AD SCZM (Automatic Differentiation Path)

## 0. Core Development Principle: "General to Framework, SBM-Specific to SBM, Zero Touches to Upstream Existing Code"

1. **General to Framework**:
   - The general two-sided interface AD assembly mechanism lives in the Framework (`framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h` and `.K`).
   - So that any module (SBM, contact, heat transfer, etc.) can reuse it in the future.
2. **SBM-Specific to SBM**:
   - Shifted geometric extrapolation, the SCZM physics core, the pure-elastic law, and the global-rotation material all stay in the SBM module (`modules/shifted_boundary_method/`).
3. **Zero Touches to Upstream Existing Code**:
   - **Strictly do not modify** any native core file maintained by the upstream MOOSE team (such as `KokkosResidualObject.h`, `KokkosDatum.h`, `KokkosSystem.K`, `KokkosKernel.h`, etc.).
   - The `accumulateTaggedInterfaceMatrix` originally planned as an addition to `KokkosResidualObject.h` is **instead fully encapsulated as a private/protected method inside `KokkosADInterfaceKernel`**, accessing `kokkosSystem()`, `_matrix_tags`, and `::Kokkos::atomic_add` directly through the `InterfaceKernelBase` inheritance chain.
   - All code added in the Framework consists of **brand-new standalone files**; existing files have zero diff.

---

## 1. Architecture Layering & Responsibility Assignment (Hierarchy & Responsibility)

```text
========================================================================================
[Layer 1: Framework General - brand-new standalone files, zero changes to native code]
Path: framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h
      framework/src/kokkos/interfacekernels/KokkosADInterfaceKernel.K
Base: InterfaceKernelBase (already exists in framework)
Responsibilities:
  - Encapsulate the general two-sided interface AD residual and Jacobian assembly loop (ResidualLoop).
  - Encapsulate the AD dual-number local-matrix accumulator (accumulateTaggedADInterfaceMatrix), self-contained, with no change to KokkosResidualObject.
  - Mirror the execution model of the native KokkosADKernel, supporting computeResidual(), computeJacobian(), computeResidualAndJacobian().
  - Provide the pure interface for derived classes computeQpResidual<Derived>(type, i, qp, neighbor_qp, elem_datum, neighbor_datum) -> ADReal.

========================================================================================
[Layer 2: SBM Module Specific - Shifted geometry & SCZM physics]
Path: modules/shifted_boundary_method/include/kokkos/interfacekernels/
      modules/shifted_boundary_method/src/kokkos/interfacekernels/
Classes:
  1. KokkosADShiftedInterfaceKernel:
     - Inherits from Moose::Kokkos::ADInterfaceKernel.
     - Responsible for shifted geometry computations such as the device-resident distance cache, trueNormal, areaCorrection, etc.
  2. KokkosADSCZMInterfaceKernelSmallStrain:
     - Inherits from KokkosADShiftedInterfaceKernel.
     - Implements computeQpResidual, consuming _traction_global (ADReal) and the optional _stress (ADReal) directional correction.
     - Omits analytic derivatives and manual tangents entirely; differentiation is done automatically by ADReal.

========================================================================================
[Layer 3: SBM Module Specific - AD CZM material chain]
Path: modules/shifted_boundary_method/include/kokkos/materials/
      modules/shifted_boundary_method/src/kokkos/materials/
Classes:
  1. KokkosADSCZMComputeDisplacementJumpSmallStrain:
     - Outputs interface_displacement_jump as MaterialProperty<ADReal, 1>.
  2. KokkosADPureElasticTractionSeparation:
     - Outputs interface_traction as MaterialProperty<ADReal, 1>.
  3. KokkosADCZMComputeGlobalTractionSmallStrain:
     - Outputs traction_global as MaterialProperty<ADReal, 1>.

========================================================================================
[Layer 4: SBM Module Specific - Action integration & testing]
Path: modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C
      modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/
Responsibilities:
  - Unlock the use_kokkos && use_automatic_differentiation combination, automatically selecting the Kokkos AD objects.
  - Build a high-precision regression test against the host-side angled_sczm_ad_elastic.i.
```

---

## 2. Detailed Technical Implementation Design

### 2.1 Framework: `Moose::Kokkos::ADInterfaceKernel` internal matrix-accumulation design

Self-contained inside `KokkosADInterfaceKernel.h`, using the already-inherited members and `kokkosSystem()` directly, with no need to modify `KokkosResidualObject.h` at all:

```cpp
template <typename Derived>
KOKKOS_FUNCTION inline void
ADInterfaceKernel::accumulateTaggedADInterfaceMatrix(const DNDerivativeType & local_ke,
                                                    const ContiguousElementID row_elem,
                                                    const unsigned int i,
                                                    const Variable & row_var,
                                                    const unsigned int comp = 0) const
{
  auto & sys = kokkosSystem(row_var.sys(comp));
  const auto ivar = row_var.var(comp);
  const auto row = sys.getElemLocalDofIndex(row_elem, i, ivar);
  const auto scaling_factor = sys.getVariableScalingFactor(ivar);

  for (unsigned int t = 0; t < _matrix_tags.size(); ++t)
  {
    const auto tag = _matrix_tags[t];

    if (sys.isMatrixTagActive(tag) && !sys.hasNodalBCMatrixTag(row, tag))
      for (unsigned int j = 0; j < local_ke.size(); ++j)
      {
        const auto col = local_ke.raw_index(j); // the AD dual number already carries the global column DOF index
        ::Kokkos::atomic_add(&sys.getMatrixValue(row, col, tag),
                             local_ke.raw_at(j) * scaling_factor);
      }
  }
}
```

### 2.2 Framework: `ADInterfaceKernel` execution and parallel loop

- Mirrors the native `KokkosInterfaceKernel`'s face-blocking and thread-splitting strategy.
- In `operator()(ResidualLoop)`:
  ```cpp
  const auto & face = kokkosInterfaceFace(_thread(tid, 1));
  auto elem_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
  auto neighbor_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());

  elem_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));

  elem_datum.do_derivatives(_computing_jacobian);
  neighbor_datum.do_derivatives(_computing_jacobian);

  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Element);
  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Neighbor);
  ```
- In `computeResidualInternal`:
  ```cpp
  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
  {
    const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
    for (unsigned int i = ib; i < ie; ++i)
      local_re[i - ib] += elem_datum.JxW(qp) *
                          kernel.template computeQpResidual<Derived>(
                              type, i, qp, neighbor_qp, elem_datum, neighbor_datum);
  }

  for (unsigned int i = ib; i < ie; ++i)
  {
    if (_computing_residual)
      accumulateTaggedInterfaceResidual(local_re[i - ib].value(), row_elem, i, row_var);
    if (_computing_jacobian)
      accumulateTaggedADInterfaceMatrix(local_re[i - ib].derivatives(), row_elem, i, row_var);
  }
  ```

### 2.3 SBM Module: `KokkosADSCZMInterfaceKernelSmallStrain` residual and directional correction

```cpp
template <typename Derived>
KOKKOS_FUNCTION ADReal
KokkosADSCZMInterfaceKernelSmallStrain::computeQpResidual(const Moose::DGResidualType type,
                                                         const unsigned int i,
                                                         const unsigned int qp,
                                                         const unsigned int neighbor_qp,
                                                         AssemblyDatum & elem_datum,
                                                         AssemblyDatum & neighbor_datum) const
{
  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;

  if (!_shifted)
    return sign * test * _traction_global(elem_datum, qp)(_component);

  auto residual = sign * test * areaCorrection(elem_datum, qp) *
                  _traction_global(elem_datum, qp)(_component);

  if (_directional_correction)
  {
    const auto normal_tangent = normalTangent(elem_datum, qp);
    const auto stress = type == Moose::Element ? _stress(elem_datum, qp)
                                               : _stress_neighbor(neighbor_datum, neighbor_qp);
    ADReal directional = 0.0;
    const unsigned int dim = this->kokkosAssembly().getDimension();
    for (unsigned int j = 0; j < dim; ++j)
      directional += stress(_component, j) * normal_tangent(j);

    residual += (type == Moose::Element ? -test * directional : test * directional);
  }
  return residual;
}
```

---

## 3. Implementation Steps and Verification/Acceptance (Step-by-Step Verifiable Goals)

### Phase 1: Framework general AD interface core (General to Framework)
- **Implementation**:
  - Add `framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h`.
  - Add `framework/src/kokkos/interfacekernels/KokkosADInterfaceKernel.K`.
  - Add the general test object `KokkosADLinearInterfaceDiffusion` under `test/include/interfacekernels/` and `test/src/`.
- **Verification**:
  - Create a `PetscJacobianTester` test under `test/tests/kokkos/interfacekernels/` to verify that the interface Jacobian produced by AD automatic differentiation matches the finite difference exactly.
- **Boundary check**: `git diff framework/` to ensure only new files, with 0 modifications to native files.

### Phase 2: SBM Shifted AD base and SCZM core (SBM Specific)
- **Implementation**:
  - Add `modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosADShiftedInterfaceKernel.h`.
  - Add `KokkosADSCZMInterfaceKernelSmallStrain.h` and `.K`.
- **Verification**:
  - Instantiate independently together with existing test materials, ensuring compilation passes and the device lambdas produce no nvlink warnings.

### Phase 3: SBM AD CZM material chain (SBM Specific)
- **Implementation**:
  - Add `KokkosADSCZMComputeDisplacementJumpSmallStrain`.
  - Add `KokkosADPureElasticTractionSeparation`.
  - Add `KokkosADCZMComputeGlobalTractionSmallStrain`.
- **Verification**:
  - Verify that each material property correctly passes `ADReal` and carries variable-derivative information.

### Phase 4: Action integration and end-to-end physics verification (SBM Specific)
- **Implementation**:
  - Modify `modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C` to construct the above Kokkos AD objects when `_use_kokkos && _use_AD`.
  - Port or mirror a test `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/angled_sczm_ad_kokkos.i`.
- **Verification**:
  - Strictly compare against the host-side [`angled_sczm_ad_elastic.i`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/test/tests/shifted_cohesive_zone/angled_sczm_ad_elastic.i) results with Exodiff (`rel_err = 1e-10, abs_zero = 1e-10`).

### Phase 5: Multithreading and MPI distributed-mesh verification
- **Verification**:
  - Run `-p2 --distributed-mesh` and `--n-threads=2` to confirm that cross-rank ghost elements and atomic accumulation have no race conditions or memory errors.

### Phase 6: Documentation and SQA compliance (preparing to upstream each part)
- **Implementation**:
  - Write `KokkosADInterfaceKernel.md` (Framework-specific).
  - Write `KokkosADSCZMInterfaceKernelSmallStrain.md` etc. (SBM-specific).
  - Annotate explicit `requirement`, `design`, and `issues` in the test specifications.

---

## 4. PR Splitting and "Upstream Each Part" Plan

Based on this design, the future upstream PR boundaries are very clean:
1. **PR 1 (Framework PR)**:
   - Contains only `KokkosADInterfaceKernel` (brand-new files) + the Framework general test + `KokkosADInterfaceKernel.md`.
   - Touches no existing file at all, so it is the easiest to get through review.
2. **PR 2 (SBM Module PR)**:
   - Contains the AD Shifted Kernel, SCZM AD Kernel, the 3 AD materials, Action support, regression tests, and the dedicated SBM documentation within SBM.
   - 100% confined within `modules/shifted_boundary_method/`.
