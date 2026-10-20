# Task G - Kokkos SCZM Directional Correction via Unified NEML2 Boundary/Interface Material Pipeline

Status: **Planning & Design Approved; Implementation-ready across 3 staged PRs**
Branch: `sczm_dev_gpu`
Baseline: `4e37469f72` (NEML2 boundary/interface execution + TSL/CZM landed)
Replaces: Obsolete `KokkosSCZMComputeLinearElasticBulkStress` ad-hoc interface material design.

---

## 1. Executive Summary & Architectural Vision

Directional correction for Shifted Cohesive Zone Modeling (SCZM) requires evaluating Cauchy stress and constitutive tangents on **both physical sides** (primary and neighbor) of an interface.

Instead of introducing a narrow, hardcoded `InterfaceMaterial` (which locks SCZM to isotropic linear elasticity and duplicates constitutive calculations), this plan implements a **unified, framework-level property pipeline**:
1. **Unified Retriever**: Extend [`NEML2ToKokkosMaterialProperty`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/framework/include/neml2/materials/NEML2ToKokkosMaterialProperty.h) to support Boundary (`SideCompute`) and Interface Neighbor (`NeighborCompute`), reusing existing Kokkos property dispatch.
2. **GPU-Resident Tensor Pipeline**: Retain LibTorch CUDA batched tensor execution with zero-copy/device-to-device unpacking into Kokkos property views.
3. **Host-Like SCZM Kernel**: Modernize [`KokkosSCZMInterfaceKernelSmallStrain`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.h) to consume standard primary/neighbor material properties (`_stress`, `_stress_neighbor`, `_tangent`, `_tangent_neighbor`) via `TwoMaterialPropertyInterface`, completely decoupling SCZM kinematics from specific constitutive material models.

```
                      ┌────────────────────────────────────────────────────────┐
                      │              NEML2 Batch Execution (GPU)               │
                      │   Evaluates bulk + interface QPs in one batched call   │
                      └──────────────────────────┬─────────────────────────────┘
                                                 │ LibTorch CUDA Tensor
                                                 ▼
                      ┌────────────────────────────────────────────────────────┐
                      │             NEML2ToKokkosMaterialProperty              │
                      │            Unified Element / Side / Neighbor           │
                      └──────────────┬──────────────────────────┬──────────────┘
                                     │                          │
                     Primary Face QPs│                          │Neighbor Face QPs
                      (SideCompute)  │                          │ (NeighborCompute)
                                     ▼                          ▼
                       _kokkos_bnd_material_props   _kokkos_neighbor_material_props
                                     │                          │
                                     │ _stress                  │ _stress_neighbor
                                     │ _tangent                 │ _tangent_neighbor
                                     │                          │
                                     └───────────┬──────────────┘
                                                 │
                                                 ▼
                      ┌────────────────────────────────────────────────────────┐
                      │          KokkosSCZMInterfaceKernelSmallStrain          │
                      │ Reads _stress(elem_datum, qp)                          │
                      │ Reads _stress_neighbor(neighbor_datum, neighbor_qp)    │
                      │ Directional correction applied with zero cross-blocks  │
                      └────────────────────────────────────────────────────────┘
```

---

## 2. Core Architectural Design

### 2.1 Batch Indexing Architecture
NEML2 indexes quadrature points into a 1D batch dimension. To bridge MOOSE Kokkos dispatch to NEML2 batch entries, `NEML2ToKokkosMaterialProperty` maintains three distinct device-resident index arrays:

```cpp
Moose::Kokkos::Array<dof_id_type> _element_batch_offset;  // Size: numKokkosElements()
Moose::Kokkos::Array<dof_id_type> _side_batch_offset;     // Size: numKokkosElementSides()
Moose::Kokkos::Array<dof_id_type> _neighbor_batch_offset; // Size: numKokkosElementSides()
```

#### Mapping Logic on Host (executed during `meshChanged()` / setup):
1. **Element (Volume)**:
   $$\text{offset}[\text{elem\_id}] = \text{neml2BatchIndex}(\text{elem}\to\text{id}())$$
2. **Side (Primary Boundary/Interface)**:
   For thread $t \in [0, \text{numKokkosElementSides}())$:
   $$[\text{elem}, \text{side}] = \text{kokkosElementSideID}(t)$$
   $$\text{offset}[t] = \text{\_execute\_neml2\_model.getSideBatchIndex}(\text{ElemSide}(\text{elem}\to\text{id}(), \text{side}))$$
