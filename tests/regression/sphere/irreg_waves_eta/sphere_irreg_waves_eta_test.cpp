#include <chrono>
#include <filesystem>
#include <iomanip>
#include <vector>
#include <iostream>
#include <fstream>

#include <chrono/assets/ChColor.h>
#include <chrono/core/ChRealtimeStep.h>

#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

// Use the namespaces of Chrono
using namespace chrono;

// usage: ./<test>.exe [DATADIR] [--nogui]
//
// If no argument is given user can set HYDROCHRONO_DATA_DIR
// environment variable to give the data_directory.
//
int main(int argc, char* argv[]) {
    std::cout << "=== SPHERE IRREGULAR WAVES ETA TEST STARTING ===" << std::endl;
    std::cout << "Chrono version: " << CHRONO_VERSION << "\n\n";

    try {
        SetChronoDataPath(CHRONO_DATA_DIR);
        std::cout << "DEBUG: Chrono data path set successfully" << std::endl;

        if (hydroc::SetInitialEnvironment(argc, argv) != 0) {
            std::cerr << "ERROR: Failed to set initial environment" << std::endl;
            return 1;
        }
        std::cout << "DEBUG: Initial environment set successfully" << std::endl;

        // Check if --nogui option is set as 2nd argument
        bool visualizationOn = false;
        if (argc > 2 && std::string("--nogui").compare(argv[2]) == 0) {
            visualizationOn = false;
        }
        std::cout << "DEBUG: Visualization mode: " << (visualizationOn ? "ON" : "OFF") << std::endl;

        std::filesystem::path DATADIR(hydroc::getDataDir());
        std::cout << "DEBUG: Data directory: " << DATADIR << std::endl;

        auto body1_meshfame =
            (DATADIR / "sphere" / "geometry" / "oes_task10_sphere.obj").lexically_normal().generic_string();
        auto h5fname = (DATADIR / "sphere" / "hydroData" / "sphere.h5").lexically_normal().generic_string();
        
        // Try multiple possible paths for the ETA file
        std::vector<std::filesystem::path> possible_eta_paths = {
            DATADIR / "sphere" / "eta" / "eta.txt",
            std::filesystem::path("C:/code/HydroChrono/build/tests/regression/Release/data/sphere/eta/eta.txt"),
            std::filesystem::path("C:/code/HydroChrono/demos/sphere/eta/eta.txt")
        };

        std::filesystem::path eta_file_path;
        bool found_eta_file = false;
        
        for (const auto& path : possible_eta_paths) {
            std::cout << "DEBUG: Checking ETA path: " << path << std::endl;
            if (std::filesystem::exists(path)) {
                eta_file_path = path;
                found_eta_file = true;
                std::cout << "DEBUG: Found ETA file at: " << eta_file_path << std::endl;
                break;
            }
        }

        if (!found_eta_file) {
            std::cerr << "ERROR: ETA file not found in any of the expected locations" << std::endl;
            return 1;
        }

        std::cout << "DEBUG: Mesh file path: " << body1_meshfame << std::endl;
        std::cout << "DEBUG: H5 file path: " << h5fname << std::endl;
        std::cout << "DEBUG: ETA file path: " << eta_file_path << std::endl;

        // Check if files exist
        if (!std::filesystem::exists(body1_meshfame)) {
            std::cerr << "ERROR: Mesh file does not exist: " << body1_meshfame << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(h5fname)) {
            std::cerr << "ERROR: H5 file does not exist: " << h5fname << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(eta_file_path)) {
            std::cerr << "ERROR: ETA file does not exist: " << eta_file_path << std::endl;
            return 1;
        }

        // Check if the .h5 file can be opened (using HDF5 C API)
        std::cout << "DEBUG: Checking if H5 file can be opened..." << std::endl;
        FILE* h5file = fopen(h5fname.c_str(), "rb");
        if (!h5file) {
            std::cerr << "ERROR: Could not open H5 file: " << h5fname << std::endl;
            return 1;
        } else {
            std::cout << "DEBUG: H5 file opened successfully." << std::endl;
            fclose(h5file);
        }

        //    // system/solver settings
        ChSystemNSC system;
        system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));
        double timestep = 0.015;
        system.SetSolverType(ChSolver::Type::GMRES);
        system.GetSolver()->AsIterative()->SetMaxIterations(300);
        ChRealtimeStepTimer realtime_timer;
        double simulationDuration = 600.0;

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
        std::vector<double> time_vector;
        std::vector<double> heave_position;
        //
        // set up body from a mesh
        std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
        std::shared_ptr<ChBody> sphereBody = chrono_types::make_shared<ChBodyEasyMesh>(  //
            body1_meshfame,                                                              // file name
            1000,                                                                        // density
            false,  // do not evaluate mass automatically
            true,   // create visualization asset
            false   // do not collide
        );
        //
        // define the body's initial conditions
        system.Add(sphereBody);
        sphereBody->SetName("body1");  // must set body name correctly! (must match .h5 file)
        sphereBody->SetPos(ChVector3d(0, 0, -2));
        sphereBody->SetMass(261.8e3);

        // create a visualization material
        auto yellow = chrono_types::make_shared<ChVisualMaterial>();
        yellow->SetDiffuseColor(ChColor(0.244f, 0.225f, 0.072f));
        sphereBody->GetVisualShape(0)->SetMaterial(0, yellow);

        std::cout << "Body created from the mesh file: " << body1_meshfame << std::endl;

        // add prismatic joint between sphere and ground (limit to heave motion only)
        auto prismatic = chrono_types::make_shared<ChLinkLockPrismatic>();
        prismatic->Initialize(sphereBody, ground, false, ChFramed(ChVector3d(0, 0, -2)), ChFramed(ChVector3d(0, 0, -5)));
        system.AddLink(prismatic);

        // create the spring between body_1 and ground. The spring end points are
        // specified in the body relative frames.
        double rest_length  = 3.0;
        double spring_coef  = 0.0;
        double damping_coef = 0.0;
        auto spring_1       = chrono_types::make_shared<ChLinkTSDA>();
        spring_1->Initialize(sphereBody, ground, false, ChVector3d(0, 0, -2),
                             ChVector3d(0, 0, -5));  // false means positions are in global frame
        spring_1->SetSpringCoefficient(spring_coef);
        spring_1->SetDampingCoefficient(damping_coef);
        system.AddLink(spring_1);

        std::vector<std::shared_ptr<ChBody>> bodies;
        bodies.push_back(sphereBody);

        // MODIFIED SECTION: Use ETA file instead of regular wave parameters
        IrregularWaveParams wave_inputs;
        wave_inputs.num_bodies_          = bodies.size();
        wave_inputs.simulation_dt_       = timestep;
        wave_inputs.simulation_duration_ = simulationDuration;
        wave_inputs.ramp_duration_       = 0.0;  // Changed from 60.0
        wave_inputs.eta_file_path_       = (DATADIR / "sphere" / "eta" / "eta.txt").lexically_normal().generic_string();  // Added ETA file
        wave_inputs.frequency_min_       = 0.001;
        wave_inputs.frequency_max_       = 1.0;
        wave_inputs.nfrequencies_        = 1000;
        // Removed wave_height_ and wave_period_ as they're not used with ETA

        std::shared_ptr<IrregularWaves> my_hydro_inputs;  // declare outside the try-catch block

        try {
            my_hydro_inputs = std::make_shared<IrregularWaves>(wave_inputs);
        } catch (const std::exception& e) {
            std::cerr << "Caught exception: " << e.what() << '\n';
        } catch (...) {
            std::cerr << "Caught unknown exception.\n";
        }

        TestHydro hydro_forces(bodies, h5fname);
        hydro_forces.AddWaves(my_hydro_inputs);

        // set up free surface from a mesh
        auto fse_plane = chrono_types::make_shared<ChBody>();
        fse_plane->SetPos(ChVector3d(0, 0, 0));
        fse_plane->SetFixed(true);
        fse_plane->EnableCollision(false);
        system.AddBody(fse_plane);

        my_hydro_inputs->SetUpWaveMesh();
        std::shared_ptr<ChBody> fse_mesh = chrono_types::make_shared<ChBodyEasyMesh>(  //
            my_hydro_inputs->GetMeshFile(),                                            // file name
            1000,                                                                      // density
            false,                                                                     // do not evaluate mass automatically
            true,                                                                      // create visualization asset
            false                                                                      // do not collide
        );
        fse_mesh->SetMass(1.0);
        fse_mesh->SetPosDt(my_hydro_inputs->GetWaveMeshVelocity());
        system.Add(fse_mesh);
        auto fse_prismatic = chrono_types::make_shared<ChLinkLockPrismatic>();
        fse_prismatic->Initialize(fse_plane, fse_mesh, ChFramed(ChVector3d(1.0, 0.0, 0.0), QuatFromAngleY(CH_PI_2)));
        system.AddLink(fse_prismatic);

        // Create a visualization material
        auto fse_texture = chrono_types::make_shared<ChVisualMaterial>();
        fse_texture->SetDiffuseColor(ChColor(0.026f, 0.084f, 0.168f));
        fse_texture->SetOpacity(0.1);
        fse_mesh->GetVisualShape(0)->SetMaterial(0, fse_texture);

        // for profiling
        auto start = std::chrono::high_resolution_clock::now();
        // main simulation loop
        ui.Init(&system, "Sphere - Irregular Waves Test");
        ui.SetCamera(8, -25, 15, 0, 0, 0);

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

        if (profilingOn) {
            std::string out_file = "results/CHRONO_SPHERE_IRREGULAR_WAVES_DURATION.txt";
            std::ofstream profilingFile(out_file);
            if (!profilingFile.is_open()) {
                if (!std::filesystem::exists("./results")) {
                    std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                              << std::endl;
                    std::filesystem::create_directories("./results");
                    profilingFile.open(out_file);
                    if (!profilingFile.is_open()) {
                        std::cout << "Still cannot open file, ending program" << std::endl;
                        return 0;
                    }
                }
            }
            profilingFile << duration << "\n";
            profilingFile.close();
        }

        if (saveDataOn) {
            std::string out_file = "results/CHRONO_SPHERE_IRREGULAR_WAVES.txt";
            std::ofstream outputFile(out_file);
            if (!outputFile.is_open()) {
                if (!std::filesystem::exists("./results")) {
                    std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                              << std::endl;
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
            outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(12) << "Heave (m)" << "\n";
            outputFile << std::left << std::setw(10) << "----------" << std::right << std::setw(12) << "----------" << "\n";

            for (int i = 0; i < time_vector.size(); i++) {
                outputFile << std::left << std::setw(10) << std::fixed << std::setprecision(3) << time_vector[i]
                           << std::right << std::setw(12) << std::fixed << std::setprecision(6) << heave_position[i] << "\n";
            }
            outputFile.close();
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL ERROR: Unknown unhandled exception in main" << std::endl;
        return 1;
    }
} 