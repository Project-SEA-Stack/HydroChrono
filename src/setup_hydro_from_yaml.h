#ifndef SETUP_HYDRO_FROM_YAML_H
#define SETUP_HYDRO_FROM_YAML_H

/*********************************************************************
 * @file  setup_hydro_from_yaml.h
 *
 * @brief Setup hydrodynamic forces from parsed YAML data.
 *********************************************************************/

#include "hydro_types.h"
#include <chrono/physics/ChBody.h>
#include <memory>
#include <vector>

// Forward declarations
class TestHydro;

/**
 * @brief Setup hydrodynamic forces from parsed YAML data.
 *
 * This function connects parsed YAML data (from hydro.yaml) to actual hydrodynamic forces.
 * It builds the appropriate WaveBase subclass from hydro_data.waves, matches body names
 * with their corresponding HDF5 files, and initializes TestHydro with the matched bodies.
 *
 * @param hydro_data Parsed hydrodynamic configuration from hydro.yaml
 * @param bodies Vector of Chrono bodies that may have hydrodynamic forces
 * @param timestep Simulation timestep (used for irregular wave setup)
 * @param sim_duration Simulation duration (used for irregular wave setup)
 * @param ramp_duration Wave ramp duration (used for irregular wave setup)
 * @return Unique pointer to initialized TestHydro object
 * @throws std::runtime_error on configuration errors
 */
std::unique_ptr<TestHydro> SetupHydroFromYAML(
    const YAMLHydroData& hydro_data,
    const std::vector<std::shared_ptr<chrono::ChBody>>& bodies,
    double timestep,
    double sim_duration,
    double ramp_duration
);

#endif  // SETUP_HYDRO_FROM_YAML_H