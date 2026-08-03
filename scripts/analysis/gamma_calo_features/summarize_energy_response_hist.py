#!/usr/bin/env python3
"""Make a compact 2D Edep-versus-Etrue histogram from derived event features."""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import math
import tarfile
from pathlib import Path


def log_edges(low: float, high: float, count: int) -> list[float]:
    return [low * math.exp(math.log(high / low) * index / count) for index in range(count + 1)]


def find_bin(value: float, edges: list[float]) -> int:
    if value < edges[0] or value > edges[-1]:
        return -1
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
    parser.add_argument("--energy-min", type=float, default=0.05)
    parser.add_argument("--energy-max", type=float, default=20.0)
    parser.add_argument("--edep-min", type=float, default=1e-5)
    parser.add_argument("--edep-max", type=float, default=25.0)
    parser.add_argument("--bins", type=int, default=54)
    args = parser.parse_args()

    x_edges = log_edges(args.energy_min, args.energy_max, args.bins)
    y_edges = log_edges(args.edep_min, args.edep_max, args.bins)
    counts = [[0] * args.bins for _ in range(args.bins)]
    active, underflow, overflow = 0, 0, 0
    with tarfile.open(args.archive, "r") as archive:
        stream = archive.extractfile("event_features.csv.gz")
        if stream is None:
            raise RuntimeError("event_features.csv.gz missing")
        with gzip.GzipFile(fileobj=stream) as unpacked:
            for row in csv.DictReader(io.TextIOWrapper(unpacked)):
                energy = float(row["true_energy_GeV"])
                edep = float(row["calo_edep_GeV"])
                if edep <= 0.0:
                    continue
                active += 1
                ix, iy = find_bin(energy, x_edges), find_bin(edep, y_edges)
                if ix < 0 or iy < 0:
                    if edep < args.edep_min:
                        underflow += 1
                    else:
                        overflow += 1
                    continue
                counts[ix][iy] += 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["true_low_GeV", "true_high_GeV", "edep_low_GeV", "edep_high_GeV", "count"])
        for ix in range(args.bins):
            for iy in range(args.bins):
                if counts[ix][iy]:
                    writer.writerow([x_edges[ix], x_edges[ix + 1], y_edges[iy], y_edges[iy + 1], counts[ix][iy]])
    print(f"calo_active={active}")
    print(f"hist_underflow={underflow}")
    print(f"hist_overflow={overflow}")


if __name__ == "__main__":
    main()
