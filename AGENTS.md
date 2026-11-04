<!--
Adapted from https://github.com/forrestchang/andrej-karpathy-skills
Licensed under the MIT License.

Section 9, and the process-narration rule in section 4, are adapted from the "Writing PETSc
Contribution Materials" section of PETSc's AGENTS.md (https://gitlab.com/petsc/petsc).
Copyright (c) 1991-2025, UChicago Argonne, LLC and the PETSc Developers and Contributors.
Licensed under the BSD 2-Clause License.
-->

# AGENTS.md

These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## Verification Cadence (Read First)

- Do not build or test after every edit; verify one coherent, reviewable change slice at a time.
- During implementation, prefer source inspection, CodeGraph, and diff review. When the slice is
  ready, format changed C++ files, run the narrowest relevant build, and run the smallest focused test.
- Run broad suites, MPI/distributed variants, and docs only for the change's blast radius, at a
  milestone, or before final handoff. Do not repeat unchanged checks or build for docs/comments/plans
  unless generated code or test discovery is affected; never claim runtime behavior without verification.

## 0. Local Harness Map

Cheng-Hau has two private stacks, each with one per-machine entry point. Define `{machine} = mac` or
`cuda`, and drive everything through `./scripts/{machine}/<entry>` (shown as `moose` below):

- `{machine} = cuda` -> `./scripts/cuda/moose_cuda.sh` -- Linux GPU box, from-scratch CUDA-Kokkos stack under `kokkos-cuda-stack/`.
- `{machine} = mac` -> `./scripts/mac/moose.sh` -- macOS Apple Silicon, Conda `moose` stack.

Common commands (identical on both machines):

- Inspect compilers, CUDA/MPI, Python, paths: `moose doctor`
- Initialize dependencies and configuration: `moose init`
- Incremental compile: `moose compile <framework|solid_mechanics|shifted_boundary_method|neml2> [-j N]`
- Run an input: `moose run <target> <input.i> [options...]`
- Test without compiling: `moose test <scope> [pattern] [options...]`
- Compile and test: `moose verify <scope> [options...]`
- Full initialization, compile, and test: `moose all`

Machine-specific:

- **cuda:** build state is `moose status`. `run`/`test`/`verify` add `--compute-device=cuda` by default;
  override with `--compute-device=cpu`. A CUDA build only proves the kernels compiled -- confirm actual
  GPU execution by running on `--compute-device=cuda`.
- **mac:** dashboard/progress is `moose dummy`; C++ GoogleTest unit tests are `moose unit [target] [pattern]`
  (and `unit` is also a `compile`/`verify` scope); install the pre-commit hook once per clone with
  `moose precheck`.

The cuda harness does not depend on or conflict with direnv. On that box `.envrc` sources
`kokkos-cuda-stack/scripts/activate.sh` when you `cd` in, exporting the CUDA-stack paths (`PETSC_DIR`,
`LIBMESH_DIR`, `WASP_DIR`, `OPAL_PREFIX`, `PATH`, `VIRTUAL_ENV`, ...). `moose_cuda.sh` runs in its own
subprocess and re-exports those same values itself (`cuda_export_settings`, plus `env.sh` to purge
Conda build hints during builds), so it produces identical results whether or not direnv has
activated. There is no need to deactivate direnv before using it.

Do not treat missing generated files such as `framework_config.h`, configured Makefiles, object files, or module binaries as a broken checkout. Fresh checkouts are expected to lack them. Run `moose doctor` before inventing build commands or asking how to configure MOOSE.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.
- If existing functionality can accomplish a subtask, use it, don't
  reimplement it.
- If existing functionality is nearly capable of a subtask, prefer
  short extensions or bug fixes of it over long rewrites of it. If the
  extension only fits by distorting the existing design, rework the design.
- Dependencies are in scope for bug fixes and feature additions, whether they
  live in a submodule or in an installed copy the build links against. The
  change belongs where the defect or missing capability is, not where the build
  is convenient.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Make the smallest change that gets the computer science and the mathematics right.**

Diff size is a tiebreaker among correct designs, and the design wins when the two
conflict. If the minimal patch requires the worse algorithm, the wrong data
structure, a formulation that only happens to work on the case at hand, or a
special case bolted onto a general routine, take the larger change instead and
say what you are widening and why.

A widening that stays inside the code you were already editing needs only that
note; proceed. For a large restructuring - spanning files beyond the ones the task
named, changing a public interface, or reworking something other code depends on -
present both paths with their costs and wait for the user to choose. Crossing into
a dependency is not by itself a large restructuring; judge the change by its size
and reach, not by which repository it lands in.

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.
- Refactor when the correct design needs it - a better interface, data layout, or
  algorithm - and leave working code alone otherwise.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace to the user's request or to the design
