#!/usr/bin/env python3
"""Plot the physical separation of a CALO energy centroid from the MC conversion point.

The input is a Stage-01 derived-feature tar archive, not raw ROOT.  The script
never changes it.  It is intended for the current vertical gamma samples, for
which the primary travels approximately along -z:

    L_parallel = z_conversion - z_cog
    d_perp     = hypot(x_cog - x_conversion, y_cog - y_conversion)
    d_3D       = hypot(d_perp, L_parallel)

These quantities describe shower development after conversion.  They must not
be interpreted as a conversion-vertex reconstruction resolution.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import io
import math
import tarfile
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import numpy as np


def load_converted_active(archive_path: Path) -> dict[str, np.ndarray]:
    values: dict[str, list[float]] = {key: [] for key in
                                      ("energy", "longitudinal", "transverse", "distance")}
    with tarfile.open(archive_path, "r") as archive:
        stream = archive.extractfile("event_features.csv.gz")
        if stream is None:
            raise RuntimeError(f"event_features.csv.gz missing in {archive_path}")
        with gzip.GzipFile(fileobj=stream) as unpacked:
            for row in csv.DictReader(io.TextIOWrapper(unpacked)):
                if int(row["converted"]) != 1 or float(row["calo_edep_GeV"]) <= 0.0:
                    continue
                dx = float(row["centroid_x_cm"]) - float(row["conversion_x_cm"])
                dy = float(row["centroid_y_cm"]) - float(row["conversion_y_cm"])
                # Current samples are vertical top-entry gamma travelling along -z.
                longitudinal = float(row["conversion_z_cm"]) - float(row["centroid_z_cm"])
                transverse = math.hypot(dx, dy)
                values["energy"].append(float(row["true_energy_GeV"]))
                values["longitudinal"].append(longitudinal)
                values["transverse"].append(transverse)
                values["distance"].append(math.hypot(longitudinal, transverse))
    return {key: np.asarray(value, dtype=float) for key, value in values.items()}


def qlabel(values: np.ndarray) -> str:
    q16, median, q84 = np.quantile(values, [0.16, 0.50, 0.84])
    return rf"$\tilde{{x}}={median:.2f}$ cm; $[q_{{16}},q_{{84}}]=[{q16:.2f},{q84:.2f}]$ cm"


def display_limit(values: np.ndarray, floor: float = 0.0) -> float:
    return max(floor, float(np.quantile(values, 0.995)) * 1.06)


def histogram_axis(axis: plt.Axes, values: np.ndarray, label: str, title: str, color: str,
                   start: float = 0.0) -> None:
    upper = display_limit(values, floor=start + 1.0)
    inside = (values >= start) & (values <= upper)
    axis.hist(values[inside], bins=70, range=(start, upper), histtype="stepfilled",
              color=color, alpha=0.72)
    axis.axvline(np.median(values), color="#172554", lw=1.5, ls="--")
    axis.set_xlabel(label)
    axis.set_ylabel("events")
    axis.set_title(title, loc="left", fontweight="bold")
    axis.text(0.03, 0.95, qlabel(values), transform=axis.transAxes, va="top", fontsize=9)
    excluded = len(values) - int(inside.sum())
    if excluded:
        axis.text(0.03, 0.84, f"displayed: {int(inside.sum()):,}; overflow: {excluded}",
                  transform=axis.transAxes, va="top", fontsize=8, color="#4b5563")
    axis.grid(axis="y", color="#d1d5db", lw=0.6)
    axis.spines[["top", "right"]].set_visible(False)


def plot_prod04(data: dict[str, np.ndarray], output: Path) -> None:
    n = len(data["distance"])
    figure, axes = plt.subplots(2, 2, figsize=(13.8, 9.0))
    figure.subplots_adjust(left=0.08, right=0.98, bottom=0.10, top=0.86, hspace=0.34, wspace=0.26)
    histogram_axis(axes[0, 0], data["longitudinal"],
                   r"longitudinal development $L_{\parallel}=z_{conv}-z_{cog}$ [cm]",
                   "Longitudinal conversion-to-centroid separation", "#2563a6")
    histogram_axis(axes[0, 1], data["transverse"],
                   r"transverse separation $d_{\perp}$ [cm]",
                   "Transverse conversion-to-centroid separation", "#059669")
    histogram_axis(axes[1, 0], data["distance"],
                   r"3D separation $d_{3D}=|\vec r_{cog}-\vec r_{conv}|$ [cm]",
                   "Three-dimensional conversion-to-centroid separation", "#9333ea")
    x_max = display_limit(data["longitudinal"], floor=1.0)
    y_max = display_limit(data["transverse"], floor=1.0)
    hist = axes[1, 1].hist2d(data["longitudinal"], data["transverse"], bins=(70, 70),
                             range=((0, x_max), (0, y_max)), cmap="viridis",
                             norm=LogNorm(vmin=1))
    axes[1, 1].set_xlabel(r"$L_{\parallel}=z_{conv}-z_{cog}$ [cm]")
    axes[1, 1].set_ylabel(r"$d_{\perp}$ [cm]")
    axes[1, 1].set_title("Longitudinal versus transverse separation", loc="left", fontweight="bold")
    axes[1, 1].spines[["top", "right"]].set_visible(False)
    colorbar = figure.colorbar(hist[3], ax=axes[1, 1], pad=0.01)
    colorbar.set_label("events / 2D bin")
    figure.suptitle("1 GeV gamma: CALO energy centroid relative to the first MC conversion point",
                    fontsize=16, fontweight="bold")
    figure.text(0.5, 0.91,
                "Selection: first-converted and CALO-active prod04 events.  "
                "These are shower-development separations, not a vertex-reconstruction resolution.",
                ha="center", fontsize=9.6, color="#374151")
    figure.text(0.5, 0.035, f"N = {n:,}.  Current geometry/sample: vertical gamma enters along −z.",
                ha="center", fontsize=9.2, color="#374151")
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=230, bbox_inches="tight", facecolor="white")
    figure.savefig(output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


def energy_summary(energy: np.ndarray, values: np.ndarray, bins: int = 12) -> dict[str, np.ndarray]:
    edges = np.geomspace(0.05, 20.0, bins + 1)
    centers, count, q16, median, q84 = [], [], [], [], []
    for lower, upper in zip(edges[:-1], edges[1:]):
        chosen = values[(energy >= lower) & ((energy < upper) if upper < edges[-1] else (energy <= upper))]
        if len(chosen) == 0:
            continue
        centers.append(math.sqrt(lower * upper))
        count.append(len(chosen))
        q16.append(float(np.quantile(chosen, 0.16)))
        median.append(float(np.quantile(chosen, 0.50)))
        q84.append(float(np.quantile(chosen, 0.84)))
    return {key: np.asarray(value) for key, value in
            {"energy": centers, "count": count, "q16": q16, "median": median, "q84": q84}.items()}


def plot_prod05(data: dict[str, np.ndarray], output: Path) -> None:
    specs = [
        ("longitudinal", r"$L_{\parallel}=z_{conv}-z_{cog}$ [cm]", "Longitudinal separation", "#2563a6"),
        ("transverse", r"$d_{\perp}$ [cm]", "Transverse separation", "#059669"),
        ("distance", r"$d_{3D}$ [cm]", "Three-dimensional separation", "#9333ea"),
    ]
    figure, axes = plt.subplots(1, 3, figsize=(17.0, 5.8))
    figure.subplots_adjust(left=0.06, right=0.99, bottom=0.17, top=0.73, wspace=0.28)
    for axis, (key, ylabel, title, color) in zip(axes, specs):
        summary = energy_summary(data["energy"], data[key])
        axis.fill_between(summary["energy"], summary["q16"], summary["q84"], color=color, alpha=0.20,
                          label=r"$[q_{16},q_{84}]$")
        axis.plot(summary["energy"], summary["median"], "o-", color=color, lw=2.1, ms=4.8,
                  label="median")
        axis.set_xscale("log")
        axis.set_xlim(0.05, 20.0)
        axis.set_xlabel(r"true gamma energy $E_{true}$ [GeV]")
        axis.set_ylabel(ylabel)
        axis.set_title(title, loc="left", fontweight="bold")
        axis.grid(which="both", color="#d1d5db", lw=0.6)
        axis.spines[["top", "right"]].set_visible(False)
        axis.legend(frameon=False, fontsize=8.4, loc="best")
    figure.suptitle("Variable-energy gamma: conversion-to-CALO-centroid separation versus true energy",
                    fontsize=15.5, fontweight="bold")
    figure.text(0.5, 0.86,
                "Selection: first-converted and CALO-active prod05 events.  "
                "Points are bin medians; bands are central 68% intervals.  "
                "Vertical gamma enters along −z.",
                ha="center", fontsize=9.3, color="#374151")
    figure.text(0.5, 0.055, f"N = {len(data['distance']):,}.  These are physical shower-development separations, not vertex-resolution metrics.",
                ha="center", fontsize=9.0, color="#374151")
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=230, bbox_inches="tight", facecolor="white")
    figure.savefig(output.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("prod04_archive", type=Path)
    parser.add_argument("prod05_archive", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    prod04 = load_converted_active(args.prod04_archive)
    prod05 = load_converted_active(args.prod05_archive)
    if len(prod04["distance"]) == 0 or len(prod05["distance"]) == 0:
        raise RuntimeError("no converted and CALO-active events found")
    plot_prod04(prod04, args.output_dir / "31_cog_conversion_separation_1GeV.png")
    plot_prod05(prod05, args.output_dir / "32_cog_conversion_separation_vs_energy.png")


if __name__ == "__main__":
    main()
