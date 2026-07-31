#!/usr/bin/env python3
"""Summarize the event-feature CSV for the first HERDOS gamma study.

Uses only the Python standard library so it can run on IHEP login nodes.
"""

import argparse
import csv
import gzip
import math
from collections import defaultdict
from pathlib import Path


def quantile(values, probability):
    values = sorted(values)
    if not values:
        return math.nan
    position = (len(values) - 1) * probability
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return values[lower]
    fraction = position - lower
    return values[lower] * (1 - fraction) + values[upper] * fraction


def describe(values):
    if not values:
        return {"n": 0}
    mean = sum(values) / len(values)
    rms = math.sqrt(sum((value - mean) ** 2 for value in values) / len(values))
    return {
        "n": len(values),
        "mean": mean,
        "rms": rms,
        "q05": quantile(values, 0.05),
        "median": quantile(values, 0.50),
        "q95": quantile(values, 0.95),
    }


def correlation(x_values, y_values):
    if len(x_values) < 2 or len(x_values) != len(y_values):
        return math.nan
    mean_x = sum(x_values) / len(x_values)
    mean_y = sum(y_values) / len(y_values)
    covariance = sum(
        (x - mean_x) * (y - mean_y) for x, y in zip(x_values, y_values)
    )
    variance_x = sum((x - mean_x) ** 2 for x in x_values)
    variance_y = sum((y - mean_y) ** 2 for y in y_values)
    denominator = math.sqrt(variance_x * variance_y)
    return covariance / denominator if denominator else math.nan


