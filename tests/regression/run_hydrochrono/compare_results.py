#!/usr/bin/env python3
"""
Generic time-series comparison script.

Purpose:
- Compare two time-series (reference vs simulation) and report error metrics
- Plot both series on a single figure and save to an output directory

Data sources supported per series:
- HDF5: provide dataset paths for time and value; optional column index for 2D datasets
- CSV/TXT: numeric table; choose time/value columns (default: 0,1)
- NPY/NPZ: a 2-column array (time,value) or named arrays

This script is intentionally generic. Model/test-specific extraction (e.g., picking
body2 heave) should be done by the caller, which then points this script at the
appropriate datasets/columns/files for a like-for-like comparison.
"""

import argparse
from pathlib import Path
from typing import Tuple, Optional

import numpy as np
import sys

try:
    import h5py  # type: ignore
except Exception:  # pragma: no cover
    h5py = None


def load_series(
    path: Path,
    *,
    time_dset: Optional[str] = None,
    val_dset: Optional[str] = None,
    time_col: Optional[int] = None,
    val_col: Optional[int] = None,
    npz_time_key: Optional[str] = None,
    npz_val_key: Optional[str] = None,
) -> Tuple[np.ndarray, np.ndarray]:
    """Load a time/value pair from path.

    - HDF5 (.h5/.hdf5): time_dset and val_dset required; val_col selects column for 2D.
    - CSV/TXT: load numeric table via numpy; time_col/val_col select columns.
    - NPY: expect shape (N,2) -> (time,value) or (2,N) -> transpose.
    - NPZ: provide npz_time_key and npz_val_key.
    """
    suffix = path.suffix.lower()
    if suffix in {".h5", ".hdf5"}:
        if h5py is None:
            raise RuntimeError("h5py not available to read HDF5")
        if not time_dset or not val_dset:
            raise ValueError("HDF5 input requires --*-time-dset and --*-val-dset")
        with h5py.File(path, "r") as f:  # type: ignore
            if time_dset not in f or val_dset not in f:
                raise KeyError(f"Missing dataset(s). time_dset={time_dset!r}, val_dset={val_dset!r}")
            t = np.asarray(f[time_dset][:], dtype=float).reshape(-1)
            arr = np.asarray(f[val_dset][:])
            if val_col is not None and arr.ndim >= 2:
                y = np.asarray(arr[:, val_col], dtype=float).reshape(-1)
            else:
                y = np.asarray(arr, dtype=float).reshape(-1)
            return t, y

    if suffix in {".npy"}:
        arr = np.load(path)
        arr = np.asarray(arr)
        if arr.ndim != 2 or (arr.shape[1] != 2 and arr.shape[0] != 2):
            raise ValueError("NPY must contain a 2-column (N,2) or (2,N) array")
        if arr.shape[0] == 2 and arr.shape[1] != 2:
            arr = arr.T
        t = np.asarray(arr[:, 0], dtype=float).reshape(-1)
        y = np.asarray(arr[:, 1], dtype=float).reshape(-1)
        return t, y

    if suffix in {".npz"}:
        with np.load(path) as npz:
            if npz_time_key is None or npz_val_key is None:
                raise ValueError("NPZ requires --*npz-time-key and --*npz-val-key")
            t = np.asarray(npz[npz_time_key], dtype=float).reshape(-1)
            y = np.asarray(npz[npz_val_key], dtype=float).reshape(-1)
            return t, y

    # CSV/TXT fallback
    data = np.genfromtxt(path, dtype=float, comments="#")
    if data.ndim == 1:
        data = data.reshape(-1, 1)
    if data.shape[1] < 2:
        raise ValueError("TXT/CSV must have at least 2 numeric columns")
    tc = 0 if time_col is None else time_col
    vc = 1 if val_col is None else val_col
    t = np.asarray(data[:, tc], dtype=float).reshape(-1)
    y = np.asarray(data[:, vc], dtype=float).reshape(-1)
    return t, y


def interpolate_to_reference(t_ref: np.ndarray, y_ref: np.ndarray, t_sim: np.ndarray, y_sim: np.ndarray):
    y_sim_interp = np.interp(t_ref, t_sim, y_sim)
    return t_ref, y_ref, y_sim_interp


def rms_relative_error(ref: np.ndarray, pred: np.ndarray) -> float:
    ref_rms = float(np.sqrt(np.mean(np.square(ref))))
    if ref_rms == 0.0:
        return float(np.sqrt(np.mean(np.square(pred))))
    return float(np.sqrt(np.mean(np.square(pred - ref))) / ref_rms)


def render_plot(
    t_ref: np.ndarray,
    y_ref: np.ndarray,
    t_sim: np.ndarray,
    y_sim: np.ndarray,
    out_dir: Path,
    title: str,
    y_label: str,
    show: bool,
    ref_label: str = "Reference",
    sim_label: str = "Simulation",
 ) -> Path:
    import matplotlib.pyplot as plt

    out_dir.mkdir(parents=True, exist_ok=True)
    fig = plt.figure(figsize=(10, 5))
    plt.plot(t_ref, y_ref, label=ref_label, lw=2)
    plt.plot(t_sim, y_sim, label=sim_label, lw=1, ls="--")
    plt.xlabel("Time (s)")
    plt.ylabel(y_label)
    plt.title(title)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    out_path = out_dir / "comparison.png"
    fig.savefig(str(out_path), dpi=150)
    if show:
        try:
            plt.show()
        except Exception:
            pass
    plt.close(fig)
    return out_path


