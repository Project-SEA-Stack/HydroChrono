HydroChrono – YAML Regression Tests (quick guide)

### Install
- `pip install -r tests/run_hydrochrono/requirements.txt`

### Run tests (simulate + compare + plots)
- All models: `python tests/run_hydrochrono/run_tests.py --all`
- Single test: `python tests/run_hydrochrono/run_tests.py --f3of-dt3 --show`
- Optional: add `--gui` for a simulation window

### Plot-only (no simulation)
- All: `python tests/run_hydrochrono/generate_plots.py --all --show`
- By model: `--f3of`, `--sphere`, `--oswec`, `--rm3`

### Folder layout (per test)
- `tests/run_hydrochrono/<model>/<test_type>/`
  - `inputs/` — `*.setup.yaml`, `*.model.yaml`, `*.simulation.yaml`, `*.hydro.yaml`
  - `expected/` — `hc_ref_<model>_<test_type>.txt`
  - `outputs/` — HDF5 + `outputs/plots/*.png`

### References and adapters
- References are simple TXT files (time + value). Some tests have extra columns (e.g., RM3, F3OF DT3).
- Per‑model `signal_adapter.py` defines how to read the right signal(s) from HDF5.

### Baselines
- Update a baseline from the latest sim:
  - `python tests/run_hydrochrono/compare_results.py --setup <path-to-setup.yaml> --update-baseline`

