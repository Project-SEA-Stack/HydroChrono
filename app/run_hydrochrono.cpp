/**
 * @file run_hydrochrono.cpp
 * @brief CLI entrypoint for the HydroChrono YAML-based runner
 */

#include <hydroc/hydrochrono_runner/run_hydrochrono_from_yaml.h>
#include <hydroc/version.h>
#include "../src/utils/misc_options.h"
#include <hydroc/logging.h>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

static void PrintBanner() noexcept { hydroc::cli::ShowBanner(); }

static void PrintVersion() noexcept { hydroc::cli::LogInfo(std::string(HYDROCHRONO_NAME) + " version " + HYDROCHRONO_VERSION); }

static void PrintInfo() noexcept { hydroc::cli::ShowBanner(); }

void PrintHelp(const char* program_name) {
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("USAGE");
    hydroc::cli::LogInfo(std::string("  ") + program_name + " [options] <input_directory>");
    hydroc::cli::LogInfo(std::string("  ") + program_name + " [options] <model.setup.yaml>");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("OPTIONS");
    hydroc::cli::LogInfo("  -h, --help           Show this help message and exit");
    hydroc::cli::LogInfo("  -v, --version        Show HydroChrono version and exit");
    hydroc::cli::LogInfo("  -i, --info           Print project and license info");
    hydroc::cli::LogInfo("      --nogui          Disable GUI visualization");
    hydroc::cli::LogInfo("      --log            Enable detailed logging to file");
    hydroc::cli::LogInfo("      --model_file     Override model YAML file (default: auto-detected)");
    hydroc::cli::LogInfo("      --sim_file       Override simulation YAML file (default: auto-detected)");
    hydroc::cli::LogInfo("      --nobanner       Disable banner display");
    hydroc::cli::LogInfo("      --quiet          Quiet mode (minimal output)");
    hydroc::cli::LogInfo("      --debug          Enable detailed simulation diagnostics");
    hydroc::cli::LogInfo("      --trace          Enable step-by-step simulation tracing (implies --debug)");
    hydroc::cli::LogInfo("      --output-h5 PATH Export results to HDF5 file with model+results");
    hydroc::cli::LogInfo("      --h5-verbose     Print detailed HDF5 discovery/sampling diagnostics");
    hydroc::cli::LogInfo("      --tag STR        Append __STR to generated HDF5 filename (before .h5)");
    hydroc::cli::LogInfo("      --fail-fast      Stop on first failed run when sweeping periods");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("EXAMPLES");
    hydroc::cli::LogInfo(std::string("  # Run simulation with GUI using directory"));
    hydroc::cli::LogInfo(std::string("  ") + program_name + " ./cases/slider_crank/");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo(std::string("  # Run simulation using setup file directly"));
    hydroc::cli::LogInfo(std::string("  ") + program_name + " ./cases/slider_crank/model.setup.yaml");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo(std::string("  # Run simulation without GUI (headless mode)"));
    hydroc::cli::LogInfo(std::string("  ") + program_name + " ./my_case/ --nogui");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo(std::string("  # Override YAML files"));
    hydroc::cli::LogInfo(std::string("  ") + program_name + " ./ --model_file alt.model.yaml --sim_file alt.sim.yaml");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("INPUT DIRECTORY");
    hydroc::cli::LogInfo("  Directory containing *.setup.yaml or individual YAML files:");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("  - *.setup.yaml         (optional, recommended)");
    hydroc::cli::LogInfo("    → defines model/simulation/hydro/output files");
    hydroc::cli::ShowEmptyLine();
    hydroc::cli::LogInfo("  - *.model.yaml         (required if no setup file)");
    hydroc::cli::LogInfo("  - *.simulation.yaml    (required if no setup file)");
    hydroc::cli::ShowEmptyLine();
}

