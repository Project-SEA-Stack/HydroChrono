#include <chrono_parsers/ChParserYAML.h>
#include <chrono/core/ChRealtimeStep.h>
#include <chrono/core/ChTypes.h>
#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>

#include <chrono/physics/ChSystem.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace hydroc {

// -----------------------------------------------------------------------------
// Utility: Resolve a path relative to the HydroChrono source if not absolute.
// -----------------------------------------------------------------------------
static std::string ResolvePath(const std::string& in_path, const std::string& base_dir) {
    std::filesystem::path p(in_path);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    return (std::filesystem::path(base_dir) / p).lexically_normal().generic_string();
}

// -----------------------------------------------------------------------------
// Main YAML runner implementation.
// -----------------------------------------------------------------------------
int RunHydroChronoFromYAML(int argc, char* argv[]) {
    try {
        // ---------------------------------------------------------------------
        // 1. CLI parsing (simple manual parsing)
        // ---------------------------------------------------------------------
        std::string model_file_arg;
        std::string sim_file_arg;
        bool nogui = false;
        bool show_help = false;

        // Simple argument parsing
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                show_help = true;
            } else if (arg == "--nogui") {
                nogui = true;
            } else if (arg == "--model_file" && i + 1 < argc) {
                model_file_arg = argv[++i];
            } else if (arg == "--sim_file" && i + 1 < argc) {
                sim_file_arg = argv[++i];
            }
        }

        if (show_help) {
            std::cout << "HydroChrono YAML simulation runner\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --model_file <path>  Path to a Chrono model YAML file\n";
            std::cout << "  --sim_file <path>    Path to a Chrono simulation YAML file\n";
            std::cout << "  --nogui              Disable GUI visualization\n";
            std::cout << "  -h, --help           Show this help message\n";
            return 0;
        }

        // Determine base directory (repository root) at runtime – assume exe launched from build/bin
        std::filesystem::path exe_dir = std::filesystem::current_path();
        std::filesystem::path repo_root = exe_dir;
        // Traverse upwards until we find a marker file like CMakeLists.txt
        while (!repo_root.empty() && !std::filesystem::exists(repo_root / "CMakeLists.txt")) {
            repo_root = repo_root.parent_path();
        }
        if (repo_root.empty()) {
            repo_root = exe_dir;  // fallback – paths must then be absolute
        }

        // Default YAML locations (relative to repo root)
        std::string default_model = (repo_root / "demos" / "yaml" / "slider_crank" / "slider_crank.model.yaml")
                                        .lexically_normal()
                                        .generic_string();
        std::string default_sim = (repo_root / "demos" / "yaml" / "slider_crank" / "slider_crank.simulation.yaml")
                                       .lexically_normal()
                                       .generic_string();

        // Pull options (with fallback)
        std::string model_file = !model_file_arg.empty() ? model_file_arg : default_model;
        std::string sim_file   = !sim_file_arg.empty() ? sim_file_arg : default_sim;

        // Resolve relative paths against repo root so they work regardless of cwd
        model_file = ResolvePath(model_file, repo_root.generic_string());
        sim_file   = ResolvePath(sim_file, repo_root.generic_string());

        std::cout << "HydroChrono YAML runner\n";
        std::cout << "  Model file     : " << model_file << "\n";
        std::cout << "  Simulation file: " << sim_file << "\n";
        std::cout << "  GUI enabled    : " << (nogui ? "false" : "true") << "\n" << std::endl;

        // ---------------------------------------------------------------------
        // 2. Load the YAML input using Chrono parser
        // ---------------------------------------------------------------------
        chrono::parsers::ChParserYAML parser;
        parser.LoadSimulationFile(sim_file);
        auto system = parser.CreateSystem();

        parser.LoadModelFile(model_file);
        parser.Populate(*system);

        // ---------------------------------------------------------------------
        // 3. Visualization settings (Irrlicht)
        // ---------------------------------------------------------------------
        std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(!nogui);
        hydroc::gui::UI& ui                  = *pui;

        ui.Init(system.get(), "HydroChrono YAML");
        ui.SetCamera(0, -50, -10, 0, 0, -10);

        // ---------------------------------------------------------------------
        // 4. Simulation loop
        // ---------------------------------------------------------------------
        double timestep = system->GetStep();
        if (timestep <= 0) {
            timestep = 1e-3;  // fallback timestep if not provided by simulation file
        }

        // Attempt to read an end time from the simulation YAML file.  Fall back to a safe default.
        double time_end = 5.0;  // default 5 seconds
        
        // Try to parse the simulation file to get end_time
        try {
            std::ifstream sim_stream(sim_file);
            if (sim_stream.is_open()) {
                std::string line;
                while (std::getline(sim_stream, line)) {
                    if (line.find("end_time:") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value_str = line.substr(pos + 1);
                            // Remove whitespace
                            value_str.erase(0, value_str.find_first_not_of(" \t"));
                            value_str.erase(value_str.find_last_not_of(" \t") + 1);
                            time_end = std::stod(value_str);
                            break;
                        }
                    }
                }
                sim_stream.close();
            }
        } catch (const std::exception&) {
            // If parsing fails, use default time_end
        }

        chrono::ChRealtimeStepTimer rt_timer;

        while (system->GetChTime() <= time_end) {
            if (!ui.IsRunning(timestep)) {
                break;
            }

            if (ui.simulationStarted) {
                system->DoStepDynamics(timestep);
                // Keep real-time pacing if GUI is active
                if (!nogui) {
                    rt_timer.Spin(timestep);
                }
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[HydroChrono YAML] Exception: " << e.what() << std::endl;
        std::cout.flush();
        std::cerr.flush();
        return 1;
    } catch (...) {
        std::cerr << "[HydroChrono YAML] Unknown exception occurred." << std::endl;
        std::cout.flush();
        std::cerr.flush();
        return 1;
    }
}

}  // namespace hydroc 