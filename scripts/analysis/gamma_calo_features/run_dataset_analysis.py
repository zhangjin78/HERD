#!/usr/bin/env python3
"""Validate a HERDOS gamma dataset and run the configuration-driven ROOT analysis."""

import argparse
import datetime as dt
import glob
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path


JOB_PATTERN = re.compile(r"job(\d{6})")


def load_config(path):
    with path.open() as source:
        config = json.load(source)
    required = ("dataset_id", "status", "generator", "production", "split_jobs")
    missing = [key for key in required if key not in config]
    if missing:
        raise ValueError(f"missing config keys: {', '.join(missing)}")
    return config


def discover(config):
    files = {}
    for filename in sorted(glob.glob(config["production"]["root_glob"])):
        match = JOB_PATTERN.search(Path(filename).name)
        if not match:
            raise ValueError(f"cannot extract job ID from {filename}")
        job = int(match.group(1))
        if job in files:
            raise ValueError(f"duplicate job {job}")
        files[job] = Path(filename)
    return files


def split_for_job(config, job):
    matches = [
        name for name, jobs in config["split_jobs"].items() if job in set(jobs)
    ]
    if len(matches) != 1:
        raise ValueError(f"job {job} belongs to {len(matches)} splits")
    return matches[0]


def inspect_root_files(files):
    try:
        import ROOT
    except ImportError as error:
        raise RuntimeError(
            "PyROOT is unavailable; source HERDOS ExternalLibs and setup.sh first"
        ) from error
    ROOT.gROOT.SetBatch(True)
    rows = []
    for job, path in sorted(files.items()):
        root_file = ROOT.TFile.Open(str(path), "READ")
        if not root_file or root_file.IsZombie():
            raise RuntimeError(f"cannot open ROOT file: {path}")
        tree = root_file.Get("events")
        if not tree:
            raise RuntimeError(f"events tree missing: {path}")
        rows.append(
            {
                "job_id": job,
                "path": str(path),
                "bytes": path.stat().st_size,
                "events": int(tree.GetEntries()),
            }
        )
        root_file.Close()
    return rows


def make_manifest(config, rows):
    expected = set(config["production"]["expected_jobs"])
    present = {row["job_id"] for row in rows}
    for row in rows:
        row["split"] = split_for_job(config, row["job_id"])
    return {
        "dataset_id": config["dataset_id"],
        "configured_status": config["status"],
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "expected_jobs": sorted(expected),
        "present_jobs": sorted(present),
        "missing_jobs": sorted(expected - present),
        "unexpected_jobs": sorted(present - expected),
        "expected_events": config["production"]["expected_events"],
        "observed_events": sum(row["events"] for row in rows),
        "complete": present == expected,
        "files": rows,
    }


def validate_manifest(config, manifest, allow_partial):
    if manifest["unexpected_jobs"]:
        raise RuntimeError(f"unexpected jobs: {manifest['unexpected_jobs']}")
    expected_per_job = config["production"]["events_per_job"]
    bad = [
        row["job_id"] for row in manifest["files"]
        if row["events"] != expected_per_job
    ]
    if bad:
        raise RuntimeError(f"jobs with incorrect event count: {bad}")
    if not manifest["complete"] and not allow_partial:
        raise RuntimeError(
            f"dataset incomplete; missing jobs {manifest['missing_jobs']}; "
            "use --allow-partial only for pipeline debugging"
        )
    if manifest["complete"]:
        if manifest["observed_events"] != manifest["expected_events"]:
            raise RuntimeError("complete dataset has incorrect total event count")
    elif config["status"] == "frozen":
        raise RuntimeError("a frozen dataset cannot be incomplete")


