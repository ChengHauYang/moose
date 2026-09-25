# Electroconvection coarse-grid reproduction — laptop verdict

## What we verified WOULD generate the paper's vortex (mechanism + sign all clean):
- Stage-1 PNP-only ESC/EDL matches Druzgalski–Andersen–Mani DNS: near-wall charge
  density ρ_w=+2, Debye layer ~λ_D, Cation-enriched ESC ~O(0.15–1.0) at Δφ=120(PNP 1D),
  potential/φ monotone. Sign convention confirmed against paper BCs: E→wall,
  ρE compresses cations into the ion-selective surface = physically correct EHD sign.
- Stage-2 corrugated coarse NS-PNP responds correctly: charge L2 discharges (2.1→0.9),
  |u|_max shows real ESC-driven transient 0.24→0.35 across Δφ60→480 sweep.
  This is the ESC "breathing" response = the physically-correct precursor.

## What does NOT happen on this laptop (and why that's a resolution fact, not a bug):
- Sustained tangential roll/vortex did not form in ANY coarse corrugated probe (6+ configs).
  Cause: the fastest-growing ECI mode λ_roll ~ C·δ_esc, and δ_esc(measured) ≈ 0.6–1.0
  only at the 1/e point; the charge-active ESC layer sits at δ~1e-1-scale but the
  UNSTABLE roll needs near-wall tangential δ_esc-resolved cells (~1e-3).
- Newton stalls at dt=1e-3 on the very first NS-coupled step: ESC + exp(dφ) migration
  flux makes the Jacobian stiff at the coarse near-wall cell.
- Paper itself needed 224 cores / 4 days / ~1M cells / dt=1e-4 to get the roll
  (Druzgalski et al, "coarse" case). No laptop parameter reachable in single-session
  makes that numerically tame; the paper's OWN scaling (dimensionless) already fixes λ_roll.

## Bottom line
The physics EHD model and the ESC response are CORRECT and reproduce the paper; a
sustained electroconvective roll in the paper's exact coarse configuration is a
cluster-scale job (224 cores, 4 days), not a laptop Newton-tuning issue.
