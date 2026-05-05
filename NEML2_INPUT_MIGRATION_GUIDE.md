# MOOSE-NEML2 Input File Migration Guide (Old Syntax → New Syntax)

> Corresponds to commit `e7936bf9` (`Adapt MOOSE-NEML2 interface for the latest NEML2`, ref #29579).
> This document is intended to guide another LLM or developer to migrate legacy MOOSE `.i` input files (containing the `[NEML2]` block and the accompanying NEML2 model files) to the new syntax.
>
> Applies to:
> - Main MOOSE input files (`.i` files containing the `[NEML2]` action block)
> - NEML2 model files (pure NEML2 `.i` defining `[Models]`, `[Solvers]`, `[Drivers]`)

---

## 0. Big Picture: Three Core Principles

1. **Semantic names replace namespace paths.** All variables that used to be named with NEML2 internal namespace paths such as `forces/X`, `state/Y`, `old_state/Z`, `old_forces/t` are now renamed to user-chosen semantic names (e.g. `neml2_strain`, `neml2_stress`, `plastic_strain`, `mandel_stress`).
2. **Auto-binding by matching name.** A MOOSE-side material property, aux variable, or function whose name matches a NEML2 input variable is **automatically picked up**. You no longer need three parallel parameter lists describing the mapping.
3. **Stateful variables use the `~N` suffix to mean "N steps back".** This replaces the old `old_state/X`, `old_forces/t` prefix style. `X~1` is equivalent to the old `old_state/X`.

---

## 1. Simplification of the `[NEML2]` Action Block

### Old (5 groups of triplet parameters, 3 fields each)

```
[NEML2]
  input = 'model.i'
  model = 'model'
  device = 'cpu'

  moose_input_types = 'MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL              MATERIAL'
  moose_inputs      = 'neml2_strain time          time          plastic_strain        equivalent_plastic_strain'
  neml2_inputs      = 'forces/E     forces/t      old_forces/t  old_state/internal/Ep old_state/internal/ep'

  moose_output_types = 'MATERIAL     MATERIAL          MATERIAL'
  moose_outputs      = 'neml2_stress plastic_strain    equivalent_plastic_strain'
  neml2_outputs      = 'state/S      state/internal/Ep state/internal/ep'

  moose_derivative_types = 'MATERIAL'
  moose_derivatives      = 'neml2_jacobian'
  neml2_derivatives      = 'state/S forces/E'

  moose_parameter_types = 'MATERIAL VARIABLE'
  moose_parameters      = 'p1_mat   p2_var'
  neml2_parameters      = 'p1       p2'
[]
```

### New (only what truly needs to be specified)

```
[NEML2]
  input = 'model.i'
  model = 'model'
  device = 'cpu'

  # Most inputs do NOT need to be listed: MOOSE auto-binds by matching name
  # against materials/aux variables/functions.
  # Only specify when there is name ambiguity, or the source is not the default kind:
  # input_types = 'MATERIAL VARIABLE'
  # inputs      = 'neml2_strain some_var'

  # Outputs are written back as MOOSE material properties by default,
  # using the same name as the NEML2 output variable.
  # To disable auto-output, set:
  # auto_output = false

  # 'derivatives' uses a paired syntax: each two consecutive names form one (output, input) pair.
  derivatives = 'neml2_stress neml2_strain'

  # To pass MOOSE materials/variables in as NEML2 model parameters:
  parameter_types = 'MATERIAL VARIABLE'
  parameters      = 'p1       p2'
[]
```

### Block-parameter cross-reference

| Old field | New approach |
| --- | --- |
| `moose_input_types` | `input_types` (usually omitted entirely) |
| `moose_inputs` + `neml2_inputs` (paired) | `inputs` (single field; auto-bound by matching name, no pairing needed) |
| `moose_output_types` / `moose_outputs` / `neml2_outputs` | **Remove the whole group.** Outputs are auto-written; turn off with `auto_output = false`. |
| `moose_derivative_types` / `moose_derivatives` | **Remove.** Derivative material properties are auto-generated, named `d<output>/d<input>`. |
| `neml2_derivatives = 'state/S forces/E'` | `derivatives = 'neml2_stress neml2_strain'` (semantic names, **paired**) |
| `moose_parameter_types` | `parameter_types` |
| `moose_parameters` + `neml2_parameters` | `parameters` (auto-bound by matching name) |
| `moose_input_kernels` | `input_kernels` (used for explicit-dynamics-style integrations) |

### Shared parameters can be hoisted across sub-blocks

Old inputs repeated `moose_*` settings inside each sub-block. The new syntax lets shared settings live at the top of `[NEML2]`:

```
[NEML2]
  input = 'models/custom_model.i'
  verbose = true
  device = 'cpu'

  derivatives = 'product A'
  export_outputs = 'sum product dproduct/dA'
  export_output_targets = 'exodus; exodus; exodus'

  [A]
    model = 'model_A'
    block = 'A'
  []
  [B]
    model = 'model_B'
    block = 'B'
  []
[]
```

### Naming convention for `derivatives`

- Inside the `[NEML2]` block: `derivatives = 'OUT IN'` (paired; multiple pairs concatenated: `'O1 I1 O2 I2 ...'`).
- The auto-generated MOOSE material property name is **`dOUT/dIN`**.
  - Example: `derivatives = 'neml2_stress neml2_strain'` → material property `dneml2_stress/dneml2_strain`
  - Example: `derivatives = 'product A'` → material property `dproduct/dA`
- Downstream references must use the new name:

```
# Old
custom_small_jacobian = 'neml2_jacobian'

# New
custom_small_jacobian = 'dneml2_stress/dneml2_strain'
```

---

## 2. Variable Names: NEML2 Namespace Path → Semantic Name

**Core rule:** Drop the `forces/`, `state/`, `state/internal/`, `old_state/`, `old_forces/` prefixes entirely. Use human-readable names. **The MOOSE side and the NEML2 side use the same name** so that auto-binding works.

### Common rename table

| Old (NEML2 path) | New (semantic) |
| --- | --- |
| `forces/E` | `neml2_strain` |
| `forces/F` | `deformation_gradient` |
| `forces/T` | `temperature` |
| `forces/t`, `old_forces/t` | **Delete** (time is handled automatically by the `TIME` source) |
| `forces/r` | `orientation` |
| `forces/R` | `orientation_matrix` |
| `forces/env_fac` | `env_factor` |
| `forces/N` | `flow_direction` |
| `state/S` | `neml2_stress` |
| `state/full_S` | `neml2_stress` (still typically the same name after projection back to R2) |
| `state/internal/Ee` | `elastic_strain` |
| `state/internal/Ep` | `plastic_strain` |
| `state/internal/ep` | `equivalent_plastic_strain` |
| `state/internal/M` | `mandel_stress` |
| `state/internal/X` | `back_stress` |
| `state/internal/O` | `over_stress` |
| `state/internal/k` | `isotropic_hardening` |
| `state/internal/fp` | `yield_function` |
| `state/internal/NM` | `flow_direction` |
| `state/internal/Nk` | `isotropic_hardening_direction` |
| `state/internal/s` | `effective_stress` or `von_mises_stress` |
| `state/Fp` | `plastic_deformation_gradient` |
| `state/Fe` | `elastic_deformation_gradient` |
| `state/Lp` | `plastic_spatial_velocity_gradient` |
| `state/tauc` | `slip_hardening` |
| `state/tau_i` | `resolved_shears` |
| `state/gamma_rate_i` | `slip_rates` |
| `state/gamma_rate` | `sum_slip_rates` |
| `state/cell_dd` | `cell_dislocation_density` |
| `state/wall_dd` | `wall_dislocation_density` |
| `state/cell_rate` | `cell_dislocation_density_rate` |
| `state/wall_rate` | `wall_dislocation_density_rate` |
| `state/ep_rate` | `equivalent_plastic_strain_rate` |
| `state/Ep_rate` | `plastic_strain_rate` |

### Stateful (back-in-time) variables

| Old | New |
| --- | --- |
| `old_state/Ep` | `plastic_strain~1` |
| `old_state/ep` | `equivalent_plastic_strain~1` |
| `old_state/cell_dd` | `cell_dislocation_density~1` |
| `old_state/Fp` | `plastic_deformation_gradient~1` |

> `~N` means N steps back in time. `~1` ≡ the old `old_*`.

### How to choose new names (for unfamiliar variables)

1. Use the physical meaning (`Ep` → plastic strain → `plastic_strain`).
2. Match the MOOSE-side material property name (so auto-binding works).
3. Use lowercase and underscores; suffix `_rate` for time derivatives; `~1` for the previous step.

---

## 3. Remove No-Longer-Needed `[Postprocessors]`

Common in old inputs:

```
[Postprocessors]
  [time]
    type = TimePostprocessor
    execute_on = 'INITIAL TIMESTEP_BEGIN'
    outputs = 'none'
  []
[]
```

paired with `POSTPROCESSOR` source pushing time into `forces/t` / `old_forces/t`.

**Delete this block entirely in the new syntax.** Time is handled by the new built-in `TIME` source; if a NEML2 model needs time, declare a `time` input internally (see §6 `R2IncrementToRate`).

---

## 4. Rewriting NEML2 Model Files (`[Models]` Section)

### 4.1 Replace every variable name with the semantic name

Within `[Models]`, every `from_var=`, `to_var=`, `tensor=`, `invariant=`, `variable=`, `stress=`, `strain=`, `mandel_stress=`, `function=`, `from=`, `to=`, etc. that contains a `forces/...`, `state/...`, or `old_state/...` value must be replaced with the semantic name from §2.

### 4.2 `SR2LinearCombination`: field renames

| Old | New |
| --- | --- |
| `from_var` | `from` |
| `to_var` | `to` |
| `coefficients` | `weights` |

```
# Old
[elastic_strain]
  type = SR2LinearCombination
  from_var = 'forces/E state/internal/Ep'
  to_var = 'state/internal/Ee'
  coefficients = '1 -1'
[]

# New
[elastic_strain]
  type = SR2LinearCombination
  from = 'neml2_strain plastic_strain'
  to = 'elastic_strain'
  weights = '1 -1'
[]
```

### 4.3 Most model objects now require explicit input/output naming

Many model objects used to rely on default `state/...` names that could be omitted. The new syntax requires you to **explicitly** list them (using semantic names):

```
# Old
[elasticity]
  type = LinearIsotropicElasticity
  coefficients = '1e5 0.3'
  coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
[]
[mandel_stress]
  type = IsotropicMandelStress
[]
[vonmises]
  type = SR2Invariant
  invariant_type = 'VONMISES'
  tensor = 'state/internal/M'
  invariant = 'state/internal/s'
[]

# New
[elasticity]
  type = LinearIsotropicElasticity
  coefficients = '1e5 0.3'
  coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
  strain = 'elastic_strain'
  stress = 'neml2_stress'
[]
[mandel_stress]
  type = IsotropicMandelStress
  cauchy_stress = 'neml2_stress'
[]
[vonmises]
  type = SR2Invariant
  invariant_type = 'VONMISES'
  tensor = 'mandel_stress'
  invariant = 'effective_stress'
[]
```

### 4.4 Plastic flow constraint: `RateIndependentPlasticFlowConstraint` → `FBComplementarity`

```
# Old
[consistency]
  type = RateIndependentPlasticFlowConstraint
[]

# New
[consistency]
  type = FBComplementarity
  a = 'yield_function'
  a_inequality = 'LE'
  b = 'flow_rate'
[]
```

### 4.5 Model name capitalization fixes

| Old | New |
| --- | --- |
| `SR2toR2` | `SR2ToR2` |

### 4.6 High-level models such as LAROMANCE: many parameters can be omitted

Many models now use semantic names as their default input/output names, so the explicit per-input lines can simply be deleted:

```
# Old
[rom_ep]
  type = LAROMANCE6DInterpolation
  model_file_name = 'models/random_value_6d_grid.json'
  model_file_variable_name = 'out_ep'
  output_rate = 'state/ep_rate'
  von_mises_stress = 'state/s'
  equivalent_plastic_strain = 'state/ep'
  cell_dislocation_density = 'old_state/cell_dd'
  wall_dislocation_density = 'old_state/wall_dd'
  temperature = 'forces/T'
  env_factor = 'forces/env_fac'
[]

# New
[rom_ep]
  type = LAROMANCE6DInterpolation
  model_file_name = 'models/random_value_6d_grid.json'
  model_file_variable_name = 'out_ep'
  output_rate = 'equivalent_plastic_strain_rate'
[]
```

> Rule of thumb: if the MOOSE-side name and the model's default parameter name (`von_mises_stress`, `equivalent_plastic_strain`, `temperature`, `env_factor`, ...) already match, omit them; keep only the truly customized fields such as `output_rate` and the model file paths.

---

## 5. `[Solvers]` and `[Models]` `NonlinearSystem` / `ImplicitUpdate`: must explicitly list unknowns / residuals / predictor

### 5.1 `NonlinearSystem` must include `unknowns` and `residuals`

```
# Old
[Solvers]
  [eq_sys]
    type = NonlinearSystem
    model = 'surface'
  []
[]

# New
[Solvers]
  [eq_sys]
    type = NonlinearSystem
    model = 'surface'
    unknowns  = 'plastic_strain equivalent_plastic_strain flow_rate'
    residuals = 'plastic_strain_residual equivalent_plastic_strain_residual complementarity'
  []
[]
```

> Rules:
> - `unknowns` lists all variables solved for in the equation system (semantic names).
> - `residuals` corresponds one-to-one with `unknowns`:
>   - `BackwardEulerTimeIntegration` on `X` produces a residual named `X_residual`.
>   - `FBComplementarity` produces a residual named `complementarity`.

### 5.2 `ImplicitUpdate` must attach a `predictor`

```
# Old
[return_map]
  type = ImplicitUpdate
  equation_system = 'eq_sys'
  solver = 'newton'
[]

# New
[predictor]
  type = ConstantExtrapolationPredictor
  unknowns_SR2    = 'plastic_strain'
  unknowns_Scalar = 'equivalent_plastic_strain flow_rate'
  # If there is an R2 unknown: unknowns_R2 = 'plastic_deformation_gradient'
[]
[return_map]
  type = ImplicitUpdate
  equation_system = 'eq_sys'
  solver = 'newton'
  predictor = 'predictor'
[]
```

> Rules:
> - Use `ConstantExtrapolationPredictor` and group the unknowns by NEML2 tensor type:
>   - `unknowns_Scalar` (scalar)
>   - `unknowns_SR2` (symmetric rank-2)
>   - `unknowns_R2` (general rank-2)
> - The list must match the `unknowns` of the `NonlinearSystem`.

---

## 6. Explicit Dynamics / Custom Kernel Interface

### 6.1 Field renames inside the `[NEML2]` block

| Old | New |
| --- | --- |
| `moose_input_kernels = 'strain'` | `input_kernels = 'neml2_strain'` |

Usually combined with `auto_output = false` because outputs are handled by the custom kernel.

### 6.2 Kernel field renames in `NEML2SmallStrain` / `NEML2StressDivergence` etc.

The `to_neml2` and `stress` fields must also use semantic names:

```
# Old
[strain]
  type = NEML2SmallStrain
  to_neml2 = 'forces/E'
[]
[residual]
  type = NEML2StressDivergence
  stress = 'state/S'
[]

# New
[neml2_strain]
  type = NEML2SmallStrain
  to_neml2 = 'neml2_strain'
[]
[residual]
  type = NEML2StressDivergence
  stress = 'neml2_stress'
[]
```

---

## 7. Update Companion MOOSE Kernels / Materials to Match

When you change the `derivatives` and output names from §1, every object referencing those material properties must follow:

```
# Old
custom_small_jacobian = 'neml2_jacobian'
reaction_rate         = neml2_dproduct_da
mat_prop              = vonmises_stress
mat_prop              = cell_dd
mat_prop              = eff_inelastic_strain

# New
custom_small_jacobian = 'dneml2_stress/dneml2_strain'
reaction_rate         = dproduct/dA
mat_prop              = von_mises_stress
mat_prop              = cell_dislocation_density
mat_prop              = equivalent_plastic_strain
```

Names in `AuxVariables`, `ICs`, and `Materials` should also align with `[NEML2]`'s input/parameter names (typically by changing lower-case/legacy names to the semantic / matching name):

```
# Old
[AuxVariables][a][]    [Materials][b][prop_names = 'b'][]    [Materials][p1_mat][prop_names='p1_mat']

# New (matches the [NEML2] inputs / parameters)
[AuxVariables][A][]    [Materials][B][prop_names = 'B'][]    [Materials][p1][prop_names='p1']
```

---

## 8. When Using `initialize_outputs`

The old `initialize_outputs` listed NEML2-path names. The new form uses semantic names and the list is usually shorter (many old entries no longer need explicit initialization):

```
# Old
initialize_outputs       = 'wall_dd      cell_dd      init_envFac'
initialize_output_values = 'init_wall_dd init_cell_dd init_envFac'

# New
initialize_outputs       = 'wall_dislocation_density cell_dislocation_density'
initialize_output_values = 'init_wall_dd init_cell_dd'
```

---

## 9. `[NEML2]` Sub-Block Naming Convention (Documentation Level)

In examples and comments, sub-block names should change from `block1/block2` to `model1/model2` to avoid confusion with MOOSE's `block` (subdomain). Pure naming convention; no functional impact.

---

## 10. Full Comparison Example: Plastic isoharden

### Main input (`isoharden.i`)

```diff
-    moose_input_types = 'MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL              MATERIAL'
-    moose_inputs = '     neml2_strain time          time          plastic_strain        equivalent_plastic_strain'
-    neml2_inputs = '     forces/E     forces/t      old_forces/t  old_state/internal/Ep old_state/internal/ep'
-
-    moose_output_types = 'MATERIAL     MATERIAL          MATERIAL'
-    moose_outputs = '     neml2_stress plastic_strain    equivalent_plastic_strain'
-    neml2_outputs = '     state/S      state/internal/Ep state/internal/ep'
-
-    moose_derivative_types = 'MATERIAL'
-    moose_derivatives = 'neml2_jacobian'
-    neml2_derivatives = 'state/S forces/E'
-  []
-[]
-
-[Postprocessors]
-  [time]
-    type = TimePostprocessor
-    execute_on = 'INITIAL TIMESTEP_BEGIN'
-    outputs = 'none'
+    derivatives = 'neml2_stress neml2_strain'
   []
 []
@@
-    custom_small_jacobian = 'neml2_jacobian'
+    custom_small_jacobian = 'dneml2_stress/dneml2_strain'
```

### NEML2 model (`isoharden_neml2.i`)

```diff
-  [elastic_strain]
-    type = SR2LinearCombination
-    from_var = 'forces/E state/internal/Ep'
-    to_var = 'state/internal/Ee'
-    coefficients = '1 -1'
-  []
+  [elastic_strain]
+    type = SR2LinearCombination
+    from = 'neml2_strain plastic_strain'
+    to = 'elastic_strain'
+    weights = '1 -1'
+  []
   [elasticity]
     type = LinearIsotropicElasticity
     coefficients = '1e5 0.3'
     coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
+    strain = 'elastic_strain'
+    stress = 'neml2_stress'
   []
   [mandel_stress]
     type = IsotropicMandelStress
+    cauchy_stress = 'neml2_stress'
   []
@@
-  [consistency]
-    type = RateIndependentPlasticFlowConstraint
-  []
+  [consistency]
+    type = FBComplementarity
+    a = 'yield_function'
+    a_inequality = 'LE'
+    b = 'flow_rate'
+  []
@@
   [eq_sys]
     type = NonlinearSystem
     model = 'surface'
+    unknowns  = 'plastic_strain equivalent_plastic_strain flow_rate'
+    residuals = 'plastic_strain_residual equivalent_plastic_strain_residual complementarity'
   []
@@
+  [predictor]
+    type = ConstantExtrapolationPredictor
+    unknowns_SR2    = 'plastic_strain'
+    unknowns_Scalar = 'equivalent_plastic_strain flow_rate'
+  []
   [return_map]
     type = ImplicitUpdate
     equation_system = 'eq_sys'
     solver = 'newton'
+    predictor = 'predictor'
   []
   [model]
     type = ComposedModel
     models = 'return_map elastic_strain elasticity'
-    additional_outputs = 'state/internal/Ep state/internal/ep'
+    additional_outputs = 'plastic_strain equivalent_plastic_strain'
   []
 []
```

---

## 11. Automated Migration Checklist

Use this list as a per-file todo. Walk through it for every `.i` to be migrated.

### A. In the main input (containing the `[NEML2]` block)

- [ ] Remove every `moose_input_types` / `moose_inputs` / `neml2_inputs`. Most can be omitted entirely; only when there is ambiguity, fall back to `input_types` + `inputs`.
- [ ] Remove every `moose_output_types` / `moose_outputs` / `neml2_outputs` (auto by default). To disable, add `auto_output = false`.
- [ ] Remove every `moose_derivative_types` / `moose_derivatives`; rename `neml2_derivatives` to `derivatives`, using semantic names paired up.
- [ ] `moose_parameter_types` → `parameter_types`; `moose_parameters` + `neml2_parameters` → `parameters` (matching name).
- [ ] `moose_input_kernels` → `input_kernels`, names changed to semantic names.
- [ ] If multiple sub-blocks share parameters, hoist the shared settings to the top of `[NEML2]`.
- [ ] Delete `TimePostprocessor` and any postprocessor whose only purpose was to feed time into NEML2.
- [ ] Update every reference to the old `neml2_jacobian` to `d<output>/d<input>`.
- [ ] Rename related entries in `AuxVariables`, `ICs`, `Materials` so they match the corresponding NEML2 input names.
- [ ] Rename items in `initialize_outputs` to semantic names.

### B. In the NEML2 model files (`[Models]` / `[Solvers]` / `[Drivers]`)

- [ ] Replace every occurrence of `forces/...`, `state/...`, `state/internal/...`, `old_state/...`, `old_forces/...` with semantic names (use the §2 table).
- [ ] `old_state/X` → `X~1` (and `~2`, `~3` as needed).
- [ ] `SR2LinearCombination`: `from_var` → `from`; `to_var` → `to`; `coefficients` → `weights`.
- [ ] `RateIndependentPlasticFlowConstraint` → `FBComplementarity` (add `a`, `a_inequality='LE'`, `b='flow_rate'`).
- [ ] `SR2toR2` → `SR2ToR2` (and similar capitalization fixes).
- [ ] Add explicit input/output names to `LinearIsotropicElasticity`, `IsotropicMandelStress`, `SR2Invariant`, `YieldFunction`, `Normality`, `*BackwardEulerTimeIntegration`, etc.
- [ ] For high-level models such as `LAROMANCE6DInterpolation`: delete every field whose value matches the default name; keep only the file path and truly customized fields such as `output_rate`.
- [ ] Add `unknowns` and `residuals` to `NonlinearSystem`.
- [ ] Add a `ConstantExtrapolationPredictor` (group the unknowns by tensor type) and attach it to `ImplicitUpdate` via `predictor =`.
- [ ] Replace `additional_outputs` in `ComposedModel` with semantic names.

### C. Naming consistency (the easiest place to slip up)

- [ ] The MOOSE-side material property name and aux variable name must **exactly match** the NEML2 input/output name.
- [ ] Both members of each `derivatives` pair are semantic names (not NEML2 paths).
- [ ] `unknowns` ↔ `residuals` correspond one-to-one; `integrate_X` → residual `X_residual`; `FBComplementarity` → residual `complementarity`.
- [ ] The combined `unknowns_*` of the `predictor` equals the `unknowns` of the `NonlinearSystem`.

### D. Things no longer needed

- [ ] WASP-related: this commit also dropped the WASP dependency for NEML2. If your scripts use `WASP_DIR` / `WASP_SRC_DIR` / `configure_wasp.sh` solely for NEML2, those are no longer needed (other tooling may still need them; keep them as appropriate).
- [ ] The `TimePostprocessor` workaround for feeding time into NEML2 is no longer needed.

---

## 12. Notes on "Auto-Binding by Matching Name"

- Auto-binding searches the candidate sources (MOOSE material properties, aux variables, functions, ...) for one whose name matches the NEML2 input.
- If **exactly one** source matches, it is used automatically with no need to declare anything in `[NEML2]`.
- If **multiple** sources share the name (ambiguity), an error is raised; in that case use `input_types` + `inputs` to explicitly disambiguate.
- For stateful variables that need values from previous steps, the corresponding MOOSE material properties (and their old values) are picked up automatically; no postprocessor needed.

---

## 13. Quick Reference: Common Migration Mistakes

| Symptom | Usually means |
| --- | --- |
| `Failed to find ... forces/E ...` | Leftover NEML2 path in a model file |
| `Material property 'neml2_jacobian' not found` | A reference site was not renamed to `dneml2_stress/dneml2_strain` |
| `unknowns and residuals must have the same length` | Missing residual or unknown in NonlinearSystem |
| `Predictor missing for unknown ...` | `ConstantExtrapolationPredictor` doesn't cover one of the unknowns |
| `Ambiguous source for input 'X'` | A material and an aux variable with the same name both exist; disambiguate via `input_types`+`inputs` |
| `IsotropicMandelStress: cauchy_stress not specified` | A model sub-block is missing its explicit input/output |

---

After all checks above pass, the new input file should run on the new NEML2 release.
