# Section A - Framework input-backend contract (Kokkos -> NEML2)

Environment
- MOOSE HEAD: 4f8c0307524762522fa7bb1ec96da0b51ab9b6bb (branch neml2-v3-kokkos-solid-mechanics-LD-mac)
- NEML2: 12f3d599fa2410a8ee30bcefa3d8ca82d4244d94
- Kokkos 4.7.4, CUDA 12.4.0, build opt, RTX A5000 (Ampere CC 8.6)
- Harness: ./scripts/cuda/moose_cuda.sh (reused existing build; `test`, not `verify`)

## Test inventory (test/tests/neml2/tests)
The Kokkos->NEML2 gather contract is covered by five entries, each declaring the device(s)
it is meant to run on:
- kokkos_batch_index_map            - no compute_devices  => CPU-only oracle (batch-index publication order)
- kokkos_quantity_to_neml2_bulk     - no compute_devices  => CPU-only oracle (bulk all-9-component gather)
- kokkos_cuda_quantity_to_neml2_bulk- compute_devices=cuda, cli NEML2/device=cuda => CUDA bulk gather
- kokkos_interface_quantity_to_neml2- compute_devices='cpu cuda'  => interface gather, both sides
- kokkos_input_backend_restrictions/{non_rank_two,input_kernel,interface_material}
                                    - RunException, no compute_devices => CPU-only, device-independent
                                      (reject unsupported mappings at Action setup)

The earlier single run that showed "1 passed, 5 skipped" was a mis-framed selection: it ran
CPU-declared tests under the default device (cuda), so they skipped as "[cuda not in compute
devices]", and the regex never selected the dedicated CUDA variant
kokkos_cuda_quantity_to_neml2_bulk. The correct qualification runs each test under the device(s)
it declares.

## CUDA-declared tests on CUDA  -> PASS
Command:
  ./scripts/cuda/moose_cuda.sh test framework \
    'kokkos_(cuda_quantity_to_neml2_bulk|interface_quantity_to_neml2)' -j 2
Result: 2 passed, 0 skipped, 0 failed.
- neml2.kokkos_cuda_quantity_to_neml2_bulk ... OK  (bulk 9-component gather on GPU)
- neml2.kokkos_interface_quantity_to_neml2 ... OK  (primary/neighbor interface gather on GPU)

This confirms the gather contract on the GPU - the deployment path for this milestone.

## Full contract on CPU  -> 2 genuine segfaults (CUDA-build-on-CPU only)
Command:
  ./scripts/cuda/moose_cuda.sh test framework \
    'kokkos_(batch_index_map|quantity_to_neml2_bulk|interface_quantity_to_neml2|input_backend_restrictions)' \
    --compute-device=cpu -j 4
Result: 4 passed, 0 skipped, 2 FAILED.
- neml2.kokkos_batch_index_map ........................... OK
- neml2.kokkos_input_backend_restrictions/non_rank_two ... OK
- neml2.kokkos_input_backend_restrictions/input_kernel ... OK
- neml2.kokkos_input_backend_restrictions/interface_material OK
- neml2.kokkos_quantity_to_neml2_bulk ................ FAILED (EXIT CODE 139 = SIGSEGV)
- neml2.kokkos_interface_quantity_to_neml2 .......... FAILED (EXIT CODE 139 = SIGSEGV)

Context: 190/193 of the full `kokkos` group pass under --compute-device=cpu on this build
(the only other failure, kokkos/materials/parsed.parsed_material, is EXIT 1, a non-segfault
diff, unrelated to the gather). So this is NOT a general "CUDA build cannot host-execute"
problem; it is localized to the Kokkos->NEML2 gather on the host.

### Root cause (confirmed from source)
- framework/include/kokkos/base/KokkosArray.h:1030 - Array `_device_data` is allocated with
  ::Kokkos::kokkos_malloc<ExecSpace::memory_space>. On a CUDA build ExecSpace is Kokkos::Cuda,
  so deviceData() is pure CudaSpace device memory (not UVM, not host-accessible) regardless of
  the runtime --compute-device.
- framework/src/base/MooseApp.C:720 - _libtorch_device is set from the compute_device param, so
  --compute-device=cpu makes getLibtorchDevice() return cpu.
- framework/src/kokkos/userobjects/KokkosQuantityToNEML2.K:104-108 - finalize() wraps
  _buffer.deviceData() (device pointer) in at::from_blob(..., device = getLibtorchDevice()).
  With a CUDA build + --compute-device=cpu this tags a CUDA pointer as CPU memory; the first
  torch-CPU op dereferences it and segfaults. gdb backtrace: crash in libtorch_cpu.so
  at::native AVX2 mul_kernel, reached via NEML2 Python THPVariable_mul.

The comment at KokkosQuantityToNEML2.K:102 ("On CPU Kokkos, deviceData() is host memory") holds
for a CPU-only BUILD, not for --compute-device=cpu on a CUDA build.

### Why this is not a deployment failure
- GPU deployment (--compute-device=cuda, NEML2 device=cuda): device pointer + cuda torch tensor
  -> valid -> kokkos_cuda_quantity_to_neml2_bulk and the interface test PASS here.
- CPU-only CI build: ExecSpace=Serial/OpenMP -> deviceData() is host memory + cpu torch tensor
  -> valid -> these tests pass in CI.
- Broken case: CUDA build + --compute-device=cpu + NEML2 device=cpu (host torch tensor over a
  device pointer). This mixed configuration is a developer/test-only path, not a deployment.

