#!/usr/bin/env python3
"""Summarize feature-conditioned raw response distributions for a mono-energy sample.

This script is designed for prod04 (1 GeV gamma).  With true energy fixed, the
raw response R=Edep/Etrue can be compared directly across shower-feature bins;
no Rnorm transformation is needed.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import json
import math
import tarfile
from pathlib import Path


def quantile(values: list[float], probability: float) -> float:
    if not values:
        return float("nan")
    ordered = sorted(values)
    index = probability * (len(ordered) - 1)
    lower, upper = int(math.floor(index)), int(math.ceil(index))
    return ordered[lower] if lower == upper else ordered[lower] + (index - lower) * (ordered[upper] - ordered[lower])


def group_by_quantiles(values: list[float], responses: list[float], groups: int) -> list[dict[str, object]]:
    edges = [quantile(values, index / groups) for index in range(groups + 1)]
    result: list[list[tuple[float, float]]] = [[] for _ in range(groups)]
    for value, response in zip(values, responses):
        if value == edges[-1]:
            index = groups - 1
        else:
            index = next((i for i in range(groups) if edges[i] <= value < edges[i + 1]), -1)
        if index >= 0:
            result[index].append((value, response))
    return [make_group(index + 1, edges[index], edges[index + 1], pair_values)
            for index, pair_values in enumerate(result)]


def make_group(index: int | str, low: float, high: float, pairs: list[tuple[float, float]]) -> dict[str, object]:
    responses = [pair[1] for pair in pairs]
    return {"bin": index, "low": low, "high": high, "n": len(responses),
            "response_q05": quantile(responses, 0.05), "response_q16": quantile(responses, 0.16),
            "response_q50": quantile(responses, 0.50), "response_q84": quantile(responses, 0.84),
            "response_q95": quantile(responses, 0.95), "hist": response_histogram(responses)}


def response_histogram(values: list[float], low: float = 0.0, high: float = 1.05, bins: int = 70) -> list[int]:
    counts = [0] * bins
    for value in values:
        if value < low or value > high:
            continue
        index = bins - 1 if value == high else int((value - low) / (high - low) * bins)
        counts[index] += 1
    return counts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    features = {"last_layer_fraction": [], "centroid_z_cm": [], "n_cells_gt_1MeV": [],
                "transverse_rms_cells": [], "max_cell_fraction": []}
    responses: list[float] = []
    active = 0
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
                responses.append(edep / energy)
                for name in features:
                    features[name].append(float(row[name]))

    grouped: dict[str, list[dict[str, object]]] = {}
    for name, values in features.items():
        if name == "last_layer_fraction":
            zero_pairs = [(value, response) for value, response in zip(values, responses) if value == 0.0]
            positive_values = [value for value in values if value > 0.0]
            positive_responses = [response for value, response in zip(values, responses) if value > 0.0]
            groups = [make_group("zero", 0.0, 0.0, zero_pairs)]
            groups.extend(group_by_quantiles(positive_values, positive_responses, 4))
            grouped[name] = groups
        else:
            grouped[name] = group_by_quantiles(values, responses, 5)

    payload = {"dataset": "prod04", "true_energy_GeV": 1.0, "calo_active": active,
               "hist_low": 0.0, "hist_high": 1.05, "hist_bins": 70, "features": grouped}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()