that request requires.

## 4. Code Comments

- Never delete correct preexisting code comments.
- Add comments when the code alone does not make clear what's happening.
- Explain every non-obvious numeric value, including tolerances, with a nearby
  comment that records its source or rationale rather than merely restating it.
- Add Doxygen comments for classes and class members whose purpose is not
  obvious from their names.
- Omit process narration and edit history. A comment records what the code does
  and why, not the sequence of changes or investigation that produced it.
- Use ASCII characters.

## 5. Code Style

- Use `make_range` for integer range-based for loops (e.g. `for (const auto i :
  make_range(n))`) instead of raw index loops. Use `index_range(container)`
  when iterating over the indices of a container. But don't use `make_range`
  in Kokkos functions or any other device functons.
- Use `libmesh_map_find` for map lookups instead of `.at()`.
- MOOSE requires C++17, so modern C++ constructs up through that standard are
  encouraged where they increase code readability. Along those lines, when a member
  of a structured binding is unused, bind it as `_` instead of avoiding the structured
  binding solely because a member is unused.
- Put multiline method implementations outside class definitions.

## 6. Tool Use

Use the Local Harness Map (Section 0) before asking about the environment or inventing
build commands. The `moose` entry point sets up its own stack: on macOS it activates the
Conda `moose` env, and on the Linux GPU box the cuda harness is a self-contained
from-scratch stack (no Conda) that also matches the direnv-activated `.envrc`.
- Prefer builtin tools over bash commands whenever possible in the vein of
  reducing permission prompting.
- Before building or performing verification OUTSIDE the harness, including running
  tests or invoking a pre-existing MOOSE executable, establish the environment instead
  of relying on an existing binary or current shell state: for a Conda stack (macOS, or
  Linux CPU work), ask whether the stack uses conda unless this has already been
  established in the conversation, and if it does ask which conda environment to activate
  and wait for the answer; for the Linux cuda stack, source its environment first
  (`.envrc` / `kokkos-cuda-stack/scripts/activate.sh`, or `cuda_runtime_env` from
  `scripts/cuda/moose_cuda_common.sh`).

## 7. TestHarness Coverage

- Do not create separate test specifications or gold files solely to exercise
  different MPI process counts, thread counts, mesh modes, recovery, or
  restep. Use the same test with the TestHarness options `-p<n>`,
  `--n-threads=<n>`, `--distributed-mesh`, `--recover`, and `--restep`.
  Split tests only when a mode intentionally has different inputs, expected
  output, or requirements.
- Give the TestHarness enough job slots for each test: `-j` must be at least
  the requested `-p` process count or `--n-threads` thread count. When MPI
  processes and threads are combined, use at least their product.
- When adding or changing a hand-coded Jacobian, including off-diagonal
  couplings, add or update a `PetscJacobianTester` test that exercises every
  coupling and coordinate system the change affects. A correct residual does
  not imply a correct Jacobian, and only a finite-difference comparison catches
  a missing or wrong term.

## 8. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## 9. Writing MOOSE Contribution Materials

Apply these rules to MOOSE contribution materials and their drafts: commit messages, pull
request descriptions, review reports and comments, documentation, and code comments. They do
not govern unrelated conversations or prescribe the user's conversational style.

- Lead with the result and why it matters. Make the text understandable without the drafting
  conversation; include verification, limitations, and reproduction details that a reviewer
  cannot get from CI.
- Write pull request descriptions as plain prose or bullets. Do not impose boilerplate section
  headings such as "Summary" or "Test plan"; only a genuinely multi-part change earns headings.
- Do not add a "Verified" or "Test plan" section. CI runs the test suite more thoroughly than
  any local sweep, so listing checks it already performs is noise to a reviewer. Report
  verification only where CI cannot reach it, such as a manual performance comparison or a
  downstream application build.
- Do not catalog what the change leaves undone in a "Not included", "Future work", or
  "Limitations" section. A related bug worth tracking belongs in its own issue, referenced in
  one line.
- Leave investigation dead ends out. A path that looked related and turned out not to be is not
  a finding, and reporting it competes with the actual change for the reviewer's attention.
- Write clear, grammatical, concise prose for peer computational scientists and engineers.
  Explain unfamiliar terms, preserve MOOSE terminology and exact names of classes, parameters,
  input syntax, commands, and diagnostics, and remove repetition and boilerplate before
  presenting.
- Use the vocabulary a MOOSE developer would use, and avoid stock phrasing that reads as
  machine-drafted. "bit-for-bit" is one such tell; say that results are unchanged.
- Prefer self-explanatory code, using comments for non-obvious behavior, correctness
  constraints, rationale, or required documentation, as described in Code Comments above.

---

**These guidelines are working if:** diffs are as small as the design allows, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
