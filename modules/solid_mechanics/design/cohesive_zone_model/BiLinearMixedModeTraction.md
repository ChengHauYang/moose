# BiLinearMixedModeTraction — Implementation Spec

> **Reference:** Camanho, P. P. and Davila, C. G., *Mixed-Mode Decohesion Finite Elements for the
> Simulation of Delamination in Composite Materials*, NASA/TM-2002-211737.

---

## 1. Overview

`BiLinearMixedModeTraction` computes the cohesive interface traction vector and its consistent
tangent as a function of the displacement jump across a cohesive interface, using a bilinear
traction-separation law that couples mode I (opening) and mode II/III (shear) fracture under
mixed-mode loading.

**Physical meaning / use case:** Models delamination initiation and propagation in laminated
composites or bonded interfaces. The law captures the elastic loading phase, damage onset via
a quadratic stress criterion, damage evolution under either the Benzeggagh–Kenane (B-K) or
power-law mixed-mode propagation criterion, and irreversible damage with optional viscous
regularization.

**Codebase position:**
- Inherits from `CZMComputeLocalTractionTotalBase` → `CZMComputeLocalTractionBase` →
  `InterfaceMaterial` (MOOSE).
- Registered as `BiLinearMixedModeTraction` in `SolidMechanicsApp`.
- Works inside the MOOSE CZM framework: traction and tangent are computed in the *interface
  (local) coordinate system* `[N, S1, S2]`, then rotated to the global frame by the
  surrounding framework objects.

---

## 2. Source Mapping

| File | Role |
|------|------|
| `include/materials/cohesive_zone_model/BiLinearMixedModeTraction.h` | Class interface, state/parameter declarations, `MixedModeCriterion` enum |
| `src/materials/cohesive_zone_model/BiLinearMixedModeTraction.C` | Full implementation: `validParams`, constructor, traction, Jacobian, all sub-computations |
| `include/materials/cohesive_zone_model/CZMComputeLocalTractionTotalBase.h` | Immediate base: declares `_interface_displacement_jump` and the stateful displacement jump interface |
| `include/materials/cohesive_zone_model/CZMComputeLocalTractionBase.h` | Root CZM base: declares `_interface_traction`, `_dinterface_traction_djump`, `_interface_displacement_jump` material properties |
| `src/materials/cohesive_zone_model/CZMComputeLocalTractionTotalBase.C` | Thin base: passes `validParams` up the chain |
| `test/tests/cohesive_zone_model/bilinear_mixed.i` | Primary regression input: mode I load, load-unload cycle, BK criterion |
| `test/tests/cohesive_zone_model/bilinear_mixed_scale_strength.i` | Variant: spatially varying normal strength via `GenericFunctionMaterial` |
| `test/tests/cohesive_zone_model/tests` | Test spec: defines all `[bilinear_mixed_mode]` tests (normal, shear, lag, power-law) |
| `doc/content/source/materials/cohesive_zone_model/BiLinearMixedModeTraction.md` | Narrative documentation: governing equations, B-K and power-law formulas, solver options |

---

## 3. Governing Equations

### 3.1 Coordinate system

All quantities are in the **interface coordinate system** `[N, S1, S2]`. Index mapping:

| Component | Direction | Code index |
|-----------|-----------|-----------|
| $\delta_n$ | Normal | `delta(0)` |
| $\delta_1$ | Shear 1 | `delta(1)` |
| $\delta_2$ | Shear 2 | `delta(2)` |

The scalar shear magnitude is $\delta_s = \sqrt{\delta_1^2 + \delta_2^2}$.

### 3.2 Mode mixity ratio

$$
\beta =
\begin{cases}
\dfrac{\delta_s}{\delta_n} & \delta_n > 0 \\[6pt]
0 & \delta_n \le 0
\end{cases}
$$

### 3.3 Single-mode initiation displacements

$$
\delta_n^0 = \frac{N}{K}, \qquad \delta_s^0 = \frac{S}{K}
$$

### 3.4 Mixed-mode initiation displacement $\delta_m^0$

$$
\delta_m^0 =
\begin{cases}
\dfrac{\delta_n^0\,\delta_s^0\,\sqrt{1+\beta^2}}{\sqrt{(\delta_s^0)^2 + (\beta\,\delta_n^0)^2}}
& \delta_n > 0 \\[8pt]
\delta_s^0 & \delta_n \le 0
\end{cases}
$$

This is derived by requiring that the quadratic stress criterion
$\left(\langle\tau_n\rangle/N\right)^2 + \left(\tau_s/S\right)^2 = 1$
is satisfied at the initiation point along the ray defined by $\beta$.

### 3.5 Mixed-mode final displacement $\delta_m^f$ — B-K criterion

$$
\delta_m^f =
\begin{cases}
\dfrac{2}{K\,\delta_m^0}\left[G_\text{Ic} + (G_\text{IIc} - G_\text{Ic})\left(\dfrac{\beta^2}{1+\beta^2}\right)^\eta\right]
& \delta_n > 0 \\[10pt]
\sqrt{2}\;\dfrac{2\,G_\text{IIc}}{S} & \delta_n \le 0
\end{cases}
$$

