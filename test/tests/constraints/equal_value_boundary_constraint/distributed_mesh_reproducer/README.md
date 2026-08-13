# Distributed EqualValueBoundaryConstraint reproducer

This directory preserves the user case that exposed a distributed-mesh crash and a reduced
framework-only input for debugging it.

Run the reduced case with at least two MPI ranks:

```text
mpiexec -n 2 ../../../../test-opt -i distributed_evbc.i
```

The failure occurs during problem initialization, before a solve, with this stack:

```text
SystemBase::augmentSendList()
libMesh::DofMap::prepare_send_list()
libMesh::System::init_data()
libMesh::System::reinit_mesh()
libMesh::EquationSystems::reinit_mesh()
FEProblemBase::init()
```

`SystemBase::augmentSendList()` iterates over `_subproblem.ghostedElems()` and calls
`_mesh.elemPtr(elem_id)`. `EqualValueBoundaryConstraint::ghostPrimary()` adds an element ID from
the node-to-element map directly to that set. On a distributed mesh, a rank can therefore retain
an ID for an element it does not store locally when `augmentSendList()` runs.

`LinearNodalConstraint` also adds node-connected element IDs directly and should be checked as
part of the eventual fix.

`original_creep.i`, `creepTest10Gr.exo`, `316H_simple.fixture`, and `grn_10_rand.tex` are the
supplied source case. The material database uses a `.fixture` suffix because this repository
ignores XML files. The original input depends on application-specific `NEMLCrystalPlasticity` and
`GrainBoundaryCavitation` objects; `distributed_evbc.i` removes those dependencies while retaining
the mesh-generation and constraint path implicated by the crash.

The reduced case is intentionally not registered in `tests` while it remains a crashing
reproducer. Add it as a two-rank regression test with the eventual fix.