struct CLIArgs {
    std::string input_directory;
    std::string model_file;
    std::string sim_file;
    bool nogui = false;
    bool log = false;
    bool nobanner = false;              // NEW: Disable banner display
    bool quiet = false;                 // NEW: Quiet mode (minimal output)
    bool debug = false;                 // NEW: Enable detailed simulation diagnostics
    bool trace = false;                 // NEW: Enable step-by-step simulation tracing
    std::string output_h5;              // NEW: Export HDF5 results path
    bool h5_verbose = false;            // NEW: HDF5 verbose diagnostics
    std::string h5_tag;                 // NEW: Optional tag appended to filename
    bool fail_fast = false;             // NEW: Stop on first failure during sweep
    bool profile = false;               // NEW: Enable runtime profiling summary
};

static CLIArgs ParseArguments(int argc, char* argv[]) {
    CLIArgs args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--nogui") {
            args.nogui = true;
        } else if (arg == "--log" || arg == "--logging") {
            args.log = true;
        } else if (arg == "--nobanner") {
            args.nobanner = true;
        } else if (arg == "--quiet") {
            args.quiet = true;
        } else if (arg == "--debug") {
            args.debug = true;
        } else if (arg == "--trace") {
            args.trace = true;
            args.debug = true;  // trace implies debug
        } else if (arg == "--model_file") {
            if (i + 1 < argc) {
                args.model_file = argv[++i];
            } else {
                hydroc::cli::LogError("ERROR: --model_file requires a file path argument");
                std::exit(1);
            }
        } else if (arg == "--sim_file") {
            if (i + 1 < argc) {
                args.sim_file = argv[++i];
            } else {
                hydroc::cli::LogError("ERROR: --sim_file requires a file path argument");
                std::exit(1);
            }
        } else if (arg == "--output-h5") {
            if (i + 1 < argc) {
                args.output_h5 = argv[++i];
            } else {
                hydroc::cli::LogError("ERROR: --output-h5 requires a file path argument");
                std::exit(1);
            }
        } else if (arg == "--h5-verbose") {
            args.h5_verbose = true;
        } else if (arg == "--tag") {
            if (i + 1 < argc) {
                args.h5_tag = argv[++i];
            } else {
                hydroc::cli::LogError("ERROR: --tag requires a value");
                std::exit(1);
            }
        } else if (arg == "--fail-fast") {
            args.fail_fast = true;
        } else if (arg == "--profile") {
            args.profile = true;
        } else if (arg.substr(0, 1) != "-") {
            // This is a positional argument (input directory)
            if (args.input_directory.empty()) {
                args.input_directory = arg;
            } else {
                hydroc::cli::LogError("ERROR: Multiple input directories specified. Only one is allowed.");
                std::exit(1);
            }
        } else {
            hydroc::cli::LogError(std::string("ERROR: Unknown option: ") + arg);
            hydroc::cli::LogInfo("Use --help for usage information.");
            std::exit(1);
        }
    }
    
    return args;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // ---------------------------------------------------------------------
    // Configure UTF-8 console output on Windows (must be first!)
    // ---------------------------------------------------------------------
#ifdef _WIN32
    // Enable UTF-8 console output on Windows
    SetConsoleOutputCP(CP_UTF8);
    std::ios_base::sync_with_stdio(false);
