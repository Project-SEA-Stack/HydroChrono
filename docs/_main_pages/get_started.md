---
layout: page
title: Quick Start
---

## 1) Download

- Get the latest Windows binaries from the [Releases]({{ site.baseurl }}/get_started/#) page (or the repo Releases page).

## 2) Run a demo

Open a terminal in the unzipped folder and try a built-in case:

```powershell
run_hydrochrono.exe demos/yaml/rm3
```

## 3) Run from a setup file or directory

HydroChrono loads a case from a directory or a `*.setup.yaml` file and prints a clean, structured summary to the console.

```powershell
run_hydrochrono.exe <input_directory>
run_hydrochrono.exe <model.setup.yaml>
```

## 4) Useful options

- `--nogui`: disable visualization
- `--log`: write a detailed file log to `<input_dir>/logs/`
- `--quiet`: suppress console output (pair with `--log`)
- `--debug` / `--trace`: increase CLI verbosity

Examples:

```powershell
run_hydrochrono.exe demos/yaml/rm3 --log
run_hydrochrono.exe demos/yaml/rm3 --quiet --log
run_hydrochrono.exe demos/yaml/rm3 --debug
```

## 5) Run your own case

Point to your folder (auto-detects `*.setup.yaml`) or the explicit setup file:

```powershell
run_hydrochrono.exe .\cases\my_model\
run_hydrochrono.exe .\cases\my_model\my_model.setup.yaml
```

### Logging details

See the full logging/CLI output guide: [Logging]({{ site.baseurl }}/logging/)

### Create a new model

Follow the minimal inputs guide: [Create a New Model]({{ site.baseurl }}/new_model/)

<p align="center">
  <img src="https://nrel.github.io/HydroChrono/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>