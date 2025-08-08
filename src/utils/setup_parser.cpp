#include "setup_parser.h"
#include <hydroc/logging.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace hydroc {

std::filesystem::path FindSetupFile(const std::filesystem::path& directory) {
    // First try the traditional model.setup.yaml for backward compatibility
    auto setup_path = directory / "model.setup.yaml";
    if (std::filesystem::exists(setup_path) && std::filesystem::is_regular_file(setup_path)) {
        return setup_path;
    }
    
    // Then search for any *.setup.yaml files
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();
                const std::string suffix = ".setup.yaml";
                if (path.extension() == ".yaml" && 
                    filename.length() >= suffix.length() && 
                    filename.compare(filename.length() - suffix.length(), suffix.length(), suffix) == 0) {
                    return path;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Directory doesn't exist or can't be read
    }
    
    return {};
}

SetupConfig ParseSetupFile(const std::filesystem::path& setup_path) {
    SetupConfig config;
    
    std::ifstream file(setup_path);
    if (!file.is_open()) {
        hydroc::cli::LogWarning("Could not open setup file: " + setup_path.string());
        return config;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Simple YAML parser - look for key: value pairs
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Find the colon separator
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        
        // Extract key and value
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // Remove inline comments (everything after #)
        size_t comment_pos = value.find('#');
        if (comment_pos != std::string::npos) {
            value = value.substr(0, comment_pos);
        }
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse known keys
        if (key == "model_file") {
            config.model_file = value;
            config.has_model_file = true;
        } else if (key == "simulation_file") {
            config.simulation_file = value;
            config.has_simulation_file = true;
        } else if (key == "hydro_file") {
            config.hydro_file = value;
            config.has_hydro_file = true;
            hydroc::debug::LogDebug(std::string("Hydrodynamics file: ") + value);
        } else if (key == "output_directory") {
            config.output_directory = value;
            config.has_output_directory = true;
            hydroc::debug::LogDebug(std::string("Output directory: ") + value + " (not used yet)");
        }
    }
    
    return config;
}

} // namespace hydroc 