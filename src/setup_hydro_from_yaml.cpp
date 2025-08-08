/*********************************************************************
 * @file  setup_hydro_from_yaml.cpp
 *
 * @brief Implementation of hydrodynamic setup from YAML data.
 *********************************************************************/

#include "setup_hydro_from_yaml.h"
#include <hydroc/hydro_forces.h> // For TestHydro
#include <hydroc/wave_types.h>    // For WaveBase, RegularWave, IrregularWaves, NoWave
#include <hydroc/logging.h>         // For Logger
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cmath> // For M_PI

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace chrono;

namespace {

/**
 * @brief Create a wave object from wave settings.
 */
std::shared_ptr<WaveBase> CreateWaveFromSettings(const WaveSettings& wave_settings, 
                                                 unsigned int num_bodies,
                                                 double timestep,
                                                 double sim_duration,
                                                 double ramp_duration) {
    if (wave_settings.type == "regular") {
        auto regular_wave = std::make_shared<RegularWave>(num_bodies);
        
        // Set wave parameters
        regular_wave->regular_wave_amplitude_ = wave_settings.height / 2.0;  // Convert height to amplitude
        regular_wave->regular_wave_omega_ = 2.0 * M_PI / wave_settings.period;  // Convert period to angular frequency
        regular_wave->regular_wave_phase_ = wave_settings.phase;
        
        hydroc::debug::LogDebug(std::string("Attached wave model: RegularWave, H=") + std::to_string(wave_settings.height) + 
                            "m, T=" + std::to_string(wave_settings.period) + "s");
        
        return regular_wave;
        
    } else if (wave_settings.type == "irregular") {
        // Create irregular wave parameters
        IrregularWaveParams params;
        params.num_bodies_ = num_bodies;
        params.simulation_dt_ = timestep;
        params.simulation_duration_ = sim_duration;
        params.ramp_duration_ = ramp_duration;
        params.wave_height_ = wave_settings.height;
        params.wave_period_ = wave_settings.period;
        params.seed_ = 1;  // Default seed, could be made configurable
        
        auto irregular_wave = std::make_shared<IrregularWaves>(params);
        
        hydroc::debug::LogDebug(std::string("Attached wave model: IrregularWaves, H=") + std::to_string(wave_settings.height) + 
                            "m, T=" + std::to_string(wave_settings.period) + "s");
        
        return irregular_wave;
        
    } else if (wave_settings.type == "no_wave" || wave_settings.type == "still_ci") {
        auto no_wave = std::make_shared<NoWave>(num_bodies);
        
        hydroc::debug::LogDebug("Attached wave model: NoWave (still water)");
        
        return no_wave;
        
    } else {
        throw std::runtime_error("Unsupported wave type: " + wave_settings.type);
    }
}

/**
 * @brief Match hydrodynamic bodies with Chrono bodies by name.
 */
std::vector<std::shared_ptr<ChBody>> MatchBodiesByName(
    const std::vector<HydroBody>& hydro_bodies,
    const std::vector<std::shared_ptr<ChBody>>& chrono_bodies,
    std::string& h5_file_path) {
    
    std::vector<std::shared_ptr<ChBody>> matched_bodies;
    
    // For now, we'll use the first H5 file found (assuming all bodies use the same file)
    // In the future, this could be enhanced to support different H5 files per body
    if (!hydro_bodies.empty()) {
        h5_file_path = hydro_bodies[0].h5_file;
    }
    
    // Match bodies by name
    for (const auto& hydro_body : hydro_bodies) {
        bool found = false;
        
        for (const auto& chrono_body : chrono_bodies) {
            if (chrono_body->GetName() == hydro_body.name) {
                matched_bodies.push_back(chrono_body);
                found = true;
                
                // Log the matched body details
                hydroc::debug::LogDebug(std::string("Body: ") + hydro_body.name + 
                          " -> h5: " + h5_file_path + 
                          ", excitation: " + (hydro_body.include_excitation ? "true" : "false") + 
                          ", radiation: " + (hydro_body.include_radiation ? "true" : "false"));
                
                break;
            }
        }
        
        if (!found) {
            hydroc::cli::LogWarning("Hydrodynamic body '" + hydro_body.name + "' not found in Chrono system");
        }
    }
    
    return matched_bodies;
}

} // anonymous namespace

std::unique_ptr<TestHydro> SetupHydroFromYAML(
    const YAMLHydroData& hydro_data,
    const std::vector<std::shared_ptr<ChBody>>& bodies,
    double timestep,
    double sim_duration,
    double ramp_duration) {
    
    // Match hydrodynamic bodies with Chrono bodies
    std::string h5_file_path;
    auto matched_bodies = MatchBodiesByName(hydro_data.bodies, bodies, h5_file_path);
    
    if (matched_bodies.empty()) {
        throw std::runtime_error("No hydrodynamic bodies found in Chrono system");
    }
    
    // Create wave object from settings
    auto wave = CreateWaveFromSettings(hydro_data.waves, matched_bodies.size(), 
                                      timestep, sim_duration, ramp_duration);
    
    // Create and initialize TestHydro
    auto test_hydro = std::make_unique<TestHydro>(matched_bodies, h5_file_path, wave);
    
    hydroc::debug::LogDebug(std::string("Initialized TestHydro with ") + std::to_string(matched_bodies.size()) + " bodies");
    
    return test_hydro;
}