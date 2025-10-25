#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>

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

    auto body1_meshfame = (DATADIR / "demos" / "f3of" / "geometry" / "base.obj").lexically_normal().generic_string();
    auto body2_meshfame = (DATADIR / "demos" / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto body3_meshfame = (DATADIR / "demos" / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "demos" / "f3of" / "hydroData" / "f3of.h5").lexically_normal().generic_string();

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

    // ---------------- DT1 set up (surge decay, flaps locked, no waves) ------------------------------
    // set up pos/rotations
    base->SetPos(ChVector3d(5.0, 0.0, -9.0));
    flapFore->SetPos(ChVector3d(5.0 + -12.5, 0.0, -9.0 + 3.5));
    flapAft->SetPos(ChVector3d(5.0 + 12.5, 0.0, -9.0 + 3.5));
    // set up revolute joints and lock them
    auto revoluteFore          = chrono_types::make_shared<ChLinkLockRevolute>();
    auto revoluteAft           = chrono_types::make_shared<ChLinkLockRevolute>();
    ChQuaternion<> revoluteRot = QuatFromAngleX(CH_PI / 2.0);  // do not change
    revoluteFore->Initialize(base, flapFore, ChFramed(ChVector3d(5.0 - 12.5, 0.0, -9.0), revoluteRot));
    system.AddLink(revoluteFore);
    revoluteAft->Initialize(base, flapAft, ChFramed(ChVector3d(5.0 + 12.5, 0.0, -9.0), revoluteRot));
    system.AddLink(revoluteAft);
    revoluteFore->Lock(true);
    revoluteAft->Lock(true);
    // create ground
    auto ground = chrono_types::make_shared<ChBody>();
    system.AddBody(ground);
    ground->SetPos(ChVector3d(0, 0, -9.0));
    ground->SetTag(-1);
    ground->SetFixed(true);
    ground->EnableCollision(false);
    // add prismatic joint between the base and ground
    auto prismatic = chrono_types::make_shared<ChLinkLockPrismatic>();
    prismatic->Initialize(ground, base, ChFramed(ChVector3d(0.0, 0.0, -9.0), QuatFromAngleY(CH_PI_2)));
    system.AddLink(prismatic);
    // add damping to prismatic joint
    auto prismatic_pto = chrono_types::make_shared<ChLinkTSDA>();
    prismatic_pto->Initialize(ground, base, true, ChVector3d(0.0, 0.0, 0.0), ChVector3d(0.0, 0.0, 0.0));
    prismatic_pto->SetSpringCoefficient(1e5);
    prismatic_pto->SetRestLength(0.0);
    system.AddLink(prismatic_pto);
    // ---------------- End DT specific set up, now add hydro forces ----------------------------------

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
    ui.Init(&system, "F3OF - Decay Test 1, Flaps locked surge decay");
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
        profilingFile.open("./results/F3OF_DT1_duration.txt");
        if (!profilingFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directory("./results");
                profilingFile.open("./results/F3OF_DT1_duration.txt");
                if (!profilingFile.is_open()) {
                    // TODO instead of ending program, skip to next saveDataOn if statment
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
        outputFile.open("./results/CHRONO_F3OF_DT1_SURGE.txt");
        if (!outputFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directory("./results");
                outputFile.open("./results/CHRONO_F3OF_DT1_SURGE.txt");
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