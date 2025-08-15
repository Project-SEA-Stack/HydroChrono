#!/usr/bin/env python3
import argparse
from pathlib import Path

import numpy as np
import h5py
import yaml
import sys
import re
from importlib import import_module


def read_yaml(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def discover_output_directory(setup_yaml: dict, setup_path: Path) -> Path:
    out_dir = setup_yaml.get("output_directory")
    if not out_dir:
        raise RuntimeError("output_directory not set in setup YAML")
    return (setup_path.parent / out_dir).resolve()


def discover_h5_file(output_dir: Path) -> Path:
    if not output_dir.exists():
        raise FileNotFoundError(f"Output directory not found: {output_dir}")
    h5_files = sorted(output_dir.glob("*.h5"))
    if not h5_files:
        raise FileNotFoundError(f"No .h5 files found under {output_dir}")
    still = [p for p in h5_files if ".still." in p.name or p.name.endswith(".still.h5")]
    if still:
        return max(still, key=lambda p: p.stat().st_mtime)
    return max(h5_files, key=lambda p: p.stat().st_mtime)

def select_signal_via_adapter(h5_path: Path):
    model = h5_path.parent.parent.parent.name.lower()
    # Resolve module path by preference: signal_adapter.py -> signals.py -> adapter.py
    base = Path(__file__).resolve().parent
    adapter_path = base / model / "signal_adapter.py"
    if not adapter_path.exists():
        candidate = base / model / "signals.py"
        adapter_path = candidate if candidate.exists() else base / model / "adapter.py"
    if not adapter_path.exists():
        raise ImportError(f"Adapter not found for model='{model}' (signal_adapter.py/signals.py/adapter.py)")
    import importlib.util
    spec = importlib.util.spec_from_file_location(f"adapter_{model}", str(adapter_path))
    if not spec or not spec.loader:
        raise ImportError(f"Failed to load adapter spec for model='{model}'")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    # try std import first for devs running as a package
    try:
        for name in [
            f"tests.run_hydrochrono.{model}.signal_adapter",
            f"tests.run_hydrochrono.{model}.signals",
            f"tests.run_hydrochrono.{model}.adapter",
            f"tests.regression.run_hydrochrono.{model}.signal_adapter",
            f"tests.regression.run_hydrochrono.{model}.signals",
            f"tests.regression.run_hydrochrono.{model}.adapter",
        ]:
            try:
                mod = import_module(name)
                return mod.select_signal(h5_path)
            except Exception:
                continue
        # fall through to fs import below
    except Exception:
        pass
    # fallback to filesystem import
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location(f"adapter_{model}", str(adapter_path))
        if not spec or not spec.loader:
            raise ImportError("invalid spec")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    except Exception as e:
        raise ImportError(f"Could not import adapter for model='{model}': {e}")
    return mod.select_signal(h5_path)


def load_reference_txt(path: Path, value_col_index: int = 1):
    if not path.exists():
        raise FileNotFoundError(f"Reference file not found: {path}")
    times, vals = [], []
    splitter = re.compile(r"[\s,]+")
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            if s.startswith("#"):
                continue
            parts = splitter.split(s)
            # Collect numeric tokens on the line
            numeric = []
            for tok in parts:
                try:
                    numeric.append(float(tok))
                except Exception:
                    continue
            if len(numeric) >= 2:
                # Pick requested value column if available; otherwise default to second numeric token
                idx = value_col_index if value_col_index < len(numeric) else 1
                times.append(numeric[0])
                vals.append(numeric[idx])
    if not times:
        raise ValueError(f"Reference TXT has no numeric data rows: {path}")
    return np.asarray(times, dtype=float), np.asarray(vals, dtype=float)


def interpolate_to_reference(t_ref, y_ref, t_sim, y_sim):
    y_sim_interp = np.interp(t_ref, t_sim, y_sim)
    return t_ref, y_ref, y_sim_interp


def rms_relative_error(ref, pred) -> float:
    ref_rms = float(np.sqrt(np.mean(np.square(ref))))
    if ref_rms == 0.0:
        return float(np.sqrt(np.mean(np.square(pred))))
    return float(np.sqrt(np.mean(np.square(pred - ref))) / ref_rms)


def render_template_plot(time_ref, z_ref, time_sim, z_sim, h5_path: Path, ref_path: Path, y_label: str, show: bool, name_suffix: str | None = None) -> None:
    # Use the standardized regression comparison template for consistent visuals.
    # Import from tests/regression/compare_template.py (support both directory layouts)
    try:
        here = Path(__file__).resolve()
        # candidates: tests/regression (when running from tests/run_hydrochrono) or tests/regression (current/parent variants)
        candidates = [
            here.parents[2],              # tests/regression when file is under tests/run_hydrochrono
            here.parents[1],              # tests/regression when file is under tests/regression/run_hydrochrono
        ]
        template_dir = None
        for cand in candidates:
            if (cand / "compare_template.py").exists():
                template_dir = cand
                break
        if template_dir is None:
            template_dir = here.parents[1] / "regression"
        sys.path.insert(0, str(template_dir))
        from compare_template import create_comparison_plot  # type: ignore
    except Exception as e:
        print(f"Plot skipped: could not import compare_template: {e}")
        # Fallback to simple plot
        _render_simple_plot(time_ref, z_ref, time_sim, z_sim, h5_path, y_label, show)
        return

    # Prepare data arrays [time, value] on the same timebase for the template
    # Use simulation timebase for both to satisfy template metrics/correlation
    ref_on_sim = np.interp(time_sim, time_ref, z_ref)
    ref_data = np.column_stack((time_sim, ref_on_sim))
    sim_data = np.column_stack((time_sim, z_sim))

    # Output to a 'plots' subfolder next to the H5 (align with template behavior)
    plots_dir = h5_path.parent / "plots"
    plots_dir.mkdir(parents=True, exist_ok=True)

    # Name and labels per template conventions (infer simple name from folder)
    folder = h5_path.parent.parent.name  # e.g., 'decay' or 'decay_dt3'
    model = h5_path.parent.parent.parent.name.lower()  # model folder name, enforce lowercase
    # Determine default suffix for primary plot in multi-signal tests
    default_suffix = None
    if name_suffix is None:
        if model == "f3of" and "decay_dt3" in folder:
            default_suffix = "fore"
        if model == "rm3" and "decay" in folder:
            default_suffix = "float"
    # Base test name (lowercase)
    test_name = f"{model} {folder} test"
    # Apply suffix (explicit overrides default)
    suffix_to_use = str(name_suffix).lower() if name_suffix else default_suffix
    if suffix_to_use:
        test_name = f"{test_name} - {suffix_to_use}"
    # Sanitize spaces to underscores for consistent filenames
    test_name = test_name.replace(" ", "_")

    try:
        # Provide relative paths for metadata panels
        def rel_to_root(path: Path) -> str:
            try:
                project_root = Path(__file__).resolve().parents[2]
                return str(Path(path).resolve().relative_to(project_root))
            except Exception:
                return str(path)

        create_comparison_plot(
            ref_data,
            sim_data,
            test_name,
            str(plots_dir),
            ref_file_path=rel_to_root(ref_path),
            test_file_path=rel_to_root(h5_path),
            executable_path=None,
            y_label=y_label,
            executable_patterns=None,
        )
        if show:
            try:
                import matplotlib.pyplot as plt
                plt.show()
            except Exception:
                pass
    except Exception as e:
        print(f"Plot skipped: template rendering failed: {e}")
        # Fallback to simple plot
        _render_simple_plot(time_ref, z_ref, time_sim, z_sim, h5_path, y_label, show)


def _render_simple_plot(time_ref, z_ref, time_sim, z_sim, h5_path: Path, y_label: str, show: bool) -> None:
    try:
        import matplotlib.pyplot as plt
        plots_dir = h5_path.parent / "plots"
        plots_dir.mkdir(parents=True, exist_ok=True)
        plt.figure(figsize=(8, 4))
        plt.plot(time_ref, z_ref, label="reference", lw=2)
        plt.plot(time_sim, z_sim, label="simulation", lw=1)
        plt.xlabel("time [s]")
        plt.ylabel(y_label or "signal")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        out = plots_dir / "comparison_fallback.png"
        plt.savefig(str(out), dpi=150)
        if show:
            try:
                plt.show()
            except Exception:
                pass
        plt.close()
    except Exception:
        pass


def main() -> int:
    p = argparse.ArgumentParser(description="HydroChrono – Compare results vs reference")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--setup", help="Path to *.setup.yaml; discover outputs and pick newest HDF5")
    g.add_argument("--h5", help="Path to an output HDF5 file to compare")
    p.add_argument("--ref", help="Path to reference TXT (time, signal)")
    p.add_argument("--update-baseline", action="store_true", help="Overwrite the reference TXT with current sim output before comparing")
    p.add_argument("--tol", type=float, default=0.02, help="RMS relative error tolerance (default 0.02)")
    p.add_argument("--show", action="store_true", help="Display plots interactively (in addition to saving)")
    args = p.parse_args()

    if args.h5:
        h5_path = Path(args.h5)
        if args.ref:
            ref_path = Path(args.ref)
        else:
            model = h5_path.parent.parent.parent.name
            ref_path = Path(__file__).resolve().parent / model / "decay" / "expected" / f"hc_ref_{model}_decay.txt"
    else:
        setup_path = Path(args.setup)
        setup_yaml = read_yaml(setup_path)
        out_dir = discover_output_directory(setup_yaml, setup_path)
        h5_path = discover_h5_file(out_dir)
        if args.ref:
            ref_path = Path(args.ref)
        else:
            model = h5_path.parent.parent.parent.name
            ref_path = Path(__file__).resolve().parent / model / "decay" / "expected" / f"hc_ref_{model}_decay.txt"

    # Support multi-signal comparisons via adapter.select_signals if available (RM3 case)
    try:
        signals = None
        # Prefer signal_adapter.py, then signals.py, then adapter.py
        base = Path(__file__).resolve().parent
        model = h5_path.parent.parent.parent.name.lower()
        for fname in ["signal_adapter.py", "signals.py", "adapter.py"]:
            adapter_path = base / model / fname
            if adapter_path.exists():
                import importlib.util
                spec = importlib.util.spec_from_file_location(f"adapter_{model}", str(adapter_path))
                if spec and spec.loader:
                    mod = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(mod)
                    if hasattr(mod, "select_signals"):
                        signals = mod.select_signals(h5_path)
                break
    except Exception:
        signals = None

    t_sim, z_sim, y_label = select_signal_via_adapter(h5_path)

    # Decide which reference column to read (default is col 1: time + value)
    # F3OF DT2 reference layout: [time, base_surge, base_pitch, fore_pitch, aft_pitch]
    # Use base_pitch (index 2) when comparing pitch in DT2.
    ref_col_index = 1
    try:
        model_name = h5_path.parent.parent.parent.name.lower()
        test_folder = h5_path.parent.parent.name.lower()
        if model_name == "f3of" and "decay_dt2" in test_folder:
            ref_col_index = 2
        if model_name == "f3of" and "decay_dt3" in test_folder:
            ref_col_index = 3  # Flap Fore Pitch
    except Exception:
        pass

    # Optionally update baseline from current simulation output
    if args.update_baseline:
        ref_path.parent.mkdir(parents=True, exist_ok=True)
        with open(ref_path, "w", encoding="utf-8") as f:
            f.write("# time(s)  signal\n")
            for ti, zi in zip(t_sim, z_sim):
                f.write(f"{ti:.9f} {zi:.9e}\n")

    t_ref, z_ref = load_reference_txt(ref_path, value_col_index=ref_col_index)
    t_cmp, z_ref_cmp, z_sim_cmp = interpolate_to_reference(t_ref, z_ref, t_sim, z_sim)
    rms_rel = rms_relative_error(z_ref_cmp, z_sim_cmp)
    status = "PASS" if rms_rel <= args.tol else "FAIL"
    print(f"{status} | {h5_path} | N={len(t_cmp)} | RMSrel={rms_rel:.4f} | tol={args.tol:.4f}")

    # Always generate the plot using the standard template
    render_template_plot(t_ref, z_ref, t_sim, z_sim, h5_path, ref_path, y_label, args.show)

    # If additional signals are available (e.g., RM3 plate, F3OF DT3 aft), render a second plot
    # Gate multi-signal plotting to known multi-signal tests to avoid stray plots
    model_name = h5_path.parent.parent.parent.name.lower()
    test_folder = h5_path.parent.parent.name.lower()
    if signals and isinstance(signals, dict) and len(signals) >= 2 and (
        (model_name == "rm3" and "decay" in test_folder) or
        (model_name == "f3of" and "decay_dt3" in test_folder)
    ):
        # Try reading second column from reference if present
        # Reload reference parsing with 2 columns
        try:
            # Re-parse file into an array of numeric tokens per line
            times2, vals2 = [], []
            splitter = re.compile(r"[\s,]+")
            with open(ref_path, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    s = line.strip()
                    if not s or s.startswith("#"):
                        continue
                    parts = splitter.split(s)
                    numeric = []
                    for tok in parts:
                        try:
                            numeric.append(float(tok))
                        except Exception:
                            continue
                    # Choose aft (4th column) for DT3; otherwise third column
                    # choose reference column for secondary signal
                    idx = 2
                    if model_name == "f3of" and "decay_dt3" in test_folder:
                        idx = 4
                    if len(numeric) > idx:
                        times2.append(numeric[0])
                        vals2.append(numeric[idx])
            if times2 and vals2:
                # Pick secondary key deterministically
                model_name = h5_path.parent.parent.parent.name.lower()
                test_folder = h5_path.parent.parent.name.lower()
                if model_name == "f3of" and "decay_dt3" in test_folder and "aft" in signals:
                    chosen_key = "aft"
                else:
                    preferred_keys = ["plate", "aft", "float", "fore"]
                    chosen_key = None
                    for k in preferred_keys:
                        if k in signals and k != next(iter(signals.keys())):
                            chosen_key = k
                            break
                    if not chosen_key:
                        # pick any key different from the primary
                        primary_key = next(iter(signals.keys()))
                        for k in signals.keys():
                            if k != primary_key:
                                chosen_key = k
                                break
                t2, y2, ylabel2 = signals[chosen_key]
                # Render plot 2
                render_template_plot(
                    np.asarray(times2, dtype=float),
                    np.asarray(vals2, dtype=float),
                    t2,
                    y2,
                    h5_path,
                    ref_path,
                    ylabel2,
                    args.show,
                    name_suffix=chosen_key.capitalize(),
                )
        except Exception:
            pass

    if status != "PASS":
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