### 3.6 Mixed-mode final displacement $\delta_m^f$ — Power-law criterion

$$
G_c^\text{mixed} = \left(\frac{1}{G_\text{Ic}}\right)^\eta + \left(\frac{\beta^2}{G_\text{IIc}}\right)^\eta
$$

$$
\delta_m^f =
\begin{cases}
\dfrac{2(1+\beta^2)}{K\,\delta_m^0}\,\left(G_c^\text{mixed}\right)^{-1/\eta}
& \delta_n > 0 \\[10pt]
\sqrt{2}\;\dfrac{2\,G_\text{IIc}}{S} & \delta_n \le 0
\end{cases}
$$

### 3.7 Effective displacement jump $\delta_m$

A regularized Macaulay bracket is used: let $H(\delta_n;\alpha)$ be the regularized Heaviside
(smooth approximation of $\langle\cdot\rangle/|\cdot|$, parameter $\alpha$).

$$
\langle\delta_n\rangle_+ = H(\delta_n;\alpha)\,\delta_n
$$

$$
\delta_m = \sqrt{\delta_1^2 + \delta_2^2 + \langle\delta_n\rangle_+^2}
$$

### 3.8 Damage variable

$$
d =
\begin{cases}
0 & \delta_m < \delta_m^0 \\[4pt]
\dfrac{\delta_m^f(\delta_m - \delta_m^0)}{\delta_m(\delta_m^f - \delta_m^0)} & \delta_m^0 \le \delta_m \le \delta_m^f \\[6pt]
1 & \delta_m > \delta_m^f
\end{cases}
$$

**Irreversibility:** $d \leftarrow \max(d,\, d^\text{old})$.

**Viscous regularization** (backward-Euler discretization of $\dot{d}_v = (d - d_v)/\mu$):

$$
d_v = \frac{d + (\mu/\Delta t)\,d^\text{old}}{\mu/\Delta t + 1}
$$

$d_v$ replaces $d$ in the traction and tangent expressions.

### 3.9 Interface traction

Split the normal jump into tension and compression parts:

$$
\langle\delta_n\rangle_+ = H\,\delta_n, \qquad \langle\delta_n\rangle_- = \delta_n - \langle\delta_n\rangle_+
$$

Define:

$$
\boldsymbol{\delta}_\text{active} = \bigl(\langle\delta_n\rangle_+,\;\delta_1,\;\delta_2\bigr)^T, \qquad
\boldsymbol{\delta}_\text{inactive} = \bigl(\langle\delta_n\rangle_-,\;0,\;0\bigr)^T
$$

Then:

$$
\boxed{\mathbf{T} = (1 - d)\,K\,\boldsymbol{\delta}_\text{active} + K\,\boldsymbol{\delta}_\text{inactive}}
$$

The active part is softened by damage; the inactive (compressive normal) part is penalty-elastic
and unaffected by damage.

---

## 4. Variables and Parameters

### Input parameters

| Symbol | Code name | Meaning | Units | Constraints |
|--------|-----------|---------|-------|-------------|
| $K$ | `penalty_stiffness` | Penalty elastic stiffness (same in all modes) | force/length³ | $K > 0$ |
| $N$ | `normal_strength` | Tensile strength (mode I) | force/length² | $N > 0$ |
| $S$ | `shear_strength` | Shear strength (mode II/III, isotropic) | force/length² | $S > 0$ |
| $G_\text{Ic}$ | `GI_c` | Mode I critical energy release rate | energy/length² | $G_\text{Ic} > 0$ |
| $G_\text{IIc}$ | `GII_c` | Mode II critical energy release rate | energy/length² | $G_\text{IIc} > 0$ |
| $\eta$ | `eta` | B-K or power-law exponent | — | $\eta > 0$ |
| $\mu$ | `viscosity` | Viscous regularization time scale | time | $\mu \ge 0$ |
| $\alpha$ | `alpha` | Heaviside regularization width | length | $\alpha > 0$ |
| — | `mixed_mode_criterion` | `BK` or `POWER_LAW` | — | enum |
| — | `lag_mode_mixity` | Use old $\boldsymbol{\delta}$ for $\beta$, $\delta_m^0$, $\delta_m^f$ | — | bool (default `true`) |
| — | `lag_displacement_jump` | Use old $\boldsymbol{\delta}$ for $\delta_m$ | — | bool (default `false`) |

`GI_c`, `GII_c`, `normal_strength`, `shear_strength` are declared as `MaterialPropertyName`
parameters — they can be constants or material properties computed elsewhere.

### State variables

| Symbol | Code name | Meaning | Stateful? |
|--------|-----------|---------|-----------|
| $d$ | `damage` | Damage variable | Yes (old preserved for irreversibility) |
| $\boldsymbol{\delta}^\text{old}$ | `interface_displacement_jump` (old) | Displacement jump at previous time step | Yes (framework) |

### Computed (non-stateful) material properties per QP

| Symbol | Code name | Meaning |
|--------|-----------|---------|
| $\beta$ | `mode_mixity_ratio` | Mode mixity ratio |
| $\delta_m^0$ | `effective_displacement_jump_at_damage_initiation` | Initiation threshold |
| $\delta_m^f$ | `effective_displacement_jump_at_full_degradation` | Full-damage threshold |
| $\delta_m$ | `effective_displacement_jump` | Current effective displacement |

