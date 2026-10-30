# Future Work: Connect NEML2 Responses to MOOSE/Kokkos AD

## Status

Future work only. This document records the integration gap and design constraints; it does not
select a final API or authorize implementation.

## Precise statement of the limitation

It is not accurate to say that NEML2 cannot differentiate its models. NEML2 can evaluate model
outputs and their derivatives, and the MOOSE `NEML2ModelExecutor` already exposes output
Jacobians through `getOutputDerivative(output_name, input_name)`.

The missing capability is narrower:

> The current NEML2-to-Kokkos material-property path does not propagate NEML2 derivatives into
> MOOSE/Kokkos `ADReal` derivatives.

`NEML2ToKokkosMaterialProperty` currently stages NEML2 output values in
`Moose::Kokkos::Array<Real>` and declares `MaterialProperty<Real, rank>`. Consequently, an output
such as stress arrives in the MOOSE material chain as a value-only property. Even if NEML2 has
computed a consistent derivative, that derivative is not part of the `ADReal` graph consumed by
an AD residual.

This is an interoperability problem between two differentiation systems, not evidence that NEML2
itself lacks differentiation support.

## Why this is separate from AD SCZM

The cohesive-only Kokkos AD SCZM chain can remain end-to-end AD because its displacement jump and
traction law operate directly on MOOSE/Kokkos `ADReal`. Directional correction additionally needs
a bulk stress whose derivatives include all displacement dependence. A value-only NEML2 stress
breaks that chain.

SCZM must not repair this locally by contracting a NEML2 tangent inside the interface kernel. Such
a bridge would make SCZM responsible for constitutive tensor conventions, stress and strain
measures, kinematics, and backend-specific derivative construction. It would also duplicate the
role of the constitutive integration layer.

Until a general differentiable constitutive boundary exists, Kokkos AD SCZM should continue to
reject `directional_correction = true` explicitly.

## Target consumer-facing contract

A successful integration should make constitutive responses look identical to downstream AD
consumers regardless of backend:

```text
AD kinematics
    -> differentiable constitutive evaluation
    -> canonical MaterialProperty<ADReal, rank> response
    -> ordinary AD materials and residual objects
    -> Jacobian extracted from ADReal::derivatives()
```

SCZM and other consumers should know only the response property and its documented physical
measure. They should not know whether derivatives came from:

- scalar-templated native Kokkos constitutive code evaluated with `Scalar = ADReal`, or
- a NEML2 Jacobian/JVP composed with the incoming MOOSE AD derivatives.

For a NEML2 response `y = f(x)`, the integration layer conceptually needs to construct

```text
y.derivatives() = (dy/dx) * x.derivatives()
```

for every differentiable input, while preserving the output value. This equation describes the
required chain rule only; it does not prescribe storage layout, a full-Jacobian implementation,
or a JVP implementation.

## Ownership boundaries

### Framework

The framework should own only backend-independent mechanisms:

- derivative-aware Kokkos volume material dispatch, if independently required;
- representation and transport of Kokkos `ADReal` material properties;
- generic composition of backend response derivatives with incoming AD seeds;
- batch-to-element/QP mapping and primary/neighbor material storage mechanisms.

A general volume `Moose::Kokkos::ADMaterial` dispatcher should be added only if this work proves
that it is independently useful beyond one NEML2 or SCZM path.

### NEML2 integration

The NEML2 integration should own:

- declaration of differentiable NEML2 inputs and requested outputs;
- evaluation and retrieval of output derivatives or JVPs;
- conversion between NEML2 tensor layouts and MOOSE tensor components;
- composition of NEML2 derivatives with MOOSE AD seeds;
- state advancement, accepted/trial-state behavior, batching, and device placement.

### Solid mechanics

Solid mechanics should own physical conventions:

- the kinematic inputs supplied to a constitutive model;
- stress and strain measures;
- tensor symmetry and Mandel/full-tensor conversion;
- finite-strain transformations such as PK2-to-PK1;
- F-bar or other nonlocal kinematic dependencies.

### SCZM

SCZM should only consume the canonical AD stress property required by directional correction. It
must remain unaware of NEML2, constitutive tangents, and derivative-layout conventions.

## Design questions that must be resolved before implementation

1. **Differentiable inputs**
   - Which NEML2 inputs carry MOOSE AD seeds?
   - Can several inputs depend on the same nonlinear variables?
   - How are old, state, parameter, and externally prescribed inputs distinguished from current
     differentiable inputs?

2. **Derivative evaluation form**
   - Should NEML2 expose a full output-input Jacobian to the transfer layer, or should the
     integration use JVPs seeded from MOOSE derivatives?
   - Which form avoids excessive memory for batched GPU execution?
   - Can the same contract support both eager and AOTI execution?

