#!/usr/bin/env python3
"""Train/evaluate reproducible traditional gamma-energy baselines."""

import argparse
import csv
import gzip
import json
import math
from bisect import bisect_right
from pathlib import Path


FEATURES = (
    "last_layer_fraction",
    "longitudinal_rms_layers",
    "transverse_rms_cells",
    "max_cell_fraction",
    "boundary_distance_cells",
    "centroid_iz",
)


def open_text(path):
    return gzip.open(path, "rt", newline="") if path.suffix == ".gz" else path.open(newline="")


def quantile(values, probability):
    values = sorted(values)
    if not values:
        return math.nan
    position = (len(values) - 1) * probability
    lo, hi = math.floor(position), math.ceil(position)
    if lo == hi:
        return values[lo]
    return values[lo] + (values[hi] - values[lo]) * (position - lo)


def median(values):
    return quantile(values, 0.5)


def load_rows(path):
    rows = []
    with open_text(path) as source:
        for row in csv.DictReader(source):
            parsed = {
                "event": int(row["event"]),
                "job_id": int(row["job_id"]),
                "split": int(row["split"]),
                "true": float(row["true_energy_GeV"]),
                "edep": float(row["calo_edep_GeV"]),
                "converted": int(row["converted"]),
            }
            for feature in FEATURES:
                value = float(row[feature])
                parsed[feature] = value if math.isfinite(value) else 0.0
            rows.append(parsed)
    return rows


def fit_monotonic_knots(rows, bins=30):
    selected = [row for row in rows if row["converted"] and row["edep"] > 0]
    # Forward calibration: bin in known training truth, then map the median
    # detector response back to truth. Inverse binning in Edep is biased by
    # high-energy leakage events that migrate into low-Edep bins.
    log_min = min(math.log(row["true"]) for row in selected)
    log_max = max(math.log(row["true"]) for row in selected)
    groups = [[] for _ in range(bins)]
    for row in selected:
        index = min(bins - 1, int((math.log(row["true"]) - log_min) /
                                  max(log_max - log_min, 1e-12) * bins))
        groups[index].append(row)
    knots = []
    for group in groups:
        if len(group) < 20:
            continue
        knots.append((
            math.log(median([row["edep"] for row in group])),
            math.log(median([row["true"] for row in group])),
        ))
    if len(knots) < 2:
        raise RuntimeError("not enough populated calibration bins")
    # Enforce a non-decreasing reconstructed energy without external libraries.
    monotonic = []
    running = -math.inf
    for x_value, y_value in sorted(knots):
        running = max(running, y_value)
        monotonic.append((x_value, running))
    return monotonic


def interpolate(knots, edep):
    x_value = math.log(max(edep, 1e-12))
    xs = [item[0] for item in knots]
    index = bisect_right(xs, x_value)
    if index == 0:
        left, right = knots[0], knots[1]
    elif index == len(knots):
        left, right = knots[-2], knots[-1]
    else:
        left, right = knots[index - 1], knots[index]
    fraction = (x_value - left[0]) / max(right[0] - left[0], 1e-12)
    return math.exp(left[1] + fraction * (right[1] - left[1]))


