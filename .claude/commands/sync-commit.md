
---

## description: Inspect the working tree, update repo workflow/convention docs when needed, then create one or more coherent git commits from the selected scope.

# /sync-commit — Document workflow changes and commit coherently

Use this when the working tree has new or changed files and the user wants the changes reviewed, optionally documented, staged, and committed.

Prefer **a small number of coherent commits** over one giant mixed commit. Group related changes together so the history is easy to read.

This command must work from any folder inside a git repository. Do not assume a fixed project layout such as `examples/`, `studies/`, `benchmarks/`, or `tests/`. Infer the repo structure from the actual paths in the working tree.

## Inputs

`$ARGUMENTS` is optional free-form context, for example:

* "only commit changes under this folder"
* "commit docs and implementation separately"
* "keep this as one commit if the changes are tightly coupled"
* "the new driver is the recommended starting point"
* "do not update AGENTS.md"

If empty, infer the intent from the diff.

## Procedure

### 1. Find repo root and scope

First determine the repository root:

```bash
git rev-parse --show-toplevel
pwd
```

Use the current folder as the default working scope **only if the user clearly asks to commit this folder / current directory / this component**.

Otherwise, inspect the whole repository working tree.

Never assume the command is being run from repo root.

### 2. Survey the working tree

Run these independent reads:

```bash
git status --short
git status
git diff
git diff --staged
git log -4 --oneline
```

Use recent commits to match the repository's commit-message style: short, informal, lower-case subjects when that is the local convention.

If there are staged changes already, treat them carefully. Do not unstage them unless necessary and clearly justified. Preserve the user's existing staging intent when it is coherent.

### 3. Identify repo documentation conventions

Before editing documentation, inspect which convention docs already exist. Common examples include:

```text
AGENTS.md
CLAUDE.md
CONTRIBUTING.md
README.md
docs/
.github/
```

Only edit a convention/workflow doc when the change affects how future contributors or agents should work in the repo.

Do not create a new top-level convention document unless the user explicitly asks or the repo clearly already follows that pattern.

Do not invent directory-index README files just to catalogue new files. New scripts, examples, experiments, or runnables are usually discoverable through their paths and git history.

### 4. Classify every changed path before staging

Classify every changed path in the selected scope.

Use meaning, not file extension alone.

Common categories:

* **Library / implementation source**

  Reusable code, core modules, package code, shared utilities, public APIs.

  Requires a convention-doc update only if public API, core workflow, architecture, or major usage pattern changed.

* **Runnable / script / experiment**

  CLI scripts, examples, demos, notebooks, benchmarks, studies, prototypes, one-off drivers.

  New runnables do not need top-level documentation by default.

* **Workflow / automation / agent instructions**

  Command files, CI, build scripts, devcontainer files, lint/test setup, environment setup, repo-agent instructions.

  Usually requires updating an existing convention doc if the workflow changed.

* **Tests / validation**

  Unit tests, integration tests, regression tests, small reproducibility examples.

  Commit with the implementation they validate when tightly coupled.

* **Documentation / notes / manuscript**

  Markdown, LaTeX, design notes, paper text, explanatory docs.

  Usually self-contained unless they change contributor workflow.

* **Config**

  Package metadata, dependency files, formatter/linter configs, CI configs, project settings.

  Commit with the behavior they support.

* **Assets**

  Images, diagrams, small static files that are directly referenced by docs/slides/examples.

  Commit with the document or runnable that uses them.

* **Derived artefacts**

  Generated outputs such as `.csv`, `.png`, `.pdf`, `.vtk`, build outputs, result folders, cache folders.

  Verify whether they are intentionally tracked. Do not give generated outputs their own convention-doc bullet.

* **Noise**

  `.DS_Store`, `__pycache__/`, editor swap files, local settings, temporary files, LaTeX intermediates, build/cache directories.

  Leave unstaged.

### 5. Decide commit groups before editing

Create a short grouping plan from the classified paths.

