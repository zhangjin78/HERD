#!/usr/bin/env python3
"""Make the report-level CALO raw-energy response summary figure."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import numpy as np


# 仅修改此处即可调整图中所有面向报告的文字；计算与分箱逻辑不受影响。
LABELS = {
    "suptitle": "CALO raw energy response in the ideal vertical-gamma sample",
    "left_title": "Two-dimensional CALO response",
    "left_xlabel": "true gamma energy $E_{true}$ [GeV]",
    "left_ylabel": "total CALO deposited energy $E_{dep}$ [GeV]",
    "left_note": "all CALO-active prod01 events\nN = 814,358",
    "left_note_color": "#111827",
    "colorbar": "events / 2D bin",
    "right_title": "Binned raw-response distribution",
    "right_xlabel": "true gamma energy $E_{true}$ [GeV]",
    "right_ylabel": "raw CALO response $R=E_{dep}/E_{true}$",
    "right_note": r"points: median $\tilde{R}$" "\n" r"blue band: $[q_{16},q_{84}]$",
}


def load_rows(path: Path) -> list[dict[str, float]]:
    with path.open() as handle:
        return [{key: float(value) for key, value in row.items()}
                for row in csv.DictReader(handle)]


def load_histogram(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    rows = load_rows(path)
    x_edges = np.array(sorted({row["true_low_GeV"] for row in rows} |
                              {row["true_high_GeV"] for row in rows}))
    y_edges = np.array(sorted({row["edep_low_GeV"] for row in rows} |
                              {row["edep_high_GeV"] for row in rows}))
    counts = np.zeros((len(x_edges) - 1, len(y_edges) - 1), dtype=float)
    x_index = {value: index for index, value in enumerate(x_edges[:-1])}
    y_index = {value: index for index, value in enumerate(y_edges[:-1])}
    for row in rows:
        counts[x_index[row["true_low_GeV"]], y_index[row["edep_low_GeV"]]] = row["count"]
    return x_edges, y_edges, counts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("response_csv", type=Path)
    parser.add_argument("prod04_csv", type=Path)
    parser.add_argument("histogram_csv", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    rows = load_rows(args.response_csv)
    reference = load_rows(args.prod04_csv)[0]
    energy = [row["energy_center_GeV"] for row in rows]
    median = [row["response_median"] for row in rows]
    q16 = [row["response_q16"] for row in rows]
    q84 = [row["response_q84"] for row in rows]
    x_edges, y_edges, counts = load_histogram(args.histogram_csv)

    color = "#2563a6"
    accent = "#c43c39"
    fig, axes = plt.subplots(1, 2, figsize=(14.2, 6.35), constrained_layout=True)

    ax = axes[0]
    mesh = ax.pcolormesh(x_edges, y_edges, counts.T, shading="auto",
                          cmap="viridis", norm=LogNorm(vmin=1, vmax=max(10, counts.max())))
    ax.plot(energy, [value * point for value, point in zip(median, energy)], "o-",
            color="#d83b32", lw=2.0, ms=4.8, label=r"$\tilde{R}(E)\times E_{true}$")
    ax.plot([0.05, 20], [0.05, 20], color="white", lw=1.3, ls="--", label=r"$E_{dep}=E_{true}$")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(0.05, 20)
    ax.set_ylim(y_edges[0], 25)
    ax.set_xlabel(LABELS["left_xlabel"])
    ax.set_ylabel(LABELS["left_ylabel"])
    ax.set_title(LABELS["left_title"], loc="left", fontweight="bold")
    ax.text(0.055, 16, LABELS["left_note"], ha="left", va="top",
            fontsize=9.5, color=LABELS["left_note_color"], fontweight="bold")
    ax.grid(which="both", color="#ffffff", lw=0.45, alpha=0.28)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(loc="lower right", frameon=True, framealpha=0.88, fontsize=8.3)
    colorbar = fig.colorbar(mesh, ax=ax, pad=0.01)
    colorbar.set_label(LABELS["colorbar"])

    ax = axes[1]
    ax.fill_between(energy, q16, q84, color=color, alpha=0.20, label="central 68% interval")
    ax.plot(energy, median, "o-", color=color, lw=2.2, ms=5.5, label="median (prod02)")
    ax.errorbar(reference["energy_center_GeV"], reference["response_median"],
                yerr=[[reference["response_median"] - reference["response_q16"]],
                      [reference["response_q84"] - reference["response_median"]]],
                fmt="s", color=accent, mfc="white", ms=7, mew=1.8, capsize=3,
                label="independent 1 GeV sample (prod01)")
    ax.axhline(1.0, color="#4b5563", lw=1.2, ls="--", label="$E_{dep}/E_{true}=1$")
    ax.set_xscale("log")
    ax.set_xlim(0.05, 20)
    ax.set_ylim(0.75, 1.03)
    ax.set_xlabel(LABELS["right_xlabel"])
    ax.set_ylabel(LABELS["right_ylabel"])
    ax.set_title(LABELS["right_title"], loc="left", fontweight="bold")
    ax.text(0.055, 0.762, LABELS["right_note"],
            ha="left", va="bottom", fontsize=9.3, color="#374151")
    ax.grid(which="both", axis="both", color="#d1d5db", lw=0.6, alpha=0.8)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(loc="lower right", frameon=False, fontsize=8.5)

    fig.suptitle(LABELS["suptitle"],
                 fontsize=16, fontweight="bold")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=230, bbox_inches="tight", facecolor="white")
    fig.savefig(args.output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


if __name__ == "__main__":
    main()
