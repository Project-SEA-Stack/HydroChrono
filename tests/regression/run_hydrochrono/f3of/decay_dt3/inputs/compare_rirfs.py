#!/usr/bin/env python3
"""
Plot k_before and k_after vs time for body 0/1/2 from CSVs:
  - rirf_body0_summary.csv
  - rirf_body1_summary.csv
  - rirf_body2_summary.csv
Produces a figure with 3 stacked subplots.
"""

import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def load_csv(path: Path) -> pd.DataFrame:
    """Load a CSV and ensure numeric dtypes for time/k columns."""
    df = pd.read_csv(path)
    # Coerce to numeric in case there are stray strings/spaces
    for col in ("time", "k_before", "k_after"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    # Drop any rows that failed to parse
    df = df.dropna(subset=["time", "k_before", "k_after"])
    return df


def main():
    files = [
        ("Body 0", Path("rirf_body0_summary.csv")),
        ("Body 1", Path("rirf_body1_summary.csv")),
        ("Body 2", Path("rirf_body2_summary.csv")),
    ]

    # Check presence up front for a friendlier error
    missing = [str(p) for _, p in files if not p.exists()]
    if missing:
        sys.stderr.write(
            "ERROR: Missing file(s):\n  - " + "\n  - ".join(missing) + "\n"
        )
        sys.exit(1)

    fig, axes = plt.subplots(nrows=3, ncols=1, sharex=True, figsize=(10, 9))
    fig.suptitle("k_before and k_after vs time", fontsize=14)

    for ax, (label, path) in zip(axes, files):
        df = load_csv(path)
        ax.plot(df["time"], df["k_before"], label="k_before")
        ax.plot(df["time"], df["k_after"], linestyle="--", label="k_after")
        ax.set_ylabel("k")
        ax.set_title(label)
        ax.grid(True, alpha=0.3)
        ax.legend()

    axes[-1].set_xlabel("time (s)")

    plt.tight_layout(rect=[0, 0, 1, 0.96])  # leave room for suptitle
    # Uncomment to save instead of just showing:
    # plt.savefig("k_vs_time_bodies.png", dpi=150)
    plt.show()


if __name__ == "__main__":
    main()
