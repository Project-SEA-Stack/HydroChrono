#include <hydroc/version.h>
#include "../utils/setup_parser.h"
#include <hydroc/logging.h>
#include "../setup_hydro_from_yaml.h"
#include "../hydro_yaml_parser.h"
#include <hydroc/hydro_forces.h>
#include <hydroc/simulation_exporter.h>

#include <chrono_parsers/yaml/ChParserMbsYAML.h>
#include <chrono/physics/ChSystem.h>
#include <chrono/physics/ChBody.h>
#include <chrono/core/ChRealtimeStep.h>
#include <hydroc/gui/guihelper.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace hydroc {

// -----------------------------------------------------------------------------
// Utility: Find the first file matching a pattern in a directory.
// -----------------------------------------------------------------------------
static std::string FindFirstFile(const std::filesystem::path& directory, const std::string& pattern) {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return "";
    }
    
    std::vector<std::filesystem::path> matches;
    
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().generic_string();
            if (filename.find(pattern) != std::string::npos) {
                matches.push_back(entry.path());
            }
        }
    }
    
    if (!matches.empty()) {
        std::sort(matches.begin(), matches.end());
        return matches.front().generic_string();
    }
    
    return "";
}

// -----------------------------------------------------------------------------
// Best-effort YAML probe: find a scalar double value for a given key (e.g., end_time)
// Used only for CLI display; does not control simulation.
// -----------------------------------------------------------------------------
static bool TryFindYamlDouble(const std::string& yaml_path, const std::string& key, double& out_value) {
    std::ifstream in(yaml_path);
    if (!in.is_open()) {
        return false;
    }
    auto ltrim = [](std::string& s) { s.erase(0, s.find_first_not_of(" \t\r\n")); };
    auto rtrim = [](std::string& s) { size_t p = s.find_last_not_of(" \t\r\n"); if (p == std::string::npos) s.clear(); else s.erase(p + 1); };
    std::string line;
    while (std::getline(in, line)) {
        ltrim(line); rtrim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string k = line.substr(0, pos);
        std::string v = line.substr(pos + 1);
        ltrim(k); rtrim(k); ltrim(v); rtrim(v);
        if (k == key) {
            try {
                out_value = std::stod(v);
                return true;
            } catch (...) {
                return false;
            }
        }
    }
    return false;
}

// Helper functions for clean separation of concerns
bool ResolveInputFiles(const std::filesystem::path& input_dir, 
                      const std::string& model_file_arg, 
                      const std::string& sim_file_arg,
                      std::string& model_file, 
                      std::string& sim_file, 
                      SetupConfig& setup_config) {
    
    // Log internal setup details to file only
    hydroc::debug::LogDebug("Checking for setup file...");
    auto setup_file_path = FindSetupFile(input_dir);
    bool using_setup_file = false;
    
    if (!setup_file_path.empty()) {
        hydroc::debug::LogDebug(std::string("Setup file found: ") + setup_file_path.generic_string());
        using_setup_file = true;
        setup_config = ParseSetupFile(setup_file_path);
        hydroc::debug::LogDebug("Setup file loaded");
        
        // Use setup file configuration unless overridden by CLI
        if (!model_file_arg.empty()) {
            model_file = model_file_arg;
            if (!std::filesystem::path(model_file).is_absolute()) {
                model_file = (input_dir / model_file).generic_string();
            }
        } else if (setup_config.has_model_file) {
            model_file = (input_dir / setup_config.model_file).generic_string();
            hydroc::debug::LogDebug(std::string("Model file from setup: ") + setup_config.model_file);
        }
        
        if (!sim_file_arg.empty()) {
            sim_file = sim_file_arg;
            if (!std::filesystem::path(sim_file).is_absolute()) {
                sim_file = (input_dir / sim_file).generic_string();
            }
        } else if (setup_config.has_simulation_file) {
            sim_file = (input_dir / setup_config.simulation_file).generic_string();
            hydroc::debug::LogDebug(std::string("Simulation file from setup: ") + setup_config.simulation_file);
        }
    } else {
        hydroc::debug::LogDebug("No setup file found, using command line arguments");
    }

    // Fallback to auto-detection if no setup file or missing fields
    if (!using_setup_file || model_file.empty()) {
        if (!model_file_arg.empty()) {
            model_file = model_file_arg;
            if (!std::filesystem::path(model_file).is_absolute()) {
                model_file = (input_dir / model_file).generic_string();
            }
        } else {
            model_file = FindFirstFile(input_dir, ".model.yaml");
            if (model_file.empty()) {
        hydroc::cli::LogError("Could not find .model.yaml file");
                hydroc::cli::LogError(std::string("Directory: ") + input_dir.generic_string());
                return false;
            }
        }
    }

    if (!using_setup_file || sim_file.empty()) {
        if (!sim_file_arg.empty()) {
            sim_file = sim_file_arg;
            if (!std::filesystem::path(sim_file).is_absolute()) {
                sim_file = (input_dir / sim_file).generic_string();
            }
        } else {
            sim_file = FindFirstFile(input_dir, ".simulation.yaml");
            if (sim_file.empty()) {
        hydroc::cli::LogError("Could not find .simulation.yaml file");
                hydroc::cli::LogError(std::string("Directory: ") + input_dir.generic_string());
                return false;
            }
        }
    }

    // Validate files exist
    hydroc::debug::LogDebug("Validating input files...");
    if (!std::filesystem::exists(model_file)) {
        hydroc::cli::LogError(std::string("Model file does not exist: ") + model_file);
        return false;
    }
    if (!std::filesystem::exists(sim_file)) {
        hydroc::cli::LogError(std::string("Simulation file does not exist: ") + sim_file);
        return false;
    }
    hydroc::debug::LogDebug("All input files validated successfully");
    
    return true;
}

