#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def read_columns(filename, x_column, temperature_column):
    with filename.open(newline="") as csv_file:
        rows = csv.DictReader(csv_file)
        return [(float(row[x_column]), float(row[temperature_column])) for row in rows]


def read_centerline(lower_filename, upper_filename):
    lower = read_columns(lower_filename, "x", "T")
    upper = read_columns(upper_filename, "x", "T")
    if len(lower) != len(upper) or any(
        x_lower != x_upper for (x_lower, _), (x_upper, _) in zip(lower, upper)
    ):
        raise ValueError("Centerline samples must have identical x coordinates")
    return [
        (x, 0.5 * (lower_t + upper_t))
        for (x, lower_t), (_, upper_t) in zip(lower, upper)
    ]


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(
        description="Compare the MOOSE horizontal centerline temperature with reference data."
    )
    parser.add_argument(
        "lower_csv",
        nargs="?",
        type=Path,
        default=directory / "rayleigh_benard_out_centerline_lower_0002.csv",
    )
    parser.add_argument(
        "upper_csv",
        nargs="?",
        type=Path,
        default=directory / "rayleigh_benard_out_centerline_upper_0002.csv",
    )
    parser.add_argument(
        "reference_csv",
        nargs="?",
        type=Path,
        default=directory / "Ra1e5_Overleaf.csv",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=directory / "mean_temperature_comparison.png",
    )
    args = parser.parse_args()

    moose = read_centerline(args.lower_csv, args.upper_csv)
    reference = read_columns(args.reference_csv, "x", "t")

    figure, axes = plt.subplots(figsize=(6.4, 4.8), constrained_layout=True)
    axes.plot(*zip(*moose), label="MOOSE", linewidth=2)
    axes.plot(*zip(*reference), label="Reference", linewidth=2, linestyle="--")
    axes.set_xlabel("x")
    axes.set_ylabel("Centerline temperature")
    axes.grid(alpha=0.25)
    axes.legend()
    figure.savefig(args.output, dpi=200)


if __name__ == "__main__":
    main()
