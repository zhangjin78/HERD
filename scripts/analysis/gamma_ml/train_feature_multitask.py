#!/usr/bin/env python3
"""Train the stage-three engineered-feature multi-task baseline.

This executable is gated on a validated/frozen dataset by default. It supports
energy, conversion and conversion-vertex heads for the current vertical data.
The direction head is activated only when direction label columns are present.
"""

import argparse
import csv
import gzip
import json
import math
import random
from pathlib import Path

from multitask_models import build_feature_multitask_model, require_torch


BASE_FEATURES = [
    "calo_edep_GeV",
    "n_cells",
    "n_cells_gt_1MeV",
    "n_cells_gt_20MeV",
    "centroid_ix",
    "centroid_iy",
    "centroid_iz",
    "transverse_rms_cells",
    "longitudinal_rms_layers",
    "max_cell_fraction",
    "boundary_distance_cells",
    "last_layer_fraction",
] + [f"layer_edep_{index}_GeV" for index in range(21)]


def open_text(path):
    return gzip.open(path, "rt", newline="") if path.suffix == ".gz" else path.open(newline="")


def finite(value, default=0.0):
    parsed = float(value)
    return parsed if math.isfinite(parsed) else default


def load_data(path):
    records = []
    with open_text(path) as source:
        reader = csv.DictReader(source)
        direction_available = all(
            name in reader.fieldnames
            for name in ("primary_dir_x", "primary_dir_y", "primary_dir_z")
        )
        for row in reader:
            record = {
                "event": int(row["event"]),
                "job_id": int(row["job_id"]),
                "split": int(row["split"]),
                "features": [finite(row[name]) for name in BASE_FEATURES],
                "true_energy": float(row["true_energy_GeV"]),
                "converted": float(row["converted"]),
                "calo_active": float(row["calo_edep_GeV"]) > 0,
                "vertex": [
                    finite(row["conversion_x_cm"]),
                    finite(row["conversion_y_cm"]),
                    finite(row["conversion_z_cm"]),
                ],
                "direction": (
                    [finite(row[name]) for name in
                     ("primary_dir_x", "primary_dir_y", "primary_dir_z")]
                    if direction_available else None
                ),
            }
            records.append(record)
    return records, direction_available


def standardization(records):
    width = len(BASE_FEATURES)
    means = [
        sum(record["features"][index] for record in records) / len(records)
        for index in range(width)
    ]
    scales = []
    for index in range(width):
        variance = sum(
            (record["features"][index] - means[index]) ** 2 for record in records
        ) / len(records)
        scales.append(max(math.sqrt(variance), 1e-8))
    return means, scales


def tensors(records, means, scales, device):
    torch, _ = require_torch()
    features = torch.tensor([
        [(value - mean) / scale for value, mean, scale in
         zip(record["features"], means, scales)]
        for record in records
    ], dtype=torch.float32, device=device)
    return {
        "features": features,
        "log_energy": torch.tensor(
            [math.log(record["true_energy"]) for record in records],
            dtype=torch.float32, device=device
        ),
        "converted": torch.tensor(
            [record["converted"] for record in records],
            dtype=torch.float32, device=device
        ),
        "energy_mask": torch.tensor(
            [record["converted"] and record["calo_active"] for record in records],
            dtype=torch.bool, device=device
        ),
        "vertex": torch.tensor(
            [record["vertex"] for record in records],
            dtype=torch.float32, device=device
        ),
        "vertex_mask": torch.tensor(
            [bool(record["converted"]) for record in records],
            dtype=torch.bool, device=device
        ),
        "direction": (
            torch.tensor(
                [record["direction"] for record in records],
                dtype=torch.float32, device=device
            ) if records and records[0]["direction"] is not None else None
        ),
    }


def batch_indices(size, batch_size, generator):
    indices = list(range(size))
    generator.shuffle(indices)
    for start in range(0, size, batch_size):
        yield indices[start:start + batch_size]


def masked_mean(values, mask):
    selected = values[mask]
    return selected.mean() if selected.numel() else values.sum() * 0


