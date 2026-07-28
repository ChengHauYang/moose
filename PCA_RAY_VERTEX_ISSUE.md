<!--
================================================================================
  Temporary working note. DO NOT `git add` or commit this file.
  Delete it once the issue is resolved (rm PCA_RAY_VERTEX_ISSUE.md).
================================================================================
-->

# PCA ray vertex-degeneracy issue on symmetric geometry

## Symptom

For a symmetric L-shaped boundary, the `pca_ray` (AUTO_PCA) point-in-polygon method reports
`AdaptiveRayContainmentCheck: No decision could be made for point ...` (mooseError) for some
background-mesh centroids.

- This is NOT a `.msh` read failure. It is the PCA ray hitting a vertex / concave corner, which
  makes the crossing parity impossible to decide reliably.
- Making the L-shape asymmetric makes the test pass, but that only *works around* the problem;
  the underlying defect is still there.

## How to reproduce

1. Symmetric L-shape (symmetry axis = y=x):
   `(1,1) -> (3,1) -> (3,2) -> (2,2) -> (2,3) -> (1,3) -> (1,1)`
2. Read it in `pca_ray_2d_msh.i` via `FileMeshGenerator`, hand it to `BoundaryMeshBuilder`
   with `save_with_name`.
3. Background: regular Cartesian mesh over `[0,4]^2`, `point_containment_method = pca_ray`.
4. A point on the diagonal (e.g. `(1.875, 1.875)`) fires a ray along y=x that hits a
   vertex / concave corner -> No decision.

The currently checked-in `test/tests/userobjects/point_in_polyhedron/l_shape_boundary.msh`
is already the *asymmetric* version (`(1,1),(3.5,1),(3.5,1.8),(1.8,1.8),(1.8,3),(1,3)`),
which sidesteps the problem for now.

## Root cause (three locations)

### A. Direction choice guarantees a vertex hit (the trigger)
`framework/src/utils/AdaptiveRayContainmentCheck.C:317`
```cpp
_ray_direction = (_dim == 3) ? _min_variance_vector : _second_variance_vector;
```
PCA of the symmetric L (centroid = (2,2), covariance matrix `[[4,-1],[-1,4]]`) gives:
- max variance (lambda=5) -> `(1,-1)` (anti-diagonal)
- second variance (lambda=3) -> `(1,1)` (diagonal y=x)

In 2D the ray uses the second-variance vector -> ray runs along **y=x**. For a symmetric shape
the PCA principal axis IS the symmetry axis, so a query point on the diagonal firing along the
ray **necessarily hits vertices** `(2,2)` and `(1,1)`. This is structural, not bad luck.

### B. Intersection counting breaks at a shared vertex (the actual defect)
`framework/src/geometries/LineSegment.C:188`
```cpp
if (s >= 0 && s <= 1 && t >= 0 && t <= 1)   // both endpoints inclusive, and no vertex-crossing rule
```
A shared vertex is counted once by each of the two adjacent edges (one with t=1, the other with
t=0, both inclusive). On top of that, floating point makes it unstable: whether t lands on `1.0`
vs `1.0000001`, or `0.0` vs `-1e-16`, depends on rounding, so the count at a single vertex
jitters between **0 / 1 / 2**. The two opposite rays then disagree on parity ->
`sidenessFromRayPair` returns `nullopt` (`AdaptiveRayContainmentCheck.C:148-150`).

### C. The fallback cannot rescue it
`framework/src/utils/AdaptiveRayContainmentCheck.C:112-122`
The fallback only probes the other OBB axes, but the OBB axes ARE the PCA principal axes
`(1,-1)` and `(1,1)`. For a symmetric shape those are all symmetry axes, so the fallback rays
graze vertices too -> it ends at `mooseError("No decision ...")`
(`AdaptiveRayContainmentCheck.C:126`).

## Why "count a coincident intersection point only once" (dedup) is not a general fix

Dedup fixes the overcount ("same point counted twice"), but it just **moves the bug from one
kind of vertex to another**:

| Vertex type | Correct contribution | Current code (both ends inclusive, double-counts) | Dedup (coincident point counted once) |
|---|---|---|---|
| True crossing (ray goes in->out, e.g. L's (2,2)) | 1 (odd) | 2 (even) WRONG | 1 (odd) OK |
| Touch (local extremum, both edges same side) | 0 or 2 (even) | 2 (even) OK | 1 (odd) WRONG |

- Current code: wrong at true-crossing vertices, right at touch vertices.
- Dedup: right at true-crossing vertices, **wrong at touch vertices**.

Counterexample where a touch bites (and is NOT caught by `isOnSurface`, because the grazed
vertex is not the query point p):
```
Rectangle [0,4]x[0,4] with a downward spike on the top edge, tip at (2,2)
Boundary (CCW): (0,0)->(4,0)->(4,4)->(2.5,4)->(2,2)->(1.5,4)->(0,4)->(0,0)
Query point p = (3.5, 2)   (interior; not on the boundary; inside bbox -> reaches counting)
Horizontal ray: (-100,2) -> p
```
Correct: left edge crossed once + tip touch(0) = 1 (odd) -> inside.
Dedup: 1 + tip counted as 1 = 2 (even) -> misclassifies as outside.

Dedup also does not fix B's undercount (both edges narrowly miss due to FP -> 0 intersections,
nothing to dedup), and "same point" needs a tolerance, which is itself a new source of fragility.

