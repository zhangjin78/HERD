#!/usr/bin/env python3
"""Plot a feature-screening result for simple transverse CALO observables."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path: Path, observable: str) -> list[dict[str, float]]:
    with path.open() as handle:
        return [{key: float(value) if key != "observable" else value for key, value in row.items()}
                for row in csv.DictReader(handle) if row["observable"] == observable]


def panel(ax, rows: list[dict[str, float]], title: str, xlabel: str) -> None:
    x = [row["x_median"] for row in rows]
    median = [row["median"] for row in rows]
    lower = [row["median"] - row["q16"] for row in rows]
    upper = [row["q84"] - row["median"] for row in rows]
    ax.fill_between(x, [value - error for value, error in zip(median, lower)],
                    [value + error for value, error in zip(median, upper)],
                    color="#2563a6", alpha=0.16, label="central 68%")
    ax.plot(x, median, "o-", color="#2563a6", lw=2.1, ms=5.5, label="median")
    ax.axhline(1.0, color="#4b5563", lw=1.15, ls="--")
    ax.set_ylim(0.88, 1.04)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r"normalized response $R_{norm}$")
    ax.set_title(title, loc="left", fontsize=12, fontweight="bold")
    ax.grid(color="#d1d5db", lw=0.65)
    ax.spines[["top", "right"]].set_visible(False)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    items = [
        ("n_cells_gt_1MeV", "Hit-crystal multiplicity", r"$N_{cell}$ with $E_i>1\,MeV$"),
        ("transverse_rms_cells", "Transverse shower width", r"$\sqrt{\mathrm{Var}_E(i_x)+\mathrm{Var}_E(i_y)}$  [cell]"),
        ("max_cell_fraction", "Maximum-cell energy fraction", r"$\max_i(E_i)/E_{dep}$"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(16, 6.25))
    fig.subplots_adjust(left=0.06, right=0.995, bottom=0.19, top=0.70, wspace=0.28)
    for axis, (name, title, xlabel) in zip(axes, items):
        panel(axis, load(args.input, name), title, xlabel)
    axes[0].legend(frameon=False, loc="lower right", fontsize=8.5)
    fig.suptitle("Simple transverse CALO summaries: feature-screening result",
                 fontsize=15.5, fontweight="bold", y=0.97)
    fig.text(0.5, 0.865,
             r"Definition: $R=E_{dep}/E_{true}$;  $R_{norm}=R/\tilde{R}(E_{true}\ \mathrm{bin})$.  "
             r"$R_{norm}=1$ is typical response at the same true energy.  Lines: medians; bands: $[q_{16},q_{84}]$.",
             ha="center", fontsize=9.3, color="#374151")
    fig.text(0.5, 0.055,
             "All CALO-active prod05 events (N = 814,358); five equal-population feature bins. This does not prove that the full 3D topology has no additional information.",
             ha="center", fontsize=9.0, color="#374151")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=230, bbox_inches="tight", facecolor="white")
    fig.savefig(args.output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


if __name__ == "__main__":
    main()
