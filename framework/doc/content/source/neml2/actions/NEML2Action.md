# NEML2Action

!if! function=hasCapability('neml2')

!syntax description /NEML2/NEML2Action

!alert note
This page is developer-facing. Users please refer to the [NEML2 syntax](syntax/NEML2/index.md) documentation.

## Overview

This is the primary action responsible for constructing objects for transferring data back and forth between MOOSE and NEML2, as well as for executing the NEML2 model.

Four types of objects are constructed by this action:

- +Gatherer+: Object responsible for gathering MOOSE data.
- +Retriever+: Object responsible for retrieving NEML2 data and assigning it back to MOOSE data structures.
- +Index generator+: Object responsible for generating the element-to-batch-index map.
- +Executor+: Object responsible for sending data gathered by gatherers to NEML2, execute the NEML2 model, and have the outputs ready for retrieval by retrievers.

Currently supported objects are summarized below.

| Type            | MOOSE object(s)                                                                                                                      |
| :-------------- | :----------------------------------------------------------------------------------------------------------------------------------- |
| Gatherer        | [MOOSEQuantityToNEML2](../userobjects/MOOSEQuantityToNEML2.md), [KokkosQuantityToNEML2](../userobjects/KokkosQuantityToNEML2.md), [KokkosInterfaceQuantityToNEML2](../userobjects/KokkosInterfaceQuantityToNEML2.md) |
| Retriever       | [NEML2ToMOOSEMaterialProperty](../materials/NEML2ToMOOSEMaterialProperty.md), [NEML2ToKokkosMaterialProperty](../materials/NEML2ToKokkosMaterialProperty.md) |
| Index generator | [NEML2BatchIndexGenerator](NEML2BatchIndexGenerator.md)                                                                              |
| Executor        | [NEML2ModelExecutor](NEML2ModelExecutor.md)                                                                                          |

Multiple gatherers and retrievers may be created by this action, but each instance of the action creates one and only one index generator and executor.  Each step performed by this action is explained in one of the following sections, in the order of execution.

## Applying common parameters

Recall that input file parameters in the common area under the `[NEML2]` block defined by the [NEML2ActionCommon](NEML2ActionCommon.md) are shared by all sub-blocks. The first step is to retrieve the common parameters and apply them to this sub-block. This happens in the constructor of the action. Note that parameters defined in the sub-block overrides the parameters defined in the common area.

## Creating the NEML2 model

By the time this action "acts", the NEML2 input file must have already been parsed and loaded by [NEML2ActionCommon](NEML2ActionCommon.md). Therefore, the NEML2 model associated with this action can be directly created using the name given by [!param](/NEML2/model).

## NEML2 model introspection

A high-level summary of the NEML2 model is printed to the console, including model name, evaluation device, as well as a table summarizing the model's variables, parameters, and buffers.

NEML2 model allows introspection, i.e., variable and parameter names, tensor types, and storage sizes can be inferred after the model is loaded. Five maps are set up upon introspection of the NEML2 model:

1. A map between MOOSE data structures and NEML2 +input variables+.
2. A map between MOOSE data structures and NEML2 +model parameters+.
3. A map between MOOSE data structures and NEML2 +output variables+.
4. A map between MOOSE data structures and NEML2 output +derivatives+.
5. A map between MOOSE data structures and NEML2 +parameter derivatives+.

These maps are used later to create objects for transferring data between MOOSE and NEML2.

## Creating MOOSEToNEML2 gatherers id=gatherer

This step involves the creation of `MOOSEToNEML2` gatherers for NEML2 input variables and model parameters. The first two maps mentioned in the previous section are used. The maps are looped through to create objects one-by-one.

The `input_backend` parameter selects the gatherer backend. Its default `moose` value uses
[MOOSEQuantityToNEML2](../userobjects/MOOSEQuantityToNEML2.md) for bulk and interface inputs. The
`kokkos` value creates [KokkosQuantityToNEML2](../userobjects/KokkosQuantityToNEML2.md) for bulk
inputs and [KokkosInterfaceQuantityToNEML2](../userobjects/KokkosInterfaceQuantityToNEML2.md) for
`interface_only` inputs.
Kokkos input gathering supports current-state full `RankTwoTensor` material properties and bulk
current-state `SymmetricRankTwoTensor` material properties, which use six-component Mandel packing;
variables, history inputs, model parameters, input kernels, and true interface-material properties
are rejected during Action setup. This selection is independent of `output_backend`.

The NEML2 input shape determines the MOOSE tensor type. For example, an NEML2 input variable of
type `SR2` maps to `SymmetricRankTwoTensor`.

## Creating the NEML2 index generator and model executor

