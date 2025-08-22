// TODO: This file will move to src/hydrochrono_hydro/ in a future refactor.

#ifndef HYDRO_TYPES_H
#define HYDRO_TYPES_H

/*********************************************************************
 * @file  hydro_types.h
 *
 * @brief Data structures for parsed hydro.yaml content.
 *********************************************************************/

#include <string>
#include <vector>
#include <array>

/**
 * @brief Configuration for a hydrodynamic body.
 */
struct HydroBody {
    std::string name = "";
    std::string h5_file = "";
    bool include_excitation = true;
    bool include_radiation = true;
    std::string radiation_calculation = "convolution";  // "convolution" or "state_space"
    // TODO: Add nonlinear buoyancy fields
    // TODO: Add drag coefficient fields
};

/**
 * @brief Configuration for wave settings.
 */
struct WaveSettings {
    std::string type = "regular";  // "regular", "irregular", "no_wave"
    double height = 0.0;
    double period = 0.0;
    double direction = 0.0;  // degrees, 0 = positive x
    double phase = 0.0;
    std::string spectrum = "pierson_moskowitz";  // "pierson_moskowitz", "jonswap", etc.
    int seed = -1; // optional irregular seed; -1 means unset
    // Sweep support (expanded values) for period; if empty, use 'period'
    std::vector<double> period_values;
    // TODO: Add spectrum parameters (peak enhancement factor, etc.)
};

/**
 * @brief Top-level container for hydrodynamic configuration data from YAML.
 */
struct YAMLHydroData {
    std::vector<HydroBody> bodies;
    WaveSettings waves;
};

#endif  // HYDRO_TYPES_H