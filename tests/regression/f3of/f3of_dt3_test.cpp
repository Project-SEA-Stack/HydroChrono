#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>
#include <chrono/physics/ChLinkMate.h>  // fixed body uses link

#include <chrono>   // std::chrono::high_resolution_clock::now
#include <iomanip>  // std::setprecision
#include <vector>   // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;

// usage: ./<demos>.exe [DATADIR] [--nogui]
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

    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;

    // Get model file names
    std::filesystem::path DATADIR(hydroc::getDataDir());

    auto body1_meshfame = (DATADIR / "f3of" / "geometry" / "base.obj").lexically_normal().generic_string();
    auto body2_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto body3_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "f3of" / "hydroData" / "f3of.h5").lexically_normal().generic_string();

    std::cout << "Resolved body1_meshfame: " << body1_meshfame << std::endl;
    std::cout << "Resolved body2_meshfame: " << body2_meshfame << std::endl;
    std::cout << "Resolved body3_meshfame: " << body3_meshfame << std::endl;
    std::cout << "Resolved h5fname: " << h5fname << std::endl;

    // system/solver settings
    ChSystemSMC system;

    system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));
    double timestep = 0.02;
    system.SetSolverType(ChSolver::Type::SPARSE_QR);
    ChRealtimeStepTimer realtime_timer;
    double simulationDuration = 300.0;

    // Create user interface
    std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);
    hydroc::gui::UI& ui                  = *pui.get();

    // some io/viz options
    bool profilingOn = true;
    bool saveDataOn  = true;
    std::vector<double> time_vector;
    std::vector<double> base_surge;
    std::vector<double> base_pitch;
    std::vector<double> fore_pitch;
    std::vector<double> aft_pitch;

    // set up body from a mesh
    std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
    std::shared_ptr<ChBody> base = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body1_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto red = chrono_types::make_shared<ChVisualMaterial>();
    red->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.1f));
    base->GetVisualShape(0)->SetMaterial(0, red);

    // define the base's initial conditions (position and rotation defined later for specific test)
    system.Add(base);
    base->SetName("body1");
    base->SetMass(1089825.0);
    base->SetInertiaXX(ChVector3d(100000000.0, 76300000.0, 100000000.0));

    std::cout << "Attempting to open mesh file: " << body2_meshfame << std::endl;
    std::shared_ptr<ChBody> flapFore = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body2_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto blue = chrono_types::make_shared<ChVisualMaterial>();
    blue->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.6f));
    flapFore->GetVisualShape(0)->SetMaterial(0, blue);

    // define the fore flap's initial conditions (position and rotation defined later for specific tests
    system.Add(flapFore);
    flapFore->SetName("body2");
    flapFore->SetMass(179250.0);
    flapFore->SetInertiaXX(ChVector3d(100000000.0, 1300000.0, 100000000.0));

    std::cout << "Attempting to open mesh file: " << body3_meshfame << std::endl;
    std::shared_ptr<ChBody> flapAft = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body3_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto green = chrono_types::make_shared<ChVisualMaterial>();
    green->SetDiffuseColor(ChColor(0.3f, 0.6f, 0.1f));
    flapAft->GetVisualShape(0)->SetMaterial(0, green);

    // define the aft flap's initial conditions (position and rotation defined later for specific tests
    system.Add(flapAft);
    flapAft->SetName("body3");
    flapAft->SetMass(179250.0);
    flapAft->SetInertiaXX(ChVector3d(100000000.0, 1300000.0, 100000000.0));

    // ---------------- DT3 set up (flap decay, base fixed, no waves) ---------------------------------
    base->SetPos(ChVector3d(0.0, 0.0, -9.0));
    double fore_ang_rad = CH_PI / 18.0;  // fore flap starts with 10 degree initial rotation
    flapFore->SetRot(QuatFromAngleY(fore_ang_rad));
    flapFore->SetPos(ChVector3d(-12.5 + 3.5 * std::cos(CH_PI / 2.0 - fore_ang_rad), 0.0,
                                -9.0 + 3.5 * std::sin(CH_PI / 2.0 - fore_ang_rad)));
    double aft_ang_rad = 0.0;
    flapAft->SetRot(QuatFromAngleY(aft_ang_rad));
    flapAft->SetPos(ChVector3d(12.5 + 3.5 * std::cos(CH_PI / 2.0 - aft_ang_rad), 0.0,
                               -9.0 + 3.5 * std::sin(CH_PI / 2.0 - fore_ang_rad)));
    // set up revolute joints with damping for each flap
    auto revoluteFore          = chrono_types::make_shared<ChLinkLockRevolute>();
    auto revoluteAft           = chrono_types::make_shared<ChLinkLockRevolute>();
    ChQuaternion<> revoluteRot = QuatFromAngleX(CH_PI / 2.0);  // do not change
    revoluteFore->Initialize(base, flapFore, ChFramed(ChVector3d(-12.5, 0.0, -9.0), revoluteRot));
    system.AddLink(revoluteFore);
    revoluteAft->Initialize(base, flapAft, ChFramed(ChVector3d(12.5, 0.0, -9.0), revoluteRot));
    system.AddLink(revoluteAft);
    // create ground
    auto ground = chrono_types::make_shared<ChBody>();
    system.AddBody(ground);
    ground->SetPos(ChVector3d(0, 0, -12.0));
    ground->SetTag(-1);
    ground->SetFixed(true);
    ground->EnableCollision(false);
    // fix base to ground with special constraint (don't use setfixed() because of mass matrix)
    auto anchor = chrono_types::make_shared<ChLinkMateGeneric>();
    anchor->Initialize(base, ground, false, base->GetVisualModelFrame(), base->GetVisualModelFrame());
    system.Add(anchor);
    anchor->SetConstrainedCoords(true, true, true, true, true, true);  // x, y, z, Rx, Ry, Rz

    // define wave parameters (not used in this demo TODO have hydroforces constructor without hydro inputs)
    auto default_dont_add_waves = std::make_shared<NoWave>(3);

    // set up hydro forces
    std::vector<std::shared_ptr<ChBody>> bodies;
    bodies.push_back(base);
    bodies.push_back(flapFore);
    bodies.push_back(flapAft);
    TestHydro hydroforces(bodies, h5fname, default_dont_add_waves);

    // for profiling
    auto start = std::chrono::high_resolution_clock::now();

    // main simulation loop
    ui.Init(&system, "F3OF - Decay Test 3, Base locked flap decay");
    ui.SetCamera(0, -50, -10, 0, 0, -10);

    while (system.GetChTime() <= simulationDuration) {
        if (ui.IsRunning(timestep) == false) break;

        if (ui.simulationStarted) {
            system.DoStepDynamics(timestep);

            // append data to output vector
            time_vector.push_back(system.GetChTime());
            base_surge.push_back(base->GetPos().x());
            base_pitch.push_back(base->GetRot().GetCardanAnglesXYZ().y());
            fore_pitch.push_back(flapFore->GetRot().GetCardanAnglesXYZ().y());
            aft_pitch.push_back(flapAft->GetRot().GetCardanAnglesXYZ().y());
        }
    }

    // for profiling
    auto end          = std::chrono::high_resolution_clock::now();
    unsigned duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (profilingOn) {
        std::ofstream profilingFile;
        std::string filename = "CHRONO_F3OF_DT3_FLAP_PITCH.txt";
        std::string filename_duration = "CHRONO_F3OF_DT3_FLAP_PITCH_DURATION.txt";
        profilingFile.open("./results/" + filename_duration);
        if (!profilingFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..." << std::endl;
                std::filesystem::create_directory("./results");
                profilingFile.open("./results/" + filename_duration);
                if (!profilingFile.is_open()) {
                    std::cout << "Still cannot open file, ending program" << std::endl;
                    return 0;
                }
            }
        }
        profilingFile << duration << " ms\n";
        profilingFile.close();
    }
    if (saveDataOn) {
        std::ofstream outputFile;
        std::string filename = "CHRONO_F3OF_DT3_FLAP_PITCH.txt";
        std::string filename_duration = "CHRONO_F3OF_DT3_FLAP_PITCH_DURATION.txt";
        outputFile.open("./results/CHRONO_F3OF_DT3_FLAP_PITCH.txt");
        if (!outputFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..." << std::endl;
                std::filesystem::create_directory("./results");
                outputFile.open("./results/CHRONO_F3OF_DT3_FLAP_PITCH.txt");
                if (!outputFile.is_open()) {
                    std::cout << "Still cannot open file, ending program" << std::endl;
                    return 0;
                }
            }
        }
        outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(16) << "Base Surge (m)"
                   << std::right << std::setw(16) << "Base Pitch (radians)" << std::right << std::setw(16)
                   << "Flap Fore Pitch (radians)" << std::right << std::setw(16) << "Flap Aft Pitch (radians)"
                   << std::endl;
        for (int i = 0; i < time_vector.size(); ++i)
            outputFile << std::left << std::setw(10) << std::setprecision(2) << std::fixed << time_vector[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << base_surge[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << base_pitch[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << fore_pitch[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << aft_pitch[i]
                       << std::endl;
        outputFile.close();
    }

    std::cout << "Simulation finished." << std::endl;
    return 0;
}