### Intermediate chain-rule derivatives (non-stateful, private)

| Symbol | Code name | Meaning |
|--------|-----------|---------|
| $\partial\beta/\partial\boldsymbol{\delta}$ | `_dbeta_ddelta` | `RealVectorValue` |
| $\partial\delta_m^0/\partial\boldsymbol{\delta}$ | `_ddelta_init_ddelta` | `RealVectorValue` |
| $\partial\delta_m^f/\partial\boldsymbol{\delta}$ | `_ddelta_final_ddelta` | `RealVectorValue` |
| $\partial\delta_m/\partial\boldsymbol{\delta}$ | `_ddelta_m_ddelta` | `RealVectorValue` |
| $\partial d/\partial\boldsymbol{\delta}$ | `_dd_ddelta` | `RealVectorValue` |

---

## 5. Conventions and Assumptions

- **Tension positive, compression negative** for the normal component $\delta_n = \delta[0]$.
- **Compression handling:** compressive normal opening is *not* softened by damage. The Heaviside
  split ensures $\boldsymbol{\delta}_\text{inactive}$ sees the full stiffness $K$ and
  $\boldsymbol{\delta}_\text{active}$ sees $(1-d)K$. Compression does not contribute to $\delta_m$
  or to damage evolution.
- **Mode III = Mode II:** The model is isotropic in the tangential plane. Both shear components
  $\delta_1$, $\delta_2$ share the same strength $S$ and toughness $G_\text{IIc}$. Only the scalar
  shear magnitude $\delta_s$ appears in the formulas.
- **Same stiffness in all modes:** $K_I = K_{II} = K_{III} = K$ (single penalty stiffness).
- **Isotropy:** $S = T$ (mode II = mode III shear strength). This assumption is hard-coded in
  the single `shear_strength` parameter.
- **Mode mixity convention:** $\beta = \delta_s / \delta_n$ (ratio of shear to normal opening),
  defined only for $\delta_n > 0$.
- **Damage irreversibility:** $d$ is monotonically non-decreasing. Unloading returns along a
  secant from the current damaged state to the origin.
- **No crack closure traction:** Closed cracks ($\delta_n < 0$) transmit purely compressive
  normal traction through penalty stiffness; no shear retention or friction.
- **Viscosity $\mu = 0$:** Recovers the inviscid model exactly.
- **Lag options are convergence aids only:** Lagging does not change the converged solution for
  sufficiently small time steps.

---

## 6. Residuals and Tangents

### 6.1 Traction (primary output)

$$
\mathbf{T}(\boldsymbol{\delta}) = (1-d)\,K\,\boldsymbol{\delta}_\text{active} + K\,\boldsymbol{\delta}_\text{inactive}
$$

### 6.2 Consistent tangent $\partial\mathbf{T}/\partial\boldsymbol{\delta}$

Define:
- $H = H(\delta_n;\alpha)$, $H' = \partial H/\partial\delta_n$
- $p = H + \delta_n H'$ (so $\partial\langle\delta_n\rangle_+ / \partial\delta_n = p$)
- $q = 1 - p$ (so $\partial\langle\delta_n\rangle_- / \partial\delta_n = q$)

$$
\frac{\partial\boldsymbol{\delta}_\text{active}}{\partial\boldsymbol{\delta}} =
\operatorname{diag}(p,\;1,\;1), \qquad
\frac{\partial\boldsymbol{\delta}_\text{inactive}}{\partial\boldsymbol{\delta}} =
\operatorname{diag}(q,\;0,\;0)
$$

The tangent (rank-2 tensor, output stored in `_dinterface_traction_djump`) is:

$$
\boxed{
\frac{\partial\mathbf{T}}{\partial\boldsymbol{\delta}}
= (1-d)\,K\,\operatorname{diag}(p,1,1)
  + K\,\operatorname{diag}(q,0,0)
  - K\,\boldsymbol{\delta}_\text{active} \otimes \frac{\partial d}{\partial\boldsymbol{\delta}}
}
$$

After viscous regularization, $\partial d/\partial\boldsymbol{\delta}$ is scaled by $1/(\mu/\Delta t + 1)$.

### 6.3 Derivative of $d$ — active damage zone

When $\delta_m^0 \le \delta_m \le \delta_m^f$ and $d \ge d^\text{old}$ (active loading):

$$
d = \frac{\delta_m^f(\delta_m - \delta_m^0)}{\delta_m(\delta_m^f - \delta_m^0)}
$$

Quotient rule (with $A = \delta_m^f$, $B = \delta_m - \delta_m^0$, $C = \delta_m(\delta_m^f - \delta_m^0)$):

