# Handover to CUDA/Linux: 3D, large deformation, and SCZM

## Read this first

This is the single authoritative handoff for continuing the current work on a Linux host with an
NVIDIA GPU. Follow this document in order. Do not treat other files under `.plans/` as current
instructions unless this document links to them.

The implementation through Phase 8 is already present. The remaining job is to close the explicit
CUDA test gaps and qualify the NEML2 v3 Kokkos input path for three-dimensional small strain, large
deformation (LD), and the shifted cohesive-zone model (SCZM). A fully GPU-resident PETSc solve is a
separate, optional qualification layer.

Use these supporting documents only when needed:

- `scripts/cuda/README.md`: authoritative syntax and behavior of the CUDA build/test wrapper.
- `.plans/sczm/archive/kokkos_quantity_to_neml2.md`: historical Phase 1-9 architecture, invariants,
  and original acceptance criteria. Use it to resolve design questions, not as the execution order.
- `kokkos-cuda-stack/ATTEMPTS.md`: historical CUDA toolchain failures and known environment traps.

Do **not** use `.plans/sczm/see_this_is_enough.md`,
`.plans/sczm/just_readthis_is_enough.md`, AD plans, or other files under `.plans/sczm/archive/` as the
current task list. They describe older snapshots or separate work tracks and may contain stale SHAs,
priorities, and commands.

CPU results are useful baselines, but they are not CUDA qualification. Likewise, a binary reporting
CUDA capability is not proof that the tested path launched CUDA kernels.

## Starting point

Use branch `neml2-v3-kokkos-solid-mechanics-LD-mac`. At the time of this handoff, both the local and
remote branch point to:

```text
82908c27a1 Add Kokkos deformation-gradient NEML2 input
61d2d03fb0 Add Mandel Kokkos NEML2 strain input
2d9e3b1ddb Add Kokkos NEML2 Action input backend
```

The expected NEML2 submodule checkout is:

```text
framework/contrib/neml2 = 12f3d599fa2410a8ee30bcefa3d8ca82d4244d94
```

Confirm rather than silently changing either revision:

```bash
git fetch origin
git switch neml2-v3-kokkos-solid-mechanics-LD-mac
git pull --ff-only origin neml2-v3-kokkos-solid-mechanics-LD-mac
git rev-parse HEAD
git submodule status framework/contrib/neml2
git status --short
```

Do not use `pull --rebase` or force-push as part of qualification. If the branch or submodule differs,
record the actual SHAs with the results. Do not reset a dirty NEML2 submodule; first determine whether
its changes are intentional.

## What is already complete

The branch contains:

- a Kokkos bulk rank-two material-property gatherer;
- a single Kokkos interface gatherer that places primary and neighbor values in the established NEML2
  batch order;
- explicit `input_backend = moose|kokkos`, independent of `output_backend`;
- six-component Mandel packing for symmetric Kokkos material inputs;
- `KokkosComputeSmallStrain`;
- `KokkosComputeDeformationGradient`, computing the full 3 by 3
  `F = I + grad(u)` property;
- SCZM primary and neighbor NEML2 Actions using the Kokkos input backend;
- CPU regression coverage for the Action restrictions, bulk/interface gathering, small-strain
  Jacobian, SCZM PK2-to-PK1 oracle, and SCZM Jacobian;
- existing CUDA-capable 3D total-Lagrangian and exact-kinematics crystal-plasticity tests.

The macOS CPU runs do **not** establish that the new Kokkos input gatherers can wrap CUDA-resident
material storage safely or that NEML2 evaluates those inputs on CUDA.

## Important coverage gaps before claiming success

Do not run the current tests, observe skips, and call the branch qualified. The current test metadata
still has these gaps:

1. `neml2_action_material_plane_strain` and its Jacobian are CPU-only and two-dimensional. They do not
   qualify three-dimensional Mandel packing on CUDA.
2. `sczm_pk2_kokkos.i` explicitly sets `NEML2/device = cpu`, and its test is CPU-only. It therefore
   does not qualify the direct Kokkos deformation-gradient input on CUDA.
3. The framework interface-gathering oracle reads gathered tensor data from the host. That is suitable
   for CPU Kokkos but must not dereference CUDA device memory.
4. Existing LD `exact_kinematics` tests are valuable CUDA baselines, but currently gather deformation
   gradient through `TorchDeformationGradient`; by themselves they do not qualify the new direct
   Kokkos input backend.

Address these gaps with the smallest test changes described below. Do not add host staging to make a
CUDA test pass.

## CUDA stack gate

