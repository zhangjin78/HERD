# ROOT multi-file and storage decisions

## TChain or RDataFrame: default analysis path

Use when several subjob ROOT files share the same tree and schema.

Advantages:

- reads files as one logical dataset without duplicating storage;
- preserves job boundaries and makes missing-file checks possible;
- supports partial-dataset debugging;
- avoids creating another large ROOT artifact.

Always construct the file list from the dataset configuration, not an unchecked
wildcard. Confirm that all expected job IDs appear exactly once.

## hadd: deliberate ROOT-level merge

Use only when a downstream program requires one ROOT file or when producing a
frozen, explicitly versioned merged artifact.

Before merging:

- verify compatible trees, branches, types, units, and truth schema;
- preserve all source ROOT files;
- ensure sufficient destination space.

After merging:

- open the result with ROOT;
- compare tree entry totals against the sum of inputs;
- check key trees and branches;
- record the input manifest and checksum.

Do not repeatedly `hadd` into an existing output. Create a new path and publish
it after validation.

## tar: packaging, not analysis

Use tar for an atomic snapshot containing many derived files such as JSON,
manifests, CSV/Parquet tables, and plots. It is useful when the filesystem is
unreliable or inefficient for publishing many small files.

Tar does not merge ROOT trees. ROOT tools cannot transparently analyze ROOT files
inside a tar archive. Raw production subjob ROOT files should remain unpacked
unless making a separate cold-storage copy.

Recommended derived-package sequence:

```text
build in scratch
→ validate contents
→ create <name>.tar.gz.partial
→ calculate SHA-256
→ rename to <name>.tar.gz
→ write/update a small manifest
```

If users need to browse plots directly, publish PNG files separately under the
canonical `figures/` tree.