$$
\frac{\partial d}{\partial\boldsymbol{\delta}} =
\frac{A'B + A(\boldsymbol{m} - \boldsymbol{i})}{C}
-\frac{AB\bigl[\boldsymbol{m}(\delta_m^f-\delta_m^0) + \delta_m(A'-\boldsymbol{i})\bigr]}{C^2}
$$

where $\boldsymbol{m} = \partial\delta_m/\partial\boldsymbol{\delta}$,
$\boldsymbol{i} = \partial\delta_m^0/\partial\boldsymbol{\delta}$,
$A' = \partial\delta_m^f/\partial\boldsymbol{\delta}$.

Outside the active zone (pre-initiation, fully damaged, or unloading) $\partial d/\partial\boldsymbol{\delta} = \mathbf{0}$.

### 6.4 Chain-rule derivatives for $\beta$, $\delta_m^0$, $\delta_m^f$

**$\partial\beta/\partial\boldsymbol{\delta}$** (when $\delta_n > 0$, $\delta_s > 0$):

$$
\frac{\partial\beta}{\partial\delta_n} = -\frac{\delta_s}{\delta_n^2}, \quad
\frac{\partial\beta}{\partial\delta_1} = \frac{\delta_1}{\delta_s\,\delta_n}, \quad
\frac{\partial\beta}{\partial\delta_2} = \frac{\delta_2}{\delta_s\,\delta_n}
$$

When $\delta_s = 0$ (pure mode I):
$$
\frac{\partial\beta}{\partial\delta_n} = 0, \quad \frac{\partial\beta}{\partial\delta_1} = 0, \quad \frac{\partial\beta}{\partial\delta_2} = 0
$$

**$\partial\delta_m^0/\partial\boldsymbol{\delta}$** via chain rule through $\beta$:

$$
\frac{\partial\delta_m^0}{\partial\boldsymbol{\delta}} = \frac{\partial\delta_m^0}{\partial\beta}\,\frac{\partial\beta}{\partial\boldsymbol{\delta}}
$$

$$
\frac{\partial\delta_m^0}{\partial\beta} = \delta_m^0 \cdot \beta\left[\frac{1}{1+\beta^2} - \frac{(\delta_n^0)^2}{(\delta_s^0)^2+(\beta\delta_n^0)^2}\right]
$$

**$\partial\delta_m^f/\partial\boldsymbol{\delta}$** — BK:

$$
\frac{\partial\delta_m^f}{\partial\boldsymbol{\delta}} =
\frac{\partial\delta_m^f}{\partial\delta_m^0}\,\frac{\partial\delta_m^0}{\partial\boldsymbol{\delta}}
+ \frac{\partial\delta_m^f}{\partial\beta}\,\frac{\partial\beta}{\partial\boldsymbol{\delta}}
$$

$$
\frac{\partial\delta_m^f}{\partial\delta_m^0} = -\frac{\delta_m^f}{\delta_m^0}
$$

$$
\frac{\partial\delta_m^f}{\partial\beta}\bigg|_\text{BK} =
\frac{2}{K\delta_m^0}(G_\text{IIc}-G_\text{Ic})\,\eta\left(\frac{\beta^2}{1+\beta^2}\right)^{\eta-1}
\frac{2\beta}{(1+\beta^2)^2}
$$

**$\partial\delta_m^f/\partial\boldsymbol{\delta}$** — power-law:

$$
\frac{\partial\delta_m^f}{\partial\beta}\bigg|_\text{PL} =
\frac{4\beta}{K\delta_m^0}(G_c^\text{mixed})^{-1/\eta}
+ \frac{2(1+\beta^2)}{K\delta_m^0}\cdot\frac{-1}{\eta}(G_c^\text{mixed})^{-1/\eta-1}
\cdot\eta\left(\frac{\beta^2}{G_\text{IIc}}\right)^{\eta-1}\frac{2\beta}{G_\text{IIc}}
$$

**$\partial\delta_m/\partial\boldsymbol{\delta}$** (when $\delta_m \ne 0$ and not lagged):

$$
\frac{\partial\delta_m}{\partial\delta_n} = \frac{\langle\delta_n\rangle_+ \cdot p}{\delta_m}, \quad
\frac{\partial\delta_m}{\partial\delta_1} = \frac{\delta_1}{\delta_m}, \quad
\frac{\partial\delta_m}{\partial\delta_2} = \frac{\delta_2}{\delta_m}
$$

where $p = \partial\langle\delta_n\rangle_+ / \partial\delta_n = H + \delta_n H'$.

---

## 7. Piecewise / Branching Behavior

### Initiation criterion

Damage begins when $\delta_m \ge \delta_m^0$. The threshold $\delta_m^0$ is mode-mix-dependent
(Section 3.4).

### Normal opening state (affects all quantities)

| Condition | $\beta$ | $\delta_m^0$ | $\delta_m^f$ |
|-----------|---------|-------------|-------------|
| $\delta_n > 0$ | $\delta_s/\delta_n$ | Mixed-mode formula | Mixed-mode formula (BK or PL) |
| $\delta_n \le 0$ | $0$ | $\delta_s^0 = S/K$ | $\sqrt{2}\cdot 2G_\text{IIc}/S$ |

### Damage evolution branches

| Condition | $d$ | $\partial d/\partial\boldsymbol{\delta}$ |
|-----------|-----|----------------------------------------|
| $\delta_m < \delta_m^0$ | $0$ | $\mathbf{0}$ |
| $\delta_m^0 \le \delta_m \le \delta_m^f$ AND $d \ge d^\text{old}$ | Bilinear formula | Full chain-rule expression |
| $\delta_m^0 \le \delta_m \le \delta_m^f$ AND $d < d^\text{old}$ (unloading) | $d^\text{old}$ | $\mathbf{0}$ |
| $\delta_m > \delta_m^f$ | $1$ | $\mathbf{0}$ |

### Traction branches

| Condition | Normal traction | Shear traction |
|-----------|----------------|---------------|
| $\delta_n > 0$ (tension) | $(1-d)K\delta_n$ | $(1-d)K\delta_{1,2}$ |
| $\delta_n \le 0$ (compression) | $K\delta_n$ (penalty, no damage) | $(1-d)K\delta_{1,2}$ |

Note: Shear components always see damage regardless of normal sign.

### Irreversibility

$d \leftarrow \max(d_\text{computed},\, d^\text{old})$. Implemented after computing $d$ but before
the viscous step.

### Viscous regularization

Always applied (passes through transparently when $\mu = 0$):
$$
d_v = \frac{d + (\mu/\Delta t)\,d^\text{old}}{\mu/\Delta t + 1}
$$

The $\partial d/\partial\boldsymbol{\delta}$ is divided by the same denominator.

### Lagging options

- `lag_mode_mixity = true` (default): $\beta$, $\delta_m^0$, $\delta_m^f$ computed from
  $\boldsymbol{\delta}^\text{old}$; all their derivatives w.r.t. $\boldsymbol{\delta}$ are zero.
- `lag_displacement_jump = true`: $\delta_m$ computed from $\boldsymbol{\delta}^\text{old}$;
  $\partial\delta_m/\partial\boldsymbol{\delta} = \mathbf{0}$.
- Both flags simultaneously: $\partial d/\partial\boldsymbol{\delta} = \mathbf{0}$ (fully lagged,
  simplest Jacobian, may need smaller time steps).

### Limit cases

| Case | Behavior |
|------|----------|
| $\delta_m = 0$ | $\partial\delta_m/\partial\boldsymbol{\delta} = \mathbf{0}$ (guarded by `absoluteFuzzyEqual`) |
| $\delta_s = 0$, $\delta_n > 0$ (pure mode I) | $\partial\beta/\partial\delta_{1,2}$ set to zero; $\partial\beta/\partial\delta_n = 0$ |
| $d = 1$ fully damaged | Traction: normal compression only + zero shear (if $\delta_n \le 0$); compressive traction if $\delta_n < 0$ |
| $\mu = 0$ | $d_v = d$; $\partial d_v/\partial\boldsymbol{\delta} = \partial d/\partial\boldsymbol{\delta}$ |

---

## 8. Algorithmic Implementation Notes

### Inputs (per quadrature point)

- `_interface_displacement_jump[_qp]` — current total displacement jump $\boldsymbol{\delta}$
- `_interface_displacement_jump_old[_qp]` — previous time step $\boldsymbol{\delta}^\text{old}$
- `_d_old[_qp]` — previous damage value $d^\text{old}$
- Material properties: `_GI_c[_qp]`, `_GII_c[_qp]`, `_N[_qp]`, `_S[_qp]`
- Scalar params: `_K`, `_eta`, `_viscosity`, `_alpha`, `_criterion`
- Time step: `_dt` (from MOOSE `_dt`)

### Outputs (per quadrature point)

- `_interface_traction[_qp]` — traction vector $\mathbf{T}$ (RealVectorValue)
- `_dinterface_traction_djump[_qp]` — tangent $\partial\mathbf{T}/\partial\boldsymbol{\delta}$ (RankTwoTensor)

### Internal state (stored as material properties)

- `_d[_qp]` — damage (stateful, preserved across time steps)

### Update sequence

```
computeModeMixity()            // sets _beta, _dbeta_ddelta
computeCriticalDisplacementJump()  // sets _delta_init, _ddelta_init_ddelta
computeFinalDisplacementJump()     // sets _delta_final, _ddelta_final_ddelta
computeEffectiveDisplacementJump() // sets _delta_m, _ddelta_m_ddelta
computeDamage()                    // sets _d, _dd_ddelta (with irreversibility + viscous)
computeTraction()                  // uses all of the above, returns T
computeTractionDerivatives()       // uses _d, _dd_ddelta, returns dT/ddelta
```

### Numerical stability

1. **Heaviside regularization:** The sharp Heaviside at $\delta_n = 0$ is regularized with width
   $\alpha$ (default `1e-10`). Increase $\alpha$ if Newton convergence is poor near zero opening.
2. **Division by $\delta_m$:** Guarded by `absoluteFuzzyEqual(_delta_m, 0)` — derivatives are
   set to zero at zero opening.
3. **Division by $\delta_s$:** Guarded by `absoluteFuzzyEqual(delta_s, 0)` — $\partial\beta/\partial\delta_{1,2}$ zero in pure mode I.
4. **Lagging:** Default `lag_mode_mixity = true` avoids differentiating $\delta_m^0$ and $\delta_m^f$
   w.r.t. $\boldsymbol{\delta}$, which is the primary source of Jacobian coupling complexity. The
   Jacobian is approximate but typically sufficient for Newton convergence with reasonable $\Delta t$.
5. **Viscous regularization:** Smooths the damage rate; use $\mu \approx 10^{-3}$–$10^{-4}$ times
   the characteristic time step as a starting point.
6. **`fillFromInputVector`:** In `computeTractionDerivatives`, the rank-two tensor is filled as a
   diagonal from `{p, 1, 1}` and `{q, 0, 0}` — this produces a diagonal tensor, consistent with
   the isotropic interface assumption.

---

## 9. Naming Guidance

| Math symbol | Code name | Notes |
|-------------|-----------|-------|
| $K$ | `_K` / `penalty_stiffness` | Clear |
| $N$ | `_N` / `normal_strength` | Clear |
| $S$ | `_S` / `shear_strength` | Clear; encompasses both S1 and S2 (isotropic) |
| $G_\text{Ic}$ | `_GI_c` / `GI_c` | Clear |
| $G_\text{IIc}$ | `_GII_c` / `GII_c` | Clear |
| $\eta$ | `_eta` / `eta` | Used for both B-K and power-law exponents — same symbol, same parameter |
| $\mu$ | `_viscosity` / `viscosity` | Clear; note it is a *time* scale, not dynamic viscosity |
| $\beta$ | `_beta` / `mode_mixity_ratio` | Matches convention in Camanho (2002) |
| $\alpha$ | `_alpha` / `alpha` | Heaviside regularization width — not the power-law exponent (doc uses $\alpha$ for both; code avoids collision by using `eta` for the propagation exponent) |
| $\delta_m^0$ | `_delta_init` / `effective_displacement_jump_at_damage_initiation` | Long but unambiguous |
| $\delta_m^f$ | `_delta_final` / `effective_displacement_jump_at_full_degradation` | Long but unambiguous |
| $\delta_m$ | `_delta_m` / `effective_displacement_jump` | Clear |
| $d$ | `_d` / `damage` | Clear |

**Potential ambiguity:** The doc uses $\alpha$ for the power-law exponent in the propagation
criterion (Eq. for power law), but the code uses `eta` for that exponent. In the code, `alpha`
means the Heaviside regularization parameter. Be careful when reading the documentation — the doc
equation numbering uses $\alpha$ for the propagation criterion, while the code uses `eta`.

---

## 10. Test Plan

### T1 — Zero displacement jump
- **Exercises:** Pre-initiation branch ($\delta_m < \delta_m^0$), zero traction.
- **Input:** $\boldsymbol{\delta} = (0, 0, 0)$.
- **Expected:** $\mathbf{T} = (0, 0, 0)$, $d = 0$.

### T2 — Pure mode I, elastic (below initiation)
- **Exercises:** Single-mode elastic loading, $\delta_m^0 = N/K$.
- **Input:** $\boldsymbol{\delta} = (0.5\,N/K, 0, 0)$.
- **Expected:** $T_n = 0.5\,N$, $T_s = 0$, $d = 0$.

### T3 — Pure mode I, at initiation
- **Input:** $\boldsymbol{\delta} = (N/K, 0, 0)$.
- **Expected:** $d = 0$ (initiation criterion just satisfied), $T_n = N$.

### T4 — Pure mode I, mid-softening (BK)
- **Exercises:** Bilinear damage formula in mode I.
- **For pure mode I:** $\beta = 0$, $\delta_m^0 = N/K$, $\delta_m^f = 2G_\text{Ic}/N$ (from BK formula with $\beta=0$).
- **Input:** $\delta_n = (\delta_m^0 + \delta_m^f)/2$, $\delta_1=\delta_2=0$.
- **Expected:** $d = \delta_m^f(\delta_m - \delta_m^0)/[\delta_m(\delta_m^f - \delta_m^0)]$ evaluated analytically; $T_n = (1-d)K\delta_n$.

### T5 — Pure mode II, elastic
- **Exercises:** Compression-normal path with shear; $\delta_n \le 0$ so $\beta=0$, $\delta_m^0 = S/K$.
- **Input:** $\boldsymbol{\delta} = (0, 0.5\,S/K, 0)$.
- **Expected:** $d = 0$, $T_1 = 0.5\,S$, $T_n = 0$.

### T6 — Pure mode II, full degradation
- **Exercises:** $\delta_m > \delta_m^f$ in shear-only loading.
- **Input:** $\boldsymbol{\delta} = (0, \delta_m^f + \epsilon, 0)$ where $\delta_m^f = 2G_\text{IIc}/S$ (pure shear, single component).
- **Expected:** $d = 1$, $T_1 = 0$, $T_n = 0$.

### T7 — Compression: no damage, full stiffness on normal
- **Exercises:** Compressive-normal split; damage does not affect normal compression.
- **Input:** $\boldsymbol{\delta} = (-\delta_n^0, 0, 0)$ (below zero).
- **Expected:** $T_n = K\delta_n$ (compressive, no damage), $d = 0$.

### T8 — Unloading from damaged state
- **Exercises:** Irreversibility condition $d \ge d^\text{old}$.
- **Setup:** Load to $\delta_m > \delta_m^0$ to accumulate $d^\text{old} > 0$; then unload to $\delta_m < \delta_m^0$.
- **Expected:** $d = d^\text{old}$ (held), traction consistent with secant stiffness $(1-d^\text{old})K$.

### T9 — Reloading after unloading
- **Exercises:** Reload path stays on the secant until it rejoins the original softening curve.
- **Expected:** $d$ does not increase until $\delta_m$ exceeds the previous maximum.

### T10 — BK vs. power-law, same $\beta$ and $\eta$
- **Exercises:** Both criteria produce different $\delta_m^f$ for the same mode mix.
- **Verify:** $\delta_m^f|_\text{BK} \ne \delta_m^f|_\text{PL}$ in general; both reduce to mode-I formula when $\beta = 0$.

### T11 — Jacobian consistency (finite-difference check)
- **Exercises:** Hand-coded tangent matches numerical Jacobian.
- **Method:** Perturb each component of $\boldsymbol{\delta}$ by $\epsilon = 10^{-7}$; compare
  $\Delta\mathbf{T}/\epsilon$ to `_dinterface_traction_djump[_qp]`.
- **Verify for:** elastic, active damage, unloading, pure compression, mixed-mode.

### T12 — Viscous regularization
- **Exercises:** $d_v$ formula; should converge to inviscid result as $\mu \to 0$ or $\Delta t \gg \mu$.
- **Input:** Single load step with $\mu > 0$, known $d^\text{old}$, known $d_\text{inviscid}$.
- **Expected:** $d_v = (d + (\mu/\Delta t)d^\text{old})/((\mu/\Delta t)+1)$.

### T13 — Zero toughness limit (degenerate)
- **Exercises:** $G_\text{Ic} \to 0$: $\delta_m^f \to 0$, immediate full damage at initiation.
- **Status:** *Inferred* behavior — not explicitly guarded in code; may produce $\delta_m^f < \delta_m^0$.

---

## 11. Minimal Input Example

```ini
[Mesh]
  [msh]
    type = GeneratedMeshGenerator
    dim = 2
    xmax = 1
    ymax = 2
    nx = 1
    ny = 2
  []
  [block1]
    type = SubdomainBoundingBoxGenerator
    input = msh
    bottom_left = '0 0 0'
    top_right   = '1 1 0'
    block_id = 1
    block_name = Block1
  []
  [block2]
    type = SubdomainBoundingBoxGenerator
    input = block1
    bottom_left = '0 1 0'
    top_right   = '1 2 0'
    block_id = 2
    block_name = Block2
  []
  [split]
    type = BreakMeshByBlockGenerator
    input = block2
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Physics/SolidMechanics/QuasiStatic]
  [all]
    strain = FINITE
    add_variables = true
    use_automatic_differentiation = true
  []
[]

[Physics/SolidMechanics/CohesiveZone]
  [interface]
    boundary = Block1_Block2
  []
[]

[Materials]
  [elastic]
    type = ADComputeFiniteStrainElasticStress
  []
  [elasticity]
    type = ADComputeElasticityTensor
    fill_method = symmetric_isotropic
    C_ijkl = '1e5 0.3'
  []
  [czm]
    type = BiLinearMixedModeTraction
    boundary = Block1_Block2
    penalty_stiffness = 1e6     # K  [Pa/m]
    GI_c  = 1e3                 # G_Ic  [J/m^2]
    GII_c = 1e2                 # G_IIc [J/m^2]
    normal_strength = 1e4       # N [Pa]
    shear_strength  = 1e3       # S [Pa]
    eta = 2.2                   # B-K exponent
    viscosity = 1e-3            # mu [s]
    displacements = 'disp_x disp_y'
    # mixed_mode_criterion = BK  (default)
    # lag_mode_mixity = true     (default)
  []
[]

[BCs]
  [bottom_fix_y]
    type = DirichletBC
    boundary = bottom
    variable = disp_y
    value = 0
  []
  [top_disp]
    type = FunctionDirichletBC
    boundary = top
    variable = disp_y
    function = 'if(t<=0.3, t, 0.3-(t-0.3))'
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  dt = 0.1
  end_time = 0.6
[]

[Outputs]
  exodus = true
[]
```

---

## 12. Open Questions / Mismatches

### OQ-1 — Documentation uses $\alpha$ for two different quantities

- **Source:** `doc/content/source/materials/cohesive_zone_model/BiLinearMixedModeTraction.md`,
  power-law criterion equation; and `validParams()` parameter `alpha`.
- **Issue:** The doc uses $\alpha$ as the power-law propagation exponent; the code uses `eta` for
  that exponent. The code uses `alpha` for the Heaviside regularization width. These are different
  quantities and the notation conflict exists only in the documentation.
- **Status:** `confirmed` — the code is internally consistent; the doc notation follows Camanho (2002)
  which uses $\alpha$ for the propagation exponent, but the code authors chose `eta` to avoid collision.

### OQ-2 — Pure compression, mode mixity and damage

- **Source:** `computeModeMixity()`, `computeDamage()`.
- **Issue:** When $\delta_n \le 0$, $\beta = 0$ and $\delta_m^0 = S/K$. Shear displacement can
  still accumulate damage. The compressive normal force does not arrest shear damage. Whether this
  matches physical expectation (e.g., crack closure with friction) is application-dependent.
- **Status:** `confirmed source behavior` — by design; no friction or shear retention under compression.

### OQ-3 — $\delta_m^f < \delta_m^0$ not guarded

- **Source:** `computeFinalDisplacementJump()`, `computeDamage()`.
- **Issue:** If input parameters are pathological ($G_\text{Ic}$ very small, large $K$), the
  formula for $\delta_m^f$ can produce $\delta_m^f < \delta_m^0$. The damage formula would give
  negative $d$ in the intermediate branch before being clamped. No explicit guard exists.
- **Status:** `inferred` — no guard observed in code; behavior for degenerate parameters is undefined.

### OQ-4 — Mode III not independently parameterized

- **Source:** `validParams()`, `computeModeMixity()`.
- **Issue:** The model hard-codes $T = S$ (mode III shear strength = mode II shear strength) and
  uses a single $G_\text{IIc}$ for both mode II and III. This is a simplifying assumption that
  may not hold for all composites.
- **Status:** `confirmed source behavior` — isotropic tangential response by design.

### OQ-5 — `lag_mode_mixity` is `true` by default, making the Jacobian inexact

- **Source:** `validParams()`, `computeModeMixity()`.
- **Issue:** The default configuration does not compute derivatives of $\beta$, $\delta_m^0$,
  $\delta_m^f$ w.r.t. $\boldsymbol{\delta}$ (they are zero when lagging). This means
  `_dinterface_traction_djump` is an approximation. Newton convergence is quadratic only when
  both lag flags are `false`.
- **Status:** `confirmed source behavior` — intentional convergence trade-off per documentation.

### OQ-6 — `fillFromInputVector` with 3-element vector produces diagonal tensor

- **Source:** `computeTractionDerivatives()`, line with `fillFromInputVector`.
- **Issue:** The call `fillFromInputVector({p, 1, 1})` populates only the diagonal of a
  `RankTwoTensor`. The off-diagonal entries are zero, consistent with the decoupled tangential
  directions. This is correct but worth confirming for any extension to anisotropic shear.
- **Status:** `confirmed source behavior`.

---

## 13. Porting / Integration Gaps

This section applies when porting `BiLinearMixedModeTraction` from MOOSE into another framework
(e.g., NEML2, FEniCS, deal.II, or a custom residual-based solver).

### Source-specific assumptions

- Traction and jump are in the **interface coordinate system** provided by the MOOSE CZM
  framework. The rotation from global to local is handled upstream.
- State storage uses MOOSE's `MaterialProperty` / `getMaterialPropertyOld` mechanism — stateful
  properties are automatically restored on restart and updated each time step.
- `_dt` (time step) is a MOOSE system variable; the viscous formula requires it.
- The `regularizedHeavyside` and `regularizedHeavysideDerivative` functions are from
  `MathUtils` in libMesh/MOOSE — they must be reimplemented or approximated in other frameworks.

### Target architecture mismatches

| Item | Source (MOOSE) behavior | Target requirement | Action needed |
|------|------------------------|-------------------|---------------|
| Coordinate frame | Interface-local `[N,S1,S2]` provided by framework | Must rotate jump to interface frame before calling; rotate traction back after | Identify rotation provider in target |
| Stateful damage | `MaterialPropertyOld` auto-managed | Explicit state variable storage and restoration | Add $d$ to state vector |
| Viscous update | Inline; requires `_dt` | `_dt` may need to be passed explicitly | Add $\Delta t$ as argument or context |
| Heaviside regularization | `MathUtils::regularizedHeavyside` | Reimplement: e.g., $H(x;\alpha) = 1/(1+e^{-x/\alpha})$ or linear ramp | Port or rewrite |
| Lag flags | Boolean input params controlling which $\boldsymbol{\delta}$ is used | Equivalent old-state access needed | Expose old state as argument |

### Reuse directly

- All scalar formulas: $\beta$, $\delta_m^0$, $\delta_m^f$ (BK and power-law), $\delta_m$, $d$.
- Chain-rule derivative expressions (Sections 6.3–6.4) — purely mathematical.
- Damage irreversibility logic.
- Viscous regularization update.

### Must reformulate

- **Residual form:** If the target uses a residual-based CZM interface (rather than traction
  evaluated directly), the traction $\mathbf{T}(\boldsymbol{\delta})$ becomes the right-hand side
  of an interface residual $\mathbf{R} = \mathbf{T}(\boldsymbol{\delta}) - \mathbf{T}_\text{applied}$.
  (`confirmed source behavior` — MOOSE evaluates traction directly; the interface kernel forms the residual.)
- **Jacobian wiring:** The `_dinterface_traction_djump` RankTwoTensor must be connected to the
  interface kernel's stiffness contribution. In other frameworks this is done differently.
  (`target-side recommendation` — verify how the tangent is consumed by the interface kernel.)
- **AD compatibility:** MOOSE supports AD variants of CZM materials. If the target uses automatic
  differentiation, the hand-coded tangent can be dropped and the traction formula alone is needed.
  (`inferred behavior` — no AD version of `BiLinearMixedModeTraction` exists in this codebase yet.)

---

*Spec generated from source at commit HEAD. Verify against current source before implementing.*
