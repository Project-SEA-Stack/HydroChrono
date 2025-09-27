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
    
    bool in_period_block = false;
    int period_block_indent = 0;
    bool period_block_seen = false;
    bool period_form_values = false;
    bool period_form_linspace = false;
    bool period_form_range = false;

    auto parse_inline_brace_kv = [](const std::string& v) -> std::vector<std::pair<std::string, std::string>> {
        // Expect something like: { start: 6.0, stop: 9.0, num: 4 }
        std::vector<std::pair<std::string, std::string>> out;
        size_t lb = v.find('{');
        size_t rb = v.find('}');
        if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return out;
        std::string inner = v.substr(lb + 1, rb - lb - 1);
        // split on commas
        std::stringstream ss(inner);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto pos = token.find(':');
            if (pos == std::string::npos) continue;
            std::string k = token.substr(0, pos);
            std::string val = token.substr(pos + 1);
            // trim
            k.erase(0, k.find_first_not_of(" \t"));
            k.erase(k.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t") + 1);
            // strip optional quotes
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
            out.emplace_back(k, val);
        }
        return out;
    };

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
        
        // Parse key-value pairs
        // - Body properties at indent == 6
        // - Wave properties at indent == 4
        // - Nested period block under waves at indent >= period_block_indent + 2 when in_period_block
        {
            std::string key;
            std::string value;
            bool should_parse = ( (in_body && indent == 6) || (in_waves && (indent == 4 || (in_period_block && indent >= period_block_indent + 2))) );
            if (should_parse && ParseYAMLLine(line, key, value)) {
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
                    if (!in_period_block && key == "type") {
                        data.waves.type = value;
                    } else if (!in_period_block && key == "height") {
                        data.waves.height = ParseDouble(value, 0.0);
                    } else if (!in_period_block && key == "period") {
                        period_block_seen = true;
                        period_form_values = period_form_linspace = period_form_range = false;
                        data.waves.period_values.clear();
                        // Scalar on same line
                        bool looks_structured = (value.find('{') != std::string::npos || value.find('[') != std::string::npos || value.empty());
                        if (!looks_structured) {
                            data.waves.period = ParseDouble(value, 0.0);
                            data.waves.period_values.push_back(data.waves.period);
                        } else {
                            // Inline forms on same line
                            // values inline list inside braces
                            if (value.find("values") != std::string::npos && value.find('[') != std::string::npos) {
                                auto lb = value.find('['); auto rb = value.find(']');
                                if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                                    std::string inner = value.substr(lb + 1, rb - lb - 1);
                                    for (char& ch : inner) if (ch == ',') ch = ' ';
                                    std::istringstream iss(inner);
                                    double v; while (iss >> v) data.waves.period_values.push_back(v);
                                    if (!data.waves.period_values.empty()) {
                                        data.waves.period = data.waves.period_values.front();
                                        period_form_values = true;
                                    }
                                }
                            }
                            // Start nested block
                            if (value.empty() || value == "|" || value == ">") {
                                in_period_block = true;
                                period_block_indent = indent;
                            }
                        }
                    } else if (in_period_block && key == "values") {
                        // period:\n  values: [a, b, c]
                        auto lb = value.find('['); auto rb = value.find(']');
                        if (lb == std::string::npos || rb == std::string::npos || rb <= lb) {
                            // invalid list, ignore
                        } else {
                            std::string inner = value.substr(lb + 1, rb - lb - 1);
                            for (char& ch : inner) if (ch == ',') ch = ' ';
                            std::istringstream iss(inner);
                            double v; data.waves.period_values.clear();
                            while (iss >> v) data.waves.period_values.push_back(v);
                            if (!data.waves.period_values.empty()) {
                                data.waves.period = data.waves.period_values.front();
                                if (period_form_linspace || period_form_range) {
                                    throw std::runtime_error("waves.period: multiple forms specified (values + other)");
                                }
                                period_form_values = true;
                            }
                        }
                    } else if (in_period_block && key == "linspace") {
                        // linspace: { start: a, stop: b, num: n }
                        auto kv = parse_inline_brace_kv(value);
                        double start = 0.0, stop = 0.0; int num = 0; bool hasS=false, hasE=false, hasN=false;
                        for (auto& p : kv) {
                            if (p.first == "start") { start = ParseDouble(p.second, 0.0); hasS = true; }
                            else if (p.first == "stop") { stop = ParseDouble(p.second, 0.0); hasE = true; }
                            else if (p.first == "num") { try { num = std::stoi(p.second); } catch (...) { num = 0; } hasN = true; }
                        }
                        if (!(hasS && hasE && hasN) || num < 2) {
                            throw std::runtime_error("waves.period: invalid linspace (require start, stop, num>=2)");
                        }
                        if (period_form_values || period_form_range) {
                            throw std::runtime_error("waves.period: multiple forms specified");
                        }
                        period_form_linspace = true;
                        data.waves.period_values.clear();
                        if (num == 2) {
                            data.waves.period_values.push_back(start);
                            data.waves.period_values.push_back(stop);
                        } else {
                            double step = (stop - start) / static_cast<double>(num - 1);
                            for (int k = 0; k < num; ++k) {
                                data.waves.period_values.push_back(start + step * static_cast<double>(k));
                            }
                        }
                        data.waves.period = data.waves.period_values.front();
                    } else if (in_period_block && key == "range") {
                        // range: { start: a, stop: b, step: s, inclusive: true }
                        auto kv = parse_inline_brace_kv(value);
                        double start = 0.0, stop = 0.0, step = 0.0; bool inclusive = true; bool hasS=false, hasE=false, hasStep=false;
                        for (auto& p : kv) {
                            if (p.first == "start") { start = ParseDouble(p.second, 0.0); hasS = true; }
                            else if (p.first == "stop") { stop = ParseDouble(p.second, 0.0); hasE = true; }
                            else if (p.first == "step") { step = ParseDouble(p.second, 0.0); hasStep = true; }
                            else if (p.first == "inclusive") { inclusive = ParseBool(p.second, true); }
                        }
                        if (!(hasS && hasE && hasStep) || step <= 0.0 || stop < start) {
                            throw std::runtime_error("waves.period: invalid range (require start<=stop, step>0)");
                        }
                        if (period_form_values || period_form_linspace) {
                            throw std::runtime_error("waves.period: multiple forms specified");
                        }
                        period_form_range = true;
                        data.waves.period_values.clear();
                        double t = start;
                        const double eps = 1e-9;
                        while (t < stop - eps) {
                            data.waves.period_values.push_back(t);
                            t += step;
                        }
                        if (inclusive) {
                            if (std::fabs((data.waves.period_values.empty() ? start : data.waves.period_values.back()) - stop) > eps) {
                                // add last if not already close to stop and within tolerance by stepping once more would overshoot but inclusive means include stop
                                data.waves.period_values.push_back(stop);
                            } else {
                                // if already at stop within eps, ensure exact stop
                                data.waves.period_values.back() = stop;
                            }
                        }
                        if (data.waves.period_values.empty()) {
                            // Degenerate (start==stop and inclusive false) not allowed per rules
                            throw std::runtime_error("waves.period: range produced no values");
                        }
                        data.waves.period = data.waves.period_values.front();
                    } else if (!in_period_block && key == "direction") {
                        data.waves.direction = ParseDouble(value, 0.0);
                    } else if (!in_period_block && key == "phase") {
                        data.waves.phase = ParseDouble(value, 0.0);
                    } else if (!in_period_block && key == "spectrum") {
                        data.waves.spectrum = value;
                    } else if (!in_period_block && key == "seed") {
                        try { data.waves.seed = std::stoi(value); } catch (...) { data.waves.seed = -1; }
                    }
                }
            }
        }
            // Detect leaving the period block when indentation reduces back to waves level
            if (in_period_block && indent <= period_block_indent) {
                in_period_block = false;
            }
    }
    
    // Don't forget to add the last body
    if (in_body && !current_body.name.empty()) {
        data.bodies.push_back(current_body);
    }
    
    // Finalize period block validation if it was started
    if (period_block_seen) {
        // Exactly one form or a scalar must have been provided
        int forms = 0;
        if (period_form_values) forms++;
        if (period_form_linspace) forms++;
        if (period_form_range) forms++;
        if (forms > 1) {
            throw std::runtime_error("waves.period: multiple forms specified");
        }
        if (data.waves.period_values.empty()) {
            if (data.waves.period > 0.0) {
                data.waves.period_values.push_back(data.waves.period);
            } else {
                throw std::runtime_error("waves.period: invalid or empty specification");
            }
        }
    } else {
        // If no structured values captured, ensure period_values mirrors scalar period
        if (data.waves.period_values.empty() && data.waves.period > 0.0) {
            data.waves.period_values.push_back(data.waves.period);
        }
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