Use the repository wrapper rather than inventing build commands. It preserves the dedicated
from-source CUDA stack and avoids contaminating it with Conda compiler or MPI wrappers.

```bash
./scripts/cuda/moose_cuda.sh doctor
./scripts/cuda/moose_cuda.sh status
```

The host must provide an NVIDIA driver, CUDA toolkit, supported GCC/GFortran, and Miniforge Python for
the dedicated NEML2 virtual environment. Defaults target CUDA 12.4 and Ampere 8.6; override
`CUDA_DIR`, `CUDA_ARCH`, and `TORCH_CUDA_ARCH_LIST` before `init` for different hardware. Do not reuse
a prefix built for another compiler or GPU architecture.

Initialize or refresh only what is stale:

```bash
export NEML2_SUPPORT=1
./scripts/cuda/moose_cuda.sh init -j 8
./scripts/cuda/moose_cuda.sh compile test -j 8
./scripts/cuda/moose_cuda.sh compile solid_mechanics -j 8
./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j 8
```

Run `compile neml2` before the MOOSE compiles only if NEML2 source changed or its installed revision is
wrong:

```bash
./scripts/cuda/moose_cuda.sh compile neml2
./scripts/cuda/moose_cuda.sh compile test -j 8
./scripts/cuda/moose_cuda.sh compile solid_mechanics -j 8
./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j 8
```

For each executable, `--show-capabilities` must report non-false `kokkos`, `cuda`, `libtorch`, and
`neml2` capabilities. `moose_cuda.sh compile` checks these automatically. A successful build alone is
not a runtime qualification.

Use one TestHarness worker on a single GPU unless the tests intentionally request MPI ranks:

```bash
export TEST_JOBS=1
```

## Close the CUDA test gaps first

Make these changes on a short qualification commit. Preserve all existing CPU tests, gold files,
oracles, and tolerances.

### 1. Three-dimensional Mandel Action test

Add a 3D counterpart of
`modules/solid_mechanics/test/tests/kokkos/linear_elasticity/neml2_action_kokkos_linear_elasticity.i`.
Reuse the existing 3D mesh, variables, boundary conditions, and gold solution from
`neml2_kokkos_linear_elasticity_3d.i`, but replace its Torch input kernel with:

- `KokkosComputeSmallStrain` producing `neml2_strain`;
- `input_backend = kokkos`;
- `output_backend = kokkos`;
- the same three Kokkos stress-divergence kernels and NEML2 derivative mapping.

Add Exodiff and `PetscJacobianTester` coverage for both `cpu` and `cuda`. This test must excite all
three normal and all three shear Mandel components; a diagonal-only displacement field is
insufficient. Keep the Mandel order `(xx, yy, zz, yz, xz, xy)` and `sqrt(2)` shear scaling unchanged.

Success proves that the Phase 7 producer, six-component packing, model execution, Kokkos outputs, and
all displacement couplings work in 3D on CUDA.

### 2. CUDA-safe bulk and interface input checks

Keep the existing CPU host-inspection oracle. Add a CUDA execution path that verifies the gathered
values by consuming them in NEML2 and checking a normal MOOSE output, rather than reading the raw
CUDA pointer on the host.

The CUDA check must cover:

- a spatially varying full rank-two bulk property with all nine components distinct;
- distinct primary and neighbor full rank-two interface properties;
- ordering-sensitive values so row or side permutations cannot pass through averaging;
- `NEML2/device = cuda`;
- first execution and repeated execution; and
- the existing mesh-rebuild case if its oracle is CUDA-safe.

Do not add `.cpu()`, a host mirror, or a host copy between Kokkos storage and the wrapped NEML2 input.
The purpose of this test is to detect exactly that class of accidental staging or invalid pointer use.

### 3. SCZM CUDA test

Keep `sczm_pk2_kokkos.i` as the common 3D fixture. Add CUDA test coverage that overrides:

```text
NEML2/device=cuda
```

and allows `compute_devices = 'cpu cuda'` where one shared test can genuinely run on both devices. A
separate CUDA test entry is acceptable when the NEML2 device override differs, but do not duplicate
the input or gold file.

The CUDA run must retain:

- `KokkosComputeDeformationGradient`;
- `input_backend = kokkos` for both primary and neighbor Actions;
- one two-sided interface gatherer per Action, not separate primary/neighbor gatherers;
- the existing full-component PK2-to-PK1 primary and neighbor oracle;
- the existing `PetscJacobianTester` ratio and difference tolerances; and
- all three displacement variables and off-diagonal Jacobian couplings.