def main() -> int:
    p = argparse.ArgumentParser(description="Compare two time-series (reference vs simulation)")
    # Reference
    p.add_argument("--ref", help="Reference data file path (H5/CSV/TXT/NPY/NPZ)")
    p.add_argument("--ref-time-dset", dest="ref_time_dset", help="H5: time dataset path")
    p.add_argument("--ref-val-dset", dest="ref_val_dset", help="H5: value dataset path")
    p.add_argument("--ref-time-col", dest="ref_time_col", type=int, default=None, help="CSV/TXT: time column index")
    p.add_argument("--ref-val-col", dest="ref_val_col", type=int, default=None, help="CSV/TXT: value column index")
    p.add_argument("--ref-col", dest="ref_col", type=int, default=None, help="H5 2D dataset column index for value")
    p.add_argument("--ref-npz-time-key", dest="ref_npz_time_key", default=None)
    p.add_argument("--ref-npz-val-key", dest="ref_npz_val_key", default=None)

    # Simulation
    p.add_argument("--sim", help="Simulation data file path (H5/CSV/TXT/NPY/NPZ)")
    p.add_argument("--sim-time-dset", dest="sim_time_dset", help="H5: time dataset path")
    p.add_argument("--sim-val-dset", dest="sim_val_dset", help="H5: value dataset path")
    p.add_argument("--sim-time-col", dest="sim_time_col", type=int, default=None, help="CSV/TXT: time column index")
    p.add_argument("--sim-val-col", dest="sim_val_col", type=int, default=None, help="CSV/TXT: value column index")
    p.add_argument("--sim-col", dest="sim_col", type=int, default=None, help="H5 2D dataset column index for value")
    p.add_argument("--sim-npz-time-key", dest="sim_npz_time_key", default=None)
    p.add_argument("--sim-npz-val-key", dest="sim_npz_val_key", default=None)

    # Options
    p.add_argument("--outdir", default=".", help="Output directory for the plot (default .)")
    p.add_argument("--title", default="Reference vs Simulation", help="Plot title")
    p.add_argument("--ylabel", default="Signal", help="Y-axis label")
    p.add_argument("--tol", type=float, default=0.02, help="RMS relative error tolerance (default 0.02)")
    p.add_argument("--show", action="store_true", help="Display plot interactively")

    args = p.parse_args()

    ref_path = Path(args.ref)
    sim_path = Path(args.sim)
    outdir = Path(args.outdir)

    # Load series
    t_ref, y_ref = load_series(
        ref_path,
        time_dset=args.ref_time_dset,
        val_dset=args.ref_val_dset,
        time_col=args.ref_time_col,
        val_col=args.ref_val_col or args.ref_col,
        npz_time_key=args.ref_npz_time_key,
        npz_val_key=args.ref_npz_val_key,
    )
    t_sim, y_sim = load_series(
        sim_path,
        time_dset=args.sim_time_dset,
        val_dset=args.sim_val_dset,
        time_col=args.sim_time_col,
        val_col=args.sim_val_col or args.sim_col,
        npz_time_key=args.sim_npz_time_key,
        npz_val_key=args.sim_npz_val_key,
    )

    # Compare
    t_cmp, ref_cmp, sim_cmp = interpolate_to_reference(t_ref, y_ref, t_sim, y_sim)
    rms_rel = rms_relative_error(ref_cmp, sim_cmp)
    status = "PASS" if rms_rel <= args.tol else "FAIL"
    print(f"{status} | N={len(t_cmp)} | RMSrel={rms_rel:.6f} | tol={args.tol:.6f}")

    # Plot using the standard regression template
    try:
        template_dir = Path(__file__).resolve().parents[1]  # tests/regression
        sys.path.insert(0, str(template_dir))
        from compare_template import create_comparison_plot  # type: ignore

        # Build ref/test arrays on a consistent timebase for the template
        ref_on_sim = np.interp(t_sim, t_ref, y_ref)
        ref_data = np.column_stack((t_sim, ref_on_sim))
        sim_data = np.column_stack((t_sim, y_sim))

        create_comparison_plot(
            ref_data,
            sim_data,
            args.title or "Comparison",
            str(outdir),
            ref_file_path=str(ref_path),
            test_file_path=str(sim_path),
            executable_path=None,
            y_label=args.ylabel,
            executable_patterns=None,
        )
    except Exception as e:
        # Fallback minimal plot
        out_path = render_plot(t_ref, y_ref, t_sim, y_sim, outdir, args.title, args.ylabel, args.show)
        print(f"Template plot failed: {e}. Fallback saved: {out_path}")

    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())