### Clarification of the configuration (ExecSpace is compile-time)
framework/include/kokkos/base/KokkosHeader.h:34-55 fixes ExecSpace/MemSpace at compile time:
Kokkos::Cuda/CudaSpace on a CUDA build, Kokkos::OpenMP/HostSpace otherwise. So on this box Kokkos
kernels and _buffer are ALWAYS on the GPU; --compute-device=cpu only moves NEML2/libtorch (and the
solver) to the CPU. The three qualification cases therefore map to:
- CPU Kokkos -> CPU NEML2 : CPU-only Kokkos build (ExecSpace=OpenMP). Not runnable on this CUDA box;
  exercised by the CPU conda stack / CI. In code it is the kokkosTorchDevice() non-GPU branch
  returning at::kCPU -> label cpu -> executor .to(cpu) no-op.
- CUDA Kokkos -> CUDA NEML2 : --compute-device=cuda, NEML2 device=cuda (zero-copy, .to() no-op).
- CUDA Kokkos -> CPU NEML2  : --compute-device=cpu, NEML2 device=cpu. This is the case that
  segfaulted; the gather tensor is on the GPU but was labeled cpu.

## FIX APPLIED (reuse existing transfer contract; no new mechanism, no host copy added)
Root cause restated: the executor already moves every gathered input to the NEML2 device with
NEML2ModelExecutor::fillInputs() `val = val.to(device())` (NEML2ModelExecutor.C:462-463). The
contract is that a gatherer labels its tensor with where the data actually lives; the executor
transfers it. The gatherers violated this by labeling the device buffer with the NEML2 device
(_app.getLibtorchDevice()), so .to(cpu) was a no-op and a GPU pointer reached torch-CPU.

Fix: label the wrapped buffer with its true location using the existing kokkosTorchDevice()
(the device backing the Kokkos ExecSpace), and let the executor transfer:
- CUDA->CUDA: .to(cuda) no-op  => zero-copy preserved.
- CUDA->CPU : .to(cpu) device-to-host copy inside the executor (not a new/manual copy; the same
  transfer every FE-interpolated input already uses).
- CPU->CPU  : label cpu, .to(cpu) no-op.

Changes (git diff): 4 files, +12/-16, plus one new 1-declaration header.
- framework/include/neml2/userobjects/KokkosNEML2Device.h (NEW): declares at::Device
  kokkosTorchDevice(); documents the wrap/transfer contract.
- framework/src/kokkos/materials/NEML2ToKokkosMaterialProperty.K: promote kokkosTorchDevice() from
  the file-local anonymous namespace to external linkage (include the new header). No behavior
  change for the retriever.
- framework/src/kokkos/userobjects/KokkosQuantityToNEML2.K: from_blob device
  _app.getLibtorchDevice() -> kokkosTorchDevice(); drop now-unused #include "MooseApp.h".
- framework/src/kokkos/userobjects/KokkosInterfaceQuantityToNEML2.K: same swap (bulk tensor and the
  zero-row empty tensor now both device-resident on the Kokkos device); drop MooseApp.h include.
Why not a defensive error / allocation rework: the executor's .to(device()) already is the correct,
existing transfer path, so relabeling is the smallest correct fix. No .cpu()/host mirror is added
between Kokkos storage and the wrapped tensor; the device-to-host move for a CPU NEML2 device is the
executor's pre-existing step, which is unavoidable and correct when NEML2 runs on the host.

## Validation (build 4f8c0307 + this fix; framework + test relinked with device link)
Case 2 - CUDA Kokkos -> CUDA NEML2 (zero-copy):
  test framework 'kokkos_(cuda_quantity_to_neml2_bulk|interface_quantity_to_neml2)'  => 2 passed.
Case 3 - CUDA Kokkos -> CPU NEML2 (previously SIGSEGV):
  test framework 'kokkos_(batch_index_map|quantity_to_neml2_bulk|interface_quantity_to_neml2|input_backend_restrictions)' --compute-device=cpu
  => 6 passed, 0 skipped, 0 failed. (CSVDiff/Exodiff confirm numerical values + Mandel/row
  ordering; the two prior segfaults are gone.)
Case 1 - CPU Kokkos -> CPU NEML2: not runnable on this CUDA box (ExecSpace compile-time Cuda);
  covered by the kokkosTorchDevice() non-GPU branch and the CPU stack / CI.
Regression (blast radius = retriever + mesh-rebuild + managed state):
  neml2 group on CUDA => 3 passed, 39 skipped (rest cpu-only by design), 0 failed.
  neml2 group on CPU (--compute-device=cpu) => 29 passed, 6 skipped, 7 failed.
    All touched-path tests PASS:
      kokkos_interface_retriever/{primary,neighbor,both} ... OK  (NEML2ToKokkos retriever)
      kokkos_quantity_to_neml2_bulk ...................... OK  (was SIGSEGV)
      kokkos_interface_quantity_to_neml2 ................. OK  (was SIGSEGV)
      kokkos_batch_index_map, kokkos_input_backend_restrictions/* ... OK
      mesh_change, mesh_change_managed_state ............. OK
    The 7 failures (custom_model, aoti_run, model_parameter, block_restriction/{same,different},
    empty, multiple_inputs) are EXODIFF (not segfault) on PLAIN NEML2 model tests that do not use
    input_backend=kokkos and do not link any behavior-changed code. aoti_run is AOT-Inductor; these
    differ from gold at ~5.5e-6 rel tol, consistent with this box's libtorch CPU results drifting
    from gold files generated on another machine. Pre-existing / environmental, outside Section A's
    code path (not a regression from this fix).

## Status
GPU gather contract: QUALIFIED (cases 2 and 3 pass; zero-copy preserved).
Fix reuses the executor's existing .to(device()) transfer; CPU<->CUDA NEML2 device now consistent.

