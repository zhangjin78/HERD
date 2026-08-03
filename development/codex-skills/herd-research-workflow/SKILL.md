---
name: herd-research-workflow
description: Organize and execute HERD/HERDOS research work reproducibly, including workspace layout, HTCondor production jobs, ROOT input handling, derived results, figures, Git boundaries, dataset validation, and machine-learning environments. Use for any task that creates, moves, analyzes, archives, or versions files in a HERD research workspace.
---

# HERD Research Workflow

Use this workflow for every HERD project task that touches jobs, data, analysis, figures, or Git.

## Start with a read-only inspection

1. Identify the workspace root and active Git repository.
2. Read relevant dataset configurations and notes.
3. Inspect `git status` without discarding unrelated user changes.
4. Locate raw ROOT inputs, job logs, derived outputs, and figures.
5. Run `scripts/check_workspace.py WORKSPACE` when available.
6. State any assumptions before moving or archiving material.

Never overwrite raw simulation ROOT files. Never modify the group-owned
`code/offline` repository as part of a personal workspace cleanup.

## Route every artifact deliberately

Follow the canonical layout in [workspace-layout.md](references/workspace-layout.md).

- Track source, configuration, and reproducibility documentation in Git.
- Keep production ROOT files under `results/production/<dataset>/`.
- Keep derived machine-readable outputs under `results/derived/<dataset>/`.
- Keep browsable plots under `figures/<dataset>/<stage>/<tag>/`.
- Keep scheduler records and logs under `runs/`.
- Put trial data under `results/tests/`.
- Move superseded, failed, or incomplete results to `results/_archive/`.

Do not retain an empty or competing output directory. For example, if plots are
published under `figures/`, do not also create `results/analysis/.../figures`.

## Treat datasets as immutable, configured units

Before analysis, require a dataset configuration containing:

- dataset ID and raw ROOT location;
- geometry, particle, generator, energy definition, and units;
- expected jobs, events per job, and total events;
- job-level train/validation/test assignment;
- software and truth-schema versions;
- random seed range;
- status: `partial`, `validated`, or `frozen`.

Use completed files for debugging while a dataset is `partial`, but label every
output `partial_NofM`. Publish formal results only after the complete dataset is
validated and frozen. Never split events from one production job across
train/validation/test.

## Choose the correct ROOT/storage operation

Read [root-and-storage.md](references/root-and-storage.md) before combining files.

- Prefer `TChain` or `RDataFrame` over a file list for analysis.
- Use `hadd` only when a deliberate merged ROOT artifact is needed.
- Use `tar` only to package a derived multi-file snapshot for reliable storage or
  transfer. A tar archive is not a ROOT merge and cannot be read by `TChain`.

Preserve subjob ROOT files. Validate event totals and schemas before and after
any `hadd`.

## Publish safely on HERDFS

HERDFS can stall or expose incomplete multi-file output. Build results in a
temporary or scratch location, verify them, then publish atomically:

1. write to a unique `.partial` path;
2. check expected files, nonzero sizes, event counts, and readable formats;
3. compute a SHA-256 checksum for packages or important artifacts;
4. rename the completed item into its final path;
5. retain a manifest recording inputs, configuration, code revision, and checksum.

Publish individual PNGs atomically when a browsable figure tree is required.

## Protect Git boundaries

The personal outer repository tracks:

- `configs/`;
- `scripts/`;
- `notes/`;
- reusable project tooling under `development/`.

It excludes raw and generated artifacts, including `data/`, `runs/`, `results/`,
`figures/`, ROOT files, tables, arrays, model weights, ONNX files, and logs.

Treat `code/offline` as the group repository or submodule. Do not stage its
contents into the personal repository. Work on a feature branch, stage exact
paths, inspect the staged diff, and never force-push `main`.

## Run production and analysis in stages

For simulation production:

1. dry-run the batch plan;
2. submit a small test job;
3. inspect scheduler status, stdout/stderr, output ROOT readability, and event count;
4. submit production jobs with unique tags and seeds;
5. validate and freeze the completed dataset.

For analysis:

1. Stage 01: data quality, feature extraction, distributions, and anomaly lists.
2. Stage 02: traditional reconstruction with train/validation/test isolation.
3. Stage 03: machine learning using the same frozen evaluation definitions.

Do not start formal downstream conclusions until the preceding stage passes its
acceptance checks.

## Build ML environments reproducibly

Read [pytorch-on-ihep.md](references/pytorch-on-ihep.md).

PyTorch is recommended for flexible 3D and multi-task model development, but it
is not mandatory. Preserve framework independence by exporting the selected
model to ONNX and validating ONNX Runtime predictions before HERDOS integration.

Never install packages into CVMFS or the HERDOS release environment. Keep the
environment outside Git, pin dependencies in Git, run only tiny smoke tests on a
login node, and submit actual training to an appropriate compute resource.

## Finish every task with evidence

Report:

- files created, moved, or archived;
- dataset status and event/file validation;
- output paths and checksums where applicable;
- exact command needed to reproduce the result;
- Git branch/commit if a commit was requested;
- remaining limitations, especially partial datasets or unverified GPU support.
