# Design & Implementation Plan: Kokkos AD SCZM (Automatic Differentiation Path)

## 0. Core Development Principle: "General to Framework, SBM-Specific to SBM, Zero Touches to Upstream Existing Code"

1. **General to Framework**:
   - 通用的雙邊介面 AD 裝配機制放在 Framework（`framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h` 與 `.K`）。
   - 供任何模組（SBM、接觸、熱傳等）未來復用。
2. **SBM-Specific to SBM**:
   - Shifted 幾何外推、SCZM 物理核心、純彈性法則與全域旋轉材料全部留在 SBM 模組（`modules/shifted_boundary_method/`）。
3. **Zero Touches to Upstream Existing Code（不碰原生現有代碼）**:
   - **嚴格不修改**任何由 upstream MOOSE 團隊維護的原生核心檔案（如 `KokkosResidualObject.h`、`KokkosDatum.h`、`KokkosSystem.K`、`KokkosKernel.h` 等）。
   - 原先規劃在 `KokkosResidualObject.h` 增加的 `accumulateTaggedInterfaceMatrix`，**完全改為封裝在 `KokkosADInterfaceKernel` 內部的私有/保護方法**，透過 `InterfaceKernelBase` 繼承鏈直接存取 `kokkosSystem()`、`_matrix_tags` 與 `::Kokkos::atomic_add`。
   - 所有在 Framework 增加的代碼均為**全新獨立檔案**，現有檔案零 diff。

---

## 1. 架構分層與責任歸屬 (Hierarchy & Responsibility)

```text
========================================================================================
[Layer 1: Framework General - 全新獨立檔案，零動原生代碼]
Path: framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h
      framework/src/kokkos/interfacekernels/KokkosADInterfaceKernel.K
Base: InterfaceKernelBase (已存在於 framework)
職責:
  - 封裝通用雙邊介面的 AD 殘差與 Jacobian 裝配循環 (ResidualLoop)。
  - 封裝 AD 雙數局部矩陣累加器 (accumulateTaggedADInterfaceMatrix)，自給自足，不依賴改動 KokkosResidualObject。
  - 對標原生 KokkosADKernel 的執行模式，支援 computeResidual()、computeJacobian()、computeResidualAndJacobian()。
  - 提供衍生類別純介面 computeQpResidual<Derived>(type, i, qp, neighbor_qp, elem_datum, neighbor_datum) -> ADReal。

========================================================================================
[Layer 2: SBM Module Specific - Shifted 幾何與 SCZM 物理]
Path: modules/shifted_boundary_method/include/kokkos/interfacekernels/
      modules/shifted_boundary_method/src/kokkos/interfacekernels/
類別:
  1. KokkosADShiftedInterfaceKernel:
     - 繼承自 Moose::Kokkos::ADInterfaceKernel。
     - 負責 device-resident distance cache、trueNormal、areaCorrection 等 shifted 幾何計算。
  2. KokkosADSCZMInterfaceKernelSmallStrain:
     - 繼承自 KokkosADShiftedInterfaceKernel。
     - 實現 computeQpResidual，消耗 _traction_global (ADReal) 與可選的 _stress (ADReal) 方向修正。
     - 完全省略解析導數與手動切線，由 ADReal 自動求導。

========================================================================================
[Layer 3: SBM Module Specific - AD CZM 材料鏈]
Path: modules/shifted_boundary_method/include/kokkos/materials/
      modules/shifted_boundary_method/src/kokkos/materials/
類別:
  1. KokkosADSCZMComputeDisplacementJumpSmallStrain:
     - 輸出 interface_displacement_jump 為 MaterialProperty<ADReal, 1>。
  2. KokkosADPureElasticTractionSeparation:
     - 輸出 interface_traction 為 MaterialProperty<ADReal, 1>。
  3. KokkosADCZMComputeGlobalTractionSmallStrain:
     - 輸出 traction_global 為 MaterialProperty<ADReal, 1>。

========================================================================================
[Layer 4: SBM Module Specific - Action 整合與測試]
Path: modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C
      modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/
職責:
  - 解鎖 use_kokkos && use_automatic_differentiation 組合，自動挑選 Kokkos AD 物件。
  - 建立對照 host 端 angled_sczm_ad_elastic.i 的高精度迴歸測試。
```

---

## 2. 詳細技術實作設計

### 2.1 Framework: `Moose::Kokkos::ADInterfaceKernel` 內部矩陣累加設計

