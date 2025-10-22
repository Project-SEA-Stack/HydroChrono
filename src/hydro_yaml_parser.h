#ifndef HYDRO_YAML_PARSER_H
#define HYDRO_YAML_PARSER_H

/*********************************************************************
 * @file  hydro_yaml_parser.h
 *
 * @brief YAML parser for hydro.yaml files.
 *********************************************************************/

#include "hydro_types.h"
#include <string>

/**
 * @brief Read and parse a hydro.yaml file into YAMLHydroData structure.
 *
 * @param hydro_file_path Path to the hydro.yaml file to parse.
 * @return YAMLHydroData containing the parsed configuration.
 * @throws std::runtime_error if the file cannot be read or parsed.
 */
YAMLHydroData ReadHydroYAML(const std::string& hydro_file_path);

// Optional helper to parse convolution mode string
enum class RadiationConvolutionModeParsed {
    Baseline,
    TaperedDirect
};


#endif  // HYDRO_YAML_PARSER_H