def root_quote(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"')


def comma_jobs(config, split):
    return ",".join(str(job) for job in config["split_jobs"][split])


def run_root_analysis(config, manifest, macro, scratch):
    generator = config["generator"]
    production = config["production"]
    calo = config["calo_index"]
    expression = (
        f'{macro}("{root_quote(production["root_glob"])}",'
        f'"{root_quote(str(scratch))}",'
        f'{len(manifest["files"])},{manifest["observed_events"]},'
        f'{generator["energy_min_GeV"]},{generator["energy_max_GeV"]},'
        f'"{comma_jobs(config, "train")}",'
        f'"{comma_jobs(config, "validation")}",'
        f'"{comma_jobs(config, "test")}",'
        f'{calo["min"]},{calo["max"]})'
    )
    completed = subprocess.run(
        ["root", "-l", "-b", "-q", expression],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    (scratch / "analysis.log").write_text(completed.stdout)
    if completed.returncode != 0 or "ANALYSIS_SUCCESS" not in completed.stdout:
        raise RuntimeError(
            f"ROOT analysis failed; inspect {scratch / 'analysis.log'}"
        )


def compress_csv(path):
    target = path.with_suffix(path.suffix + ".gz")
    with path.open("rb") as source, gzip.open(target, "wb", compresslevel=6) as out:
        shutil.copyfileobj(source, out)
    path.unlink()
    return target


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def publish(scratch, destination, mode):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if mode == "directory":
        if destination.exists():
            raise RuntimeError(f"refusing to overwrite existing result: {destination}")
        shutil.copytree(scratch, destination)
        return destination, None
    final = destination.with_suffix(".tar")
    partial = final.with_suffix(final.suffix + ".partial")
    if final.exists() or partial.exists():
        raise RuntimeError(f"refusing to overwrite existing package: {final}")
    try:
        with tarfile.open(partial, "w") as archive:
            for path in sorted(scratch.rglob("*")):
                archive.add(path, arcname=path.relative_to(scratch))
        package_hash = sha256(partial)
        partial.rename(final)
        return final, package_hash
    except Exception:
        # Keep a failed package explicitly marked .partial for diagnosis.
        raise


def publish_figures(scratch, figures_base, dataset_id, tag):
    destination = figures_base / dataset_id / "stage01" / tag
    destination.mkdir(parents=True, exist_ok=True)
    published = []
    for source in sorted(scratch.glob("*.png")):
        target = destination / source.name
        source_hash = sha256(source)
        if target.exists():
            if target.stat().st_size > 0 and sha256(target) == source_hash:
                published.append(target)
                continue
            raise RuntimeError(f"refusing to replace non-matching figure: {target}")
        partial = target.with_suffix(target.suffix + ".partial")
        if partial.exists():
            partial.unlink()
        shutil.copyfile(source, partial)
        if sha256(partial) != source_hash:
            raise RuntimeError(f"figure checksum mismatch: {source.name}")
        partial.rename(target)
        published.append(target)
    return published


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config", type=Path)
    parser.add_argument(
        "--macro",
        type=Path,
        default=Path(__file__).with_name("analyze_gamma_calo_ml.C"),
    )
    parser.add_argument(
        "--summary-script",
        type=Path,
        default=Path(__file__).with_name("summarize_gamma_stage1.py"),
    )
    parser.add_argument("--allow-partial", action="store_true")
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--checksums", action="store_true")
    parser.add_argument("--tag")
    parser.add_argument(
        "--publish-mode",
        choices=("tar", "directory"),
        default="tar",
        help="tar is the safe default for HERDFS; directory is for local /tmp tests",
    )
    parser.add_argument(
        "--results-base",
        type=Path,
        default=Path("/herdfs/user/zhangjin0101/HERD/results/derived"),
    )
    parser.add_argument(
        "--figures-base",
        type=Path,
        default=Path("/herdfs/user/zhangjin0101/HERD/figures"),
    )
    parser.add_argument("--no-publish-figures", action="store_true")
    args = parser.parse_args()

    config = load_config(args.config)
    files = discover(config)
    rows = inspect_root_files(files)
    manifest = make_manifest(config, rows)
    validate_manifest(config, manifest, args.allow_partial)
    if args.checksums:
        for row in manifest["files"]:
            row["sha256"] = sha256(Path(row["path"]))

    print(json.dumps(manifest, indent=2, ensure_ascii=False))
    if args.validate_only:
        return

    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    default_tag = "complete" if manifest["complete"] else (
        f"partial_{len(files)}of{len(manifest['expected_jobs'])}"
    )
    tag = args.tag or f"{default_tag}_{timestamp}"
    destination = (
        args.results_base / config["dataset_id"] / "stage01" / tag
    )
    scratch_root = Path(
        os.environ.get("TMPDIR", f"/tmp/{os.environ.get('USER', 'user')}")
    )
    scratch_root.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(
        prefix=f"gamma_stage01_{config['dataset_id']}_", dir=scratch_root
    ))
    try:
        (scratch / "dataset_config.json").write_text(
            json.dumps(config, indent=2, ensure_ascii=False) + "\n"
        )
        (scratch / "dataset_manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
        )
        run_root_analysis(config, manifest, args.macro.resolve(), scratch)
        features = scratch / "event_features.csv"
        if not features.exists():
            raise RuntimeError("event_features.csv was not produced")
        with features.open() as source:
            lines = sum(1 for _ in source)
        if lines != manifest["observed_events"] + 1:
            raise RuntimeError(
                f"feature row count {lines - 1} != {manifest['observed_events']}"
            )
        quality_directory = scratch / "quality"
        quality = subprocess.run(
            [
                sys.executable,
                str(args.summary_script.resolve()),
                str(features),
                str(quality_directory),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        (scratch / "quality_check.log").write_text(quality.stdout)
        if quality.returncode != 0:
            raise RuntimeError("stage-one quality summary failed")
        for quality_csv in quality_directory.glob("*.csv"):
            compress_csv(quality_csv)
        compressed = compress_csv(features)
        products = {
            str(path.relative_to(scratch)): {
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
            for path in sorted(scratch.rglob("*")) if path.is_file()
        }
        (scratch / "products_manifest.json").write_text(
            json.dumps(products, indent=2, ensure_ascii=False) + "\n"
        )
        published, package_hash = publish(
            scratch, destination, args.publish_mode
        )
        print(f"PUBLISHED={published}")
        if package_hash:
            print(f"PACKAGE_SHA256={package_hash}")
        if not args.no_publish_figures:
            figures = publish_figures(
                scratch, args.figures_base, config["dataset_id"], tag
            )
            print(f"FIGURES_PUBLISHED={len(figures)}")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
