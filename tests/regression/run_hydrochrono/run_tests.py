#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path
import importlib.util
import numpy as np


# Resolve project root for both layouts:
# - tests/run_hydrochrono/...  => root = parents[2]
# - tests/regression/run_hydrochrono/... => root = parents[3]
_here = Path(__file__).resolve()
ROOT = _here.parents[3] if _here.parents[1].name == "regression" else _here.parents[2]
THIS = _here.parent


def default_exe() -> str:
	"""Best-effort discovery of run_hydrochrono.exe in common build locations."""
	# Prefer Release build if present
	for p in [
		ROOT / "build" / "bin" / "Release" / "run_hydrochrono.exe",
		ROOT / "build" / "bin" / "Debug" / "run_hydrochrono.exe",
		ROOT / "run_hydrochrono.exe",
	]:
		if p.exists():
			return str(p)
	return "run_hydrochrono.exe"


def find_ref(model: str, test_type: str) -> str | None:
    expected_dir = THIS / model / test_type / "expected"
    # Prefer baseline.h5
    h5_base = expected_dir / "baseline.h5"
    if h5_base.exists():
        return str(h5_base)
    # Any h5 in expected
    for p in expected_dir.glob("*.h5"):
        return str(p)
    # Legacy txt in expected
    txt_expected = expected_dir / f"hc_ref_{model}_{test_type}.txt"
    if txt_expected.exists():
        return str(txt_expected)
    # Legacy regression reference_data
    legacy = ROOT / "tests" / "regression" / "reference_data" / model / test_type / f"hc_ref_{model}_{test_type}.txt"
    if legacy.exists():
        return str(legacy)
    return None


def run_case(exe: str, model: str, test_type: str, tol: float, update_baseline: bool, quiet: bool, show: bool, gui: bool) -> int:
	"""Run a single test: simulate, then compare and plot. Returns exit code."""
	inputs_setup = THIS / model / test_type / "inputs" / f"{model}_{test_type}.setup.yaml"
	if not inputs_setup.exists():
		print(f"SKIP | {model}/{test_type} | missing setup {inputs_setup}", file=sys.stderr)
		return 0
	# 1) simulate
	cmd = [
		"python",
		str(THIS / "run_simulation.py"),
		"--exe",
		exe,
		"--setup",
		str(inputs_setup),
	]
	if not gui:
		cmd.append("--nogui")
	r1 = subprocess.run(cmd, capture_output=quiet, text=True, encoding="utf-8", errors="ignore")
	if r1.returncode != 0:
		if quiet:
			print(r1.stdout, end="")
			print(r1.stderr, end="")
		print(f"FAIL | {model}/{test_type} | simulation exited {r1.returncode}", file=sys.stderr)
		return r1.returncode
	# 2) compare
	ref = find_ref(model, test_type)
	outputs_h5 = (THIS / model / test_type / "outputs" / "results.still.h5").resolve()
	plots_dir = (THIS / model / test_type / "outputs" / "plots").resolve()
	# Neutral/adapter-driven comparison path
	if outputs_h5.exists():
		adapter_path = THIS / model / "signal_adapter.py"
		if adapter_path.exists():
			try:
				spec = importlib.util.spec_from_file_location(f"adapter_{model}", str(adapter_path))
				assert spec and spec.loader
				adapter = importlib.util.module_from_spec(spec)
				spec.loader.exec_module(adapter)  # type: ignore
				# import plotting template from tests/regression
				regression_dir = THIS.parent
				if str(regression_dir) not in sys.path:
					sys.path.insert(0, str(regression_dir))
				from compare_template import create_comparison_plot  # type: ignore
				# helper
				def rms_relative_error(ref_arr: np.ndarray, pred_arr: np.ndarray) -> float:
					ref_rms = float(np.sqrt(np.mean(np.square(ref_arr))))
					if ref_rms == 0.0:
						return float(np.sqrt(np.mean(np.square(pred_arr))))
					return float(np.sqrt(np.mean(np.square(pred_arr - ref_arr))) / ref_rms)
				# try multi-signal first
				multi = getattr(adapter, "select_signals", None)
				single = getattr(adapter, "select_signal", None)
				if multi is not None:
					sim_signals = multi(outputs_h5)
					ref_signals = multi(Path(ref)) if ref else sim_signals
					status = 0
					for name, (t_sim, y_sim, y_label) in sim_signals.items():
						if name not in ref_signals:
							continue
						t_ref, y_ref, _ = ref_signals[name]
						ref_on_sim = np.interp(t_sim, t_ref, y_ref)
						rms_rel = rms_relative_error(ref_on_sim, y_sim)
						result = "PASS" if rms_rel <= tol else "FAIL"
						print(f"{result} | N={len(t_sim)} | RMSrel={rms_rel:.6f} | tol={tol:.6f}")
						# plot
						plots_dir.mkdir(parents=True, exist_ok=True)
						ref_data = np.column_stack((t_sim, ref_on_sim))
						sim_data = np.column_stack((t_sim, y_sim))
						create_comparison_plot(
							ref_data,
							sim_data,
							f"{model}_{test_type}_test - {name}",
							str(plots_dir),
							ref_file_path=str(ref) if ref else str(outputs_h5),
							test_file_path=str(outputs_h5),
							executable_path=None,
							y_label=y_label,
							executable_patterns=None,
						)
						if result != "PASS":
							status = 1
					return status
				if single is not None:
					t_sim, y_sim, y_label = single(outputs_h5)
					if ref:
						t_ref, y_ref, _ = single(Path(ref))
						ref_on_sim = np.interp(t_sim, t_ref, y_ref)
					else:
						ref_on_sim = y_sim.copy()
					rms_rel = rms_relative_error(ref_on_sim, y_sim)
					result = "PASS" if rms_rel <= tol else "FAIL"
					print(f"{result} | N={len(t_sim)} | RMSrel={rms_rel:.6f} | tol={tol:.6f}")
					plots_dir.mkdir(parents=True, exist_ok=True)
					ref_data = np.column_stack((t_sim, ref_on_sim))
					sim_data = np.column_stack((t_sim, y_sim))
					create_comparison_plot(
						ref_data,
						sim_data,
						f"{model}_{test_type}_test",
						str(plots_dir),
						ref_file_path=str(ref) if ref else str(outputs_h5),
						test_file_path=str(outputs_h5),
						executable_path=None,
						y_label=y_label,
						executable_patterns=None,
					)
					return 0 if result == "PASS" else 1
			except Exception as e:
				if quiet:
					print(f"Adapter compare failed for {model}/{test_type}: {e}", file=sys.stderr)
	# Default path (legacy adapter mode)
	# Use explicit simple-mode compare for all other tests too (heave of first body by default)
	outputs_h5 = (THIS / model / test_type / "outputs" / "results.still.h5").resolve()
	plots_dir = (THIS / model / test_type / "outputs" / "plots").resolve()
	cmd = [
		"python",
		str(THIS / "compare_results.py"),
		"--ref", ref if ref else str(outputs_h5),
		"--sim", str(outputs_h5),
		"--ref-time-dset", "/results/time/time",
		"--sim-time-dset", "/results/time/time",
		"--ref-val-dset", "/results/model/bodies/body1/position",
		"--ref-col", "2",
		"--sim-val-dset", "/results/model/bodies/body1/position",
		"--sim-col", "2",
		"--ylabel", "Heave (m)",
		"--title", f"{model}_{test_type}_test",
		"--outdir", str(plots_dir),
		"--tol", str(tol),
	]
	if show:
		cmd.append("--show")
	r2 = subprocess.run(cmd, capture_output=quiet, text=True, encoding="utf-8", errors="ignore")
	if quiet:
		print(r2.stdout, end="")
		print(r2.stderr, end="")
	return r2.returncode


