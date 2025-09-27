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

// Function to compute cross product
std::array<double, 3> cross(std::array<double, 3> v1, std::array<double, 3> v2) {
    return {v1[1] * v2[2] - v1[2] * v2[1], v1[2] * v2[0] - v1[0] * v2[2], v1[0] * v2[1] - v1[1] * v2[0]};
}

// Function to compute dot product
double dot(std::array<double, 3> v1, std::array<double, 3> v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

// Function to normalize a vector
std::array<double, 3> normalize(std::array<double, 3> v) {
    double norm = sqrt(dot(v, v));
    return {v[0] / norm, v[1] / norm, v[2] / norm};
}

// Function to rotate a vector
std::array<double, 3> rotate_vector_3d(std::array<double, 3> vector,
                                       std::array<double, 3> axis,
                                       double angle_in_degrees) {
    // Convert the angle from degrees to radians
    double angle_in_radians = angle_in_degrees * M_PI / 180.0;

    // Normalize the axis vector
    axis = normalize(axis);

    // Apply the rotation to the vector
    std::array<double, 3> rotated_vector;
    for (int i = 0; i < 3; i++) {
        rotated_vector[i] = vector[i] * cos(angle_in_radians) + cross(axis, vector)[i] * sin(angle_in_radians) +
                            axis[i] * dot(axis, vector) * (1 - cos(angle_in_radians));
    }

    return rotated_vector;
}

// Function to add two vectors
std::array<double, 3> add_vectors(std::array<double, 3> v1, std::array<double, 3> v2) {
    return {v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]};
}

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

    // Get model file names - use regression test data directory
    std::filesystem::path DATADIR = std::filesystem::current_path();

    auto body1_meshfame = (DATADIR / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto body2_meshfame = (DATADIR / "geometry" / "base.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "hydroData" / "oswec.h5").lexically_normal().generic_string();

    // system/solver settings
    ChSystemNSC system;

    system.SetGravitationalAcceleration(ChVector3d(0.0, 0.0, -9.81));
    double timestep = 0.03;
    // system.SetTimestepperType(ChTimestepper::Type::HHT);
    system.SetSolverType(ChSolver::Type::GMRES);
    // system.GetSolver()->AsIterative()->SetMaxIterations(300);  // the higher, the easier to keep the constraints
    // satisfied.
    ChRealtimeStepTimer realtime_timer;
    double simulationDuration = 400.0;

    // Create user interface
    std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);
    hydroc::gui::UI& ui                  = *pui.get();

    // some io/viz options
    bool profilingOn = true;
    bool saveDataOn  = true;
    std::vector<double> time_vector;
    std::vector<double> flap_rot;

    std::array<double, 3> origin_to_hinge = {0, 0, -8.9};
    std::array<double, 3> hinge_to_cg     = {0, 0, 5};
    std::array<double, 3> axis            = {0, 1, 0};
    double angle_in_degrees               = 10;

    std::array<double, 3> rotated_hinge_to_cg = rotate_vector_3d(hinge_to_cg, axis, angle_in_degrees);

    std::array<double, 3> new_cg = add_vectors(origin_to_hinge, rotated_hinge_to_cg);

    std::cout << "The original vector is [" << hinge_to_cg[0] << ", " << hinge_to_cg[1] << ", " << hinge_to_cg[2] << "]"
              << std::endl;
    std::cout << "The rotated vector is [" << rotated_hinge_to_cg[0] << ", " << rotated_hinge_to_cg[1] << ", "
              << rotated_hinge_to_cg[2] << "]" << std::endl;
    std::cout << "The rotated vector is [" << new_cg[0] << ", " << new_cg[1] << ", " << new_cg[2] << "]" << std::endl;

    // set up body from a mesh
    std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
    std::shared_ptr<ChBody> flap_body = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body1_meshfame,
        1000,   // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto red = chrono_types::make_shared<ChVisualMaterial>();
    red->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.1f));
    flap_body->GetVisualShape(0)->SetMaterial(0, red);

    // define the float's initial conditions
    system.Add(flap_body);
    flap_body->SetName("body1");
    auto ang_rad = CH_PI / 18.0;
    flap_body->SetPos(ChVector3d(new_cg[0], new_cg[1], new_cg[2]));
    flap_body->SetRot(QuatFromAngleY(ang_rad));
    flap_body->SetMass(127000.0);
    flap_body->SetInertiaXX(ChVector3d(1.85e6, 1.85e6, 1.85e6));
    // notes: mass and inertia added to added mass and system mass correctly.

    // set up body from a mesh
    std::cout << "Attempting to open mesh file: " << body2_meshfame << std::endl;
    std::shared_ptr<ChBody> base_body = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body2_meshfame,
        1000,   // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto blue = chrono_types::make_shared<ChVisualMaterial>();
    blue->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.6f));
    base_body->GetVisualShape(0)->SetMaterial(0, blue);

    // define the plate's initial conditions
    system.Add(base_body);
    base_body->SetName("body2");
    base_body->SetPos(ChVector3d(0, 0, -10.15));
    base_body->SetMass(999);
    base_body->SetInertiaXX(ChVector3d(1, 1, 1));
    // base_body->SetFixed(true);

    // create ground
    auto ground = chrono_types::make_shared<ChBody>();
    system.AddBody(ground);
    ground->SetPos(ChVector3d(0, 0, -10.15));
    ground->SetTag(-1);
    ground->SetFixed(true);
    ground->EnableCollision(false);
    // fix base to ground with special constraint (don't use setfixed() because of mass matrix)
    auto anchor = chrono_types::make_shared<ChLinkMateGeneric>();
    anchor->Initialize(base_body, ground, false, base_body->GetVisualModelFrame(), base_body->GetVisualModelFrame());
    system.Add(anchor);
    anchor->SetConstrainedCoords(true, true, true, true, true, true);  // x, y, z, Rx, Ry, Rz

    // define base-fore flap joint
    ChQuaternion<> revoluteRot = QuatFromAngleX(CH_PI / 2.0);
    auto revolute              = chrono_types::make_shared<ChLinkLockRevolute>();
    revolute->Initialize(base_body, flap_body, ChFramed(ChVector3d(0.0, 0.0, -8.9), revoluteRot));
    system.AddLink(revolute);

    auto default_dont_add_waves = std::make_shared<NoWave>(2);

    //// attach hydrodynamic forces to body
    std::vector<std::shared_ptr<ChBody>> bodies;
    bodies.push_back(flap_body);
    bodies.push_back(base_body);
    TestHydro blah(bodies, h5fname, default_dont_add_waves);

    // for profiling
    auto start = std::chrono::high_resolution_clock::now();

    // main simulation loop
    ui.Init(&system, "OSWEC - Decay Test");
    ui.SetCamera(0, -50, -10, 0, 0, -10);

    while (system.GetChTime() <= simulationDuration) {
        if (ui.IsRunning(timestep) == false) break;

        if (ui.simulationStarted) {
            system.DoStepDynamics(timestep);

            // append data to output vector
            time_vector.push_back(system.GetChTime());
            flap_rot.push_back(flap_body->GetRot().GetCardanAnglesXYZ().y());
        }
    }

    // for profiling
    auto end          = std::chrono::high_resolution_clock::now();
    unsigned duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (profilingOn) {
        std::ofstream profilingFile;
        profilingFile.open("./results/CHRONO_OSWEC_DECAY_DURATION.txt");
        if (!profilingFile.is_open()) {
            if (!std::filesystem::exists("./results")) {
                std::cout << "Path " << std::filesystem::absolute("./results") << " does not exist, creating it now..."
                          << std::endl;
                std::filesystem::create_directory("./results/");
                profilingFile.open("./results/CHRONO_OSWEC_DECAY_DURATION.txt");
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
        // Create results directory if it doesn't exist
        if (!std::filesystem::exists("./results")) {
            std::cout << "Creating results directory..." << std::endl;
            std::filesystem::create_directory("./results");
        }
        
        std::ofstream outputFile;
        outputFile.open("./results/CHRONO_OSWEC_DECAY.txt");
        if (!outputFile.is_open()) {
            std::cout << "Failed to open output file for writing" << std::endl;
            return 1;
        }
        outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(16)
                   << "Flap Rotation y (radians)" << std::right << std::setw(16) << "Flap Rotation y (degrees)"
                   << std::endl;
        for (int i = 0; i < time_vector.size(); ++i)
            outputFile << std::left << std::setw(10) << std::setprecision(2) << std::fixed << time_vector[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << flap_rot[i] << std::right
                       << std::setw(16) << std::setprecision(4) << std::fixed << flap_rot[i] * 360.0 / 6.28
                       << std::endl;
        outputFile.close();
    }
    return 0;
}