Run the same SCZM test in serial and with two MPI ranks plus distributed mesh. Use TestHarness flags
rather than creating another input or gold file.

If the existing PK2-to-PK1 oracle is device-safe, retain it unchanged. If it is not, move only the
final pass/fail reduction to a CUDA-safe mechanism; do not weaken component coverage.

## Qualification sequence

Run the sequence in order. Stop at the first failure so a downstream SCZM error is not used to debug a
broken stack or gatherer.

### A. Framework input-backend contract

After making the CUDA-safe oracle changes:

```bash
./scripts/cuda/moose_cuda.sh verify framework \
  'kokkos_(batch_index_map|quantity_to_neml2_bulk|interface_quantity_to_neml2|input_backend_restrictions)' \
  -j 1
```

Acceptance:

- no selected test is skipped because of `compute_device`;
- bulk all-nine-component mapping passes;
- primary/neighbor side ordering passes;
- unsupported mappings still fail during Action setup; and
- repeated execution and mesh-rebuild coverage pass.

### B. Three-dimensional small-strain Action path

Run the new 3D Action Exodiff and Jacobian tests:

```bash
./scripts/cuda/moose_cuda.sh verify solid_mechanics \
  'neml2_action_material_3d(_jacobian)?' -j 1
```

Use the actual test names if they differ. Acceptance requires both the reference solution and the
finite-difference Jacobian comparison. A 2D plane-strain pass is not a substitute.

### C. Three-dimensional large-deformation path

First run the existing numerical baseline:

```bash
./scripts/cuda/moose_cuda.sh test solid_mechanics \
  'neml2_input_gatherer.(deformation_gradient|full_tensor_bridge|total_lagrangian_jacobian|exact_kinematics)$' \
  -j 1 --heavy
```

These tests select the CUDA Kokkos execution space, but their inputs currently set the NEML2 model to
`device = cpu`. They qualify 3D total-Lagrangian Kokkos assembly, full nonsymmetric rank-two/rank-four
transfers, state advancement, and Jacobians against established numerical references; they do not by
themselves qualify CUDA NEML2 evaluation.

Then run the existing 3D LD Step 4 fixture, which explicitly uses CUDA NEML2 and CUDA Kokkos assembly
with CPU PETSc. Start with a reduced smoke problem before the default benchmark size:

```bash
./scripts/cuda/moose_cuda.sh run solid_mechanics \
  modules/solid_mechanics/test/tests/kokkos/plasticity_LD/step4_plasticity_gpu_neml2_kokkos_cpu_petsc.i \
  N=8 Executioner/num_steps=2 Executioner/dt=5e-3 Outputs/exodus=false
```

The Step 4 run must converge with finite residuals and show both Kokkos and NEML2/PyTorch CUDA kernels
in Nsight. Its input still uses `TorchDeformationGradient`, so it qualifies the deployed LD CUDA stack,
not the new direct `input_backend = kokkos` transfer. That direct transfer is qualified by the bulk and
SCZM tests in sections A and D.

Also run the stabilized numerical baseline if F-bar is in the intended deployment scope:

```bash
./scripts/cuda/moose_cuda.sh test solid_mechanics \
  'neml2_input_gatherer.(total_lagrangian_jacobian_stabilized|fbar_cp_jacobian|exact_kinematics_stabilized)$' \
  -j 1 --heavy
```

### D. Three-dimensional SCZM direct Kokkos input

Run the CUDA-enabled SCZM PK2 test and Action-mode checks:

```bash
./scripts/cuda/moose_cuda.sh verify shifted_boundary_method \
  'sczm_pk2_(kokkos_jacobian|action_modes)' -j 1
```

Then run the same PK2 Jacobian fixture with two ranks and distributed mesh:

```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method \
  'sczm_pk2_kokkos_jacobian' -j 2 -p2 --distributed-mesh
```

Also retain the established stateful interface regression, running its serial and distributed forms
with their intended process counts:

```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method \
  'sczm_stateful_serial' -j 1

./scripts/cuda/moose_cuda.sh test shifted_boundary_method \
  'sczm_stateful_distributed' -j 2 -p2 --distributed-mesh
```

If a test reports `SKIP`, inspect the reason. A skip caused by `compute_devices`, `heavy`, MPI slots,
or mesh mode is not a pass.

Acceptance requires serial and distributed success, both interface sides, the full PK2 and tangent
oracle, and the unchanged Jacobian tolerances.

## Prove CUDA execution with Nsight Systems

`nvidia-smi` is not evidence for short MOOSE runs because its polling interval can miss all kernels.
Profile at least one representative test from each of these paths:

