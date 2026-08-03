#!/usr/bin/env python3
"""Plot energy-controlled response residuals against longitudinal features."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path: Path, observable: str) -> list[dict[str, float]]:
    with path.open() as handle:
        return [{key: float(value) if key not in {"observable", "bin"} else value
                 for key, value in row.items()}
                for row in csv.DictReader(handle) if row["observable"] == observable]


def ranges(rows: list[dict[str, float]], precision: int = 3, fraction: bool = False) -> list[str]:
    labels = []
    for row in rows:
        low, high = row["low"], row["high"]
        if low == high == 0:
            labels.append("0")
        elif fraction and high < 0.001:
            labels.append(f"{low:.1e}–{high:.1e}")
        elif fraction and high < 0.1:
            labels.append(f"{low:.{precision}f}–{high:.{precision}f}")
        else:
            labels.append(f"{low:.{precision}f}–{high:.{precision}f}")
    return labels


def panel(ax, rows: list[dict[str, float]], labels: list[str], title: str, x_label: str) -> None:
    x = list(range(len(rows)))
    medians = [row["median"] for row in rows]
    lower = [row["median"] - row["q16"] for row in rows]
    upper = [row["q84"] - row["median"] for row in rows]
    ax.errorbar(x, medians, yerr=[lower, upper], fmt="o-", ms=6, lw=2.2,
                capsize=3.2, color="#2563a6")
    ax.axhline(1.0, color="#4b5563", ls="--", lw=1.15)
    ax.set_xticks(x, labels, rotation=18, ha="right", fontsize=8.2)
    ax.set_xlabel(x_label)
    ax.set_ylim(0.0, 1.10)
    ax.set_ylabel(r"normalized response $R_{norm}=R/\tilde{R}(E_{true})$")
    ax.set_title(title, loc="left", fontweight="bold")
    ax.grid(axis="y", color="#d1d5db", lw=0.7)
    ax.spines[["top", "right"]].set_visible(False)
    for position, row in zip(x, rows):
        ax.text(position, 0.025, f"N={int(row['n'])/1000:.1f}k", ha="center", va="bottom",
                fontsize=7.7, color="#4b5563")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    last = load(args.input, "last_layer_fraction")
    depth = load(args.input, "centroid_z_cm")
    fig, axes = plt.subplots(1, 2, figsize=(13.8, 7.1))
    fig.subplots_adjust(left=0.07, right=0.995, bottom=0.23, top=0.75, wspace=0.16)
    last_labels = ["0", "<2×10⁻⁴", "2×10⁻⁴–2×10⁻³", "2×10⁻³–3×10⁻²", ">3×10⁻²"]
    panel(axes[0], last, last_labels, "Last-layer energy fraction",
          r"$f_{last}=E_{iz=20}/E_{dep}$  (five groups)")
    panel(axes[1], depth, ranges(depth, precision=1), "Energy-weighted shower centroid",
          r"$z_{cog}=\sum_i E_i z_i / E_{dep}$  [cm]  (five groups)")
    fig.suptitle("Longitudinal CALO features and energy-response residuals",
                 fontsize=16, fontweight="bold", y=0.97)
    fig.text(0.5, 0.895,
             r"Definition: $R=E_{dep}/E_{true}$;  $R_{norm}=R/\tilde{R}(E_{true}\ \mathrm{bin})$.  "
             r"$R_{norm}=1$ means typical response at the same true energy; points are medians and bars are $[q_{16},q_{84}]$.",
             ha="center", fontsize=9.5, color="#374151")
    fig.text(0.5, 0.045,
             "All CALO-active prod05 events (N = 814,358). z is the HERDOS global coordinate; this plot does not assign an independent physical direction to its sign.",
             ha="center", fontsize=9.4, color="#374151")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=230, bbox_inches="tight", facecolor="white")
    fig.savefig(args.output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


if __name__ == "__main__":
    main()
