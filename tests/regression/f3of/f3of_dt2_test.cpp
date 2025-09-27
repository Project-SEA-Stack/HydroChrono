#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>

#include <chrono>   // std::chrono::high_resolution_clock::now
#include <iomanip>  // std::setprecision
#include <vector>   // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;

int main(int argc, char* argv[]) {
    std::cout << "Chrono version: " << CHRONO_VERSION << "\n\n";

    SetChronoDataPath(CHRONO_DATA_DIR);

    if (hydroc::SetInitialEnvironment(argc, argv) != 0) {
        return 1;
    }

    // Get model file names
    std::filesystem::path DATADIR(hydroc::getDataDir());

    auto body1_meshfame = (DATADIR / "f3of" / "geometry" / "base.obj").lexically_normal().generic_string();
    auto body2_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto body3_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "f3of" / "hydroData" / "f3of.h5").lexically_normal().generic_string();

    // system/solver settings
    ChSystemSMC system;

    system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));
    double timestep = 0.02;
    system.SetSolverType(ChSolver::Type::SPARSE_QR);
    ChRealtimeStepTimer realtime_timer;
    double simulationDuration = 300.0;

    // some io/viz options
    bool profilingOn = true;
    bool saveDataOn  = true;
    std::vector<double> time_vector;
    std::vector<double> base_surge;
    std::vector<double> base_pitch;
    std::vector<double> fore_pitch;
    std::vector<double> aft_pitch;
    
    // Output file names
    std::string filename = "CHRONO_F3OF_DT2_PITCH.txt";
    std::string filename_duration = "CHRONO_F3OF_DT2_PITCH_DURATION.txt";

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

    // ---------------- DT2 set up (flaps locked, base pitch decay, no waves) ---------------------------------
    // adjust initial pitch here only, rotations and positions calcuated from this:
    double ang_rad = CH_PI / 18.0;
    // set up pos/rotations (do not modify unless you know what you're doing)
    base->SetPos(ChVector3d(0.0, 0.0, -9.0));
    base->SetRot(QuatFromAngleY(ang_rad));
    flapFore->SetRot(QuatFromAngleY(ang_rad));
    flapAft->SetRot(QuatFromAngleY(ang_rad));
    flapFore->SetPos(ChVector3d(-12.5 * std::cos(ang_rad) + 3.5 * std::sin(ang_rad), 0.0,
                                -9.0 + 12.5 * std::sin(ang_rad) + 3.5 * std::cos(ang_rad)));
    flapAft->SetPos(ChVector3d(12.5 * std::cos(ang_rad) + 3.5 * std::sin(ang_rad), 0.0,
                               -9.0 - 12.5 * std::sin(ang_rad) + 3.5 * std::cos(ang_rad)));

    // set up revolute joints and lock them (do not modify unless you know what you're doing)
    auto revoluteFore          = chrono_types::make_shared<ChLinkLockRevolute>();
    auto revoluteAft           = chrono_types::make_shared<ChLinkLockRevolute>();
    ChQuaternion<> revoluteRot = QuatFromAngleX(CH_PI / 2.0);  // do not change
    revoluteFore->Initialize(
        base, flapFore,
        ChFramed(ChVector3d(-12.5 * std::cos(ang_rad), 0.0, -9.0 + 12.5 * std::sin(ang_rad)), revoluteRot));
    system.AddLink(revoluteFore);
    revoluteAft->Initialize(
        base, flapAft,
        ChFramed(ChVector3d(12.5 * std::cos(ang_rad), 0.0, -9.0 - 12.5 * std::sin(ang_rad)), revoluteRot));
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
    // add revolute joint between the base and ground
    auto base_rev = chrono_types::make_shared<ChLinkLockRevolute>();
    base_rev->Initialize(base, ground, ChFramed(ChVector3d(0.0, 0.0, -9.0), revoluteRot));
    system.AddLink(base_rev);

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
    while (system.GetChTime() <= simulationDuration) {
        system.DoStepDynamics(timestep);

        // append data to output vector
        time_vector.push_back(system.GetChTime());
        base_surge.push_back(base->GetPos().x());
        base_pitch.push_back(base->GetRot().GetCardanAnglesXYZ().y());
        fore_pitch.push_back(flapFore->GetRot().GetCardanAnglesXYZ().y());
        aft_pitch.push_back(flapAft->GetRot().GetCardanAnglesXYZ().y());
    }

    // for profiling
    auto end          = std::chrono::high_resolution_clock::now();
    unsigned duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (profilingOn) {
        std::ofstream profilingFile;
        profilingFile.open("./results/" + filename_duration);
        if (!profilingFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directories("./results");
                profilingFile.open("./results/" + filename_duration);
            }
        }
        profilingFile << duration << std::endl;
        profilingFile.close();
    }

    if (saveDataOn) {
        std::ofstream outputFile;
        outputFile.open("./results/" + filename);
        if (!outputFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directories("./results");
                outputFile.open("./results/" + filename);
            }
        }
        outputFile << "Time (s)    Base Surge (m)Base Pitch (radians)Flap Fore Pitch (radians)Flap Aft Pitch (radians)" << std::endl;
        for (int i = 0; i < time_vector.size(); i++) {
            outputFile << std::fixed << std::setprecision(4) << time_vector[i] << "                " << base_surge[i]
                       << "         " << base_pitch[i] << "         " << fore_pitch[i] << "          " << aft_pitch[i]
                       << std::endl;
        }
        outputFile.close();
    }

    std::cout << "Simulation completed in " << duration << " milliseconds." << std::endl;
    return 0;
} 