在 `KokkosADInterfaceKernel.h` 內部自給自足，直接使用已繼承的屬性與 `kokkosSystem()`，完全無需修改 `KokkosResidualObject.h`：

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
        const auto col = local_ke.raw_index(j); // AD 雙數已自帶全域 column DOF 編號
        ::Kokkos::atomic_add(&sys.getMatrixValue(row, col, tag),
                             local_ke.raw_at(j) * scaling_factor);
      }
  }
}
```

### 2.2 Framework: `ADInterfaceKernel` 執行與並行循環

- 對標原生 `KokkosInterfaceKernel` 的面分塊與執行緒切分策略。
- 在 `operator()(ResidualLoop)` 中：
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
- 在 `computeResidualInternal` 中：
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

### 2.3 SBM Module: `KokkosADSCZMInterfaceKernelSmallStrain` 殘差與方向修正

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

## 3. 實作步驟與驗證驗收 (Step-by-Step Verifiable Goals)

### Phase 1: Framework 通用 AD 介面核心 (General to Framework)
- **實作**：
  - 新增 `framework/include/kokkos/interfacekernels/KokkosADInterfaceKernel.h`。
  - 新增 `framework/src/kokkos/interfacekernels/KokkosADInterfaceKernel.K`。
  - 在 `test/include/interfacekernels/` 與 `test/src/` 新增通用測試物件 `KokkosADLinearInterfaceDiffusion`。
- **驗證**：
  - 在 `test/tests/kokkos/interfacekernels/` 建立 `PetscJacobianTester` 測試，驗證 AD 自動微分算出的介面 Jacobian 與有限差分完全吻合。
- **邊界確認**：`git diff framework/` 確保只有新檔案，原生檔案 0 修改。

### Phase 2: SBM Shifted AD 基底與 SCZM 核心 (SBM Specific)
- **實作**：
  - 新增 `modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosADShiftedInterfaceKernel.h`。
  - 新增 `KokkosADSCZMInterfaceKernelSmallStrain.h` 與 `.K`。
- **驗證**：
  - 獨立實例化並搭配既有測試材料，確保編譯通過與 device lambda 無 nvlink 警告。

### Phase 3: SBM AD CZM 材料鏈 (SBM Specific)
- **實作**：
  - 新增 `KokkosADSCZMComputeDisplacementJumpSmallStrain`。
  - 新增 `KokkosADPureElasticTractionSeparation`。
  - 新增 `KokkosADCZMComputeGlobalTractionSmallStrain`。
- **驗證**：
  - 驗證各材料屬性均正確傳遞 `ADReal` 並帶有變數導數資訊。

### Phase 4: Action 整合與端對端物理驗證 (SBM Specific)
- **實作**：
  - 修改 `modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C`，在 `_use_kokkos && _use_AD` 時構建上述 Kokkos AD 物件。
  - 移植或鏡像建立測試 `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/angled_sczm_ad_kokkos.i`。
- **驗證**：
  - 與 host 端 [`angled_sczm_ad_elastic.i`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/test/tests/shifted_cohesive_zone/angled_sczm_ad_elastic.i) 的結果以 Exodiff 嚴格對比（`rel_err = 1e-10, abs_zero = 1e-10`）。

### Phase 5: 多執行緒與 MPI 分散式網格驗證
- **驗證**：
  - 執行 `-p2 --distributed-mesh` 與 `--n-threads=2`，確認跨 rank 的 ghost element 與原子累加無競爭或記憶體錯誤。

### Phase 6: 文檔與 SQA 合規（為各回本家做準備）
- **實作**：
  - 撰寫 `KokkosADInterfaceKernel.md`（Framework 專用）。
  - 撰寫 `KokkosADSCZMInterfaceKernelSmallStrain.md` 等（SBM 專用）。
  - 測試規格內標註明確的 `requirement`、`design`、`issues`。

---

## 4. PR 切分與「各回本家」規劃

依據此設計，未來的 Upstream PR 邊界非常乾淨：
1. **PR 1 (Framework PR)**：
   - 僅包含 `KokkosADInterfaceKernel`（全新檔案）+ Framework 通用測試 + `KokkosADInterfaceKernel.md`。
   - 完全不碰任何既有檔案，審查最容易通過。
2. **PR 2 (SBM Module PR)**：
   - 包含 SBM 內的 AD Shifted Kernel、SCZM AD Kernel、3 個 AD 材料、Action 支援、回歸測試與專屬 SBM 文檔。
   - 100% 侷限在 `modules/shifted_boundary_method/` 內部。