Prefer groups such as:

```text
commit 1: update commit helper workflow
  - .claude/commands/sync-commit.md
  - AGENTS.md or CLAUDE.md if needed

commit 2: add solver reconstruction path
  - src/.../*.py
  - tests/.../*.py
  - docs note directly explaining the behavior

commit 3: update runtime study notes
  - docs/.../*.md
  - related figure assets if referenced
```

Good grouping is based on intent:

```text
commit 1: refine commit workflow command
commit 2: add focused runtime helper
commit 3: document benchmark setup
```

Bad grouping is based only on file type:

```text
commit 1: all python files
commit 2: all markdown files
commit 3: all images
```

Keep related files together. Do not split so aggressively that the history becomes noisy.

### 6. Edit convention docs only when needed

Edit `AGENTS.md`, `CLAUDE.md`, `CONTRIBUTING.md`, `README.md`, or another existing convention doc only when the change affects future repo operation.

Good reasons to edit convention docs:

* New required workflow.
* New command that contributors or agents should use.
* Changed build, test, lint, or environment setup.
* Changed repo architecture or canonical entry point.
* Changed commit, review, or release convention.

Bad reasons to edit convention docs:

* A new one-off script was added.
* A result file was generated.
* A note or paper section was edited.
* A file was renamed but no workflow changed.
* You want to create a directory catalogue.

Self-check:

* Every new documentation line must refer to a path, command, or workflow that exists in the repo or staged set.
* Do not document every new runnable.
* Do not document derived artefacts as conventions.
* Do not create new index files unless the repo already uses that pattern or the user explicitly asks.

### 7. Stage and commit by group

For each commit group:

* Stage files explicitly with `git add <path>`.
* Never use `git add -A`.
* Never use `git add .`.
* Use `git status` to confirm the staged set.
* Commit that group before moving to the next group.

Example:

```bash
git add .claude/commands/sync-commit.md AGENTS.md
git status
git commit -m "refine commit workflow command"
```

Then continue with the next group:

```bash
git add path/to/source.py path/to/test.py
git status
git commit -m "add focused reconstruction helper"
```

Use a HEREDOC body only when the reason is not obvious from the diff:

```bash
git commit -m "$(cat <<'EOF'
refine commit workflow command

Make the helper work from any subfolder and avoid assuming
a fixed repo layout when grouping commits.
EOF
)"
```

### 8. Commit-message rules

Each commit should have:

* A one-line subject under 72 characters.
* A short, informal style matching recent commits.
* A 2–3 line body only when the reason is not obvious.
* No vague subjects like `update files`, `misc changes`, or `fix stuff`.

Prefer:

```text
refine commit workflow command
add reconstruction diagnostic
document runtime study setup
ignore generated latex outputs
```

Avoid:

```text
updates
misc
fix
commit changes
```

### 9. Report

After committing, report:

* Commit hash for each commit.
* Files included in each commit.
* Intentionally unstaged files and why.
* Files skipped as noise or generated artefacts.
* Whether convention docs were changed or intentionally left unchanged.
* Confirmation that nothing was pushed.

## Hard Rules

* Never push unless explicitly asked.
* Do not amend a commit without explicit user permission.
* Never amend a pushed commit.
* Never use `--no-verify`.
* Never use `--no-gpg-sign`.
* Never use `git add -A`.
* Never use `git add .`.
* Never stage `.DS_Store`, `__pycache__/`, editor swap files, local settings, temporary files, LaTeX intermediates, or obvious cache/build artefacts.
* Never create new directory-index README files only to list files.
* Do not give derived/output files their own bullet in convention docs.
* If the tree has nothing meaningful to commit, stop without committing and report what was skipped.
* If there are multiple unrelated meaningful changes, make multiple coherent commits instead of one mixed commit.
* If the user limits the scope to a folder, do not commit unrelated changes outside that scope.
* If existing staged changes are unrelated to the requested scope, do not mix them into the new commit.
* Do not push.