- 3D Mandel Action;
- 3D LD Step 4 with CUDA NEML2; and
- 3D SCZM PK2.

For example:

```bash
mkdir -p cuda-qualification
nsys profile --force-overwrite=true --trace=cuda,nvtx,mpi \
  -o cuda-qualification/sczm_pk2 \
  ./scripts/cuda/moose_cuda.sh run shifted_boundary_method \
  modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_pk2_kokkos.i \
  NEML2/device=cuda Outputs/exodus=false

nsys stats --report cuda_gpu_kern_sum cuda-qualification/sczm_pk2.nsys-rep \
  > cuda-qualification/sczm_pk2.cuda-kernels.txt
```

The report must contain MOOSE Kokkos kernels and CUDA work from the NEML2/PyTorch evaluation. Record
the report, command, GPU model, driver, CUDA version, branch SHA, and NEML2 SHA. Do not require a
performance speedup for correctness qualification.

## Optional full-GPU PETSc solve

The correctness qualification above may use CPU PETSc matrices and vectors while Kokkos assembly and
NEML2 execute on CUDA. Qualify the full solve separately with the existing LD Step 5 driver:

```bash
MESH_N=8 NUM_STEPS=2 DT=5e-3 \
  modules/solid_mechanics/test/tests/kokkos/plasticity_LD/run_step5.sh
```

This uses Kokkos vectors and `aijkokkos`. The current script passes `-use_gpu_aware_mpi 0`, so MPI may
stage communication through host memory. That is valid for a single-GPU functional smoke test but is
not GPU-aware-MPI performance qualification.

For multi-rank GPU-resident PETSc, first verify:

```bash
kokkos-cuda-stack/prefix/bin/ompi_info \
  | grep -iE 'opal_built_with_cuda_support|MPI extensions'
```

Only remove `-use_gpu_aware_mpi 0` after the installed MPI reports CUDA support and PETSc, libMesh,
NEML2, and MOOSE have all been built against that same MPI. Do not change solver type or tolerances to
hide a numerical failure.

## Failure triage

Use the first failing layer:

| Symptom | Check first |
| --- | --- |
| CUDA/NEML2 capability is false | `doctor`, `status`, build identity, and NEML2 venv |
| Illegal address or segfault at first gather | Kokkos pointer memory space, tensor device, and absence of host dereference |
| CPU passes but CUDA gives permuted values | row-major 3 by 3 packing, Mandel order/factors, batch offsets, and side ordering |
| First execution fails but later execution works | index-generator execution group and device offset-map initialization |
| Mesh-rebuild failure | invalidation and regeneration of face/element batch maps |
| Neighbor-only SCZM mismatch | primary-QP to neighbor-storage-QP bijection and neighbor material data selection |
| Jacobian mismatch | do not loosen tolerance; isolate input tensor, NEML2 derivative, PK2-to-PK1 conversion, then kernel contraction |
| Two-rank failure only | distributed interface ownership/ghosting and CUDA-aware MPI configuration |
| Test unexpectedly skips | `compute_devices`, capabilities, `--heavy`, `-p`, `-j`, and `--distributed-mesh` |
| `aijkokkos` MPI error 76 | MPI is not CUDA-aware; restore `-use_gpu_aware_mpi 0` or rebuild the full stack consistently |

Build and runtime logs are written under `kokkos-cuda-stack/logs/`. Historical stack failures and
known toolchain traps are documented in `kokkos-cuda-stack/ATTEMPTS.md`.

## Definition of done

CUDA qualification is complete only when all of the following are recorded:

- exact MOOSE and NEML2 SHAs and a clean explanation of any local changes;
- non-false `kokkos`, `cuda`, `libtorch`, and `neml2` capabilities;
- CUDA-safe bulk and two-sided interface input tests with ordering-sensitive values;
- a 3D six-component Mandel Action Exodiff and Jacobian pass;
- the 3D exact-kinematics LD numerical baseline and Jacobian pass, plus a converged LD Step 4 run with
  CUDA NEML2;
- a 3D SCZM PK2 serial pass with the existing full-component oracle and Jacobian tolerance;
- the same SCZM path passing with two MPI ranks and distributed mesh;
- Nsight reports proving CUDA kernels for 3D Mandel, LD, and SCZM paths;
- no host staging added between Kokkos material storage and the wrapped NEML2 input tensor; and
- a clear statement of whether PETSc remained on CPU, used staged `aijkokkos`, or used GPU-aware MPI.

Do not report CUDA qualification from build success, CPU tests, skipped tests, `nvidia-smi`, or the LD
Torch-input baseline alone.