3. **Neighbor (Counterpart Interface Side)**:
   For thread $t \in [0, \text{numKokkosElementSides}())$:
   $$[\text{elem}, \text{side}] = \text{kokkosElementSideID}(t)$$
   $$\text{neighbor\_ptr} = \text{elem}\to\text{neighbor\_ptr}(\text{side})$$
   $$\text{neighbor\_side} = \text{neighbor\_ptr}\to\text{which\_neighbor\_am\_i}(\text{elem})$$
   $$\text{offset}[t] = \text{\_execute\_neml2\_model.getSideBatchIndex}(\text{ElemSide}(\text{neighbor\_ptr}\to\text{id}(), \text{neighbor\_side}))$$

All three mapping tables are constructed on the Host and transferred to Kokkos device memory via `.copyToDevice()`.

### 2.2 Material Property Storage & Dispatch
We reuse the native Kokkos material property storage layout in `FEProblemBase`:
* `Moose::BLOCK_MATERIAL_DATA`: volume element storage (`_kokkos_material_props`).
* `Moose::BOUNDARY_MATERIAL_DATA` / `FACE_MATERIAL_DATA`: primary face storage (`_kokkos_bnd_material_props`).
* `Moose::NEIGHBOR_MATERIAL_DATA`: neighbor face storage (`_kokkos_neighbor_material_props`).

#### Dispatch Routing in `NEML2ToKokkosMaterialProperty`:
* When `_bnd == false && _neighbor == false`: dispatches `ElementCompute` over elements using `_element_batch_offset`.
* When `_bnd == true && _neighbor == false`: dispatches `SideCompute` over boundary sides using `_side_batch_offset`.
* When `_neighbor == true`: dispatches `NeighborCompute` over interface sides using `_neighbor_batch_offset`.

#### Device Kernel Execution:
```cpp
template <unsigned int rank, bool full>
template <typename Derived>
KOKKOS_FUNCTION void
NEML2ToKokkosMaterialProperty<rank, full>::computeQpProperties(const unsigned int qp,
                                                              Datum & datum) const
{
  auto prop = _prop(datum, qp);
  dof_id_type base_batch = 0;

  if (_broadcast_output)
    base_batch = 0;
  else if (_neighbor)
    base_batch = _neighbor_batch_offset[datum.sidePropertyIdx()];
  else if (_bnd)
    base_batch = _side_batch_offset[datum.sidePropertyIdx()];
  else
    base_batch = _element_batch_offset[datum.elemID()];

  const auto batch = base_batch + static_cast<dof_id_type>(qp);

  // Unpack staged GPU tensor (Mandel or full format) into Kokkos MaterialProperty
  unpackTensorToProperty(prop, _staged_output, batch);
}
```

### 2.3 SCZM Directional Correction Integration
Modernize `KokkosSCZMInterfaceKernelSmallStrain` to achieve 1:1 host formulation parity:

#### 1. Constructor:
```cpp
_stress(getKokkosMaterialPropertyByName<Real, 2>(_base_name + getParam<MaterialPropertyName>("stress"))),
_stress_neighbor(getKokkosNeighborMaterialPropertyByName<Real, 2>(_base_name + getParam<MaterialPropertyName>("stress"))),
_tangent(getKokkosMaterialPropertyByName<Real, 4>(_base_name + getParam<MaterialPropertyName>("tangent"))),
_tangent_neighbor(getKokkosNeighborMaterialPropertyByName<Real, 4>(_base_name + getParam<MaterialPropertyName>("tangent")))
```

#### 2. Residual:
```cpp
const auto normal_tangent = normalTangent(elem_datum, qp);
const auto stress_primary = _stress(elem_datum, qp);
const auto stress_neighbor = _stress_neighbor(neighbor_datum, neighbor_qp);

Real directional = 0.0;
if (type == Moose::Element)
{
  for (const auto j : make_range(3))
    directional += stress_primary(_component, j) * normal_tangent(j);
  residual -= test * directional;
}
else
{
  for (const auto j : make_range(3))
    directional += stress_neighbor(_component, j) * normal_tangent(j);
  residual += test * directional;
}
```

#### 3. Jacobian:
* `ElementElement`: uses `_tangent(elem_datum, qp)` with unshifted natural $\nabla \phi$.
* `NeighborNeighbor`: uses `_tangent_neighbor(neighbor_datum, neighbor_qp)` with unshifted natural $\nabla \phi_{\text{neighbor}}$.
* `ElementNeighbor` & `NeighborElement`: strictly zero correction.

---

## 3. Critical Architectural Invariants & Checks

1. **Neighbor Property Support Gate**:
   * Verify that `getKokkosNeighborMaterialPropertyByName` correctly links to `_kokkos_neighbor_material_props`.
   * Verify `FEProblemBase::prepareKokkosMaterials` executes `_kokkos_materials[Moose::NEIGHBOR_MATERIAL_DATA]` in proper dependency order.
