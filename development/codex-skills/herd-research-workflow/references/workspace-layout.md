# Canonical HERD workspace layout

```text
HERD/
├── code/
│   └── offline/                         HERDOS group repository/submodule
├── configs/
│   └── datasets/                        tracked dataset manifests and splits
├── development/
│   └── codex-skills/                    tracked project-specific Codex skills
├── notes/
│   └── analysis/                        tracked plans, dictionaries, tutorials
├── scripts/
│   ├── analysis/                        tracked extraction/reconstruction/ML
│   ├── condor/                          tracked batch submission and workers
│   └── environment/                     tracked environment specifications
├── figures/                             generated, browsable, ignored by Git
│   └── <dataset>/<stage>/<tag>/
├── results/                             generated, ignored by Git
│   ├── production/<dataset>/            immutable raw subjob ROOT files
│   ├── derived/<dataset>/               derived features and result snapshots
│   ├── tests/<test-id>/                 explicitly non-production outputs
│   └── _archive/<archive-id>/           superseded/failed/incomplete material
└── runs/                                Condor records and logs, ignored by Git
```

## Naming rules

- Dataset ID describes particle, energy/generator, geometry, and production tag.
- Analysis output uses `<dataset>/<stage>/<tag>`.
- Partial analysis tags include `partial_<completed>of<expected>`.
- Frozen releases use an immutable version or date tag.
- Failed results never remain beside current results; archive them with a reason.

## Git ignore baseline

```gitignore
/data/
/runs/
/results/
/figures/
*.root
*.csv
*.h5
*.hdf5
*.parquet
*.npz
*.npy
*.pt
*.pth
*.onnx
*.log
*.out
*.err
```

Keep small hand-written CSV files only through an explicit negation rule and a
documented reason. Do not weaken these ignores simply to make results visible on
GitHub; publish a result index and reproduction command instead.
