#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace hydroc {

/// Structure to hold parsed setup file configuration
struct SetupConfig {
    std::string model_file;
    std::string simulation_file;
    std::string hydro_file;        // Not used yet, prepared for future
    std::string output_directory;  // Not used yet, prepared for future
    
    bool has_model_file = false;
    bool has_simulation_file = false;
    bool has_hydro_file = false;
    bool has_output_directory = false;
};

/// Parse a model.setup.yaml file and return configuration
/// @param setup_path Path to the model.setup.yaml file
/// @return SetupConfig structure with parsed values
SetupConfig ParseSetupFile(const std::filesystem::path& setup_path);

/// Check if a model.setup.yaml file exists in the given directory
/// @param directory Directory to check for model.setup.yaml
/// @return Path to setup file if it exists, empty path otherwise
std::filesystem::path FindSetupFile(const std::filesystem::path& directory);

} // namespace hydroc 