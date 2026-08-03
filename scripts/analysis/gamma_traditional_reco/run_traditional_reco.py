#!/usr/bin/env python3
"""Run the immutable-config CALO traditional reconstruction stage."""
import argparse
import datetime as dt
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

JOB_RE = re.compile(r"job(\d{6})")


def digest(path):
    out = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            out.update(block)
    return out.hexdigest()


def jobs(config):
    found = {}
    for name in sorted(glob.glob(config["production"]["root_glob"])):
        match = JOB_RE.search(Path(name).name)
        if not match:
            raise RuntimeError(f"job number missing from {name}")
        found[int(match.group(1))] = Path(name)
    expected = set(config["production"]["expected_jobs"])
    if set(found) != expected:
        raise RuntimeError(f"expected jobs {sorted(expected)}, found {sorted(found)}")
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config", type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--macro", type=Path,
                        default=Path(__file__).with_name("traditional_calo_reco.C"))
    parser.add_argument("--results-base", type=Path,
                        default=Path("/herdfs/user/zhangjin0101/HERD/results/derived"))
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    if config.get("status") != "frozen":
        raise RuntimeError("formal traditional reconstruction requires a frozen dataset")
    found = jobs(config)
    expected_events = config["production"]["expected_events"]
    train = ",".join(map(str, config["split_jobs"]["train"]))
    valid = ",".join(map(str, config["split_jobs"]["validation"]))
    test = ",".join(map(str, config["split_jobs"]["test"]))
    scratch_parent = Path(os.environ.get("TMPDIR", f"/tmp/{os.environ.get('USER', 'user')}"))
    scratch_parent.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix=f"traditional_{config['dataset_id']}_", dir=scratch_parent))
    try:
        (scratch / "dataset_config.json").write_text(json.dumps(config, indent=2) + "\n")
        manifest = {"dataset_id": config["dataset_id"], "expected_events": expected_events,
                    "jobs": [{"job_id": job, "path": str(path), "bytes": path.stat().st_size}
                             for job, path in found.items()]}
        (scratch / "dataset_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        expr = (
            f'{args.macro.resolve()}("{config["production"]["root_glob"]}","{scratch}",'
            f'{len(found)},{expected_events},"{train}","{valid}","{test}")'
        )
        run = subprocess.run(["root", "-l", "-b", "-q", expr], text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (scratch / "analysis.log").write_text(run.stdout)
        if run.returncode or "TRADITIONAL_RECO_SUCCESS" not in run.stdout:
            raise RuntimeError("ROOT reconstruction failed; see analysis.log")
        expected = {"traditional_reco.csv", "traditional_reco.root", "numeric_summary.txt"}
        missing = [item for item in expected if not (scratch / item).is_file()]
        if missing:
            raise RuntimeError(f"missing products: {missing}")
        products = {str(path.relative_to(scratch)): {"bytes": path.stat().st_size,
                    "sha256": digest(path)} for path in scratch.rglob("*") if path.is_file()}
        (scratch / "products_manifest.json").write_text(json.dumps(products, indent=2) + "\n")
        destination = args.results_base / config["dataset_id"] / "stage02" / f"{args.tag}.tar"
        if destination.exists():
            raise RuntimeError(f"refusing to overwrite {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        local_tar = scratch.parent / f"{scratch.name}.tar"
        with tarfile.open(local_tar, "w") as archive:
            for path in sorted(scratch.rglob("*")):
                archive.add(path, arcname=path.relative_to(scratch))
        checksum = digest(local_tar)
        partial = destination.with_suffix(".tar.partial")
        shutil.copyfile(local_tar, partial)
        if digest(partial) != checksum:
            raise RuntimeError("package checksum mismatch")
        partial.rename(destination)
        print(f"PUBLISHED={destination}")
        print(f"PACKAGE_SHA256={checksum}")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    main()
