# Plan: Generalize `INSADMomentumEHDForce` to N species with arbitrary valences

## Motivation

The current `INSADMomentumEHDForce` kernel hardcodes exactly two species with implicit
valences `z_+ = +1` and `z_- = -1`:

```cpp
// modules/navier_stokes/src/kernels/INSADMomentumEHDForce.C
return -(_c_pos[_qp] - _c_neg[_qp]) * (-_grad_phi[_qp]);
```

The correct, general form of the electrokinetic (electro-hydrodynamic) body force is:

$$
\vec{f}_{EK} \;=\; \rho_e \, \vec{E} \;=\; \Bigl(\sum_{i=1}^{N_\text{sp}} z_i \, c_i\Bigr) \, (-\nabla \phi).
$$

Both the charge density and the electric field must be assembled at each quadrature
point. The reference implementation shows this pattern clearly:

```cpp
// reference implementation (external TalyFEM code)
double charge_density = 0.0;
for (int s = 0; s < nSpecies; s++) {
    double c_i = p_data_->valueFEM(fe, NSPNPNodeData::CONCENTRATION_1 + s);
    charge_density += z[s] * c_i;
}
TALYFEMLIB::ZEROPTV dphi;
for (int i = 0; i < fe.nsd(); i++)
    dphi(i) = p_data_->valueDerivativeFEM(fe, NSPNPNodeData::POTENTIAL, i);
for (int i = 0; i < fe.nsd(); i++)
    f_EK(i) = charge_density * dphi(i);
```

Goal: replace the two-species hardcoded form with an N-species formulation that couples
to (a) a list of concentration variables and (b) a list of valences, while still
coupling to the potential gradient.

## Scope

**In scope**
- Refactor `INSADMomentumEHDForce.{h,C}` so that it accepts:
  - `concentrations` — a variadic list of coupled AD scalar variables.
  - `valences` — a `std::vector<Real>` (one per concentration) supplying `z_i`.
  - `potential` — unchanged; the coupled AD scalar potential variable.
- Update `.plans/tests/electro_convective_instability.i` (uncommitted test) to use
  the new parameter names.
- Update `.plans/tests/PNP2NS.md` to reflect the new interface.

**Out of scope**
- No changes to VMS/SUPG stabilization hooks (`INSADObjectTracker`,
  `ehd_strong_residual`, etc.). Keep the existing "has_coupled_force" registration
  exactly as it is today — it is a known limitation carried over from the current
  kernel.
- No changes to the sign convention. The current kernel returns the residual
  `+ρ_e ∇φ` on the LHS, which corresponds to the physical body force
  `-ρ_e ∇φ = ρ_e (-∇φ) = ρ_e \vec{E}` (with the standard `E = -∇φ`). Keep this.
- No new gold files or additional TestHarness spec entries; the existing test file
  is edited in place.

## File-by-file changes

### 1. Header — `modules/navier_stokes/include/kernels/INSADMomentumEHDForce.h`

Replace the two-species members with vectors sized `_n_species`:

```cpp
#pragma once

#include "ADVectorKernelValue.h"

/**
 * Computes the electro-hydrodynamic (EHD) body force
 *   \f$\vec{F} = \rho_e \vec{E} = \bigl(\sum_i z_i c_i\bigr)(-\nabla \phi)\f$
 * for an arbitrary number of ionic species and applies it to the momentum
 * equation. The kernel returns \f$-\vec{F}\f$ because it lives on the LHS
 * (\f$M - F = 0\f$).
 */
class INSADMomentumEHDForce : public ADVectorKernelValue
{
public:
  INSADMomentumEHDForce(const InputParameters & parameters);
  static InputParameters validParams();

protected:
  ADRealVectorValue precomputeQpResidual() override;

  /// Number of coupled ionic species (== _concentrations.size() == _valences.size()).
  const std::size_t _n_species;

  /// Coupled concentration values, one entry per species.
  std::vector<const ADVariableValue *> _concentrations;

  /// Valence (charge number) z_i for each species; matches _concentrations order.
  const std::vector<Real> _valences;

  /// Gradient of the electric potential.
  const ADVariableGradient & _grad_phi;
};
```

Notes for the implementer:
- Use `std::vector<const ADVariableValue *>` (pointer-to-value) rather than
  references so that the container can be built by index in the constructor
  body — this is the established pattern in other MOOSE kernels that take a
  variadic coupled variable (e.g. `ADCoupledForce` handles multiple components
  via indices into `coupledComponents`).
- `_n_species` is stored so the residual loop does not have to re-query
  container size and to allow a size-consistency check in the constructor.

### 2. Source — `modules/navier_stokes/src/kernels/INSADMomentumEHDForce.C`

Replacement body:

