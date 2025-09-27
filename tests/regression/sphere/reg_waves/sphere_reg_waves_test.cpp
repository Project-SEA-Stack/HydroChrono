#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>

#include <chrono>  // std::chrono::high_resolution_clock::now
#include <filesystem>
#include <iomanip>  // std::setprecision
#include <vector>   // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;

// usage: ./<test>.exe [DATADIR] [--nogui]
//
// If no argument is given user can set HYDROCHRONO_DATA_DIR
// environment variable to give the data_directory.
//
int main(int argc, char* argv[]) {
    SetChronoDataPath(CHRONO_DATA_DIR);

    std::vector<double> task10_wave_amps_0005 = {0.044, 0.078, 0.095, 0.123, 0.177, 0.24, 0.314, 0.397, 0.491, 0.594};
    std::vector<double> task10_wave_amps_002  = {0.177, 0.314, 0.380, 0.491, 0.706, 0.961, 1.256, 1.589, 1.962, 2.374};
    std::vector<double> task10_wave_amps      = task10_wave_amps_002;

    double task10_wave_omegas[]    = {2.094395102, 1.570796327, 1.427996661, 1.256637061, 1.047197551,
                                      0.897597901, 0.785398163, 0.698131701, 0.628318531, 0.571198664};
    double task10_damping_coeffs[] = {398736.034, 118149.758, 90080.857,  161048.558, 322292.419,
                                      479668.979, 633979.761, 784083.286, 932117.647, 1077123.445};
    int reg_wave_num_max           = task10_wave_amps.size();

    std::cout << reg_wave_num_max;

    for (int reg_wave_num = 1; reg_wave_num <= reg_wave_num_max; ++reg_wave_num) {
        std::cout << "Chrono version: " << CHRONO_VERSION << "\n\n";

        if (hydroc::SetInitialEnvironment(argc, argv) != 0) {
            return 1;
        }

        // Check if --nogui option is set as 2nd argument
        bool visualizationOn = false;
        if (argc > 2 && std::string("--nogui").compare(argv[2]) == 0) {
            visualizationOn = false;
        }

        // Get model file names using regression test data structure
        std::filesystem::path DATADIR(hydroc::getDataDir());

        auto body1_meshfname =
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
        double simulation_duration = 600.0;

        // Create user interface
        std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);

        hydroc::gui::UI& ui = *pui.get();

        // Setup Ground
        auto ground = chrono_types::make_shared<ChBody>();
        system.AddBody(ground);
        ground->SetPos(ChVector3d(0, 0, -5));
        ground->SetTag(-1);
        ground->SetFixed(true);
        ground->EnableCollision(false);

        // some io/viz options
        bool profilingOn = true;
        bool saveDataOn  = true;

        // Output timeseries
        std::vector<double> time_vector;
        std::vector<double> heave_position;

        // set up body from a mesh
        std::cout << "Attempting to open mesh file: " << body1_meshfname << std::endl;
        std::shared_ptr<ChBody> sphereBody = chrono_types::make_shared<ChBodyEasyMesh>(  //
            body1_meshfname,                                                             // file name
            1000,                                                                        // density
            false,  // do not evaluate mass automatically
            true,   // create visualization asset
            false   // do not collide
        );

        // define the body's initial conditions
        system.Add(sphereBody);
        sphereBody->SetName("body1");  // must set body name correctly! (must match .h5 file)
        sphereBody->SetPos(ChVector3d(0, 0, -2));
        sphereBody->SetMass(261.8e3);

        // Create a visualization material
        auto red = chrono_types::make_shared<ChVisualMaterial>();
        red->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.1f));
        sphereBody->GetVisualShape(0)->SetMaterial(0, red);

        // add prismatic joint between sphere and ground (limit to heave motion only)
        auto prismatic = chrono_types::make_shared<ChLinkLockPrismatic>();
        prismatic->Initialize(sphereBody, ground, false, ChFramed(ChVector3d(0, 0, -2)),
                              ChFramed(ChVector3d(0, 0, -5)));
        system.AddLink(prismatic);

        // Create the spring between body_1 and ground. The spring end points are
        // specified in the body relative frames.
        double rest_length  = 3.0;
        double spring_coef  = 0.0;
        double damping_coef = task10_damping_coeffs[reg_wave_num - 1];
        auto spring_1       = chrono_types::make_shared<ChLinkTSDA>();
        spring_1->Initialize(sphereBody, ground, false, ChVector3d(0, 0, -2),
                             ChVector3d(0, 0, -5));  // false means positions are in global frame
        // spring_1->SetRestLength(rest_length); // if not set, the rest length is calculated from initial position
        spring_1->SetSpringCoefficient(spring_coef);
        spring_1->SetDampingCoefficient(damping_coef);
        system.AddLink(spring_1);

        auto my_hydro_inputs                     = std::make_shared<RegularWave>(1);
        my_hydro_inputs->regular_wave_amplitude_ = task10_wave_amps[reg_wave_num - 1];    // 0.095;
        my_hydro_inputs->regular_wave_omega_     = task10_wave_omegas[reg_wave_num - 1];  // 1.427996661;

        std::vector<std::shared_ptr<ChBody>> bodies;
        bodies.push_back(sphereBody);
        TestHydro hydro_forces(bodies, h5fname);
        hydro_forces.AddWaves(my_hydro_inputs);
        // for profiling
        auto start = std::chrono::high_resolution_clock::now();

        // main simulation loop
        ui.Init(&system, "Sphere - Regular Waves Test");
        ui.SetCamera(8, -25, 15, 0, 0, 0);

        while (system.GetChTime() <= simulation_duration) {
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

        if (profilingOn) {
            std::string out_file = "./results/CHRONO_SPHERE_REG_WAVES_" + std::to_string(reg_wave_num) + "_DURATION.txt";
            std::ofstream outputFile(out_file);
            // profilingFile.open("./results/sphere_reg_waves_duration.txt");
            if (!outputFile.is_open()) {
                if (!std::filesystem::exists("./results")) {
                    std::cout << "Path " << std::filesystem::absolute("./results")
                              << " does not exist, creating it now..." << std::endl;
                    std::filesystem::create_directories("./results");
                    outputFile.open(out_file);
                    if (!outputFile.is_open()) {
                        std::cout << "Still cannot open file, ending program" << std::endl;
                        return 0;
                    }
                }
            }
            outputFile << duration << "\n";
            outputFile.close();
        }

        if (saveDataOn) {
            std::string out_file = "./results/CHRONO_SPHERE_REG_WAVES_" + std::to_string(reg_wave_num) + ".txt";
            std::ofstream outputFile(out_file);
            if (!outputFile.is_open()) {
                if (!std::filesystem::exists("./results")) {
                    std::cout << "Path " << std::filesystem::absolute("./results")
                              << " does not exist, creating it now..." << std::endl;
                    std::filesystem::create_directories("./results");
                    outputFile.open(out_file);
                    if (!outputFile.is_open()) {
                        std::cout << "Still cannot open file, ending program" << std::endl;
                        return 0;
                    }
                }
            }
            outputFile.precision(10);
            outputFile.width(12);
            outputFile << "Wave #: \t" << reg_wave_num << "\n";
            outputFile << "Wave amplitude (m): \t" << my_hydro_inputs->regular_wave_amplitude_ << "\n";
            outputFile << "Wave omega (rad/s): \t" << my_hydro_inputs->regular_wave_omega_ << "\n";
            outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(12)
                       << "Heave (m)"
                       //<< std::right << std::setw(18) << "Heave Vel (m/s)"
                       //<< std::right << std::setw(18) << "Heave Force (N)"
                       << "\n";
            outputFile << std::left << std::setw(10) << "----------" << std::right << std::setw(12)
                       << "----------"
                       //<< std::right << std::setw(18) << "----------"
                       //<< std::right << std::setw(18) << "----------"
                       << "\n";

            for (int i = 0; i < time_vector.size(); i++) {
                outputFile << std::left << std::setw(10) << std::fixed << std::setprecision(3) << time_vector[i]
                           << std::right << std::setw(12) << std::fixed << std::setprecision(6) << heave_position[i]
                           //<< std::right << std::setw(18) << std::fixed << std::setprecision(6) << heave_velocity[i]
                           //<< std::right << std::setw(18) << std::fixed << std::setprecision(6) << heave_force[i]
                           << "\n";
            }
            outputFile.close();
        }

        // Clear vectors for next iteration
        time_vector.clear();
        heave_position.clear();
    }

    return 0;
} 