3. **Tensor representation**
   - Where are symmetric Mandel factors applied to values and derivatives?
   - How are rank-two and rank-four full tensors represented?
   - How are dimensions below 3D handled without silently changing the constitutive convention?

4. **Physical measures**
   - Every response property must identify its stress measure and the kinematic measure with
     respect to which NEML2 differentiated it.
   - Finite-strain transformations must themselves be differentiable and must not rely on a
     small-strain tangent lift.

5. **Stateful models**
   - Trial evaluation must not prematurely commit state.
   - Residual-only and Jacobian evaluations must use mutually consistent state and batch maps.
   - Derivative caching must be invalidated when inputs, state, active elements, or batch layout
     change.

6. **Batching and device execution**
   - Values, derivative data, AD seeds, and composed responses should remain on the target device
     during the constitutive update.
   - Batch offsets for volume, side, primary, and neighbor data must remain bijective with material
     storage QPs.
   - The design must account for derivative memory scaling before choosing full Jacobians over
     JVPs.

7. **F-bar and nonlocal kinematics**
   - Derivatives must include every local and nonlocal dependency introduced by the kinematic
     formulation.
   - A local constitutive tangent alone is insufficient if the supplied kinematics depend on
     element averages or other coupled quantities.
   - F-bar support must be validated as part of the kinematics-to-response chain, not patched in
     SCZM.

8. **Failure behavior**
   - Missing requested NEML2 derivatives, incompatible shapes, unsupported measures, and stale
     batch mappings should fail explicitly rather than produce value-only AD properties.

## Candidate architecture to evaluate

The following is a design direction, not an approved API:

1. A differentiable input adapter gathers both each NEML2 input value and its MOOSE/Kokkos AD
   derivative seeds.
2. NEML2 evaluates batched response values and either output-input Jacobians or equivalent JVPs.
3. A derivative-composition adapter applies the chain rule on device.
4. The adapter writes a canonical `MaterialProperty<ADReal, rank>` with both values and MOOSE
   derivative indices preserved.
5. Solid-mechanics AD materials perform any required differentiable stress-measure transforms.
6. Downstream kernels and interface materials consume that property without backend-specific code.

Native Kokkos constitutive models should preferably meet the same consumer contract by templating
the constitutive calculation on the scalar type and evaluating it directly with `ADReal`.

## Approaches explicitly not accepted as the general solution

- A SCZM-specific tangent contraction.
- Reconstructing AD stress from `Real stress + rank-four tangent + displacement gradient` as the
  universal constitutive API.
- Assuming small-strain kinematics in a framework-level differentiability contract.
- Copying only NEML2 values into an `ADReal` property with empty derivatives.
- Treating NEML2 derivatives as automatically equivalent to MOOSE AD derivatives without an
  explicit chain rule and tensor-layout contract.
- Adding a general AD material dispatcher solely to make one SCZM case compile.

A tangent lift may still be useful as a narrowly scoped test backend, but it cannot define the
production architecture.

## Independent regression layers

Implementation should be split into separately reviewable and testable layers:

1. **Framework AD material infrastructure**
   - A simple volume producer/consumer chain verifies that seeded solution derivatives survive
     material-property storage and reach an AD residual.

2. **Derivative composition**
   - A small analytic batched model with multiple inputs verifies the chain rule, shared nonlinear
     dependencies, tensor component mapping, and derivative indices.

3. **NEML2 value and derivative transfer**
   - Compare composed AD derivatives against finite differences for a stateless model on CPU.
   - Exercise symmetric Mandel and full-tensor mappings independently.

4. **Stateful constitutive behavior**
   - Verify trial/commit semantics, repeated residual/Jacobian evaluations, timestep rejection, and
     remapping after batch-layout changes.

5. **Solid-mechanics finite-strain path**
   - Verify the selected kinematic and stress measures, including differentiable stress
     transformations.

6. **F-bar/nonlocal path**
   - Use a case whose Jacobian requires the nonlocal dependency; compare against finite
     differences.

7. **SCZM directional correction**
   - Only after the preceding layers pass, enable AD directional correction and compare the full
     interface Jacobian against finite differences, including primary/neighbor couplings.

CPU/OpenMP tests can be developed locally. CUDA/XPU execution and device-memory behavior require
CI or suitable GPU hardware; passing CPU tests alone is not evidence of GPU correctness.

## Completion criteria

This future work is complete only when:

- a NEML2 constitutive output can be exposed as a Kokkos `MaterialProperty<ADReal, rank>`;
- its MOOSE derivative entries include all declared differentiable NEML2 inputs through an
  explicit, tested chain rule;
- stateful, batched, tensor-layout, and device semantics are documented and tested;
- finite-strain and F-bar dependencies are represented without SCZM-specific handling; and
- AD SCZM directional correction consumes the canonical property without tangent contraction or
  knowledge of the NEML2 backend.
