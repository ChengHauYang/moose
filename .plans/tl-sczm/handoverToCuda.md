# Handover to CUDA/Linux: Kokkos Total-Lagrangian SCZM (TL-SCZM)

## Read this first

This is the single authoritative handoff for qualifying the newly implemented Kokkos Total-Lagrangian
Shifted Cohesive Zone Method (TL-SCZM) on a Linux host equipped with an NVIDIA GPU (`{machine} = cuda`).

The complete host and Kokkos implementation, Action integration, documentation, and regression suite
are verified and passing on macOS Apple Silicon CPU (`44 passed, 4 skipped, 0 failed` in
`kokkos_shifted_interface`, `43 passed, 0 failed` in `shifted_cohesive_zone`).

Per project rules (AGENTS.md Section 6):
> "Do not claim CUDA verification unless the CUDA build and relevant tests have passed."

This document details the exact steps to build with `./scripts/cuda/moose_cuda.sh` and verify execution
with `--compute-device=cuda` on the Linux GPU box.

---

## Current State and Branch Information

- **Branch:** `neml2-v3-kokkos-solid-mechanics-LD-mac`
- **Commit:** Working commit with all TL-SCZM source, headers, Action updates, and tests
- **NEML2 submodule checkout:**
  ```text
  framework/contrib/neml2 = 12f3d599fa2410a8ee30bcefa3d8ca82d4244d94
  ```

---

## What Has Been Implemented

1. **Pure Device Mathematical Tools:**
   [`modules/shifted_boundary_method/include/kokkos/base/KokkosCohesiveZoneModelTools.h`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/include/kokkos/base/KokkosCohesiveZoneModelTools.h)
   - Cyclic Jacobi eigensolver for symmetric $3\times 3$ matrices (`symmetricEigenvaluesEigenvectors3x3`) without dynamic heap allocation.
   - Polar decomposition $F = R U$ (`polarDecomposition3x3`).
   - Reference rotation from interface normal $N_{\text{ref}}$ (`referenceRotation`).
   - Closed-form Chen & Wheeler (1993) rotation derivative $\partial R/\partial F$ (`computedRdF`).
   - Derivative of inverse deformation gradient $\partial F^{-1}/\partial F$ (`computedFinversedF`).
   - Derivative of area ratio $\partial A_{\text{ratio}}/\partial F$ (`computeDAreaRatioDF`).

2. **Total-Lagrangian Displacement Jump Material:**
   [`KokkosSCZMComputeDisplacementJumpTotalLagrangian`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/include/kokkos/materials/KokkosSCZMComputeDisplacementJumpTotalLagrangian.h) (`.h` / `.K`)
   - Computes surrogate deformation gradient $F = \frac{1}{2}(F_{\text{elem}} + F_{\text{neigh}})$.
   - Evaluates polar decomposition $R$, reference rotation $R_{\text{ref}}$, total rotation $R_{\text{total}} = R R_{\text{ref}}$.
   - Evaluates Taylor-shifted global displacement jump $\Delta u_{\text{global}}$ and local jump $\Delta u_{\text{local}} = R_{\text{total}}^T \Delta u_{\text{global}}$.

3. **Total-Lagrangian Global Traction Material:**
   [`KokkosSCZMComputeGlobalTractionTotalLagrangian`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/include/kokkos/materials/KokkosSCZMComputeGlobalTractionTotalLagrangian.h) (`.h` / `.K`)
   - Computes reference normal $N_{\text{ref}}$ and Nanson area ratio $A_{\text{ratio}} = J \|F^{-T} N_{\text{ref}}\|$.
   - Transforms local traction to Cauchy traction $t = R_{\text{total}} t_{\text{local}}$ and first Piola-Kirchhoff traction $t_{\text{PK1}} = A_{\text{ratio}} t$.
   - Computes complete rank-two jump Jacobian $\partial t_{\text{PK1}}/\partial \Delta u_{\text{global}}$ and rank-three deformation-gradient Jacobian $\partial t_{\text{PK1}}/\partial F$.

