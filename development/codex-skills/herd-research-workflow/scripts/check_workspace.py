#!/usr/bin/env python3
"""Read-only structural checks for a HERD research workspace."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


EXPECTED_DIRS = (
    "code",
    "configs",
    "development",
    "notes",
    "scripts",
    "figures",
    "results",
    "runs",
)

GENERATED_SUFFIXES = {
    ".root",
    ".csv",
    ".h5",
    ".hdf5",
    ".parquet",
    ".npz",
    ".npy",
    ".pt",
    ".pth",
    ".onnx",
    ".log",
    ".out",
    ".err",
}


def inspect(root: Path) -> dict:
    missing = [name for name in EXPECTED_DIRS if not (root / name).is_dir()]
    zero_figures = []
    partial_files = []
    misplaced_root = []
    generated_in_tracked_areas = []

    for path in root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(root)
        rel_text = rel.as_posix()

        if ".git/" in f"{rel_text}/":
            continue
        if path.name.endswith(".partial") or ".partial." in path.name:
            partial_files.append(rel_text)
        if rel.parts and rel.parts[0] == "figures" and path.suffix.lower() == ".png":
            if path.stat().st_size == 0:
                zero_figures.append(rel_text)
        if path.suffix.lower() == ".root":
            allowed = (
                rel_text.startswith("results/production/")
                or rel_text.startswith("results/tests/")
                or rel_text.startswith("results/_archive/")
            )
            if not allowed:
                misplaced_root.append(rel_text)
        if rel.parts and rel.parts[0] in {"configs", "development", "notes", "scripts"}:
            if path.suffix.lower() in GENERATED_SUFFIXES:
                generated_in_tracked_areas.append(rel_text)

    nested_offline_git = (root / "code" / "offline" / ".git").exists()
    return {
        "workspace": str(root),
        "missing_directories": missing,
        "nested_offline_git_detected": nested_offline_git,
        "zero_byte_pngs": zero_figures,
        "partial_files": partial_files,
        "misplaced_root_files": misplaced_root,
        "generated_files_in_tracked_areas": generated_in_tracked_areas,
        "ok": not any(
            (missing, zero_figures, partial_files, misplaced_root, generated_in_tracked_areas)
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workspace", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    root = args.workspace.expanduser().resolve()
    if not root.is_dir():
        parser.error(f"workspace is not a directory: {root}")

    report = inspect(root)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"workspace: {report['workspace']}")
        print(f"status: {'OK' if report['ok'] else 'NEEDS ATTENTION'}")
        for key in (
            "missing_directories",
            "zero_byte_pngs",
            "partial_files",
            "misplaced_root_files",
            "generated_files_in_tracked_areas",
        ):
            values = report[key]
            print(f"{key}: {len(values)}")
            for value in values:
                print(f"  - {value}")
        print(
            "nested_offline_git_detected: "
            f"{report['nested_offline_git_detected']}"
        )
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
