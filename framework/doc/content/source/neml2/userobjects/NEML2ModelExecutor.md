# NEML2ModelExecutor

!if! function=hasCapability('neml2')

!syntax description /UserObjects/NEML2ModelExecutor

!alert note
Users are +NOT+ expected to directly use this object in an input file. Instead, it is always recommended to use the [NEML2 action](syntax/NEML2/index.md).

## Description

This object uses the specified NEML2 material model to perform mesh-wise (or subdomain-wise) batched material update.

Each NEML2 model +input variable+ is gathered from MOOSE by a `MOOSEToNEML2` user object (gatherer) given in [!param](/UserObjects/NEML2ModelExecutor/gatherers). Optionally, NEML2 model +parameters+ can also be gathered from MOOSE by gatherers given in [!param](/UserObjects/NEML2ModelExecutor/param_gatherers).

Each model +output+ and its +derivatives+ with respect to input variables and model parameters can be retireved by a [NEML2ToMOOSEMaterialProperty](NEML2ToMOOSEMaterialProperty.md) material object.

Kokkos FE-interpolated inputs use an element/quadrature-point batch `[nelem, nqp, ...base]`.
Flat gathered inputs and parameters are reshaped to match this batch when their base shapes agree with the model metadata.
The cpp-eager runtime evaluates this native layout directly.
For cpp-AOTI, the model handle broadcasts inputs and parameters to the common batch and collapses only the dynamic batch axes to `[nelem * nqp, ...base]` before evaluation.
Outputs and input and parameter Jacobians are restored to the native batch layout so Kokkos material properties retain their element mapping and managed state retains its layout.
Base-only outputs and derivative blocks remain unbatched.

## NEML2 model execution

The actual execution of the NEML2 model takes place in the `execute()` method. The model execution involves five steps:

1. Re-allocate the model, if necessary
2. Fill out model input variables and parameters
3. Apply the predictor
4. Solve, i.e., perform the material update
5. Extract model output variables and their derivatives

Note that the model is only re-allocated when the gathered batch size and the model's batch size do not match, which could happen

- Before the very first material update;
- After a mesh-change event which results in a change in the number of quadrature points in the operating subdomain.

!syntax parameters /UserObjects/NEML2ModelExecutor

!if-end!

!else

!include neml2/neml2_warning.md