std::shared_ptr<chrono::ChSystem> InitializeChronoSystem(const std::string& model_file, const std::string& sim_file) {
    hydroc::debug::LogDebug("Initializing Chrono system from YAML inputs...");
    
    try {
        hydroc::debug::LogDebug("Creating Chrono YAML parser");
        auto parser = chrono::parsers::ChParserMbsYAML();
        
        hydroc::debug::LogDebug(std::string("Loading simulation file: ") + sim_file);
        parser.LoadSimulationFile(sim_file);
        
        hydroc::debug::LogDebug("Creating system");
        auto system = parser.CreateSystem();
        
        hydroc::debug::LogDebug(std::string("Loading model file: ") + model_file);
        parser.LoadModelFile(model_file);
        
        hydroc::debug::LogDebug("Analyzing mesh files referenced in YAML model");
        std::filesystem::path model_dir = std::filesystem::path(model_file).parent_path();
        hydroc::debug::LogDebug(std::string("Model directory: ") + model_dir.generic_string());
        
        hydroc::debug::LogDebug("Populating system");
        parser.Populate(*system);
        hydroc::debug::LogDebug("System populated successfully");
        
        return system;
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("Failed to initialize Chrono system: ") + e.what());
        return nullptr;
    }
}

void DisplaySimulationSummary(const std::string& input_directory,
                             const std::string& model_file,
                             const std::string& sim_file,
                             const SetupConfig& setup_config,
                             chrono::ChSystem* system,
                             bool nogui,
                             const YAMLHydroData* hydro_data = nullptr) {
    
    // Get simulation parameters
    double timestep = 0.0;
    // Prefer YAML-declared time_step for display; fallback to system value
    if (!TryFindYamlDouble(sim_file, "time_step", timestep) || timestep <= 0.0) {
        timestep = system->GetStep();
    }
    int num_bodies = system->GetBodies().size();
    int num_constraints = system->GetLinks().size();
    int num_hydro_bodies = hydro_data ? hydro_data->bodies.size() : 0;
    
    // Modern, visually impressive summary display with consistent box styling
    std::vector<std::string> summary_content;
    
    // Input files section with proper alignment
    summary_content.push_back(hydroc::cli::CreateAlignedLine("🎯", "Simulation", std::filesystem::path(input_directory).filename().string()));
    summary_content.push_back(hydroc::cli::CreateAlignedLine("📁", "Directory", input_directory));
    summary_content.push_back(hydroc::cli::CreateAlignedLine("📄", "Model", std::filesystem::path(model_file).filename().string()));
    summary_content.push_back(hydroc::cli::CreateAlignedLine("⚙️", "Config", std::filesystem::path(sim_file).filename().string()));
    
    if (setup_config.has_hydro_file) {
        summary_content.push_back(hydroc::cli::CreateAlignedLine("🌊", "Hydro", setup_config.hydro_file));
    } else {
        summary_content.push_back(hydroc::cli::CreateAlignedLine("🌊", "Hydro", "None (no forces)"));
    }
    
    summary_content.push_back(""); // Empty line for spacing
    
    // System configuration section with proper alignment
    summary_content.push_back(hydroc::cli::CreateAlignedLine("🔗", "Chrono Bodies", std::to_string(num_bodies)));
    if (num_hydro_bodies > 0) {
        summary_content.push_back(hydroc::cli::CreateAlignedLine("🌊", "Hydro Bodies", std::to_string(num_hydro_bodies)));
    }
    summary_content.push_back(hydroc::cli::CreateAlignedLine("🔗", "Constraints", std::to_string(num_constraints)));
    double simulation_duration = 0.0;
    if (TryFindYamlDouble(sim_file, "end_time", simulation_duration) && simulation_duration > 0.0) {
        summary_content.push_back(hydroc::cli::CreateAlignedLine("⏱️", "Simulation Duration", hydroc::FormatNumber(simulation_duration, 1) + " s"));
    }
    summary_content.push_back(hydroc::cli::CreateAlignedLine("⏱️", "Time Step", hydroc::FormatNumber(timestep, 3) + " s"));
    summary_content.push_back(hydroc::cli::CreateAlignedLine("🖥️", "GUI", nogui ? "Disabled" : "Enabled"));
    
    if (setup_config.has_output_directory) {
        summary_content.push_back(hydroc::cli::CreateAlignedLine("📁", "Output", setup_config.output_directory));
    }
    
    hydroc::cli::ShowSectionBox("🚀 HydroChrono Simulation", summary_content);
    hydroc::cli::ShowEmptyLine();
}

