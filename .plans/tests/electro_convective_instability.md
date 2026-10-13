# Plan: Electro-Convective Instability Test

## Reference
Based on Section `\subsection{Electro-convective instability}\label{sec:alimani}` from `papers/NS-PNP/arXiv-2301.05985v3/methods_paper_arxiv.tex`.

## Objective
Create a test case for electro-convective instability near an ion-selective surface. This test will be added in the same directory as `electro_osmotic_flow.i`, as `modules/combined/test/tests/electro_osmotic_flow/electro_convective_instability.i`.

## Domain & Mesh
- **Geometry**: Rectangular channel of dimensions $1 \times 1$ (reduced from paper's $8 \times 1$ to reduce mesh size for testing).
- **Mesh**: 
  - For the test suite, we will artificially widen the Electric Double Layer (EDL) (e.g., using $\lambda \sim 0.1$) to allow for a coarser mesh while keeping the test fast.
  - Mesh parameters: `nx=16`, `ny=64` with a bias of `bias_y = 1.1` to pack elements densely near the bottom boundary ($y=0$).
  - The side walls ($x=0, 1$) will use periodic boundary conditions. (original: x = 0~ 8 is too large)

## Variables
- `potential` ($\phi$)
- `c_positive` ($c_+$)
- `c_negative` ($c_-$)
- `velocity` (vector)
- `pressure` ($p$)

## Boundary Conditions (Table 2 from paper)
| Boundary         | Velocity (u, v) | Potential ($\phi$) | $c_+$ (Cation) | $c_-$ (Anion) |
|------------------|-----------------|--------------------|----------------|---------------|
| **Top Wall**     | Dirichlet: 0, 0 | Dirichlet: 20      | Dirichlet: 1   | Dirichlet: 1  |
| **Bottom Wall**  | Dirichlet: 0, 0 | Dirichlet: 0       | Dirichlet: 2   | Zero flux     |
| **Side Walls**   | Periodic        | Periodic           | Periodic       | Periodic      |

## Initial Conditions
- A fully developed Electric Double Layer (EDL) plus a 1% random perturbation in concentrations to trigger the electrohydrodynamic instability.
- In the MOOSE test, this can be achieved using a `RandomIC` combined with a `ParsedFunction` or by running a stationary PNP solve before coupling NS.

## Execution
- **Transient solve** (Navier-Stokes + PNP).
- The instability triggers chaotic fingering in the charge density, which necessitates stabilization (VMS for Navier-Stokes, SUPG/cross-wind for PNP).
- Timestepping: Use a manageable timestep (e.g., $\sim 10^{-2}$ or adaptive) to capture the transient instability fingering without exceeding CI test time limits.

## Postprocessors
- **Net Species Flux**: Extract the quantitative current/flux through the top boundary to compare against benchmark data ($|j_T^\pm|$).
  $$j_T^\pm = -\frac{1}{L_T} \int_{\Gamma_{top}} \left( \nabla c^\pm + z^\pm c^\pm \nabla \phi \right) \cdot \hat{n}_T \, d\Gamma$$
- This will be added via appropriate MOOSE side flux / current Postprocessors.
