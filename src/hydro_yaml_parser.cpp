/*********************************************************************
 * @file  hydro_yaml_parser.cpp
 *
 * @brief Implementation of YAML parser for hydro.yaml files.
 *********************************************************************/

#include "hydro_yaml_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace {

/**
 * @brief Get the indentation level of a line.
 * 
 * @param line The line to check
 * @return Number of spaces at the beginning of the line
 */
int GetIndentation(const std::string& line) {
    int indent = 0;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            indent++;
        } else {
            break;
        }
    }
    return indent;
}

/**
 * @brief Simple YAML line parser for key-value pairs.
 * 
 * @param line The line to parse
 * @param key Output key
 * @param value Output value
 * @return true if successfully parsed, false otherwise
 */
bool ParseYAMLLine(const std::string& line, std::string& key, std::string& value) {
    // Remove leading/trailing whitespace
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    // Skip empty lines and comments
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }
    
    // Find the colon separator
    size_t colon_pos = trimmed.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    // Extract key and value
    key = trimmed.substr(0, colon_pos);
    value = trimmed.substr(colon_pos + 1);
    
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // Remove quotes if present
    if (value.length() >= 2 && value[0] == '"' && value[value.length()-1] == '"') {
        value = value.substr(1, value.length() - 2);
    }
    
    return true;
}

/**
 * @brief Parse a double value from string.
 * 
 * @param str The string to parse
 * @param default_val Default value if parsing fails
 * @return Parsed double value
 */
double ParseDouble(const std::string& str, double default_val = 0.0) {
    try {
        return std::stod(str);
    } catch (const std::exception&) {
        return default_val;
    }
}

/**
 * @brief Parse a boolean value from string.
 * 
 * @param str The string to parse
 * @param default_val Default value if parsing fails
 * @return Parsed boolean value
 */
bool ParseBool(const std::string& str, bool default_val = true) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "true" || lower == "yes" || lower == "1") {
        return true;
    } else if (lower == "false" || lower == "no" || lower == "0") {
        return false;
    }
    return default_val;
}

/**
 * @brief Resolve a relative path based on the YAML file's location.
 * 
 * @param path The path to resolve (can be relative or absolute)
 * @param yaml_file_path The path to the YAML file
 * @return Resolved absolute path
 */
std::string ResolvePath(const std::string& path, const std::string& yaml_file_path) {
    std::filesystem::path file_path(path);
    
    if (file_path.is_absolute()) {
        return path;
    }
    
    std::filesystem::path yaml_path(yaml_file_path);
    std::filesystem::path yaml_dir = yaml_path.parent_path();
    std::filesystem::path resolved_path = yaml_dir / file_path;
    
    try {
        return std::filesystem::weakly_canonical(resolved_path).string();
    } catch (const std::exception&) {
        return resolved_path.string();
    }
}

} // anonymous namespace

YAMLHydroData ReadHydroYAML(const std::string& hydro_file_path) {
    YAMLHydroData data;
    
    std::ifstream file(hydro_file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open hydro file: " + hydro_file_path);
    }
    
    std::string line;
    bool in_hydrodynamics = false;
    bool in_bodies = false;
    bool in_waves = false;
    bool in_body = false;
    HydroBody current_body;
    int line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Get indentation level
        int indent = GetIndentation(line);
        
        // Remove leading/trailing whitespace
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        
        // Check for section headers based on indentation
        if (indent == 0 && trimmed == "hydrodynamics:") {
            in_hydrodynamics = true;
            in_bodies = false;
            in_waves = false;
            in_body = false;
            continue;
        }
        
        if (!in_hydrodynamics) {
            continue; // Skip everything until we find hydrodynamics section
        }
        
        // Check for subsections (indent = 2)
        if (indent == 2 && trimmed == "bodies:") {
            in_bodies = true;
            in_waves = false;
            in_body = false;
            continue;
        }
        
        if (indent == 2 && trimmed == "waves:") {
            // Save current body before switching to waves section
            if (in_body && !current_body.name.empty()) {
                data.bodies.push_back(current_body);
            }
            in_waves = true;
            in_bodies = false;
            in_body = false;
            continue;
        }
        
        // Check for body list item (indent = 4, starts with "- name")
        if (in_bodies && indent == 4 && trimmed.substr(0, 6) == "- name") {
            // Save previous body if exists
            if (in_body && !current_body.name.empty()) {
                data.bodies.push_back(current_body);
            }
            
            // Start new body
            current_body = HydroBody();
            in_body = true;
            
            // Parse the name from this line (remove the "- " prefix first)
            std::string name_line = trimmed.substr(2); // Remove "- "
            std::string key, value;
            if (ParseYAMLLine(name_line, key, value)) {
                if (key == "name") {
                    current_body.name = value;
                }
            }
            continue;
        }
        
        // Parse key-value pairs (indent = 6 for body properties, indent = 4 for wave properties)
        if ((in_body && indent == 6) || (in_waves && indent == 4)) {
            std::string key, value;
            if (ParseYAMLLine(line, key, value)) {
                if (in_body) {
                    // Parse body properties
                    if (key == "name") {
                        current_body.name = value;
                    } else if (key == "h5_file") {
                        current_body.h5_file = ResolvePath(value, hydro_file_path);
                    } else if (key == "include_excitation") {
                        current_body.include_excitation = ParseBool(value, true);
                    } else if (key == "include_radiation") {
                        current_body.include_radiation = ParseBool(value, true);
                    } else if (key == "radiation_calculation") {
                        current_body.radiation_calculation = value;
                    }
                } else if (in_waves) {
                    // Parse wave properties
                    if (key == "type") {
                        data.waves.type = value;
                    } else if (key == "height") {
                        data.waves.height = ParseDouble(value, 0.0);
                    } else if (key == "period") {
                        data.waves.period = ParseDouble(value, 0.0);
                    } else if (key == "direction") {
                        data.waves.direction = ParseDouble(value, 0.0);
                    } else if (key == "phase") {
                        data.waves.phase = ParseDouble(value, 0.0);
                    } else if (key == "spectrum") {
                        data.waves.spectrum = value;
                    }
                }
            }
        }
    }
    
    // Don't forget to add the last body
    if (in_body && !current_body.name.empty()) {
        data.bodies.push_back(current_body);
    }
    
    // Validate that we found the required sections
    if (!in_hydrodynamics) {
        throw std::runtime_error("No 'hydrodynamics:' section found in hydro file: " + hydro_file_path);
    }
    
    if (data.bodies.empty()) {
        std::cerr << "WARNING: No bodies found in hydro file: " << hydro_file_path << std::endl;
    }
    
    return data;
}