#!/usr/bin/env python3
"""Plot raw-response distributions in feature bins for the mono-energetic prod04 sample.

All user-facing titles and labels are grouped in LABELS below so they can be
edited without touching the calculation or binning logic.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


LABELS = {
    "long_title": "1 GeV gamma: longitudinal shower features versus raw CALO response",
    "trans_title": "1 GeV gamma: transverse shower-feature response distributions",
    "definition": r"$R=E_{dep}/E_{true}$;  here $E_{true}=1\,GeV$, so no energy-normalization is applied.  Each curve is the normalized $R$ distribution within one feature bin.",
    "response_x": r"raw CALO response $R=E_{dep}/E_{true}$",
    "density_y": "normalized entries / response bin",
}


def label_range(group: dict[str, object], feature: str) -> str:
    low, high = float(group["low"]), float(group["high"])
    if feature == "last_layer_fraction" and str(group["bin"]) == "zero":
        return "$f_{last}=0$"
    if feature == "last_layer_fraction":
        return f"{low:.2g}–{high:.2g}"
    if feature == "centroid_z_cm":
        return f"{low:.1f}–{high:.1f} cm"
    if feature == "n_cells_gt_1MeV":
        return f"{low:.0f}–{high:.0f}"
    if feature == "transverse_rms_cells":
        return f"{low:.2f}–{high:.2f}"
    if feature == "max_cell_fraction":
        return f"{low:.2f}–{high:.2f}"
    return f"{low:.3g}–{high:.3g}"


def density_axis(ax, groups: list[dict[str, object]], feature: str, title: str) -> None:
    colors = plt.cm.viridis(np.linspace(0.12, 0.88, len(groups)))
    response_edges = np.linspace(0.0, 1.05, 71)
    for color, group in zip(colors, groups):
        counts = np.array(group["hist"], dtype=float)
        if counts.sum() == 0:
            continue
        density = counts / counts.sum() / np.diff(response_edges)
        centers = 0.5 * (response_edges[:-1] + response_edges[1:])
        label = (f"{label_range(group, feature)}  (N={int(group['n']):,}; "
                 f"$\\tilde{{R}}$={float(group['response_q50']):.3f}; "
                 f"$[q_{{16}},q_{{84}}]$=[{float(group['response_q16']):.3f},{float(group['response_q84']):.3f}])")
        ax.step(centers, density, where="mid", color=color, lw=1.8, label=label)
    ax.axvline(1.0, color="#4b5563", ls="--", lw=1.0, label="$R=1$")
    ax.set_xlim(0.0, 1.05)
    ax.set_xlabel(LABELS["response_x"])
    ax.set_ylabel(LABELS["density_y"])
    ax.set_title(title, loc="left", fontsize=12, fontweight="bold")
    ax.grid(color="#d1d5db", lw=0.6)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(fontsize=7.1, frameon=False, loc="upper left")


def population_axis(ax, groups: list[dict[str, object]], feature: str, title: str, x_label: str) -> None:
    names = [label_range(group, feature) for group in groups]
    counts = [int(group["n"]) for group in groups]
    ax.bar(range(len(groups)), counts, color="#5b8ec1")
    ax.set_xticks(range(len(groups)), names, rotation=20, ha="right", fontsize=8.1)
    ax.set_ylabel("events")
    ax.set_xlabel(x_label)
    ax.set_title(title, loc="left", fontsize=12, fontweight="bold")
    ax.grid(axis="y", color="#d1d5db", lw=0.6)
    ax.spines[["top", "right"]].set_visible(False)
    for x, count in enumerate(counts):
        ax.text(x, count, f"{count/1000:.1f}k", ha="center", va="bottom", fontsize=7.6, color="#374151")


def plot_longitudinal(payload: dict[str, object], output: Path) -> None:
    features = payload["features"]
    last, zcog = features["last_layer_fraction"], features["centroid_z_cm"]
    fig, axes = plt.subplots(2, 2, figsize=(14.4, 8.4))
    fig.subplots_adjust(left=0.07, right=0.985, bottom=0.09, top=0.80, hspace=0.50, wspace=0.24)
    population_axis(axes[0, 0], last, "last_layer_fraction", "Last-layer feature-bin populations",
                    r"$f_{last}=E_{iz=0}/E_{dep}$")
    density_axis(axes[0, 1], last, "last_layer_fraction", "Raw-response distribution in each $f_{last}$ bin")
    population_axis(axes[1, 0], zcog, "centroid_z_cm", "Energy-centroid-z feature-bin populations",
                    r"$z_{cog}=\sum_i E_i z_i/E_{dep}$  [cm]")
    density_axis(axes[1, 1], zcog, "centroid_z_cm", "Raw-response distribution in each $z_{cog}$ bin")
    fig.suptitle(LABELS["long_title"], fontsize=16, fontweight="bold", y=0.98)
    fig.text(0.5, 0.905, LABELS["definition"], ha="center", fontsize=9.5, color="#374151")
    fig.text(0.5, 0.025, f"Selection: CALO-active prod04 events (N = {int(payload['calo_active']):,}). Points/quantiles in the legend summarize each full distribution.",
             ha="center", fontsize=9.2, color="#374151")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=230, bbox_inches="tight", facecolor="white")
    fig.savefig(output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


def plot_transverse(payload: dict[str, object], output: Path) -> None:
    features = payload["features"]
    specs = [
        ("n_cells_gt_1MeV", "Hit-crystal multiplicity: response distributions"),
        ("transverse_rms_cells", "Transverse RMS: response distributions"),
        ("max_cell_fraction", "Maximum-cell fraction: response distributions"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(17.2, 6.45))
    fig.subplots_adjust(left=0.055, right=0.99, bottom=0.15, top=0.70, wspace=0.32)
    for axis, (feature, title) in zip(axes, specs):
        density_axis(axis, features[feature], feature, title)
    fig.suptitle(LABELS["trans_title"], fontsize=16, fontweight="bold", y=0.97)
    fig.text(0.5, 0.865, LABELS["definition"], ha="center", fontsize=9.2, color="#374151")
    fig.text(0.5, 0.055,
             r"Features: $N_{cell}(E_i>1\,MeV)$;  $RMS_\perp=\sqrt{Var_E(i_x)+Var_E(i_y)}$ [cell];  $f_{max}=\max_i(E_i)/E_{dep}$.  Five feature bins per panel.",
             ha="center", fontsize=9.0, color="#374151")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=230, bbox_inches="tight", facecolor="white")
    fig.savefig(output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    payload = json.loads(args.input.read_text())
    plot_longitudinal(payload, args.output_dir / "03_longitudinal_feature_distributions_1GeV.png")
    plot_transverse(payload, args.output_dir / "04_transverse_feature_distributions_1GeV.png")


if __name__ == "__main__":
    main()
