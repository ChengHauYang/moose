# Implementation Plan: KokkosQuantityToNEML2 Generalization

This document outlines the detailed implementation plan for expanding the `KokkosQuantityToNEML2` gatherer in the NEML2-MOOSE Kokkos integration.

## 1. Fix `num_qps` Calculation (Completed)
- [x] Use `_buffer.size() / num_elems` instead of `_element_batch_offset[1] - _element_batch_offset[0]`.
- [x] Test with a single-element mesh to ensure it does not access `_element_batch_offset[1]` and cause an out-of-bounds error.

## 2. Verify Current `Real, state=0` Path
- [x] Test with `output_backend = kokkos`.
- [x] Use a current `Real` VARIABLE.
- [x] Verify that MOOSE successfully instantiates `KokkosRealToNEML2`.
- [x] Verify that NEML2 can directly read the device buffer on the device.
- [x] Verify that the result matches the host backend (`MOOSERealToNEML2`).

## 3. Test Fallback Behavior
- [x] When `output_backend = default` or `host`, ensure the system automatically falls back to using `MOOSERealToNEML2`.
- [x] For scalar, Function, old variable, and non-`Real` inputs, verify that the host gatherer is still used.
- [x] Ensure the code still compiles and runs correctly in environments without Kokkos enabled.

## 4. Implement Old State Support (state = 1)
- [x] Modify `KokkosQuantityToNEML2.h` to pass the corresponding tag based on the `state` when constructing `_v`:
  ```cpp
  _v(prepareVariableAndState<T, state>(this->_fe_problem, _tid, params.get<std::string>("from_moose")),
     stateTagName(state))
  ```
- [x] Add the Factory registration for `state = 1` in `KokkosQuantityToNEML2.K`:
  ```cpp
  template class KokkosQuantityToNEML2<Real, 1>;
  registerKokkosUserObject("MooseApp", KokkosOldRealToNEML2);
  ```
- [x] Modify `NEML2Action.C` to relax the condition so that `history_order == 1` can also select the Kokkos gatherer.
- [x] Write tests: Verify that current and old values are correctly distinguished and passed.

## 5. Implement Other Scalar / State Versions
- [x] Implement `state = 2` (corresponding to `Moose::OLDER_SOLUTION_TAG`), and add `KokkosOlderRealToNEML2`.
- [x] Add a safeguard: If `state >= 3`, throw a clear `mooseError` (since MOOSE does not support this by default).
- [x] Test that historical data is correct across timesteps, nonlinear iterations, and restarts.

## 6. Evaluate and Implement Vector Support (RealVectorValue)
- [x] Create template traits for `_v` to switch the `VariableValue` type based on `T`:
  - `T = Real` -> `Moose::Kokkos::VariableValue`
  - `T = RealVectorValue` -> `Moose::Kokkos::VectorVariableValue` (returns `Real3`)
- [x] Modify the Tensor shape logic in `gatheredData()`:
  - If `T == Real`, the shape is `[num_elems, num_qps]`.
  - If `T == RealVectorValue`, expand the shape to `[num_elems, num_qps, 3]`.
- [x] Verify that the memory layout can be safely zero-copied to `at::from_blob` (it is known that `Real3` is a tightly packed `double[3]`).
- [x] Register `KokkosRealVectorValueToNEML2`.
- [x] Write tests: Compare results against the host backend.

## 7. Confirm Tensor / Material Property Support Strategy
- [x] Clearly distinguish between "MOOSE Variable" and "MOOSE Material Property". The current class is designed for Variables.
- [x] If support for `RankTwoTensor` or other Tensor types is definitely needed in the future, evaluate whether to design a dedicated `KokkosMaterialPropertyToNEML2` to avoid violating the single responsibility principle and overengineering.

## 8. Complete Factory Registration
- [x] Ensure that every new specialization has a corresponding `template class` instantiation.
- [x] Ensure that the names in `registerKokkosUserObject` perfectly match the strings dynamically assembled by `NEML2Action`:
  - `KokkosRealToNEML2`
  - `KokkosOldRealToNEML2`
  - `KokkosOlderRealToNEML2`
  - `KokkosRealVectorValueToNEML2`
  - `KokkosOldRealVectorValueToNEML2`
  - `KokkosOlderRealVectorValueToNEML2`

## 9. Test Data Shape and Layout
- [ ] Test meshes with multiple elements and multiple quadrature points.
- [ ] Test cases where elements have different or the same number of QPs.
- [ ] Verify that the element-major, QP-minor order perfectly matches the layout produced by the host gatherer.
- [ ] Verify that the input shape received by the NEML2 model matches its expected definition.

## 10. Test GPU / CPU Kokkos
- [ ] Test Kokkos host execution (`--kokkos-threads`).
- [ ] Test CUDA GPU execution (`--kokkos-device-id`).
- [ ] Verify that no unnecessary device-to-host or host-to-device memory copies occur during the gather process on the GPU path.
- [ ] Ensure tensor device, dtype, and CUDA device index are correctly configured.

## 11. Test Execution Timing and Synchronization
- [ ] Test `EXEC_INITIAL`.
- [ ] Test `EXEC_LINEAR`.
- [ ] Test `EXEC_NONLINEAR`.
- [ ] Verify that Kokkos variable caching and updating are fully completed before the gatherer executes.

## 12. Test Failures and Exceptions
- [ ] Use an unregistered gatherer type (test error catching).
- [ ] Pass an unsupported input type.
- [ ] Test with an empty mesh or 0 elements.
- [ ] Test the edge case where `num_qps == 0`.
- [ ] Attempt to run GPU configurations in an environment that does not support CUDA.
- [ ] Verify that all error messages are clear and that no silent fallbacks or segmentation faults occur.
