---
layout: page
title: Create a New Model (Inputs)
permalink: /new_model/
---

This guide shows a minimal folder structure and YAML templates to set up a new HydroChrono model. It assumes you already have your geometry mesh and a hydrodynamic BEMIO `.h5` file.

## 1) Folder layout

```
my_model/
  my_model.setup.yaml
  my_model.model.yaml
  my_model.simulation.yaml
  my_model.hydro.yaml
  assets/
    hydroData/
      my_model.h5          # your BEMIO hydrodynamics file (from Capytaine, etc.)
    geometry/              # optional visualization meshes
  outputs/                 # created by the runner
```

## 2) Setup file (entry point)

```yaml
model_file: my_model.model.yaml
simulation_file: my_model.simulation.yaml
hydro_file: my_model.hydro.yaml

# Output directory is authoritative for the runner. Relative to this setup file.
output_directory: ./outputs
```

## 3) Model file (Chrono multibody)

```yaml
chrono-version: 9.0

model:
  name: my_model
  angle_degrees: false

  data_path:
    type: RELATIVE
    root: "."

  bodies:
    - name: body1
      location: [0, 0, 0]
      mass: 1000
      fixed: false
      inertia:
        moments: [100, 100, 100]
        products: [0, 0, 0]
      com:
        location: [0, 0, 0]
        orientation: [0, 0, 0]
      visualization:
        shapes:
          - type: SPHERE
            radius: 1.0
            location: [0, 0, 0]
            color: [0.2, 0.4, 0.8]

    - name: ground
      location: [0, 0, 0]
      mass: 1
      fixed: true
      inertia:
        moments: [1, 1, 1]
        products: [0, 0, 0]
      com:
        location: [0, 0, 0]
        orientation: [0, 0, 0]

  joints:
    - name: heave_constraint
      type: PRISMATIC
      body1: ground
      body2: body1
      location: [0, 0, 0]
      axis: [0, 0, 1]

  tsdas:
    - name: heave_tsda
      type: TSDA
      body1: ground
      body2: body1
      point1: [0, 0, 0]
      point2: [0, 0, 0]
      spring_coefficient: 0.0
      damping_coefficient: 0.0
      free_length: 0.0
      visualization:
        type: SPRING
        radius: 0.05
        resolution: 60
        turns: 12
```

## 4) Simulation file

```yaml
chrono-version: 9.0

simulation:
  contact_method: SMC
  time_step: 0.01
  end_time: 10.0
  enforce_realtime: false
  gravity: [0, 0, -9.81]

  integrator:
    type: HHT
    rel_tolerance: 1e-4
    abs_tolerance_states: 1e-4
    abs_tolerance_multipliers: 1e2
    max_iterations: 50

  solver:
    type: GMRES
    max_iterations: 100
    tolerance: 1e-8

  visualization:
    type: MODEL_FILE
    render_fps: 120
    enable_shadows: true
    camera:
      vertical: Z
```

## 5) Hydrodynamics file

```yaml
hydrodynamics:
  bodies:
    - name: body1
      h5_file: ./assets/hydroData/my_model.h5

  waves:
    type: still            # or regular/irregular, etc.
```

## 6) Run it

From the parent directory containing `my_model/`:

```powershell
run_hydrochrono.exe .\my_model\
```

Or directly with the setup file:

```powershell
run_hydrochrono.exe .\my_model\my_model.setup.yaml
```

Outputs are written to `my_model/outputs/`.


<p align="center">
  <img src="https://nrel.github.io/HydroChrono/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>