2. **Primary vs Neighbor QP Correspondence**:
   * For non-conforming or mirrored side orientations, verify that neighbor QP indexing adheres to `kokkosInterfaceNeighborQp(face, qp)`.
3. **Handling `interface_only = true`**:
   * When `interface_only = true`, `_elem_to_batch_index` is empty. The retriever must guard against accessing `neml2BatchIndex(elem->id())`, only activating `_side_batch_offset` and `_neighbor_batch_offset`.
4. **GPU Memory Staging**:
   * Guarantee that `_staged_output` uses `Kokkos::MemoryTraits<Kokkos::Unmanaged>` over `output.data_ptr<Real>()` on CUDA/HIP/XPU, preventing host-device PCIe round-trips.

---

## 4. Staged Implementation Roadmap (3 PRs)

### PR 1: Boundary `SideCompute` for `NEML2ToKokkosMaterialProperty`
* **Objective**: Enable single-sided boundary evaluation for NEML2 Kokkos output.
* **Scope**:
  - Add `_side_batch_offset` to `NEML2ToKokkosMaterialProperty`.
  - Populate mapping via `NEML2BatchIndexGenerator::getSideBatchIndex`.
  - Update `computeProperties()` to dispatch `SideCompute` when `_bnd == true`.
  - Allow `NEML2Action` to configure boundary-restricted Kokkos retrievers when `boundary` or `interface` is set.
* **Verification & Stop Gate**:
  - Regression test with NEML2 evaluating surface stress on external boundary.
  - Zero degradation on existing element-only NEML2 Kokkos tests.

### PR 2: Interface `NeighborCompute` & Two-Sided Property Pipeline
* **Objective**: Enable simultaneous primary and neighbor side evaluation across interfaces.
* **Scope**:
  - Add `_neighbor_batch_offset` using counterpart neighbor element/side lookup.
  - Support `_neighbor = true` dispatch to write into `_kokkos_neighbor_material_props`.
  - Update `NEML2Action` to instantiate both primary and neighbor retrievers when `interface` is declared.
  - Handle `interface_only = true` cleanly without requiring element batch indices.
* **Verification & Stop Gate**:
  - Heterogeneous test with distinct primary and neighbor bulk materials (e.g., $E_1 = 100, E_2 = 200$).
  - Assert that primary face properties and neighbor face properties have no cross-contamination.

### PR 3: SCZM Host-Like Modernization & Action Integration
* **Objective**: Wire Kokkos SCZM directional correction to standard material properties.
* **Scope**:
  - Refactor `KokkosSCZMInterfaceKernelSmallStrain` to consume `_stress`, `_stress_neighbor`, `_tangent`, `_tangent_neighbor`.
  - Update `ShiftedCohesiveZoneAction` to validate that NEML2 provides the requested stress/tangent on `interface = ${boundary}` (matching host lines 326-357).
  - Deprecate/retire mandatory declaration of `KokkosSCZMComputeLinearElasticBulkStress`.
  - Enable full nonlinear/plastic constitutive models to drive directional correction.
* **Verification & Stop Gate**:
  - Host vs. Kokkos agreement on 2D and 3D angled interface benchmarks at `rel_err = 1e-10`.
  - `PetscJacobianTester` verification of exact directional correction Jacobian.

---

## 5. Comprehensive Verification & Test Matrix

### 5.1 Test Specifications

| Test Case | Purpose | Success Metric |
| :--- | :--- | :--- |
| **`neml2_bnd_stress`** | PR 1 single-sided boundary stress retrieval | Matches host `NEML2ToMOOSEMaterialProperty` on boundary |
| **`neml2_two_sided_hetero`** | PR 2 verify independent primary and neighbor batch indexing | $\sigma_{\text{primary}} = 100$, $\sigma_{\text{neighbor}} = 200$ without indexing leak |
| **`neml2_interface_only`** | PR 2 verify model without volume mesh batch indices | Runs without error when `interface_only = true` |
| **`sczm_angled_linear_parity`** | PR 3 linear elastic comparison against host oracle | Host & Kokkos Exodus outputs identical (`abs_zero = 1e-10`) |
| **`sczm_angled_neml2_plastic`** | PR 3 nonlinear/viscoplastic NEML2 directional correction | Quadratic Newton convergence + exact analytic Jacobian |
| **`sczm_parallel_ghost`** | Distributed mesh MPI partition across interface (-p2) | Ghost neighbor material property matches local rank |

### 5.2 Mandatory Environmental Protocol
As required by root `AGENTS.md`:
* Before invoking `run_tests` or MOOSE executables, ask user for conda environment confirmation.
* Run serial and distributed tests:
  ```bash
  ./run_tests -j2 -p2 --distributed-mesh --re 'kokkos_shifted_interface|neml2'
  ```
* CUDA/XPU device compilation and execution required before merge.