def format_stats(stats):
    if not stats["n"]:
        return "n=0"
    return (
        f"n={stats['n']}, mean={stats['mean']:.6g}, "
        f"RMS={stats['rms']:.6g}, q05={stats['q05']:.6g}, "
        f"median={stats['median']:.6g}, q95={stats['q95']:.6g}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    args.output_directory.mkdir(parents=True, exist_ok=True)

    split = defaultdict(lambda: defaultdict(list))
    converted_z = []
    converted_edep = []
    true_energies = []
    anomalous = []
    converted_zero = []
    converted_low_tail = []
    category_counts = defaultdict(int)

    opener = gzip.open if args.csv_file.suffix == ".gz" else open
    with opener(args.csv_file, "rt", newline="") as source:
        reader = csv.DictReader(source)
        for row in reader:
            job = int(row["split"])
            converted = int(row["converted"])
            unconverted = int(row["unconverted_final"])
            true_energy = float(row["true_energy_GeV"])
            true_energies.append(true_energy)
            edep = float(row["calo_edep_GeV"])
            n_cells = int(row["n_cells"])
            n_cells_1 = int(row["n_cells_gt_1MeV"])

            split[job]["events"].append(1.0)
            split[job]["edep_all"].append(edep)
            split[job]["converted"].append(float(converted))
            split[job]["zero_edep"].append(float(edep == 0))
            if converted:
                split[job]["edep_converted"].append(edep)
                z = float(row["conversion_z_cm"])
                converted_z.append(z)
                converted_edep.append(edep)
                response = edep / true_energy if true_energy > 0 else math.nan
                if response < 0.8:
                    category_counts["converted_response_lt_0p8"] += 1
                    converted_low_tail.append(
                        {
                            "event": int(row["event"]),
                            "split": job,
                            "true_energy_GeV": true_energy,
                            "edep": edep,
                            "response": response,
                            "conversion_x_cm": row["conversion_x_cm"],
                            "conversion_y_cm": row["conversion_y_cm"],
                            "conversion_z_cm": row["conversion_z_cm"],
                        }
                    )
                if edep == 0:
                    converted_zero.append(
                        {
                            "event": int(row["event"]),
                            "split": job,
                            "conversion_x_cm": row["conversion_x_cm"],
                            "conversion_y_cm": row["conversion_y_cm"],
                            "conversion_z_cm": row["conversion_z_cm"],
                            "pair_energy_share": row["pair_energy_share"],
                            "pair_opening_deg": row["pair_opening_deg"],
                        }
                    )
            if unconverted and edep > 0:
                category_counts["unconverted_nonzero"] += 1
                anomalous.append(
                    {
                        "event": int(row["event"]),
                        "split": job,
                        "edep": edep,
                        "n_cells": n_cells,
                        "n_cells_1MeV": n_cells_1,
                        "centroid_ix": row["centroid_ix"],
                        "centroid_iy": row["centroid_iy"],
                        "centroid_iz": row["centroid_iz"],
                    }
                )
            elif unconverted:
                category_counts["unconverted_zero"] += 1

    anomaly_path = args.output_directory / "unconverted_nonzero_events.csv"
    with anomaly_path.open("w", newline="") as target:
        fieldnames = list(anomalous[0]) if anomalous else ["event"]
        writer = csv.DictWriter(target, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(anomalous)

    for filename, records in (
        ("converted_zero_edep_events.csv", converted_zero),
        ("converted_low_tail_events.csv", converted_low_tail),
    ):
        path = args.output_directory / filename
        with path.open("w", newline="") as target:
            fieldnames = list(records[0]) if records else ["event"]
            writer = csv.DictWriter(target, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(records)

    report = []
    report.append("# 第一阶段补充质量检查\n")
    report.append("## 入射能量\n\n")
    energy_stats = describe(true_energies)
    report.append(f"- Etrue：{format_stats(energy_stats)}\n")
    if energy_stats["n"] and energy_stats["q05"] != energy_stats["q95"]:
        positive = [value for value in true_energies if value > 0]
        log_min, log_max = math.log(min(positive)), math.log(max(positive))
        log_counts = [0] * 20
        for energy in positive:
            index = min(
                19,
                int((math.log(energy) - log_min) /
                    max(log_max - log_min, 1e-12) * 20),
            )
            log_counts[index] += 1
        mean_count = sum(log_counts) / len(log_counts)
        count_cv = math.sqrt(
            sum((count - mean_count) ** 2 for count in log_counts) /
            len(log_counts)
        ) / mean_count
        report.append(
            f"- 20 个等宽 log(E) 区间计数相对标准差：{count_cv:.6f}"
            "（E^-1 生成应近似均匀）\n"
        )
    report.append("## 子作业一致性\n")
    report.append(
        "| split | 事件数 | 转换比例 | 零沉积比例 | "
        "converted Edep mean (GeV) | RMS (GeV) |\n"
    )
    report.append("|---:|---:|---:|---:|---:|---:|\n")
    for job in sorted(split):
        count = len(split[job]["events"])
        conversion_fraction = sum(split[job]["converted"]) / count
        zero_fraction = sum(split[job]["zero_edep"]) / count
        edep_stats = describe(split[job]["edep_converted"])
        report.append(
            f"| {job} | {count} | {conversion_fraction:.6%} | "
            f"{zero_fraction:.6%} | {edep_stats['mean']:.6f} | "
            f"{edep_stats['rms']:.6f} |\n"
        )

    anomaly_edep = [item["edep"] for item in anomalous]
    anomaly_cells = [float(item["n_cells"]) for item in anomalous]
    anomaly_cells_1 = [float(item["n_cells_1MeV"]) for item in anomalous]
    report.append("\n## 未转换但 CALO 非零\n\n")
    report.append(
        f"- 事例数：{category_counts['unconverted_nonzero']}\n"
        f"- 零沉积未转换事例：{category_counts['unconverted_zero']}\n"
        f"- Edep：{format_stats(describe(anomaly_edep))}\n"
        f"- 非零晶体数：{format_stats(describe(anomaly_cells))}\n"
        f"- 超过 1 MeV 晶体数：{format_stats(describe(anomaly_cells_1))}\n"
        "- 事件清单：`unconverted_nonzero_events.csv`\n"
    )

    report.append("\n## 低能尾和转换深度\n\n")
    report.append(
        f"- converted 且 Edep/Etrue < 0.8："
        f"{category_counts['converted_response_lt_0p8']} 个\n"
        f"- converted 且 Edep = 0：{len(converted_zero)} 个\n"
        f"- converted 样本中 corr(zconv, Edep)："
        f"{correlation(converted_z, converted_edep):.6f}\n"
        "- 事件清单：`converted_low_tail_events.csv`、"
        "`converted_zero_edep_events.csv`\n"
        "- 线性相关系数只能作为提示；转换深度呈分层和非线性结构，"
        "后续应按几何区间分别比较分布。\n"
    )

    report_path = args.output_directory / "stage1_quality_summary.md"
    report_path.write_text("".join(report), encoding="utf-8")
    print(report_path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
