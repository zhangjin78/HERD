#!/usr/bin/env python3
"""Summarize raw CALO energy response from a stage-01 derived tar archive.

The archive is only a transport/storage package.  This script opens its
``event_features.csv.gz`` member directly; it does not merge ROOT files and it
never changes raw simulation data.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import math
import tarfile
from pathlib import Path


def quantile(values: list[float], probability: float) -> float:
    """Linear-interpolated sample quantile, after sorting values in place."""
    if not values:
        return float("nan")
    values.sort()
    index = (len(values) - 1) * probability
    low = int(math.floor(index))
    high = int(math.ceil(index))
    if low == high:
        return values[low]
    return values[low] + (index - low) * (values[high] - values[low])


def log_edges(low: float, high: float, count: int) -> list[float]:
    step = math.log(high / low) / count
    return [low * math.exp(step * index) for index in range(count + 1)]


def bin_index(value: float, edges: list[float]) -> int | None:
    if value < edges[0] or value > edges[-1]:
        return None
    if value == edges[-1]:
        return len(edges) - 2
    low, high = 0, len(edges) - 1
    while high - low > 1:
        middle = (low + high) // 2
        if value < edges[middle]:
            high = middle
        else:
            low = middle
    return low


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--energy-min", type=float, required=True)
    parser.add_argument("--energy-max", type=float, required=True)
    parser.add_argument("--bins", type=int, default=12)
    args = parser.parse_args()

    edges = log_edges(args.energy_min, args.energy_max, args.bins)
    responses = [[] for _ in range(args.bins)]
    generated = [0] * args.bins
    active = [0] * args.bins

    with tarfile.open(args.archive, "r") as archive:
        member = archive.getmember("event_features.csv.gz")
        packed = archive.extractfile(member)
        if packed is None:
            raise RuntimeError("event_features.csv.gz is missing from archive")
        with gzip.GzipFile(fileobj=packed) as uncompressed:
            rows = csv.DictReader(io.TextIOWrapper(uncompressed))
            for row in rows:
                energy = float(row["true_energy_GeV"])
                edep = float(row["calo_edep_GeV"])
                index = bin_index(energy, edges)
                if index is None:
                    continue
                generated[index] += 1
                if edep <= 0.0:
                    continue
                active[index] += 1
                responses[index].append(edep / energy)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        fields = ["energy_low_GeV", "energy_high_GeV", "energy_center_GeV",
                  "generated", "calo_active", "active_fraction",
                  "response_q16", "response_median", "response_q84",
                  "relative_68_width"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for index, values in enumerate(responses):
            q16 = quantile(values, 0.16)
            q50 = quantile(values, 0.50)
            q84 = quantile(values, 0.84)
            writer.writerow({
                "energy_low_GeV": edges[index],
                "energy_high_GeV": edges[index + 1],
                "energy_center_GeV": math.sqrt(edges[index] * edges[index + 1]),
                "generated": generated[index],
                "calo_active": active[index],
                "active_fraction": active[index] / generated[index] if generated[index] else float("nan"),
                "response_q16": q16,
                "response_median": q50,
                "response_q84": q84,
                "relative_68_width": (q84 - q16) / (2.0 * q50) if q50 > 0 else float("nan"),
            })


if __name__ == "__main__":
    main()
