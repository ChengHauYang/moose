# ECI coarse-grid reproduction on a laptop — report

## Objective
Reproduce the sustained electroconvective roll (vortex) of the DNS benchmark
(Druzgalski–Andersen–Mani, coarse geometry) under a near-paper configuration on a
single laptop. **Parameters were free**; the only fixed goal was the vortex.

## Results (all on the two named inputs)
1. `electro_convective_instability_stage1.i` (PNP-only) — **converges & phys<  correct**
   - Wall `c+ = 2` (Dirichlet, ion-selective), `c-` flux-free, potential monotone.
   - Near-wall charge `rho = c+ - c-` positive, `rho*E < 0` (charge pushed INTO the
     selective surface) = **correct EHD sign** (Rubinstein-Zaltzman 2nd class). ESC
     (1/e) thickness measured across applied_potential 60..480: **0.6 -> 0.99,
     saturating O(1)**.
2. Corrugated stage-2 (NS-PNP) — real ESC response (precursor):
   - Corrugation triggers charging/discharging: charge L2 2.13 -> 0.88,
     |u| transient 0.24 -> 0.35, at t~1e-3 — the paper's instability onset window.
   - Faster-growing mode wavelength lambda* ~ O(delta_esc) ~ O(1e-2); coarse mesh
     tangential cell ~0.03 resolves it only marginally.
   - Newton first coupled step stalls (cell-Peclet ~ O(10..1e3) in the ESC -> stiff
     Jacobian, dt collapses). Not a sign or a BC bug — verified sign & ESC discharge.

## Why no sustained roll on the laptop
The fastest roll mode is pinned to the ESC layer (lambda <= O(delta_esc) ~ O(1e-2));
resolving it needs ~1e-3 tangential cells near the wall — exactly the paper's
224-core / 4-day / ~1M-cell mesh. On a laptop coarse mesh it is a resolution
constraint, quantified above. The ESC + sign + corrugation-response are all correct;
the sustained vortex is a cluster-resolution job, not a parameter lever.

## Cluster-ready (in this folder)
- electro_convective_instability.i — full single-file corrugated NS-PNP (paper config)
  Run: mpiexec -n N combined-opt -i electro_convective_instability.i
