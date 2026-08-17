# EqualValueBoundaryConstraint Mesh Comparison

The finite-strain `EqualValueBoundaryConstraint` case produces the same result within normal numerical tolerance when using replicated or distributed meshes with 1, 2, or 3 MPI ranks. The results are not bit-for-bit identical.

| Mesh | MPI ranks | Final strain |
| --- | ---: | ---: |
| Distributed | 1 | `0.00029662992177241` |
| Distributed | 2 | `0.00029662992177241` |
| Distributed | 3 | `0.00029662992177242` |
| Replicated | 1 | `0.00029662992174238` |
| Replicated | 2 | `0.00029662992174238` |
| Replicated | 3 | `0.00029662992174237` |

## Interpretation

- Changing the processor count from 1 to 3 has essentially no effect within either mesh type.
- Distributed and replicated results differ by approximately `3.0e-14` in absolute strain.
- The relative difference is approximately `1.0e-10`.
- All configurations report `2.966299e-04` at the usual displayed precision.
- The small difference is consistent with floating-point operation ordering, mesh partitioning, and parallel reductions. It does not indicate a behavioral difference.
- A cross-mesh CSV comparison should use approximately `rel_err = 1e-9` or an appropriate absolute tolerance. The existing `rel_err = 1e-12` is suitable for processor-count comparisons within the distributed-mesh results, but is too strict for comparing distributed against replicated meshes.
