#!/usr/bin/env python3
"""Run a single HydroChrono simulation from a setup YAML.

This thin wrapper only executes the binary; it does not compare or plot.
"""
import argparse
import subprocess
import sys
from pathlib import Path


def main() -> int:
	"""CLI entrypoint: invoke run_hydrochrono.exe with the given setup YAML."""
	p = argparse.ArgumentParser(description="HydroChrono – Run simulation (no compare)")
	p.add_argument("--exe", required=True, help="Path to run_hydrochrono.exe")
	p.add_argument("--setup", required=True, help="Path to *.setup.yaml")
	p.add_argument("--nogui", action="store_true", help="Pass --nogui to the runner")
	args = p.parse_args()

	# Basic validation for clearer errors in CI/CLI usage
	exe_path = Path(args.exe)
	setup_path = Path(args.setup)
	if not exe_path.exists():
		print(f"Executable not found: {exe_path}", file=sys.stderr)
		return 2
	if not setup_path.exists():
		print(f"Setup YAML not found: {setup_path}", file=sys.stderr)
		return 2

	cmd = [str(exe_path), str(setup_path)]
	if args.nogui:
		cmd.append("--nogui")

	proc = subprocess.run(cmd)
	return proc.returncode


if __name__ == "__main__":
	raise SystemExit(main())