4. **Total-Lagrangian Interface Kernel:**
   [`KokkosSCZMInterfaceKernelTotalLagrangian`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosSCZMInterfaceKernelTotalLagrangian.h) (`.h` / `.K`)
   - Assembles residual $(N_{\text{ref}} \cdot n_{\text{surr}}) t_{\text{PK1}} \psi$ and optional directional correction.
   - Evaluates complete on- and off-diagonal Jacobians across all 4 interface coupling blocks (Element-Element, Element-Neighbor, Neighbor-Element, Neighbor-Neighbor), including volumetric locking correction.

5. **Action Integration:**
   [`modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C`](file:///Users/chenghau.yang/Documents/Package/moose-sczm/modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C)
   - Dispatches to Kokkos TL objects when `strain = FINITE && use_kokkos = true`.
   - Explicitly rejects Kokkos AD combinations with `"Kokkos SCZM currently supports only non-AD objects."`.

---

## Step-by-Step Instructions on the CUDA Machine

### Step 1: Update Working Tree on Linux GPU Box

```bash
git fetch origin
git switch neml2-v3-kokkos-solid-mechanics-LD-mac
git pull --ff-only origin neml2-v3-kokkos-solid-mechanics-LD-mac
git submodule update --init framework/contrib/neml2
```

Confirm tree cleanliness and hashes:
```bash
git status --short
git submodule status framework/contrib/neml2
```

### Step 2: Environment Inspection

Verify the CUDA stack prerequisites:
```bash
./scripts/cuda/moose_cuda.sh doctor
./scripts/cuda/moose_cuda.sh status
```

Check that `nvcc`, `nvidia-smi`, `mpicc`, and CUDA-Kokkos libraries are reported `[OK]`.

### Step 3: Incremental Compilation

Compile `shifted_boundary_method` in the CUDA stack:
```bash
./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j 16
```

Verify that `shifted_boundary_method-opt` links successfully against `libshifted_boundary_method_kokkos-opt.la`.

### Step 4: CPU Baseline Test

Run the newly implemented TL-SCZM test suite with `--compute-device=cpu` as a sanity baseline:
```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl --compute-device=cpu
```
All 14 tests should pass.

### Step 5: CUDA Execution and Qualification

Run the TL-SCZM suite on the NVIDIA GPU (`--compute-device=cuda` is default in `moose_cuda.sh`):
```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl --compute-device=cuda
```

Verify individual key tests:
1. **2D Angled TL Interface (Exodiff & Jacobian):**
   ```bash
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl_2d_kokkos_matches_host --compute-device=cuda
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl_2d_jacobian --compute-device=cuda
   ```
2. **3D Angled TL Interface (Exodiff & Jacobian):**
   ```bash
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl_3d_kokkos_matches_host --compute-device=cuda
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl_3d_jacobian --compute-device=cuda
   ```
3. **Nonsymmetric PK1 Stress & Directional Correction (NEML2 on CUDA):**
   ```bash
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method sczm_tl_pk1_kokkos_jacobian --compute-device=cuda
   ```
4. **Full Kokkos Shifted Interface Suite on CUDA:**
   ```bash
   ./scripts/cuda/moose_cuda.sh test shifted_boundary_method kokkos_shifted_interface --compute-device=cuda
   ```

---

## CUDA Qualification Checklist

- [ ] `moose_cuda.sh compile shifted_boundary_method` builds clean without warnings or kernel launch attribute errors.
- [ ] No host-mesh pointers dereferenced on device (all data accessed via `AssemblyDatum`, `Moose::Kokkos::Array`, and `Kokkos::View`).
- [ ] Cyclic Jacobi eigensolver converges uniformly across all CUDA threads in a warp without divergent spin loops.
- [ ] PetscJacobianTester confirms hand-coded device Jacobians match finite differences to $\le 10^{-7}$ relative error.
- [ ] Exodiff tests reproduce gold reference solutions on GPU.
- [ ] Any test failure is classified as algorithmic vs device memory layout/race condition.
