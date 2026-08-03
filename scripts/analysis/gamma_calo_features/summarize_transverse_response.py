#!/usr/bin/env python3
"""Quantify energy-controlled response dependence on transverse CALO features."""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import math
import tarfile
from pathlib import Path


def quantile(values: list[float], p: float) -> float:
    if not values:
        return float("nan")
    values.sort()
    position = p * (len(values) - 1)
    lower, upper = int(math.floor(position)), int(math.ceil(position))
    return values[lower] if lower == upper else values[lower] + (position - lower) * (values[upper] - values[lower])


def log_edges(low: float, high: float, count: int) -> list[float]:
    return [low * math.exp(math.log(high / low) * number / count) for number in range(count + 1)]


def energy_bin(value: float, edges: list[float]) -> int:
    if value == edges[-1]:
        return len(edges) - 2
    for index in range(len(edges) - 1):
        if edges[index] <= value < edges[index + 1]:
            return index
    return -1


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--energy-min", type=float, default=0.05)
    parser.add_argument("--energy-max", type=float, default=20.0)
    parser.add_argument("--energy-bins", type=int, default=12)
    parser.add_argument("--quantile-bins", type=int, default=5)
    args = parser.parse_args()

    edges = log_edges(args.energy_min, args.energy_max, args.energy_bins)
    responses = [[] for _ in range(args.energy_bins)]
    records: list[tuple[int, float, float, float, float]] = []
    with tarfile.open(args.archive, "r") as archive:
        member = archive.extractfile("event_features.csv.gz")
        if member is None:
            raise RuntimeError("event_features.csv.gz missing")
        with gzip.GzipFile(fileobj=member) as unpacked:
            for row in csv.DictReader(io.TextIOWrapper(unpacked)):
                energy = float(row["true_energy_GeV"])
                edep = float(row["calo_edep_GeV"])
                if edep <= 0.0:
                    continue
                index = energy_bin(energy, edges)
                if index < 0:
                    continue
                response = edep / energy
                responses[index].append(response)
                records.append((index, response, float(row["n_cells_gt_1MeV"]),
                                float(row["transverse_rms_cells"]), float(row["max_cell_fraction"])))

    medians = [quantile(group, 0.5) for group in responses]
    features = {
        "n_cells_gt_1MeV": [record[2] for record in records],
        "transverse_rms_cells": [record[3] for record in records],
        "max_cell_fraction": [record[4] for record in records],
    }
    residuals = [record[1] / medians[record[0]] for record in records]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        names = ["observable", "bin", "low", "high", "x_median", "n", "q16", "median", "q84"]
        writer = csv.DictWriter(handle, fieldnames=names)
        writer.writeheader()
        for observable, values in features.items():
            qedges = [quantile(values, number / args.quantile_bins) for number in range(args.quantile_bins + 1)]
            groups = [[] for _ in range(args.quantile_bins)]
            xgroups = [[] for _ in range(args.quantile_bins)]
            for value, residual in zip(values, residuals):
                if value == qedges[-1]:
                    index = args.quantile_bins - 1
                else:
                    index = next((number for number in range(args.quantile_bins)
                                  if qedges[number] <= value < qedges[number + 1]), -1)
                if index >= 0:
                    groups[index].append(residual)
                    xgroups[index].append(value)
            for index, group in enumerate(groups):
                writer.writerow({"observable": observable, "bin": index + 1,
                                 "low": qedges[index], "high": qedges[index + 1],
                                 "x_median": quantile(xgroups[index], 0.5), "n": len(group),
                                 "q16": quantile(group, 0.16), "median": quantile(group, 0.5),
                                 "q84": quantile(group, 0.84)})


if __name__ == "__main__":
    main()
