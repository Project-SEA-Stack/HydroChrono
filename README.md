# HydroChrono

**HydroChrono** is an emerging hydrodynamics simulation tool designed to model complex ocean systems. Seamlessly integrated with the [Project Chrono](https://projectchrono.org/) physics engine, it offers a powerful C++ API for a wide range of simulations.

## Capabilities

HydroChrono extends the versatility of Project Chrono to hydrodynamic applications, enabling you to simulate multibody wave energy converters (WECs) and floating offshore wind turbines (FOWTs) with Chrono::FEA for flexible bodies.

## Get Started

Visit the [HydroChrono website](https://nrel.github.io/HydroChrono/) for information about the underlying theory, build instructions, and usage details. Currently, HydroChrono is available as a source build, offering customization and optimization opportunities. A binary distribution with Python wrapper is in development and coming soon.

## Limitations

- Currently only supports first-order linear potential flow forces.
- Hydrodynamic forces are currently limited to rigid bodies.

## Vision and Future Goals

- Expand hydrodynamics to incorporate nonlinear and 2nd order forces.
- Integrate advanced simulation features using Project Chrono's DEM and FEA modules.
- Support seamless transitioning from potential flow to CFD and SPH for detailed FSI analysis.
- Develop a Python API to broaden accessibility and ease of use.
- Foster and support open-source collaboration in the research community.

## Build Instructions

### Prerequisites

- **Visual Studio 2022** with C++ toolchain
- **CMake 3.18+**
- **Project Chrono** (built from source; tested v9.0.0–v9.0.1)
- **HDF5 1.10.8+**
- **Eigen 3.4+** (header-only unzip is fine)
- **Irrlicht 1.8.x** (optional but required for GUI helper)
- **Python 3.8+** (only for docs/tools)

### Building from Source (using `build-config.json` + script)

1. **Clone the repository**
   ```powershell
   git clone https://github.com/NREL/HydroChrono.git
   cd HydroChrono
   ```

2. **Create your local config from the example**
   ```powershell
   copy build-config.example.json build-config.json
   ```

3. **Edit `build-config.json`** and set paths for your machine

   **Example:**
   ```json
   {
     "ChronoDir": "C:/path/to/chrono/build/cmake",
     "Hdf5Dir": "C:/path/to/hdf5/share/cmake",
     "EigenDir": "C:/path/to/eigen-3.4.0",
     "IrrlichtDir": "C:/path/to/irrlicht-1.8.4",
     "PythonRoot": "C:/Users/<you>/.conda/envs/<env>",
     "CMakeModulePath": "C:/path/to/chrono/build/cmake"
   }
   ```

4. **Build (from repo root)**

   - Clean configure + build (default `Release`):
     ```powershell
     .\quick-build.ps1 -Clean
     ```
   - Build a different configuration (e.g., `Debug`):
     ```powershell
     .\quick-build.ps1 -BuildType Debug
     ```

4. **Post-Build Steps**

Copy the following DLL files from your Chrono build directory to your build directory's `build/bin` folder:
- ChronoEngine.dll
- ChronoEngine_irrlicht.dll (if using Irrlicht)
- Irrlicht.dll (if using Irrlicht)

5. **Run the tests**
   ```powershell
   cd build
   ctest -C Release
   ```

---

### Clean Build

```powershell
# From the project root
Remove-Item -Recurse -Force build
# then reconfigure/build
.\quick-build.ps1 -Clean
```

For more detailed build instructions, including Visual Studio setup and running demos, see the [developer documentation](https://nrel.github.io/HydroChrono/developer_docs/build_instructions.html).