## IMPLEMENTED (2D only) - pending build/test verification

Chosen approach: node-id grouping + side test (NOT coordinate dedup, NOT jitter), scoped to `_dim == 2`.
The 3D path is unchanged.

Files touched:
- `framework/include/utils/AdaptiveRayContainmentCheck.h`
  - new `struct VertexTopology { Point pos; std::vector<Point> neighbors; }`
  - new member `std::map<dof_id_type, VertexTopology> _vertex_topology`
  - new method decls: `countCrossings2D`, `vertexCrossingContribution`, `buildVertexTopology`
  - added `#include <map>`
- `framework/src/utils/AdaptiveRayContainmentCheck.C`
  - anonymous namespace: `enum class EdgeHit {NONE, INTERIOR, AT_P0, AT_P1}`, `cross2D`,
    `classifyRayEdgeHit2D` (parametric-t band `edge_param_tol = 1e-8`, scale-free parallel test)
  - `countCrossings` branches to `countCrossings2D` when `_dim == 2`
  - `countCrossings2D`: interior hits += 1; endpoint hits collected into a `std::set<dof_id_type>`
    (grouping); each grouped vertex resolved by `vertexCrossingContribution`
  - `vertexCrossingContribution`: side test via `cross2D(seg_dir, neighbor - V)`; opposite signs -> 1,
    same side (touch) -> 0, collinear incident edge (`side_tol = 1e-9`) -> 0 (defer to fallback)
  - `buildVertexTopology` called from the constructor when `_dim == 2`
  - added `#include <set>`
- `unit/src/PointInPolygonTest.C` - 3 new gtests:
  - `SymmetricLDiagonalVertexHits` (AUTO_PCA; the reported bug)
  - `TrueCrossingAtVertex` (USER_SPECIFIED +x diamond; fails before / passes after)
  - `TangentTouchAtSpikeTip` (USER_SPECIFIED +x; touch must count 0)

VERIFIED (moose conda env, opt):
- moose-unit: all 7 AdaptiveRayContainmentCheck gtests pass (3 new + 4 existing).
- app-level: all 17 userobjects/point_in_polyhedron tests pass (no regression).
- clang-format: git clang-format run over the staged changes (only reflowed one call in the test).

OPEN DECISION (not a bug, a test-quality question):
- `PcaFallbackUsesFallbackDirection` relied on the OLD double-count making the two primary rays
  disagree (counts 2 vs 3) so the fallback fired. With correct vertex counting the disagreeing ray
  now counts vertex (2,4) once, so both primary rays are even and return OUTSIDE WITHOUT the
  fallback. Result (OUTSIDE) unchanged and the test still passes, but it no longer exercises the
  fallback. The fallback is still live for genuine collinear-with-edge degeneracies. Options:
  (a) leave it, (b) add a collinear-with-edge geometry test that keeps exercising the fallback.
  Do NOT silently rewrite the existing test.

## Correct fix directions (reference)

1. **Side-based crossing-number (real fix, changes B)**
   For each edge, test its endpoints' side relative to the ray using the sweep coordinate (the
   axis perpendicular to the ray):
   ```
   count += ( (a_side <= 0) != (b_side <= 0) )
   ```
   The `<=` gives a half-open ("one end closed, one end open") convention: a true crossing counts
   exactly 1, and a touch cancels to 0 or 2. No dedup, no tolerance needed. Note that a plain
   t-based half-open (t in [0,1)) does NOT fix the touch case; you must test the *side*.

2. **Ray jitter (workaround, changes A)**
   In `:317`, perturb the ray direction by a tiny irrational angle so the ray essentially never
   hits a vertex; B/C are all sidestepped and the counting core is untouched. Cost: the ray is no
   longer strictly along the PCA axis (negligible effect).

The two can be combined (jitter removes the "hit a vertex exactly" instability; the side-based
rule guarantees correct vertex counting when a hit does happen).

## Acceptance when fixed

- Add back a symmetric-L regression test (a point on the diagonal).
- Add a "downward spike + interior p" touch regression test.
- Both must return stable, correct inside/outside with no No-decision error.
- Delete this file once done.
