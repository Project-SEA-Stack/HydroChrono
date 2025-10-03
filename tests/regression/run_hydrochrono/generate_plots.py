#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


# Resolve project root for both layouts
_here = Path(__file__).resolve()
ROOT = _here.parents[3] if _here.parents[1].name == "regression" else _here.parents[2]
THIS = _here.parent


def find_ref(model: str, test_type: str) -> str | None:
	"""Find reference TXT for (model, test_type), preferring the new layout."""
	# Prefer new run_hydrochrono expected dir
	preferred = THIS / model / test_type / "expected" / f"hc_ref_{model}_{test_type}.txt"
	if preferred.exists():
		return str(preferred)
	# Fallback to legacy regression reference_data
	legacy = ROOT / "tests" / "regression" / "reference_data" / model / test_type / f"hc_ref_{model}_{test_type}.txt"
	if legacy.exists():
		return str(legacy)
	return None


def compare_only(model: str, test_type: str, show: bool, quiet: bool) -> int:
	"""Run comparison+plot only, discovering the latest HDF5 via the setup YAML."""
	inputs_setup = THIS / model / test_type / "inputs" / f"{model}_{test_type}.setup.yaml"
	if not inputs_setup.exists():
		print(f"SKIP | {model}/{test_type} | missing setup {inputs_setup}", file=sys.stderr)
		return 0
	cmd = [
		"python",
		str(THIS / "compare_results.py"),
		"--setup",
		str(inputs_setup),
	]
	ref = find_ref(model, test_type)
	if ref:
		cmd.extend(["--ref", ref])
	if show:
		cmd.append("--show")
	res = subprocess.run(cmd, capture_output=quiet, text=True, encoding="utf-8", errors="ignore")
	if quiet:
		print(res.stdout, end="")
		print(res.stderr, end="")
	return res.returncode


def main() -> int:
	"""CLI to regenerate plots from existing outputs (no simulation)."""
	parser = argparse.ArgumentParser(description="HydroChrono – regenerate comparison plots from existing outputs")
	parser.add_argument("--all", action="store_true", help="Generate plots for all tests")
	parser.add_argument("--show", action="store_true", help="Display plots interactively (default off)")
	parser.add_argument("--quiet", action="store_true", help="Suppress subprocess logs (summary only)")
	# Model selectors (broad)
	parser.add_argument("--f3of", action="store_true", help="Generate plots for all F3OF decay tests (dt1, dt2, dt3)")
	parser.add_argument("--sphere", action="store_true", help="Generate plots for IEA sphere decay")
	parser.add_argument("--oswec", action="store_true", help="Generate plots for OSWEC decay")
	parser.add_argument("--rm3", action="store_true", help="Generate plots for RM3 decay")
	# Fine-grained F3OF selectors
	parser.add_argument("--f3of-dt1", action="store_true", help="Generate plots for F3OF DT1")
	parser.add_argument("--f3of-dt2", action="store_true", help="Generate plots for F3OF DT2")
	parser.add_argument("--f3of-dt3", action="store_true", help="Generate plots for F3OF DT3")
	args = parser.parse_args()

	selections: list[tuple[str, str]] = []
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
		if args.sphere:
			selections.append(("iea_sphere", "decay"))
		if args.oswec:
			selections.append(("oswec", "decay"))
		if args.rm3:
			selections.append(("rm3", "decay"))
		if args.f3of:
			selections.extend([
				("f3of", "decay_dt1"),
				("f3of", "decay_dt2"),
				("f3of", "decay_dt3"),
			])
		if args.f3of_dt1:
			selections.append(("f3of", "decay_dt1"))
		if args.f3of_dt2:
			selections.append(("f3of", "decay_dt2"))
		if args.f3of_dt3:
			selections.append(("f3of", "decay_dt3"))
		if not selections:
			print("No selections. Use --all or one of --sphere/--oswec/--rm3/--f3of or --f3of-dt1/--f3of-dt2/--f3of-dt3", file=sys.stderr)
			return 2

	overall = 0
	for model, test_type in selections:
		code = compare_only(model, test_type, args.show, args.quiet)
		if code != 0:
			overall = code
	return overall


if __name__ == "__main__":
	raise SystemExit(main())