#endif

    // Check for hidden options first (before any other processing)
    if (hydroc::misc::HandleHiddenOptions(argc, argv)) {
        return 0;
    }
    
    // Check for help/version/info flags first (before requiring input directory)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            hydroc::LoggingConfig cfg;
            cfg.enable_cli_output = true;
            cfg.enable_file_output = false;
            cfg.console_level = hydroc::LogLevel::Info;
            cfg.file_level = hydroc::LogLevel::Info;
            hydroc::Initialize(cfg);
            PrintHelp(argv[0]);
            hydroc::Shutdown();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            hydroc::LoggingConfig cfg;
            cfg.enable_cli_output = true;
            cfg.enable_file_output = false;
            cfg.console_level = hydroc::LogLevel::Info;
            cfg.file_level = hydroc::LogLevel::Info;
            hydroc::Initialize(cfg);
            PrintVersion();
            hydroc::Shutdown();
            return 0;
        } else if (arg == "--info" || arg == "-i") {
            hydroc::LoggingConfig cfg;
            cfg.enable_cli_output = true;
            cfg.enable_file_output = false;
            cfg.console_level = hydroc::LogLevel::Info;
            cfg.file_level = hydroc::LogLevel::Info;
            hydroc::Initialize(cfg);
            PrintInfo();
            hydroc::Shutdown();
            return 0;
        }
    }
    
    // Parse command line arguments
    CLIArgs args = ParseArguments(argc, argv);
    
    // Validate required input directory or setup file
    if (args.input_directory.empty()) {
        hydroc::cli::LogError("ERROR: Input directory or setup file is required");
        hydroc::cli::ShowEmptyLine();
        hydroc::cli::LogInfo(std::string("Usage: ") + argv[0] + " [options] <input_directory_or_setup_file>");
        hydroc::cli::LogInfo("Use --help for more information.");
        return 1;
    }
    
    // Check if input is a setup file or directory
    std::filesystem::path input_path(args.input_directory);
    if (std::filesystem::exists(input_path)) {
        if (std::filesystem::is_regular_file(input_path)) {
            // Check if it's a setup file
            if (input_path.extension() == ".yaml") {
                const std::string filename = input_path.filename().string();
                const std::string suffix = ".setup.yaml";
                if (filename.length() >= suffix.length() && 
                    filename.compare(filename.length() - suffix.length(), suffix.length(), suffix) == 0) {
                    // Convert setup file path to directory path
                    args.input_directory = input_path.parent_path().string();
                    hydroc::cli::LogInfo(std::string("Loaded setup file: ") + input_path.string());
                } else {
                    hydroc::cli::LogError("ERROR: File provided is not a valid .setup.yaml file");
                    hydroc::cli::LogInfo(std::string("  Path: ") + args.input_directory);
                    hydroc::cli::LogInfo("  Expected: Directory or any file ending in '.setup.yaml'");
                    return 1;
                }
            }
        } else if (!std::filesystem::is_directory(input_path)) {
            hydroc::cli::LogError("ERROR: Path is neither a directory nor a regular file");
            hydroc::cli::LogInfo(std::string("  Path: ") + args.input_directory);
            return 1;
        }
    } else {
        hydroc::cli::LogError("ERROR: Input path does not exist");
        hydroc::cli::LogInfo(std::string("  Path: ") + args.input_directory);
        return 1;
    }
    
    // Note: Banner will be rendered by the YAML runner
    
    // Prepare arguments for the YAML runner
    std::vector<std::string> runner_args;
    runner_args.push_back(argv[0]);  // program name
    
    // Add input directory
    runner_args.push_back(args.input_directory);
    
    // Add optional flags
    if (args.nogui) {
        runner_args.push_back("--nogui");
    }
    
    // Add logging flag if requested
    if (args.log) {
        runner_args.push_back("--log");
    }
    
    // Add new CLI options
    if (args.nobanner) {
        runner_args.push_back("--nobanner");
    }
    
    if (args.quiet) {
        runner_args.push_back("--quiet");
    }
    
    if (args.debug) {
        runner_args.push_back("--debug");
    }
    
    if (args.trace) {
        runner_args.push_back("--trace");
    }
    
    if (args.profile) {
        runner_args.push_back("--profile");
    }
    
    if (!args.model_file.empty()) {
        runner_args.push_back("--model_file");
        runner_args.push_back(args.model_file);
    }
    
    if (!args.sim_file.empty()) {
        runner_args.push_back("--sim_file");
        runner_args.push_back(args.sim_file);
    }
    if (!args.output_h5.empty()) {
        runner_args.push_back("--output-h5");
        runner_args.push_back(args.output_h5);
    }
    if (args.fail_fast) {
        runner_args.push_back("--fail-fast");
    }
    
    // Convert to argc/argv format for the runner
    std::vector<char*> runner_argv;
    for (const auto& arg : runner_args) {
        runner_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    // Call the YAML runner
    return hydroc::RunHydroChronoFromYAML(static_cast<int>(runner_argv.size()), runner_argv.data());
}
