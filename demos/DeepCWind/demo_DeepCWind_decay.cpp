#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>

#include <chrono>   // std::chrono::high_resolution_clock::now
#include <iomanip>  // std::setprecision
#include <vector>   // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;

// usage: ./sphere_deca.exe [DATADIR] [--nogui]
//
// If no argument is given user can set HYDROCHRONO_DATA_DIR
// environment variable to give the data_directory.
//
int main(int argc, char* argv[]) {
    // auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Chrono version: " << CHRONO_VERSION << "\n\n";

    if (hydroc::SetInitialEnvironment(argc, argv) != 0) {
        return 1;
    }

    // Check if --nogui option is set as 2nd argument
    bool visualizationOn = true;
    if (argc > 2 && std::string("--nogui").compare(argv[2]) == 0) {
        visualizationOn = false;
    }

    std::filesystem::path DATADIR(hydroc::getDataDir());

    auto body1_meshfame = (DATADIR / "demos" / "DeepCWind" / "geometry" / "deepcwind.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "demos" / "DeepCWind" / "hydroData" / "deepcwind.h5").lexically_normal().generic_string();

    // system/solver settings
    ChSystemSMC system;
    system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));
    double timestep = 0.08;
    system.SetTimestepperType(ChTimestepper::Type::HHT);
    system.SetSolverType(ChSolver::Type::GMRES);
    system.GetSolver()->AsIterative()->SetMaxIterations(
        300);  // the higher, the easier to keep the constraints satisfied.
    double simulationDuration = 1000.0;

    // Create user interface
    std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);
    hydroc::gui::UI& ui                  = *pui.get();

    // some io/viz options
    bool profilingOn = true;
    bool saveDataOn  = true;
    std::vector<double> time_vector;
    std::vector<double> base_pitch;
    std::vector<double> base_surge;

    // set up base from a mesh
    std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
    std::shared_ptr<ChBody> base = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body1_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // define the base's initial conditions
    system.Add(base);
    base->SetName("body1");
    auto cg = ChVector3d(0.0, 0.0, -7.53);
    // offset used for heave/surge decay test
    auto offset = ChVector3d(0.0, 0.0, 0.0);
    base->SetPos(cg + offset);
    // Use for pitch decay test
    double ang_rad = -3.95 * CH_PI / 180.0;
    base->SetRot(QuatFromAngleY(ang_rad));
    base->SetMass(1.419625e7);
    base->SetInertiaXX(ChVector3d(1.2898e10, 1.2851e10, 1.4189e10));

    // add fixed ground for linear damping (surge or pitch)
    auto ground = chrono_types::make_shared<ChBody>();
    system.AddBody(ground);
    ground->SetPos(cg);
    ground->SetRot(QuatFromAngleX(CH_PI / 2.0));
    ground->SetTag(-1);
    ground->SetFixed(true);
    ground->EnableCollision(false);

    // define damping in pitch
    auto rot_damp = chrono_types::make_shared<ChLinkRSDA>();
    // need to set damping to 31 MN-m/(rad/s)
    rot_damp->SetDampingCoefficient(31e6);
    // puts Z axis for link into screen, keeping x axis the same (to the right)
    ChQuaternion<> rev_rot = QuatFromAngleX(CH_PI / 2.0);  // do not change
    rot_damp->Initialize(base, ground, false, ChFramed(cg, rev_rot), ChFramed(cg, rev_rot));
    system.AddLink(rot_damp);

    // set up hydro forces
    std::vector<std::shared_ptr<ChBody>> bodies;
    bodies.push_back(base);
    TestHydro hydroforces(bodies, h5fname);

    // for profiling
    auto start = std::chrono::high_resolution_clock::now();

    // main simulation loop
    ui.Init(&system, "DeepCWind pitch decay test");
    ui.SetCamera(0, -70, -10, 0, 0, -10);

    while (system.GetChTime() <= simulationDuration) {
        if (ui.IsRunning(timestep) == false) break;

        if (ui.simulationStarted) {
            system.DoStepDynamics(timestep);

            // append data to output vector
            time_vector.push_back(system.GetChTime());
            base_surge.push_back(base->GetPos().x());
            base_pitch.push_back(base->GetRot().GetCardanAnglesXYZ().y());
        }
    }

    // for profiling
    auto end          = std::chrono::high_resolution_clock::now();
    unsigned duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (profilingOn) {
        std::ofstream profilingFile;
        profilingFile.open("./results/DeepCWind_decay_duration.txt");
        if (!profilingFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directory("./results");
                profilingFile.open("./results/DeepCWind_decay_duration.txt");
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
        outputFile.open("./results/DeepCWind_decay.txt");
        if (!outputFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directory("./results");
                outputFile.open("./results/DeepCWind_decay.txt");
                if (!outputFile.is_open()) {
                    std::cout << "Still cannot open file, ending program" << std::endl;
                    return 0;
                }
            }
        }
        outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(16) << "Base Surge (m)"
                   << std::right << std::setw(16) << "Base Pitch (radians)" << std::endl;
        for (int i = 0; i < time_vector.size(); ++i)
            outputFile << std::left << std::setw(10) << std::setprecision(2) << std::fixed << time_vector[i]
                       << std::right << std::setw(16) << std::setprecision(8) << std::fixed << base_surge[i]
                       << std::right << std::setw(16) << std::setprecision(8) << std::fixed << base_pitch[i]
                       << std::endl;
        outputFile.close();
    }
    return 0;
}