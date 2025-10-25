/*********************************************************************
 * @file  test_hydro_yaml_parser.cpp
 *
 * @brief Unit tests for the hydro YAML parser.
 *********************************************************************/

#include <hydroc/config.h>
#include "../../src/hydro_yaml_parser.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <cmath>

// Simple assertion macro for testing
#define ASSERT_EQ(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            std::cerr << "ASSERT_EQ failed: " << #actual << " = " << (actual) \
                      << ", expected " << (expected) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

#define ASSERT_DOUBLE_EQ(actual, expected) \
    do { \
        if (std::abs((actual) - (expected)) > 1e-10) { \
            std::cerr << "ASSERT_DOUBLE_EQ failed: " << #actual << " = " << (actual) \
                      << ", expected " << (expected) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERT_TRUE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cerr << "ASSERT_FALSE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

/**
 * @brief Get the path to test data files relative to the project root.
 */
std::string GetTestDataPath(const std::string& filename) {
    auto DATADIR = std::filesystem::absolute(std::filesystem::path(HC_DATA_DIR));
    return DATADIR.string() + "/yaml/" + filename;
}

/**
 * @brief Test parsing a single sphere body with regular waves.
 */
int TestParsesSphereFile() {
    std::cout << "Running TestParsesSphereFile..." << std::endl;
    
    std::string test_file = GetTestDataPath("test_sphere.hydro.yaml");
    
    // Check that the test file exists
    ASSERT_TRUE(std::filesystem::exists(test_file));
    
    // Parse the YAML file
    YAMLHydroData data = ReadHydroYAML(test_file);
    
    // Check body parsing
    ASSERT_EQ(data.bodies.size(), 1);
    ASSERT_EQ(data.bodies[0].name, "sphere");
    ASSERT_TRUE(data.bodies[0].h5_file.find("test_sphere.h5") != std::string::npos);
    ASSERT_TRUE(data.bodies[0].include_excitation);  // default value
    ASSERT_TRUE(data.bodies[0].include_radiation);   // default value
    ASSERT_EQ(data.bodies[0].radiation_calculation, "convolution");  // default value
    
    // Check wave settings
    ASSERT_EQ(data.waves.type, "regular");
    ASSERT_DOUBLE_EQ(data.waves.height, 1.5);
    ASSERT_DOUBLE_EQ(data.waves.period, 7.0);
    ASSERT_DOUBLE_EQ(data.waves.direction, 0.0);
    ASSERT_DOUBLE_EQ(data.waves.phase, 0.0);  // default value
    ASSERT_EQ(data.waves.spectrum, "pierson_moskowitz");  // default value
    
    std::cout << "TestParsesSphereFile passed!" << std::endl;
    return 0;
}

/**
 * @brief Test parsing multiple bodies with still water conditions.
 */
int TestParsesMultiBodyFile() {
    std::cout << "Running TestParsesMultiBodyFile..." << std::endl;
    
    std::string test_file = GetTestDataPath("test_multi.hydro.yaml");
    
    // Check that the test file exists
    ASSERT_TRUE(std::filesystem::exists(test_file));
    
    // Parse the YAML file
    YAMLHydroData data = ReadHydroYAML(test_file);
    
    // Check body parsing
    ASSERT_EQ(data.bodies.size(), 2);
    
    // Check first body
    ASSERT_EQ(data.bodies[0].name, "float");
    ASSERT_TRUE(data.bodies[0].h5_file.find("rm3_float.h5") != std::string::npos);
    ASSERT_TRUE(data.bodies[0].include_excitation);  // default value
    ASSERT_TRUE(data.bodies[0].include_radiation);   // default value
    ASSERT_EQ(data.bodies[0].radiation_calculation, "convolution");  // default value
    
    // Check second body
    ASSERT_EQ(data.bodies[1].name, "spar");
    ASSERT_TRUE(data.bodies[1].h5_file.find("rm3_spar.h5") != std::string::npos);
    ASSERT_TRUE(data.bodies[1].include_excitation);  // default value
    ASSERT_TRUE(data.bodies[1].include_radiation);   // default value
    ASSERT_EQ(data.bodies[1].radiation_calculation, "convolution");  // default value
    
    // Check wave settings
    ASSERT_EQ(data.waves.type, "still_ci");
    ASSERT_DOUBLE_EQ(data.waves.height, 0.0);  // default value
    ASSERT_DOUBLE_EQ(data.waves.period, 0.0);  // default value
    ASSERT_DOUBLE_EQ(data.waves.direction, 0.0);  // default value
    ASSERT_DOUBLE_EQ(data.waves.phase, 0.0);  // default value
    ASSERT_EQ(data.waves.spectrum, "pierson_moskowitz");  // default value
    
    std::cout << "TestParsesMultiBodyFile passed!" << std::endl;
    return 0;
}

/**
 * @brief Test that relative file paths are resolved correctly.
 */
int TestResolvesRelativePaths() {
    std::cout << "Running TestResolvesRelativePaths..." << std::endl;
    
    std::string test_file = GetTestDataPath("test_sphere.hydro.yaml");
    
    // Parse the YAML file
    YAMLHydroData data = ReadHydroYAML(test_file);
    
    // Check that the h5_file path is resolved relative to the YAML file location
    std::string h5_file = data.bodies[0].h5_file;
    
    // The path should be absolute and contain the test file directory
    ASSERT_TRUE(std::filesystem::path(h5_file).is_absolute());
    ASSERT_TRUE(h5_file.find("hydroData") != std::string::npos);
    ASSERT_TRUE(h5_file.find("test_sphere.h5") != std::string::npos);
    
    std::cout << "TestResolvesRelativePaths passed!" << std::endl;
    return 0;
}

/**
 * @brief Test error handling for missing files.
 */
int TestHandlesMissingFile() {
    std::cout << "Running TestHandlesMissingFile..." << std::endl;
    
    std::string missing_file = GetTestDataPath("nonexistent.hydro.yaml");
    
    // Check that the file doesn't exist
    ASSERT_FALSE(std::filesystem::exists(missing_file));
    
    // Try to parse the missing file - should throw an exception
    try {
        YAMLHydroData data = ReadHydroYAML(missing_file);
        std::cerr << "Expected exception for missing file, but none was thrown" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        // Expected exception
        std::string error_msg = e.what();
        ASSERT_TRUE(error_msg.find("Could not open hydro file") != std::string::npos);
        ASSERT_TRUE(error_msg.find("nonexistent.hydro.yaml") != std::string::npos);
    }
    
    std::cout << "TestHandlesMissingFile passed!" << std::endl;
    return 0;
}

/**
 * @brief Test error handling for malformed YAML (missing hydrodynamics section).
 */
int TestHandlesMalformedYAML() {
    std::cout << "Running TestHandlesMalformedYAML..." << std::endl;
    
    // Create a temporary malformed YAML file
    std::string malformed_file = GetTestDataPath("malformed.hydro.yaml");
    std::ofstream file(malformed_file);
    file << "bodies:\n";
    file << "  - name: test\n";
    file << "    h5_file: test.h5\n";
    file.close();
    
    // Try to parse the malformed file - should throw an exception
    try {
        YAMLHydroData data = ReadHydroYAML(malformed_file);
        std::cerr << "Expected exception for malformed YAML, but none was thrown" << std::endl;
        std::filesystem::remove(malformed_file);
        return 1;
    } catch (const std::runtime_error& e) {
        // Expected exception
        std::string error_msg = e.what();
        ASSERT_TRUE(error_msg.find("No 'hydrodynamics:' section found") != std::string::npos);
    }
    
    // Clean up
    std::filesystem::remove(malformed_file);
    
    std::cout << "TestHandlesMalformedYAML passed!" << std::endl;
    return 0;
}

/**
 * @brief Test default values for optional fields.
 */
int TestDefaultValues() {
    std::cout << "Running TestDefaultValues..." << std::endl;
    
    // Create a minimal YAML file with only required fields
    std::string minimal_file = GetTestDataPath("minimal.hydro.yaml");
    std::ofstream file(minimal_file);
    file << "hydrodynamics:\n";
    file << "  bodies:\n";
    file << "    - name: test\n";
    file << "    h5_file: test.h5\n";
    file << "  waves:\n";
    file << "    type: regular\n";
    file.close();
    
    // Parse the minimal file
    YAMLHydroData data = ReadHydroYAML(minimal_file);
    
    // Check default values
    ASSERT_EQ(data.bodies.size(), 1);
    ASSERT_EQ(data.bodies[0].name, "test");
    ASSERT_TRUE(data.bodies[0].include_excitation);  // default: true
    ASSERT_TRUE(data.bodies[0].include_radiation);   // default: true
    ASSERT_EQ(data.bodies[0].radiation_calculation, "convolution");  // default
    
    ASSERT_EQ(data.waves.type, "regular");
    ASSERT_DOUBLE_EQ(data.waves.height, 0.0);  // default
    ASSERT_DOUBLE_EQ(data.waves.period, 0.0);  // default
    ASSERT_DOUBLE_EQ(data.waves.direction, 0.0);  // default
    ASSERT_DOUBLE_EQ(data.waves.phase, 0.0);  // default
    ASSERT_EQ(data.waves.spectrum, "pierson_moskowitz");  // default
    
    // Clean up
    std::filesystem::remove(minimal_file);
    
    std::cout << "TestDefaultValues passed!" << std::endl;
    return 0;
}

int main() {
    std::cout << "Starting hydro YAML parser unit tests..." << std::endl;
    
    int result = 0;
    
    // Run all tests
    result |= TestParsesSphereFile();
    result |= TestParsesMultiBodyFile();
    result |= TestResolvesRelativePaths();
    result |= TestHandlesMissingFile();
    result |= TestHandlesMalformedYAML();
    result |= TestDefaultValues();
    
    if (result == 0) {
        std::cout << "\nAll tests passed!" << std::endl;
    } else {
        std::cout << "\nSome tests failed!" << std::endl;
    }
    
    return result;
}