```cpp
#include "INSADMomentumEHDForce.h"
#include "INSADObjectTracker.h"
#include "FEProblemBase.h"

registerMooseObject("NavierStokesApp", INSADMomentumEHDForce);

InputParameters
INSADMomentumEHDForce::validParams()
{
  InputParameters params = ADVectorKernelValue::validParams();
  params.addClassDescription(
      "Computes the electro-hydrodynamic (EHD) body force "
      "F = (sum_i z_i c_i) * (-grad phi) and applies it to the momentum equation.");
  params.addRequiredCoupledVar(
      "concentrations",
      "Ionic species concentrations c_i. One coupled variable per species; the order "
      "must match the 'valences' parameter.");
  params.addRequiredParam<std::vector<Real>>(
      "valences",
      "Valence (signed charge number) z_i of each species. Must have the same length "
      "as 'concentrations'.");
  params.addRequiredCoupledVar("potential", "Electric potential phi.");
  return params;
}

INSADMomentumEHDForce::INSADMomentumEHDForce(const InputParameters & parameters)
  : ADVectorKernelValue(parameters),
    _n_species(coupledComponents("concentrations")),
    _valences(getParam<std::vector<Real>>("valences")),
    _grad_phi(adCoupledGradient("potential"))
{
  if (_valences.size() != _n_species)
    paramError("valences",
               "The number of valences (", _valences.size(),
               ") must match the number of coupled concentrations (", _n_species, ").");

  _concentrations.reserve(_n_species);
  for (const auto i : make_range(_n_species))
    _concentrations.push_back(&adCoupledValue("concentrations", i));

  // Preserve the existing INSAD tracker hook so INSADMaterial knows a body force
  // exists on these blocks. VMS/SUPG stabilization of the EHD term itself is a
  // separate concern and is intentionally left as-is (see plan Scope).
  auto & obj_tracker = const_cast<INSADObjectTracker &>(
      _fe_problem.getUserObject<INSADObjectTracker>("ins_ad_object_tracker"));
  for (const auto block_id : blockIDs())
    obj_tracker.set("has_coupled_force", true, block_id);
}

ADRealVectorValue
INSADMomentumEHDForce::precomputeQpResidual()
{
  // charge_density = sum_i z_i * c_i(qp)
  ADReal charge_density = 0;
  for (const auto i : make_range(_n_species))
    charge_density += _valences[i] * (*_concentrations[i])[_qp];

  // Physical force: F = charge_density * (-grad phi)
  // Kernel is on LHS (M - F = 0), so we return -F = charge_density * grad phi.
  return charge_density * _grad_phi[_qp];
}
```

Notes for the implementer:
- Use `make_range` for the loop per `CLAUDE.md` §5.
- Use `paramError("valences", ...)` (not `mooseError`) so the failure points at
  the offending input parameter.
- `ADReal` is the correct accumulator type to keep AD derivatives through the
  concentration sum.
- Do not add a fallback for `_n_species == 0`; `addRequiredCoupledVar` guarantees
  at least one coupled variable, and the constructor size check catches valence
  mismatch.

### 3. Input file — `modules/combined/test/tests/electro_osmotic_flow/electro_convective_instability.i`

Replace the current `[ehd_force]` block:

```ini
  [ehd_force]
    type = INSADMomentumEHDForce
    variable = velocity
    c_positive = c_positive
    c_negative = c_negative
    potential = potential
  []
```

with the generalized form:

```ini
  [ehd_force]
    type = INSADMomentumEHDForce
    variable = velocity
    concentrations = 'c_positive c_negative'
    valences = '1 -1'
    potential = potential
  []
```

Do not touch any other block in this file.

### 4. Documentation — `.plans/tests/PNP2NS.md`

Two small edits:

- In the "Requires" list under §1 Header, replace the three-line
  `c_positive` / `c_negative` / `potential` bullet list with:
  - `concentrations` (variadic coupled AD scalar variables, one per species)
  - `valences` (`std::vector<Real>`, one per species, same order as `concentrations`)
  - `potential` (coupled AD scalar variable)
- In §2 Computation, replace the two-species `return` snippet with the
  N-species version from this plan.

Leave the "Alternative (Without C++)" and "Verification" sections untouched.

## Verification steps

Success criterion: the generalized kernel reproduces the current kernel exactly
when configured with `concentrations = 'c_positive c_negative'` and
`valences = '1 -1'`, and correctly generalizes for other configurations.

1. **Build** — rebuild the `navier_stokes` module (and `combined` for the test).
   Ask the user first about the conda MOOSE stack before invoking any build
   command, per CLAUDE.md §6.
2. **Regression parity** — run the existing
   `electro_osmotic_flow/electro_convective_instability` test after the input
   file edit. Its numerical output must not change to within solver tolerance,
   since `charge_density = 1*c_+ + (-1)*c_- = c_+ - c_-` reproduces the old
   two-species expression bit-identically.
3. **Constructor guardrail** — temporarily set `valences = '1'` (mismatched
   length) locally and confirm the run aborts with the `paramError` message on
   `valences`. Revert.
4. **Three-species smoke** — optionally, in a scratch input, add a third
   `concentrations` entry with `z = 0` and verify the residual is unchanged
   (a zero-valence species must not contribute to the force). Do **not** commit
   this scratch input.

## Non-goals / explicit deferrals

- **VMS stabilization of the EHD source term.** The current kernel already
  skips the "strong residual" injection into `INSADMaterial`; this plan does
  not fix that. Handling it correctly requires a separate material that
  computes an AD `RealVectorValue` strong residual for `ρ_e (-∇φ)` and hooks
  it into `INSADObjectTracker` similarly to `INSADMomentumCoupledForce`. Track
  as follow-up work.
- **Sign convention audit.** The physical sign (`E = -∇φ`) is preserved from
  the existing kernel and is standard. The reference snippet in the request
  writes `f_EK = charge_density * ∇φ`, which is the *LHS residual* form (i.e.
  it equals `-F_physical`); this plan intentionally follows the MOOSE
  convention of returning the LHS residual from `precomputeQpResidual`, so
  `return charge_density * _grad_phi[_qp]` matches the reference literally
  while remaining physically correct.
- **Renaming the kernel.** Keep the class name `INSADMomentumEHDForce`.