// -----------------------------------------------------------------------------
// Main YAML runner implementation.
// -----------------------------------------------------------------------------
int RunHydroChronoFromYAML(int argc, char* argv[]) {
    try {
        // ---------------------------------------------------------------------
        // 0. Configure UTF-8 console output on Windows
        // ---------------------------------------------------------------------
#ifdef _WIN32
        // Enable UTF-8 console output on Windows
        SetConsoleOutputCP(CP_UTF8);
        std::ios_base::sync_with_stdio(false);
#endif

        // ---------------------------------------------------------------------
        // 1. CLI parsing (simplified - main CLI handling is done in main())
        // ---------------------------------------------------------------------
        std::string model_file_arg;
        std::string sim_file_arg;
        std::string input_directory;
        bool nogui = false;
        bool quiet_mode = false;
        bool enable_logging = false; // default: only log when --log is supplied
        bool debug_mode = false;
        bool trace_mode = false;

        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "--model" && i + 1 < argc) {
                model_file_arg = argv[++i];
            } else if (arg == "--sim" && i + 1 < argc) {
                sim_file_arg = argv[++i];
            } else if (arg == "--nogui") {
                nogui = true;
            } else if (arg == "--log") {
                enable_logging = true;
            } else if (arg == "--no-log") {
                enable_logging = false;
            } else if (arg == "--debug") {
                debug_mode = true;
            } else if (arg == "--trace") {
                trace_mode = true;
                debug_mode = true; // trace implies debug
            } else if (arg == "--nobanner") {
                // Optional: could disable banner; currently handled by enable_cli_output
            } else if (arg == "--quiet") {
                quiet_mode = true;
            } else if (arg.substr(0, 1) != "-") {
                // This is a positional argument (input directory)
                if (input_directory.empty()) {
                    input_directory = arg;
                }
            }
        }

        // Input directory should already be validated by main()
        std::filesystem::path input_dir(input_directory);

        // Setup logging
        std::string log_file_path;
        if (enable_logging) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << "hydrochrono_yaml_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".log";
            
            // Create logs directory in the input directory
            std::filesystem::path input_path(input_directory);
            std::filesystem::path logs_dir = input_path / "logs";
                if (!std::filesystem::exists(logs_dir)) {
                    std::filesystem::create_directories(logs_dir);
            }
            
            log_file_path = (logs_dir / ss.str()).generic_string();
        }
        
        // Initialize logging
        hydroc::LoggingConfig log_cfg;
        log_cfg.log_file_path = log_file_path;
        log_cfg.enable_cli_output = !quiet_mode;
        log_cfg.enable_file_output = !log_file_path.empty();
        log_cfg.enable_debug_logging = debug_mode; // gate dev logs
        // Console threshold: Debug if --debug, else Info. File threshold: always Debug to capture details.
        log_cfg.console_level = debug_mode ? hydroc::LogLevel::Debug
                                           : hydroc::LogLevel::Info;
        log_cfg.file_level = hydroc::LogLevel::Debug;
        hydroc::Initialize(log_cfg);
        hydroc::cli::ShowBanner();

        // ---------------------------------------------------------------------
        // 2. Setup and File Resolution (all internal - Debug level)
        // ---------------------------------------------------------------------
        std::string model_file;
        std::string sim_file;
        SetupConfig setup_config;
        
        if (!ResolveInputFiles(input_dir, model_file_arg, sim_file_arg, model_file, sim_file, setup_config)) {
            hydroc::Shutdown();
            return 1;
        }

        // ---------------------------------------------------------------------
        // 3. Initialize Chrono System (all internal - Debug level)
        // ---------------------------------------------------------------------
        auto system = InitializeChronoSystem(model_file, sim_file);
        if (!system) {
            hydroc::Shutdown();
            return 1;
        }

        // ---------------------------------------------------------------------
        // 4. Display Clean Summary Block (CLI visible)
        // ---------------------------------------------------------------------
        hydroc::cli::ShowSectionSeparator();
        DisplaySimulationSummary(input_directory, model_file, sim_file, setup_config, system.get(), nogui);
        
                // ---------------------------------------------------------------------
        // 5. Setup hydrodynamic forces and display wave info
        // ---------------------------------------------------------------------
        std::unique_ptr<TestHydro> test_hydro;
        YAMLHydroData hydro_data;
        // Prefer YAML-declared time_step for integration; fallback to system's step
        double loop_dt = system->GetStep();
        {
            double yaml_dt = 0.0;
            if (TryFindYamlDouble(sim_file, "time_step", yaml_dt) && yaml_dt > 0.0) {
                loop_dt = yaml_dt;
            }
        }
        
        if (setup_config.has_hydro_file) {
            std::filesystem::path hydro_file = std::filesystem::path(input_directory) / setup_config.hydro_file;
            hydroc::debug::LogDebug(std::string("Setting up hydrodynamic forces from: ") + hydro_file.generic_string());
            
            if (std::filesystem::exists(hydro_file)) {
                try {
                    hydroc::debug::LogDebug("Parsing hydro file...");
                    hydro_data = ReadHydroYAML(hydro_file.string());
                    hydroc::debug::LogDebug(std::string("Parsed ") + std::to_string(hydro_data.bodies.size()) + " body(ies)");
                    
                    // Get all bodies from the system
                    hydroc::debug::LogDebug("Finding Chrono bodies in system...");
                    std::vector<std::shared_ptr<chrono::ChBody>> bodies;
                    for (auto& body : system->GetBodies()) {
                        bodies.push_back(body);
                    }
                    hydroc::debug::LogDebug(std::string("Found ") + std::to_string(bodies.size()) + " Chrono body(ies)");
                    
                    // Setup hydrodynamic forces
                    hydroc::debug::LogDebug("Initializing TestHydro...");
                    // Provide a neutral horizon (Chrono governs runtime via YAML/UI)
                    test_hydro = SetupHydroFromYAML(hydro_data, bodies, loop_dt, /*time_end_hint*/ 0.0, 0.0);
                    hydroc::debug::LogDebug("Hydrodynamic forces initialized successfully");
                    
                    // Display wave information to CLI with enhanced formatting
                    hydroc::cli::ShowWaveModel(hydro_data.waves.type, 
                                           hydro_data.waves.height, 
                                           hydro_data.waves.period,
                                           hydro_data.waves.direction,
                                           hydro_data.waves.phase);
                    
                    // Example warning injection removed (kept in file logs only via interception)
                    
                } catch (const std::exception& e) {
                    hydroc::cli::LogError(std::string("Failed to setup hydrodynamic forces: ") + e.what());
                    hydroc::cli::CollectWarning("Continuing without hydrodynamic forces...");
                    hydroc::cli::ShowSummaryLine("🌊", "Type", "None (setup failed)", hydroc::LogColor::Yellow);
                }
            } else {
                hydroc::cli::LogWarning("Hydro file not found: " + hydro_file.generic_string());
                hydroc::cli::ShowSummaryLine("🌊", "Type", "None (file not found)", hydroc::LogColor::Yellow);
            }
        } else {
            hydroc::debug::LogDebug("No hydro file specified, running without hydrodynamic forces");
            hydroc::cli::ShowSummaryLine("🌊", "Type", "None (still water)", hydroc::LogColor::White);
        }

        // ---------------------------------------------------------------------
        // 6. Setup visualization (all internal - Debug level)
        // ---------------------------------------------------------------------
        // ========== TEMPORARY DIAGNOSTIC CODE FOR GUI CRASH DEBUGGING ==========
        // TODO: Remove this diagnostic block once GUI crash is resolved
        
        // Guard visualization setup - can be easily disabled for debugging
        bool enable_visualization_debug = true;  // TODO: Make this a command line flag
        
        hydroc::debug::LogDebug("🔍 PRE-VISUALIZATION: System state check");
        hydroc::debug::LogDebug(std::string("System fully initialized: ") + (system ? "YES" : "NO"));
        hydroc::debug::LogDebug(std::string("Bodies in system: ") + std::to_string(system->GetBodies().size()));
        hydroc::debug::LogDebug(std::string("System time: ") + hydroc::FormatNumber(system->GetChTime(), 6) + " s");
        
        // Log body states before visualization setup
        hydroc::debug::LogDebug("🔍 PRE-VISUALIZATION: Body states");
        auto pre_vis_bodies = system->GetBodies();
        for (size_t i = 0; i < pre_vis_bodies.size(); i++) {
            auto body = pre_vis_bodies[i];
            std::string body_name = body->GetName();
            if (body_name.empty()) {
                body_name = "Body" + std::to_string(i);
            }
            
            auto pos = body->GetPos();
            auto vel = body->GetPosDt();
            
            // Quick NaN/Inf check before visualization
            bool state_valid = std::isfinite(pos.x()) && std::isfinite(pos.y()) && std::isfinite(pos.z()) &&
                              std::isfinite(vel.x()) && std::isfinite(vel.y()) && std::isfinite(vel.z());
            
            if (!state_valid) {
                hydroc::cli::LogWarning("⚠️ INVALID BODY STATE detected in " + body_name + " before visualization setup!");
                enable_visualization_debug = false;  // Disable visualization if invalid state detected
            }
            
            hydroc::debug::LogDebug(std::string("  ") + body_name + " pos: (" + 
                       hydroc::FormatNumber(pos.x(), 3) + ", " + 
                       hydroc::FormatNumber(pos.y(), 3) + ", " + 
                       hydroc::FormatNumber(pos.z(), 3) + ") valid: " + (state_valid ? "YES" : "NO"));
        }
        // ========== END TEMPORARY DIAGNOSTIC CODE ==========
        
        hydroc::debug::LogDebug("Setting up visualization...");
        
        // ========== GUARDED VISUALIZATION SETUP ==========
        // TODO: Remove guards once GUI crash is resolved
        std::shared_ptr<hydroc::gui::UI> pui;
        try {
            hydroc::debug::LogDebug("🔍 Creating UI object (CreateUI)...");
            pui = hydroc::gui::CreateUI(!nogui && enable_visualization_debug);
            hydroc::debug::LogDebug("✅ UI object created successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during CreateUI: ") + e.what());
            hydroc::cli::LogWarning("Disabling visualization due to CreateUI failure");
            pui = hydroc::gui::CreateUI(true);  // Force nogui mode
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during CreateUI");
            hydroc::cli::LogWarning("Disabling visualization due to CreateUI failure");
            pui = hydroc::gui::CreateUI(true);  // Force nogui mode
        }
        
        hydroc::gui::UI& ui = *pui;

        try {
            hydroc::debug::LogDebug("🔍 Initializing UI with system...");
            ui.Init(system.get(), "HydroChrono YAML");
            hydroc::debug::LogDebug("✅ UI initialized successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during UI.Init: ") + e.what());
            hydroc::cli::LogWarning("UI initialization failed, continuing with limited functionality");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during UI.Init");
            hydroc::cli::LogWarning("UI initialization failed, continuing with limited functionality");
        }
        
        try {
            hydroc::debug::LogDebug("🔍 Setting camera position...");
            ui.SetCamera(0, -50, -10, 0, 0, -10);
            hydroc::debug::LogDebug("✅ Camera set successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during SetCamera: ") + e.what());
            hydroc::cli::LogWarning("Camera setup failed, using default position");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during SetCamera");
            hydroc::cli::LogWarning("Camera setup failed, using default position");
        }
        // ========== END GUARDED VISUALIZATION SETUP ==========
        
        hydroc::debug::LogDebug("Visualization setup complete");

        // ---------------------------------------------------------------------
        // 6.5. System Readiness Summary (CLI visible)
        // ---------------------------------------------------------------------
        hydroc::cli::ShowSectionSeparator();
        
        // Log system readiness message
        hydroc::cli::LogSuccess("✅ Chrono system initialized — ready to begin simulation loop");
        hydroc::cli::ShowEmptyLine();
        
        // Create system diagnostics summary
        std::vector<std::string> system_info_lines;
        system_info_lines.push_back(hydroc::cli::CreateAlignedLine("🔗", "Bodies", std::to_string(system->GetBodies().size())));
        system_info_lines.push_back(hydroc::cli::CreateAlignedLine("⚙️", "Constraints", std::to_string(system->GetLinks().size())));
        system_info_lines.push_back(hydroc::cli::CreateAlignedLine("⏱️", "Time Step", hydroc::FormatNumber(loop_dt, 4) + " s"));
        
        // Calculate approximate DOF (6 * num_bodies - constraint equations)
        int num_bodies = system->GetBodies().size();
        int num_constraints = system->GetLinks().size();
        int approx_dof = num_bodies * 6;  // Rough estimate
        system_info_lines.push_back(hydroc::cli::CreateAlignedLine("🎯", "Est. Degrees of Freedom", std::to_string(approx_dof)));
        
        hydroc::cli::ShowSectionBox("System Configuration", system_info_lines);

        // 🧭 Advanced solver diagnostics (conditional on debug/trace mode)
        if (debug_mode) {
            hydroc::cli::ShowEmptyLine();
            std::vector<std::string> solver_info_lines;
            
            // Get solver information
            auto solver = system->GetSolver();
            if (solver) {
                // Basic solver information
                solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("🔧", "Solver Type", "ChSolver (default)"));
                solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("🎯", "Max Iterations", "150 (default)"));
                solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("📐", "Tolerance", "1e-10 (default)"));
                
                // Try to get more specific solver info if available
                try {
                    // Note: ChSystem methods for solver details may vary by Chrono version
                    solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("🔍", "Solver State", "Active"));
                } catch (...) {
                    solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("🔍", "Solver State", "Unknown"));
                }
            } else {
                solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("⚠️", "Solver", "No solver detected"));
            }
            
            // System DOF information (more accurate if debug mode)
            solver_info_lines.push_back(hydroc::cli::CreateAlignedLine("📊", "System DOF", "Computing..."));
            
            hydroc::cli::ShowSectionBox("Solver Configuration", solver_info_lines);
        }

        // ---------------------------------------------------------------------
        // 6.9. Optional HDF5 exporter: write outputs under setup-configured folder
        // ---------------------------------------------------------------------
        std::unique_ptr<hydroc::SimulationExporter> exporter;
        std::filesystem::path resolved_output_dir;
        if (setup_config.has_output_directory && !setup_config.output_directory.empty()) {
            try {
                resolved_output_dir = std::filesystem::path(input_directory) / setup_config.output_directory;
                std::error_code ec;
                std::filesystem::create_directories(resolved_output_dir, ec);

                // Name output file based on wave type for predictability
                std::string wave_type = hydro_data.waves.type.empty() ? std::string("still") : hydro_data.waves.type;
                std::filesystem::path output_h5 = resolved_output_dir / (std::string("results.") + wave_type + ".h5");

                hydroc::SimulationExporter::Options exp_opts;
                exp_opts.output_path = output_h5.generic_string();
                exp_opts.input_model_file = model_file;
                exp_opts.input_simulation_file = sim_file;
                if (setup_config.has_hydro_file) {
                    exp_opts.input_hydro_file = (std::filesystem::path(input_directory) / setup_config.hydro_file).generic_string();
                }
                exp_opts.output_directory = resolved_output_dir.generic_string();
                exp_opts.scenario_type = wave_type;
                exporter = std::make_unique<hydroc::SimulationExporter>(exp_opts);

                // Write static info and model before stepping
                exporter->WriteSimulationInfo(system.get(), std::string(""), std::filesystem::path(model_file).filename().generic_string(), loop_dt, /*duration_seconds*/ 0.0);
                exporter->WriteModel(system.get());
                exporter->BeginResults(system.get(), /*expected_steps*/ 0);
            } catch (const std::exception& e) {
                hydroc::cli::LogWarning(std::string("HDF5 exporter disabled: ") + e.what());
                exporter.reset();
            }
        }

        // ---------------------------------------------------------------------
        // 7. Run simulation
        // ---------------------------------------------------------------------
        auto start_time = std::chrono::steady_clock::now();
        
        
        // Log simulation loop entry
        hydroc::cli::LogInfo("🕒 Entering simulation loop...");
        bool first_step = true;
        int step_count = 0;
        double initial_time = system->GetChTime();
        double previous_time = initial_time;
        
        // Get first body for position/velocity tracking (if available)
        std::shared_ptr<chrono::ChBody> first_body = nullptr;
        if (!system->GetBodies().empty()) {
            first_body = system->GetBodies()[0];
        }
        
        // Determine planned end time for headless runs (nogui)
        double yaml_end_time = 0.0;
        TryFindYamlDouble(sim_file, "end_time", yaml_end_time);

        if (nogui) {
            const double end_time_bound = (yaml_end_time > 0.0) ? yaml_end_time : 40.0;
            while (system->GetChTime() < end_time_bound) {
                double current_time = system->GetChTime();
                try {
                    system->DoStepDynamics(loop_dt);
                    step_count++;
                    if (exporter) {
                        exporter->RecordStep(system.get());
                    }
                    previous_time = current_time;
                } catch (const std::exception& e) {
                    hydroc::cli::LogError(std::string("🔥 Exception during DoStepDynamics at step ") + std::to_string(step_count) + ": " + e.what());
                    hydroc::cli::LogError(std::string("Simulation time: ") + hydroc::FormatNumber(current_time, 6) + " s");
                    hydroc::cli::LogError(std::string("Step size: ") + hydroc::FormatNumber(loop_dt, 6) + " s");
                    break;
                } catch (...) {
                    hydroc::cli::LogError(std::string("🔥 Unknown exception during DoStepDynamics at step ") + std::to_string(step_count));
                    hydroc::cli::LogError(std::string("Simulation time: ") + hydroc::FormatNumber(current_time, 6) + " s");
                    break;
                }
            }
        } else {
            // GUI-driven loop: respects pause via ui.simulationStarted and closes when window stops
            while (ui.IsRunning(loop_dt)) {
                // Only step simulation if not paused (matches demo pattern)
                if (ui.simulationStarted) {
                double current_time = system->GetChTime();
                
                // 🔁 Step-level diagnostics (every step when in trace mode, conditional otherwise)
                if (trace_mode) {
                    std::string step_info = "⏱️ t = " + hydroc::FormatNumber(current_time, 3) + " s";
                    
                    // Add first body position/velocity if available and in trace mode
                    if (first_body) {
                        auto pos = first_body->GetPos();
                        auto vel = first_body->GetPosDt();
                        step_info += " | Body0: pos=(" + hydroc::FormatNumber(pos.x(), 2) + "," + 
                                   hydroc::FormatNumber(pos.y(), 2) + "," + hydroc::FormatNumber(pos.z(), 2) + ")";
                        step_info += " vel=(" + hydroc::FormatNumber(vel.x(), 2) + "," + 
                                   hydroc::FormatNumber(vel.y(), 2) + "," + hydroc::FormatNumber(vel.z(), 2) + ")";
                    }
                    
                    hydroc::debug::LogDebug(step_info);
                } else if (debug_mode && step_count % 25 == 0) {
                    // In debug mode, log every 25 steps
                    hydroc::debug::LogDebug(std::string("⏱️ t = ") + hydroc::FormatNumber(current_time, 3) + " s (step " + std::to_string(step_count) + ")");
                } else if (step_count % 50 == 0) {
                    // Standard logging every 50 steps
                    hydroc::debug::LogDebug(std::string("⏱️ t = ") + hydroc::FormatNumber(current_time, 3) + " s (step " + std::to_string(step_count) + ")");
                }
                
                try {
                    // 🧯 Scoped try/catch around DoStepDynamics
                    system->DoStepDynamics(loop_dt);
                    step_count++;
                    if (exporter) {
                        exporter->RecordStep(system.get());
                    }
                    
                    // 🧯 After first step - check if simulation time advanced
                    if (first_step) {
                        double new_time = system->GetChTime();
                        if (std::abs(new_time - current_time) < 1e-12) {
                            hydroc::cli::LogWarning("⚠️ Simulation did not progress — check constraints, initial state, or instability");
                            hydroc::cli::LogWarning(std::string("Time before step: ") + hydroc::FormatNumber(current_time, 6) + " s");
                            hydroc::cli::LogWarning(std::string("Time after step:  ") + hydroc::FormatNumber(new_time, 6) + " s");
                            hydroc::cli::LogWarning(std::string("Time difference:  ") + hydroc::FormatNumber(new_time - current_time, 10) + " s");
                            
                            // Log additional diagnostics for stalled simulation
                            if (debug_mode) {
                                hydroc::debug::LogDebug("🔍 Checking system state for stall...");
                                hydroc::debug::LogDebug(std::string("Bodies count: ") + std::to_string(system->GetBodies().size()));
                                hydroc::debug::LogDebug(std::string("Constraints count: ") + std::to_string(system->GetLinks().size()));
                                if (first_body) {
                                    auto pos = first_body->GetPos();
                                    auto vel = first_body->GetPosDt();
                                    hydroc::debug::LogDebug(std::string("First body position: (") + 
                                              hydroc::FormatNumber(pos.x(), 6) + ", " + 
                                              hydroc::FormatNumber(pos.y(), 6) + ", " + 
                                              hydroc::FormatNumber(pos.z(), 6) + ")");
                                    hydroc::debug::LogDebug(std::string("First body velocity: (") + 
                                              hydroc::FormatNumber(vel.x(), 6) + ", " + 
                                              hydroc::FormatNumber(vel.y(), 6) + ", " + 
                                              hydroc::FormatNumber(vel.z(), 6) + ")");
                                }
                            }
                        } else {
                            if (debug_mode) {
                                hydroc::debug::LogDebug(std::string("✅ Simulation progressing normally (Δt = ") + 
                                          hydroc::FormatNumber(new_time - current_time, 6) + " s)");
                            }
                        }
                        
                        // ========== TEMPORARY DIAGNOSTIC CODE FOR GUI CRASH DEBUGGING ==========
                        // TODO: Remove this diagnostic block once GUI crash is resolved
                        hydroc::debug::LogDebug("🔍 POST-FIRST-STEP: Logging all body states for GUI crash debugging");
                        
                        // Log position and velocity of each Chrono body
                        auto bodies = system->GetBodies();
                        for (size_t i = 0; i < bodies.size(); i++) {
                            auto body = bodies[i];
                            std::string body_name = body->GetName();
                            if (body_name.empty()) {
                                body_name = "Body" + std::to_string(i);
                            }
                            
                            auto pos = body->GetPos();
                            auto vel = body->GetPosDt();
                            auto ang_vel = body->GetAngVelParent();
                            
                            // Check for NaN/Inf in body state
                            bool has_invalid_state = false;
                            std::string invalid_components;
                            
                            if (!std::isfinite(pos.x()) || !std::isfinite(pos.y()) || !std::isfinite(pos.z())) {
                                has_invalid_state = true;
                                invalid_components += "position ";
                            }
                            if (!std::isfinite(vel.x()) || !std::isfinite(vel.y()) || !std::isfinite(vel.z())) {
                                has_invalid_state = true;
                                invalid_components += "velocity ";
                            }
                            double ang_vel_x = ang_vel.x();
                            double ang_vel_y = ang_vel.y(); 
                            double ang_vel_z = ang_vel.z();
                            if (!std::isfinite(ang_vel_x) || !std::isfinite(ang_vel_y) || !std::isfinite(ang_vel_z)) {
                                has_invalid_state = true;
                                invalid_components += "angular_velocity ";
                            }
                            
                            if (has_invalid_state) {
                                hydroc::cli::LogWarning("⚠️ INVALID BODY STATE DETECTED in " + body_name + ": " + invalid_components);
                                hydroc::cli::LogWarning("  Position: (" + std::to_string(pos.x()) + ", " + std::to_string(pos.y()) + ", " + std::to_string(pos.z()) + ")");
                                hydroc::cli::LogWarning("  Velocity: (" + std::to_string(vel.x()) + ", " + std::to_string(vel.y()) + ", " + std::to_string(vel.z()) + ")");
                                hydroc::cli::LogWarning("  Angular Vel: (" + std::to_string(ang_vel_x) + ", " + std::to_string(ang_vel_y) + ", " + std::to_string(ang_vel_z) + ")");
                            } else {
                                hydroc::debug::LogDebug(std::string("✅ ") + body_name + " state valid:");
                                hydroc::debug::LogDebug(std::string("  Position: (") + 
                                          hydroc::FormatNumber(pos.x(), 6) + ", " + 
                                          hydroc::FormatNumber(pos.y(), 6) + ", " + 
                                          hydroc::FormatNumber(pos.z(), 6) + ")");
                                hydroc::debug::LogDebug(std::string("  Velocity: (") + 
                                          hydroc::FormatNumber(vel.x(), 6) + ", " + 
                                          hydroc::FormatNumber(vel.y(), 6) + ", " + 
                                          hydroc::FormatNumber(vel.z(), 6) + ")");
                                hydroc::debug::LogDebug(std::string("  Angular Vel: (") + 
                                          hydroc::FormatNumber(ang_vel_x, 6) + ", " + 
                                          hydroc::FormatNumber(ang_vel_y, 6) + ", " + 
                                          hydroc::FormatNumber(ang_vel_z, 6) + ")");
                            }
                        }
                        hydroc::debug::LogDebug("🔍 END POST-FIRST-STEP DIAGNOSTICS");
                        // ========== END TEMPORARY DIAGNOSTIC CODE ==========
                        
                        first_step = false;
                    }
                    
                    // 🔁 Every N steps - log solver convergence info (if available)
                    if (debug_mode && step_count % 25 == 0) {
                        // Note: Solver convergence info might not be directly accessible in all Chrono versions
                        // This is a placeholder for where such information would be logged
                        if (trace_mode) {
                            hydroc::debug::LogDebug(std::string("🔍 Step ") + std::to_string(step_count) + " solver info: [convergence data not available]");
                        }
                    }
                    
                    previous_time = current_time;
                    
                } catch (const std::exception& e) {
                    // Enhanced exception handling with more diagnostics
                    hydroc::cli::LogError(std::string("🔥 Exception during DoStepDynamics at step ") + std::to_string(step_count) + ": " + e.what());
                    hydroc::cli::LogError(std::string("Simulation time: ") + hydroc::FormatNumber(current_time, 6) + " s");
                    hydroc::cli::LogError(std::string("Step size: ") + hydroc::FormatNumber(loop_dt, 6) + " s");
                    
                    if (debug_mode && first_body) {
                        auto pos = first_body->GetPos();
                        auto vel = first_body->GetPosDt();
                        hydroc::cli::LogError("First body state at failure:");
                        hydroc::cli::LogError(std::string("  Position: (") + hydroc::FormatNumber(pos.x(), 6) + ", " + 
                                    hydroc::FormatNumber(pos.y(), 6) + ", " + hydroc::FormatNumber(pos.z(), 6) + ")");
                        hydroc::cli::LogError(std::string("  Velocity: (") + hydroc::FormatNumber(vel.x(), 6) + ", " + 
                                    hydroc::FormatNumber(vel.y(), 6) + ", " + hydroc::FormatNumber(vel.z(), 6) + ")");
                    }
                    
                    hydroc::cli::LogWarning("This may indicate numerical instability, constraint conflicts, or configuration issues");
                    break;  // Exit simulation loop on exception
                } catch (...) {
                    // Catch any other exceptions with enhanced diagnostics
                    hydroc::cli::LogError(std::string("🔥 Unknown exception during DoStepDynamics at step ") + std::to_string(step_count));
                    hydroc::cli::LogError(std::string("Simulation time: ") + hydroc::FormatNumber(current_time, 6) + " s");
                    hydroc::cli::LogError("This indicates a serious system error");
                    break;  // Exit simulation loop on exception
                }
            }
        }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Final results display (CLI visible)
        hydroc::cli::ShowSimulationResults(system->GetChTime(), static_cast<int>(system->GetChTime() / loop_dt), duration.count() / 1000.0);

        // Finalize HDF5 output with runtime metadata
        if (exporter) {
            double wall_s = std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();
            exporter->SetRunMetadata(std::string(""), std::string(""), wall_s, step_count, loop_dt, system->GetChTime());
            exporter->Finalize();
        }
        
        // Display warnings section if any warnings were collected
        hydroc::cli::DisplayWarnings();
        
        // Display log file location
        hydroc::cli::ShowLogFileLocation(log_file_path);
        
        // Enhanced footer with tagline
        hydroc::cli::ShowFooter();

        // Shutdown the logger
        hydroc::Shutdown();

        return 0;
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("Exception: ") + e.what());
        hydroc::Shutdown();
        return 1;
    } catch (...) {
        hydroc::cli::LogError("Unknown exception occurred.");
        hydroc::Shutdown();
        return 1;
    }
}

}  // namespace hydroc 