Next, a [NEML2BatchIndexGenerator](NEML2BatchIndexGenerator.md) and a [NEML2ModelExecutor](NEML2ModelExecutor.md) excutor are created. The list of [!param](/UserObjects/NEML2ModelExecutor/gatherers) and [!param](/UserObjects/NEML2ModelExecutor/param_gatherers) are automatically filled out by this action.

## Creating NEML2ToMOOSE retrievers

The last step is creating the `NEML2ToMOOSE` retrievers to retrieve NEML2 model outputs (and their derivatives) back to MOOSE. This step is similar to [creating MOOSEToNEML2 gatherers](NEML2Action.md#gatherer).

### Interface-only output id=interface_output

With [!param](/NEML2/interface_only) set to `true` the model is evaluated at the quadrature points of
both sides of the sideset named by [!param](/NEML2/interface), so each side of a split interface
carries its own constitutive state. The retrievers follow: for every output and derivative the action
creates one retriever that publishes the primary side into the boundary material data, which an
interface consumer reads with `getMaterialProperty`, and a second one that publishes the neighbor side
into the neighbor material data, which it reads with `getNeighborMaterialProperty`. The neighbor
retriever is registered as an interface material so that it runs after the neighbor element and its
material data have been reinitialized. Both backends behave the same way, and
[!param](/NEML2/interface_output_side) selects which sides are published.

Because these retrievers are boundary restricted, they declare their properties on the sideset rather
than on the adjacent subdomains, so a bulk model emitting the same output names on those subdomains
does not collide with them. Where both are active the interface values take precedence: on the
interface sideset the interface retrievers run after the bulk face and neighbor material copies, so a
consumer reading such a name there sees the interface model's value, while the bulk values are
untouched in the volume and on every other face.

## Debugging a failed constitutive update id=dump

A NEML2 constitutive update can fail to converge for some elements -- a *recoverable* return-map failure (a Newton divergence or max-iterations in the material solve). MOOSE then cuts the time step and retries, so the exact inputs that triggered the failure are normally lost by the time the run ends. Set [!param](/NEML2/dump_inputs_on_failure) to `true` to capture them: on each failed update the executor serializes the whole local batch of NEML2 input tensors (strains, temperature, old state, model parameters, ...) to a per-rank TorchScript file `<model>_count<c>_rank<r>.pt`.

The counter `<c>` is advanced collectively, so it is identical on every rank for a given failed update; since MOOSE streams only rank 0 to the console, the reported message names the `<model>_count<c>_rank_*.pt` glob (one file per failing rank) along with the time step, `execute_on` flag, and nonlinear iteration for context.

Each file can be loaded offline with `torch.jit.load` -- its buffers are the model's input variables (characters that are invalid in identifiers, such as the `~` in old-state lag names, are replaced by `_`) -- so the model can be re-evaluated on exactly the failing batch and the divergence diagnosed independently of MOOSE. This is a debugging aid; leave it off (the default) for production runs.

To make that diagnosis near-zero-boilerplate, NEML2 can attach a *failure context* to the recoverable `ConvergenceError` a non-converged constitutive update raises. Set the `NEML2_CAPTURE_SOLVE_FAILURE` environment variable before the failing solve and the exception carries two extra attributes:

- `converged_mask` -- a per-batch-entry boolean tensor, `True` where that quadrature point converged. Isolating *which* points diverged is then a single `(~e.converged_mask).nonzero()`, rather than re-running each entry as its own size-1 batch.
- `unknowns` -- a `{unknown-variable name -> best-effort iterate}` map: the state the Newton solve got stuck at, ready to inspect. Each value is a NEML2-typed tensor when replaying an eager model, or a plain `torch.Tensor` for a compiled one; both carry the dynamic-batch leading dim.

The capture is identical whether you load the eager model `.i` or the `<model>_aoti.i` compiled stub, and identical to what MOOSE sees at runtime. It is off by default because it costs an extra masked re-solve on each failure; the boilerplate below turns it on at import time.

The following boilerplate (1) loads a dumped file and maps its buffers back onto the model's inputs, and (2) re-runs the model on the whole failing batch to reproduce the failure and read the failing entries + stuck state off the exception (export `NEML2_LOGS="newton=info"` to additionally print the per-iteration residual history):

!listing test/tests/neml2/debug_dumped_inputs.py
         start=import os
         end=class DumpedInputReplayTest
         include-end=False
         language=python
         caption=Re-evaluate a NEML2 model on a dumped failing batch and read the failure context (from [!param](/NEML2/dump_inputs_on_failure) with `NEML2_CAPTURE_SOLVE_FAILURE`).

!syntax parameters /NEML2/NEML2ActionCommon

These input parameters correspond to the sub-blocks under the `[NEML2]` block. The usage of the `[NEML2]` block is explained in details in the [NEML2 syntax](syntax/NEML2/index.md) documentation.

!if-end!

!else

!include neml2/neml2_warning.md
