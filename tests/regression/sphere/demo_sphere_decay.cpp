#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>

#include <chrono>      // std::chrono::high_resolution_clock::now
#include <filesystem>  // c++17 only
#include <iomanip>     // std::setprecision
#include <vector>      // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;

// C++17 filesystem helper to ensure a directory exists
void ensure_directory_exists(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::cout << "Path " << std::filesystem::absolute(path) << " does not exist, creating it now..." << std::endl;
        std::filesystem::create_directory(path);
    }
}

// usage: ./sphere_deca.exe [DATADIR] [--nogui]
//
// If no argument is given user can set HYDROCHRONO_DATA_DIR
// environment variable to give the data_directory.
//
int main(int argc, char* argv[]) {
    std::cout << "Chrono version: " << CHRONO_VERSION << "\n\n";

    SetChronoDataPath(CHRONO_DATA_DIR);

    if (hydroc::SetInitialEnvironment(argc, argv) != 0) {
        return 1;
    }

    // Check if --nogui option is set as 2nd argument
    bool visualizationOn = true;
    if (argc > 2 && std::string("--nogui").compare(argv[2]) == 0) {
        visualizationOn = false;
    }

    // Get model file names
    std::filesystem::path DATADIR(hydroc::getDataDir());

    auto body1_meshfame =
        (DATADIR / "sphere" / "geometry" / "oes_task10_sphere.obj").lexically_normal().generic_string();
    auto h5fname = (DATADIR / "sphere" / "hydroData" / "sphere.h5").lexically_normal().generic_string();

    // system/solver settings
    ChSystemNSC system;

    system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));

    double timestep = 0.015;
    system.SetSolverType(ChSolver::Type::GMRES);
    system.GetSolver()->AsIterative()->SetMaxIterations(
        300);  // the higher, the easier to keep the constraints satisfied.
    ChRealtimeStepTimer realtime_timer;
    double simulationDuration = 40.0;

    // Create user interface
    std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);

    hydroc::gui::UI& ui = *pui.get();

    bool profilingOn = true;
    bool saveDataOn  = true;

    // Output timeseries
    std::vector<double> time_vector;
    std::vector<double> heave_position;

    // set up body from a mesh
    std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
    std::shared_ptr<ChBody> sphereBody = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body1_meshfame,                                                              // file name
        1000,                                                                        // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // do not collide
    );

    // define the body's initial conditions
    sphereBody->SetName("body1");  // must set body name correctly! (must match .h5 file)
    sphereBody->SetPos(ChVector3d(0, 0, -1));
    sphereBody->SetMass(261.8e3);

    // Create a visualization material
    auto cadet_blue = chrono_types::make_shared<ChVisualMaterial>();
    cadet_blue->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.1f));
    sphereBody->GetVisualShape(0)->SetMaterial(0, cadet_blue);

    system.Add(sphereBody);

    // define wave parameters (not used in this demo)
    // Todo define a way to use TestHydro without hydro_inputs/waves
    // HydroInputs my_hydro_inputs;
    // my_hydro_inputs.mode = WaveMode::noWaveCIC;
    // my_hydro_inputs.regular_wave_amplitude = 0.022;
    // my_hydro_inputs.regular_wave_omega = 2.10;

    auto default_dont_add_waves = std::make_shared<NoWave>(1);

    // attach hydrodynamic forces to body
    std::vector<std::shared_ptr<ChBody>> bodies;
    bodies.push_back(sphereBody);

    TestHydro hydro_forces(bodies, h5fname);
    hydro_forces.AddWaves(default_dont_add_waves);

    // for profiling
    auto start = std::chrono::high_resolution_clock::now();

    // main simulation loop
    ui.Init(&system, "Sphere - Decay Test");

    while (system.GetChTime() <= simulationDuration) {
        if (ui.IsRunning(timestep) == false) break;

        if (ui.simulationStarted) {
            system.DoStepDynamics(timestep);

            // append data to output vector
            time_vector.push_back(system.GetChTime());
            heave_position.push_back(sphereBody->GetPos().z());
        }
    }

    // for profiling
    auto end          = std::chrono::high_resolution_clock::now();
    unsigned duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Create results directory if it doesn't exist
    std::filesystem::path results_dir("./results");
    ensure_directory_exists(results_dir);

    if (profilingOn) {
        std::ofstream profilingFile(results_dir / "CHRONO_SPHERE_DECAY_HEAVE_DURATION.txt");
        if (profilingFile.is_open()) {
            profilingFile << duration << " ms\n";
            profilingFile.close();
        } else {
            std::cout << "Error: Could not open profiling file for writing." << std::endl;
        }
    }

    if (saveDataOn) {
        std::ofstream outputFile(results_dir / "CHRONO_SPHERE_DECAY_HEAVE.txt");
        if (outputFile.is_open()) {
            outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(12)
                       << "Heave (m)"
                       //<< std::right << std::setw(18) << "Heave Vel (m/s)"
                       //<< std::right << std::setw(18) << "Heave Force (N)"
                       << std::endl;
            for (int i = 0; i < time_vector.size(); ++i)
                outputFile << std::left << std::setw(12) << std::setprecision(6) << std::fixed << time_vector[i]
                           << std::right << std::setw(12) << std::setprecision(6) << std::fixed << heave_position[i]
                           << std::endl;
            outputFile.close();
        } else {
            std::cout << "Error: Could not open output file for writing." << std::endl;
            return 1; // Return an error code
        }
    }

    std::cout << "Simulation finished." << std::endl;
    return 0;
}