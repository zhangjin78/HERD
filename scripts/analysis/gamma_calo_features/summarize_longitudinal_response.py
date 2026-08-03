#!/usr/bin/env python3
"""Measure response residuals against CALO longitudinal observables.

The residual is normalized within narrow true-energy bins.  This prevents a
response-versus-shape trend from being an artifact of the broad E^-1 energy
spectrum, and makes the result appropriate as stage-01 evidence for a later
leakage correction or machine-learning feature.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import math
import tarfile
from pathlib import Path


def q(values: list[float], probability: float) -> float:
    if not values:
        return float("nan")
    values.sort()
    index = probability * (len(values) - 1)
    lower, upper = int(math.floor(index)), int(math.ceil(index))
    return values[lower] if lower == upper else values[lower] + (index - lower) * (values[upper] - values[lower])


def log_edges(low: float, high: float, count: int) -> list[float]:
    return [low * math.exp(math.log(high / low) * index / count) for index in range(count + 1)]


def index_of(value: float, edges: list[float]) -> int:
    if value == edges[-1]:
        return len(edges) - 2
    for index in range(len(edges) - 1):
        if edges[index] <= value < edges[index + 1]:
            return index
    return -1


def grouped(values: list[tuple[float, float]], edges: list[float]) -> list[tuple[float, float, int, float, float, float]]:
    bins = [[] for _ in range(len(edges) - 1)]
    for variable, residual in values:
        index = index_of(variable, edges)
        if index >= 0:
            bins[index].append(residual)
    return [(edges[index], edges[index + 1], len(group), q(group, 0.16), q(group, 0.50), q(group, 0.84))
            for index, group in enumerate(bins)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--energy-min", type=float, default=0.05)
    parser.add_argument("--energy-max", type=float, default=20.0)
    parser.add_argument("--energy-bins", type=int, default=12)
    args = parser.parse_args()

    energy_edges = log_edges(args.energy_min, args.energy_max, args.energy_bins)
    raw_response: list[list[float]] = [[] for _ in range(args.energy_bins)]
    records: list[tuple[int, float, float, float]] = []

    with tarfile.open(args.archive, "r") as archive:
        packed = archive.extractfile("event_features.csv.gz")
        if packed is None:
            raise RuntimeError("event_features.csv.gz missing")
        with gzip.GzipFile(fileobj=packed) as uncompressed:
            for row in csv.DictReader(io.TextIOWrapper(uncompressed)):
                energy = float(row["true_energy_GeV"])
                edep = float(row["calo_edep_GeV"])
                if edep <= 0.0:
                    continue
                energy_index = index_of(energy, energy_edges)
                if energy_index < 0:
                    continue
                response = edep / energy
                last_fraction = float(row["last_layer_fraction"])
                centroid_z = float(row["centroid_z_cm"])
                raw_response[energy_index].append(response)
                records.append((energy_index, response, last_fraction, centroid_z))

    response_medians = [q(values, 0.5) for values in raw_response]
    positive_last = [last_fraction for _, _, last_fraction, _ in records if last_fraction > 0.0]
    positive_edges = [q(positive_last, level) for level in (0.0, 0.25, 0.50, 0.75, 1.0)]
    depth_values = [depth for _, _, _, depth in records]
    depth_edges = [q(depth_values, level) for level in (0.0, 0.20, 0.40, 0.60, 0.80, 1.0)]

    last_zero: list[float] = []
    last_positive: list[tuple[float, float]] = []
    depth_pairs: list[tuple[float, float]] = []
    for energy_index, response, last_fraction, centroid_z in records:
        residual = response / response_medians[energy_index]
        depth_pairs.append((centroid_z, residual))
        if last_fraction == 0.0:
            last_zero.append(residual)
        else:
            last_positive.append((last_fraction, residual))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        columns = ["observable", "bin", "low", "high", "n", "q16", "median", "q84"]
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerow({"observable": "last_layer_fraction", "bin": "0", "low": 0.0, "high": 0.0,
                         "n": len(last_zero), "q16": q(last_zero, 0.16), "median": q(last_zero, 0.5), "q84": q(last_zero, 0.84)})
        for index, (low, high, count, q16, q50, q84) in enumerate(grouped(last_positive, positive_edges), 1):
            writer.writerow({"observable": "last_layer_fraction", "bin": str(index), "low": low, "high": high,
                             "n": count, "q16": q16, "median": q50, "q84": q84})
        for index, (low, high, count, q16, q50, q84) in enumerate(grouped(depth_pairs, depth_edges), 1):
            writer.writerow({"observable": "centroid_z_cm", "bin": str(index), "low": low, "high": high,
                             "n": count, "q16": q16, "median": q50, "q84": q84})


if __name__ == "__main__":
    main()