def solve(matrix, vector):
    size = len(vector)
    augmented = [list(matrix[row]) + [vector[row]] for row in range(size)]
    for column in range(size):
        pivot = max(range(column, size), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1e-14:
            raise RuntimeError("singular regression matrix")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        scale = augmented[column][column]
        augmented[column] = [value / scale for value in augmented[column]]
        for row in range(size):
            if row == column:
                continue
            factor = augmented[row][column]
            augmented[row] = [
                value - factor * reference
                for value, reference in zip(augmented[row], augmented[column])
            ]
    return [augmented[row][-1] for row in range(size)]


def standardized_features(rows):
    means = {name: sum(row[name] for row in rows) / len(rows) for name in FEATURES}
    scales = {}
    for name in FEATURES:
        variance = sum((row[name] - means[name]) ** 2 for row in rows) / len(rows)
        scales[name] = max(math.sqrt(variance), 1e-12)
    return means, scales


def design(row, means, scales):
    return [1.0] + [(row[name] - means[name]) / scales[name] for name in FEATURES]


def fit_leakage(rows, knots, ridge):
    selected = [row for row in rows if row["converted"] and row["edep"] > 0]
    means, scales = standardized_features(selected)
    size = len(FEATURES) + 1
    matrix = [[0.0] * size for _ in range(size)]
    vector = [0.0] * size
    for row in selected:
        x_values = design(row, means, scales)
        target = math.log(row["true"] / interpolate(knots, row["edep"]))
        for i in range(size):
            vector[i] += x_values[i] * target
            for j in range(size):
                matrix[i][j] += x_values[i] * x_values[j]
    for index in range(1, size):
        matrix[index][index] += ridge
    return {"coefficients": solve(matrix, vector), "means": means, "scales": scales}


def predict(row, knots, leakage):
    raw = row["edep"]
    calibrated = interpolate(knots, raw) if raw > 0 else 0.0
    if not leakage or raw <= 0:
        return raw, calibrated, calibrated
    correction = sum(
        coefficient * value for coefficient, value in zip(
            leakage["coefficients"],
            design(row, leakage["means"], leakage["scales"]),
        )
    )
    return raw, calibrated, calibrated * math.exp(correction)


def score(rows, knots, leakage):
    ratios = [
        predict(row, knots, leakage)[2] / row["true"]
        for row in rows if row["converted"] and row["edep"] > 0
    ]
    return median([abs(ratio - 1.0) for ratio in ratios])


def metrics(rows, predictor_index, energy_min, energy_max, bins=20):
    edges = [
        math.exp(math.log(energy_min) +
                 index / bins * math.log(energy_max / energy_min))
        for index in range(bins + 1)
    ]
    groups = [[] for _ in range(bins)]
    total = [0] * bins
    active = [0] * bins
    for row in rows:
        index = min(bins - 1, max(
            0, bisect_right(edges, row["true"]) - 1
        ))
        total[index] += 1
        if row["converted"] and row["edep"] > 0:
            active[index] += 1
            groups[index].append(row["predictions"][predictor_index] / row["true"])
    output = []
    for index, ratios in enumerate(groups):
        output.append({
            "energy_low_GeV": edges[index],
            "energy_high_GeV": edges[index + 1],
            "events_all": total[index],
            "events_selected": active[index],
            "selection_efficiency": active[index] / total[index] if total[index] else 0,
            "median_bias": median(ratios) - 1 if ratios else None,
            "resolution_68": (
                0.5 * (quantile(ratios, 0.84) - quantile(ratios, 0.16))
                if ratios else None
            ),
        })
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("features", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--energy-min", type=float, required=True)
    parser.add_argument("--energy-max", type=float, required=True)
    args = parser.parse_args()
    args.output_directory.mkdir(parents=True, exist_ok=False)

    rows = load_rows(args.features)
    train = [row for row in rows if row["split"] == 0]
    validation = [row for row in rows if row["split"] == 1]
    test = [row for row in rows if row["split"] == 2]
    if not train or not validation or not test:
        raise RuntimeError("train, validation, and test must all be non-empty")

    knots = fit_monotonic_knots(train)
    candidates = [(score(validation, knots, None), None, None)]
    for ridge in (1e-3, 1e-2, 1e-1, 1.0, 10.0, 100.0):
        model = fit_leakage(train, knots, ridge)
        candidates.append((score(validation, knots, model), ridge, model))
    _, selected_ridge, leakage = min(candidates, key=lambda item: item[0])

    for row in rows:
        row["predictions"] = predict(row, knots, leakage)

    report = {
        "selection": "converted && calo_edep_GeV > 0",
        "split_counts": {
            "train": len(train), "validation": len(validation), "test": len(test)
        },
        "selected_ridge": selected_ridge,
        "calibration_knots_log": knots,
        "leakage_model": leakage,
        "test_metrics": {
            name: metrics(test, index, args.energy_min, args.energy_max)
            for index, name in enumerate(("raw_sum", "monotonic", "leakage_corrected"))
        },
    }
    (args.output_directory / "traditional_metrics.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    with (args.output_directory / "test_predictions.csv").open("w", newline="") as target:
        writer = csv.writer(target)
        writer.writerow([
            "event", "job_id", "true_energy_GeV", "raw_sum_GeV",
            "monotonic_GeV", "leakage_corrected_GeV"
        ])
        for row in test:
            writer.writerow([
                row["event"], row["job_id"], row["true"], *row["predictions"]
            ])
    print(f"TRADITIONAL_BASELINE_SUCCESS output={args.output_directory}")


if __name__ == "__main__":
    main()
