# NS--PNP on MOOSE manuscript instructions

Keep `AGENTS.md` and `CLAUDE.md` identical.

## Scope and files

- This folder contains only the paper. The MOOSE source tree is at `../../..`. Check the implementation whenever manuscript details must match the code, including signs, discretizations, solver settings, algorithm steps, and reported numbers.
- `pnp_moose.tex`: complete manuscript; `elsarticle` (`3p,times`), built with pdflatex.
- `bibtex.bib`: only bibliography; style `elsarticle-num-names`.
- `.claude/skills/fix-bibtex/`: citation-key canonicalization skill.
- Never hand-edit `pnp_moose.{aux,bbl,blg,log,out,fls,fdb_latexmk,pdf,spl}` or edit/commit `*.bak-*`.
- Every external file referenced by `pnp_moose.tex` must live under this manuscript folder, normally in `Figures/` or `inputs/`, and must be tracked by Git. Never reference simulation outputs through `../` paths or absolute local paths; copy adopted artifacts into the manuscript first.

## Build

```sh
latexmk -pdf pnp_moose.tex
```

The build uses pdflatex and bibtex. Rebuild after citation changes to regenerate `pnp_moose.bbl`.

## Writing

- Write direct, technical, reader-friendly prose in the present tense. Prefer concrete subjects and verbs, one job per sentence, and evidence-bounded claims.
- Define every symbol, acronym, dimensionless number, and technical term at first operational use.
- Introduce mathematics progressively: governing equations, boundary conditions, discretization, implementation, and validation.
- State sign conventions, orientations, units, evaluation locations, and applicability where they first matter.
- Check all MOOSE object names, parameters, defaults, and solver behavior against the source and input files rather than inferring them.
- Present validation from simple isolating tests to coupled demonstrations. Support accuracy, conservation, robustness, and performance claims quantitatively.
- Keep numerical data for tables and quantitative plots in committed CSV files. Do not hard-code numerical table rows or plot coordinates in `pnp_moose.tex`.
- Every table and figure must be cited and interpreted in the body. Captions must be self-contained.
- Match every claim to its evidence and qualifiers, including mesh, norm, timestep, solver tolerances, process/thread count, and hardware when relevant.
- Use `\citet{key}` when authors are grammatical subjects and `\citep{key}` for parenthetical citations.
- Citation keys use `<surname><year><first-title-word>`. Rename them only through the `fix-bibtex` skill, which updates `bibtex.bib` and all citation commands together.

## Editing

- Keep manuscript paragraphs as single long lines and do not rewrap untouched prose.
- Add recurring notation commands in the preamble rather than defining them ad hoc in the body.
- Preserve draft markers and TODO comments until the underlying issue is resolved.
