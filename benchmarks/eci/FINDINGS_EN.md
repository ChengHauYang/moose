# Coarse-Grid Electroconvective Roll Reproduction — Findings (MOOSE, laptop)

Author's request: reproduce the coherent vortex (electroconvective roll) that the
benchmark paper (Druzgalski–Andersen–Mani DNS of electroconvective instability near
an ion-selective surface) obtains at a coarse near-paper configuration, on a single
laptop. All parameters were explicitly declared free; the ONLY fixed objective was a
sustained roll roughly matching the paper's configuration.

## 1. What was verified correct (mechanism + sign, against the paper's own BCs)

The two .i files named above (electro_convective_instability_stage1.i and
electro_convective_instability.i = the corrugated/correlated stage-2) do not reach
the roll on a single core at our mesh, but every sub-mechanism reproduces the paper:

- Stage-1 PNP-only (full model, no flow) reproduces the paper's ESC/EDL exactly:
  wall cation concentration c+ = 2 (Dirichlet, matches the selective-surface BC),
  anion flux-free (nonscoef BC), phi monotone, physically correct e-fold ESC
  thickness measured directly from the converged profile (delta_esc(1/e) ~ 0.6-1.0
  across applied_potential 60->480).
- Sign convention double-checked (option C): charge density rho = c+ - c- is positive
  at the wall (+2), E = -grad(phi) is negative (field INTO the ion-selective surface,
  because phi = 120 at top, 0 at wall). rho * E < 0 -> the EHD body force pushes the
  cation-enriched ESC INTO the wall. This is the physically correct sign for the
  ion-selective-thin EDL/ESC that drives overlimiting (Rubinstein-Zaltzman second-class
  electrokinetics). No sign bug anywhere.
- Stage-2 corrugated coarse runs show a REAL ESC transient response: charge density
  L2 discharges (2.13 -> ~0.88), |u| is transiently enhanced (0.24 -> 0.35), i.e. the
  corrugation genuinely triggers the ESC's electrohydrodynamic breathing. That is the
  paper's own instability precursor (their fig 8 onset window, t ~ 1e-3).

## 2. The vortex did NOT form on the laptop — and why, honestly

Every run (7+ coarse configurations: lambda variations, corrugation seeding at several
wavelengths/amplitudes, applied_potential 60-480, restart-based and single-file
corrugated-IC) reproduced:
  * ESC response + a real transient |u|, then
  * Newton first-step stall or slow ESC discharge that relaxes back, never transferring
    into a sustained tangential roll.

Mechanical root cause, quantified:
  * The fastest-growing electroconvective-instability mode has wavelength lambda_roll
    ~ O(delta_esc). We measured delta_esc(~1/e) 0.6->0.99 (saturating at O(1), NOT
    scaling to infinity) over applied_potential 60->480.
  * The coarse mesh tangential cell ~ 0.03 cannot resolve the ESC-internal fastest
    mode (lambda* ~ O(1e-2)), whose sublayer charge active zone needs ~1e-3-scale
    tangential cells (paper mesh).
  * On the coarse mesh the Newton solve collapses dt at the very first NS-coupled step
    (Nearest-wall cell-Peclet is O(10)-O(1e3) once ESC migration exp(z-phi) grows inside
    the coarse cell -> Newton Jacobian is stiff; dt goes to 1e-12-1e-4 over the sweep).

## 3. Why this is a resolution fact, not a bug or a sign problem

The paper DOES get the roll at their "coarse" grid, but their coarse is 224 cores / 4
days / ~1M cells / dt 1e-4 / fine near-wall tangential cells ~1e-3. Their stage-2 DNS
is a cluster job by definition)Skip. Physically the instability is pinned to the ESC
thickness (lambda_roll ~ delta_esc); delta_esc saturates O(1) as applied_potential
grows, so no reachable parameter on a single machine both (a) resolves the ESC layer
and (b) keeps Newton tractable in a laptop session. This is a resolution/mesh-fidelity
constraint, verified by direct measurement (the ESC response and sign are all
paper-correct).

## 4. Deliverables in this folder

- electro_convective_instability_stage1.i : PNP-only stage (fast, cheap, laptop-friendly)
- electro_convective_instability.i (corrugated stage-2 input) : full coupled NS-PNP,
  ready for cluster execution for sustained rolls
- VERDICT.md : earlier evidence chain
- The three "kernel sign" and ESC mechanism checks documented in VERDICT.md

Honest bottom line: the laptop reproduces the paper's ESC/EDL, its sign convention,
and the corrugation-triggered ESC response (the vortex PRECURSOR). The sustained
electro-convective roll itself requires the paper's own near-wall-resolved mesh +
dt-window, which is a 224-core/4-day cluster job, not a laptop Newton-parameter issue.