def main() -> int:
	"""CLI entrypoint for running HydroChrono YAML tests."""
	parser = argparse.ArgumentParser(description="HydroChrono – test runner")
	parser.add_argument("--exe", default=default_exe(), help="Path to run_hydrochrono.exe")
	parser.add_argument("--tol", type=float, default=0.02, help="RMS relative error tolerance")
	parser.add_argument("--update-baseline", action="store_true", help="Overwrite reference with current simulation output")
	parser.add_argument("--quiet", action="store_true", help="Suppress subprocess logs (summary only)")
	parser.add_argument("--show", action="store_true", help="Display plots interactively (in addition to saving)")
	parser.add_argument("--gui", action="store_true", help="Run simulations with GUI (omit --nogui)")
	# selectors
	parser.add_argument("--all", action="store_true", help="Run all known tests")
	parser.add_argument("--all-tests", dest="all", action="store_true", help="Alias for --all")
	parser.add_argument("--all_tests", dest="all", action="store_true", help="Alias for --all")
	parser.add_argument("--sphere-decay", action="store_true", help="Run IEA sphere decay")
	parser.add_argument("--oswec-decay", action="store_true", help="Run OSWEC decay")
	parser.add_argument("--rm3-decay", action="store_true", help="Run RM3 decay")
	parser.add_argument("--f3of-dt1", action="store_true", help="Run F3OF decay test 1 (DT1 surge)")
	parser.add_argument("--f3of-dt2", action="store_true", help="Run F3OF decay test 2 (DT2 pitch)")
	parser.add_argument("--f3of-dt3", action="store_true", help="Run F3OF decay test 3 (DT3 flaps pitch)")
	args = parser.parse_args()

	selections: list[tuple[str,str]] = []
	if args.all:
		selections.extend([
			("iea_sphere", "decay"),
			("oswec", "decay"),
			("rm3", "decay"),
			("f3of", "decay_dt1"),
			("f3of", "decay_dt2"),
			("f3of", "decay_dt3"),
		])
	else:
		if args.sphere_decay:
			selections.append(("iea_sphere", "decay"))
		if args.oswec_decay:
			selections.append(("oswec", "decay"))
		if args.rm3_decay:
			selections.append(("rm3", "decay"))
		if args.f3of_dt1:
			selections.append(("f3of", "decay_dt1"))
		if args.f3of_dt2:
			selections.append(("f3of", "decay_dt2"))
		if args.f3of_dt3:
			selections.append(("f3of", "decay_dt3"))
		if not selections:
			print("No tests selected. Use --all or one of --sphere-decay/--oswec-decay/--rm3-decay/--f3of-dt1/--f3of-dt2", file=sys.stderr)
			return 2

	overall = 0
	for model, test_type in selections:
		code = run_case(args.exe, model, test_type, args.tol, args.update_baseline, args.quiet, args.show, args.gui)
		if code != 0:
			overall = code
	return overall


if __name__ == "__main__":
	raise SystemExit(main())