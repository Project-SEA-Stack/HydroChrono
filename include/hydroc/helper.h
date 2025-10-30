#ifndef HELPER_H
#define HELPER_H

#pragma once

#include <Eigen/Dense>  // Need for the container function
#include <fstream>
#include <iostream>
#include <hydroc/logging.h>
#include <string>
#include <filesystem>  // C++17

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

/**@brief Returns last index of vector element below value.
 *
 * @param value Input value
 * @param ticks Array of ticks from which to find lower-bound index (assuming ascending order)
 *
 */
size_t get_lower_index(double value, const std::vector<double>& ticks);

/**@brief Base namespace for HydroChrono library
 *
 */
namespace hydroc {

/**@brief Set initial environment
 *
 * Use command line argument or env variables to set some static data at initialization.
 *
 * Set the main data directory ..
 *
 * @param argc number of argument (same as for main function)
 * @param argv arguments of main function
 * @return 1 on error 0 else
 */
int SetInitialEnvironment(int argc, char* argv[]) noexcept;

/**@brief Get base name of data directory
 *
 * @return the string containing the path in standard format
 */
std::string getDataDir() noexcept;

/**@brief C++ 17 filesystem helper to ensure a directory exists
 *
 */
void ensure_directory_exists(const std::filesystem::path& path);

std::string getDemoOutDir();
std::string getTestOutDir();

template <typename T>
void WriteDataToFile(const std::vector<T>& data, const std::string& filename) {
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        for (const auto& item : data) {
            outFile << item << std::endl;
        }
        outFile.close();
    } else {
        hydroc::cli::LogError(std::string("Unable to open the file for writing: ") + filename);
    }
};

// TODO: move Misc writecontainer type functions to different file
// TODO move WriteContainerToFile generic declaration to a .h file instead of .cpp
/**
 * @brief Prints contents of 1D Container data to given file.
 *
 * @param container the 1D array/vector to write to file
 * @param file_name file to write container to
 */
template <typename Container>
void WriteContainerToFile(const Container& container, const std::string& file_name);

/**
 * @brief Prints contents of std::vector<double> data to given file.
 *
 * @param container std::vector<double> to write to file
 * @param file_name file to write container to
 */
template <>
void inline WriteContainerToFile<std::vector<double>>(const std::vector<double>& container,
                                                      const std::string& file_name) {
    std::ofstream output_file(file_name);

    if (!output_file) {
        hydroc::cli::LogError(std::string("Error: Unable to open the file: ") + file_name);
        return;
    }

    for (const double value : container) {
        output_file << value << std::endl;
    }

    output_file.close();
};

/**
 * @brief Prints contents of Eigen::VectorXd data to given file.
 *
 * @param container Eigen::VectorXd to write to file
 * @param file_name file to write container to
 */
template <>
void inline WriteContainerToFile<Eigen::VectorXd>(const Eigen::VectorXd& container, const std::string& file_name) {
    std::ofstream output_file(file_name);

    if (!output_file) {
        hydroc::cli::LogError(std::string("Error: Unable to open the file: ") + file_name);
        return;
    }

    for (int i = 0; i < container.size(); ++i) {
        output_file << container[i] << std::endl;
    }

    output_file.close();
};

}  // end namespace hydroc

#endif