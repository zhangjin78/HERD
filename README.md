# HERD offline workspace

Personal workspace for developing and testing the HERDOS `offline` framework.

## Layout

- `code/`: HERDOS `offline` source repository, managed as a Git submodule.
- `notes/`: research notes and experiment records tracked by this workspace.
- `data/`: input and processed data; not tracked by Git.
- `runs/`: per-run configurations, logs, and intermediate outputs; not tracked by Git.
- `results/`: generated analysis results; not tracked by Git.
- `figures/`: generated figures; not tracked by Git.
- `enc/`: local software environment; not tracked by Git.

Source-code changes must be committed inside `code/`. The outer repository records
which `offline` commit is used together with workspace notes and supporting scripts.

## Clone with the source submodule

```bash
git clone <workspace-repository-url> HERD
cd HERD
git submodule update --init --recursive
```

## Reproducibility

For each important test, record the date, purpose, command, configuration, input
dataset, output location, and the `code/` commit hash in `notes/`.
