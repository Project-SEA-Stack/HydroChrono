---
layout: page
title: "HydroChrono: Quick Tour"
permalink: /about/
---

<p align="center"><img src="{{ site.baseurl }}/assets/img/hydrochrono_banner.png" width="80%" /></p>

HydroChrono (Hydrodynamics for Project Chrono) is a hydrodynamics simulation toolkit built on [Project Chrono](https://projectchrono.org/). It is designed for simulating wave energy converters (WECs) and other complex ocean systems, and is **100% free and open‑source** end‑to‑end — no proprietary dependencies required.

It uses Boundary Element Method (BEM) hydrodynamic coefficients (e.g., from Capytaine) to model added mass, radiation damping, and wave excitation, and runs time‑domain simulations via the Cummins equation on Chrono multibody models. Results are exported to portable HDF5 for analysis and verification.

## Quick start (CLI)

After downloading a release, open a terminal in the app folder and run:

```powershell
run_hydrochrono.exe -h     # help
run_hydrochrono.exe -i     # info/banner

# Run a case by directory (auto-detects setup)
run_hydrochrono.exe .\cases\my_model\

# Or run directly from a setup file
run_hydrochrono.exe .\cases\my_model\my_model.setup.yaml

# Some useful options
run_hydrochrono.exe .\cases\my_model\ --nogui --quiet --log
```

<p align="center"><img src="{{ site.baseurl }}/assets/img/cli_example.png" width="75%" /></p>

### GUI Example

Use the GUI to visually inspect the assembled multibody system (bodies, joints, actuators) and verify that YAML inputs are wired correctly. Use the `--nogui` option to disable visualization straight from CLI, or change the settings in the `*.simulation.yaml` file.

<p align="center"><img src="{{ site.baseurl }}/assets/img/gui_example.png" width="40%" /></p>

## YAML-based UI

Describe your system in text files that can be versioned and automated:

```
cases/my_model/
  my_model.setup.yaml       # references the files below (recommended)
  my_model.model.yaml       # bodies, joints, actuators
  my_model.simulation.yaml  # time step, duration, GUI, waves
  my_model.hydro.yaml       # hydrodynamics
```

- `my_model.setup.yaml` — orchestrates which inputs to run
- `my_model.model.yaml` — Chrono multibody system (bodies, joints, constraints, actuators)
- `my_model.simulation.yaml` — time step, duration, output options, GUI flags
- `my_model.hydro.yaml` — BEMIO `.h5` path and hydrodynamics mapping/wave inputs

Run with either the folder path (auto‑detects `*.setup.yaml`) or the setup file directly:

```powershell
run_hydrochrono.exe .\cases\my_model\
run_hydrochrono.exe .\cases\my_model\my_model.setup.yaml
```

<p align="center"><img src="{{ site.baseurl }}/assets/img/yaml_example.png" width="25%" /></p>

## HDF5 outputs (portable)

Simulations produce a single `.h5` file with time series and model results. Typical datasets include:

- Body position (XYZ): `/results/model/bodies/<body_name>/position`
- Body orientation (roll, pitch, yaw): `/results/model/bodies/<body_name>/orientation_xyz`
- Translational spring–dampers: `/results/model/tsdas/<actuator_name>/...`
- Rotational spring–dampers: `/results/model/rsdas/<actuator_name>/...`

View and plot with HDFView or common Python tools.

<p align="center"><img src="{{ site.baseurl }}/assets/img/h5_example.png" width="75%" /></p>

## Verification snapshot

Comparison from the OSWEC decay test used in the regression suite.

<p align="center"><img src="{{ site.baseurl }}/assets/img/oswec_decay_test_comparison.png" width="66%" /></p>

## Developers

Full build and contribution docs: [Developer documentation]({{ site.baseurl }}/developer_docs/build_instructions)

## Papers

- Ogden, 2023 — HydroChrono background, theory, and implementation details: [PDF]({{ site.baseurl }}/assets/papers/Ogden2023%20-%20HydroChrono.pdf)
- Ogden, 2025 — Automated design exploration with meshing, Capytaine, and HydroChrono in the loop: [PDF]({{ site.baseurl }}/assets/papers/Ogden2025%20-%20Automated%20Design%20Exploration%20of%20TALOS%20Using%20TOP-WEC.pdf)

<p align="center">
  <img src="{{ site.baseurl }}/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>