def loss_terms(outputs, data, indices):
    torch, _ = require_torch()
    energy_mask = data["energy_mask"][indices]
    vertex_mask = data["vertex_mask"][indices]
    energy = masked_mean(
        (outputs["log_energy"] - data["log_energy"][indices]) ** 2,
        energy_mask,
    )
    conversion = torch.nn.functional.binary_cross_entropy_with_logits(
        outputs["conversion_logit"], data["converted"][indices]
    )
    vertex = masked_mean(
        ((outputs["vertex_cm"] - data["vertex"][indices]) ** 2).mean(dim=1),
        vertex_mask,
    )
    direction = outputs["log_energy"].sum() * 0
    if data["direction"] is not None:
        cosine = (
            outputs["direction"] * data["direction"][indices]
        ).sum(dim=1).clamp(-1, 1)
        direction = (1 - cosine).mean()
    return {
        "energy": energy,
        "conversion": conversion,
        "vertex": vertex,
        "direction": direction,
    }


def evaluate(model, data, batch_size):
    torch, _ = require_torch()
    totals = {"energy": 0.0, "conversion": 0.0, "vertex": 0.0, "direction": 0.0}
    count = 0
    model.eval()
    with torch.no_grad():
        for start in range(0, len(data["features"]), batch_size):
            indices = slice(start, start + batch_size)
            outputs = model(data["features"][indices])
            terms = loss_terms(outputs, data, indices)
            size = len(data["features"][indices])
            for name, value in terms.items():
                totals[name] += value.item() * size
            count += size
    return {name: value / max(count, 1) for name, value in totals.items()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("features", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--batch-size", type=int, default=2048)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--seed", type=int, default=20250731)
    parser.add_argument("--allow-partial", action="store_true")
    args = parser.parse_args()

    if "partial_" in str(args.features) and not args.allow_partial:
        raise RuntimeError("partial datasets require --allow-partial and are diagnostic only")
    args.output_directory.mkdir(parents=True, exist_ok=False)

    torch, _ = require_torch()
    random.seed(args.seed)
    torch.manual_seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    records, direction_available = load_data(args.features)
    train_records = [record for record in records if record["split"] == 0]
    validation_records = [record for record in records if record["split"] == 1]
    test_records = [record for record in records if record["split"] == 2]
    if not train_records or not validation_records or not test_records:
        raise RuntimeError("all three job-level splits are required")

    means, scales = standardization(train_records)
    train = tensors(train_records, means, scales, device)
    validation = tensors(validation_records, means, scales, device)
    test = tensors(test_records, means, scales, device)
    model = build_feature_multitask_model(len(BASE_FEATURES)).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.learning_rate)
    generator = random.Random(args.seed)
    best_state, best_validation = None, math.inf
    history = []

    for epoch in range(args.epochs):
        model.train()
        for indices in batch_indices(len(train_records), args.batch_size, generator):
            optimizer.zero_grad()
            outputs = model(train["features"][indices])
            terms = loss_terms(outputs, train, indices)
            loss = terms["energy"] + terms["conversion"] + 0.01 * terms["vertex"]
            if direction_available:
                loss = loss + terms["direction"]
            loss.backward()
            optimizer.step()
        validation_metrics = evaluate(model, validation, args.batch_size)
        selection = (
            validation_metrics["energy"] + validation_metrics["conversion"] +
            0.01 * validation_metrics["vertex"]
        )
        history.append({"epoch": epoch, **validation_metrics})
        if selection < best_validation:
            best_validation = selection
            best_state = {
                name: value.detach().cpu().clone()
                for name, value in model.state_dict().items()
            }

    model.load_state_dict(best_state)
    test_metrics = evaluate(model, test, args.batch_size)
    torch.save({
        "model_state": best_state,
        "feature_names": BASE_FEATURES,
        "means": means,
        "scales": scales,
        "direction_available": direction_available,
        "seed": args.seed,
    }, args.output_directory / "feature_multitask.pt")
    (args.output_directory / "training_summary.json").write_text(
        json.dumps({
            "device": str(device),
            "direction_head_trained": direction_available,
            "split_counts": {
                "train": len(train_records),
                "validation": len(validation_records),
                "test": len(test_records),
            },
            "history": history,
            "test_losses": test_metrics,
        }, indent=2) + "\n"
    )
    print(f"ML_TRAINING_SUCCESS output={args.output_directory}")


if __name__ == "__main__":
    main()
