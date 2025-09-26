#ifndef HELPER_H
#define HELPER_H

#pragma once

#include <Eigen/Dense>  // Need for the container function
#include <fstream>
#include <iostream>
#include <string>

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
 * Use command line argument or env variables to set s
 * ome static data at initialization.
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
template <typename T>
void WriteDataToFile(const std::vector<T>& data, const std::string& filename) {
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        for (const auto& item : data) {
            outFile << item << std::endl;
        }
        outFile.close();
    } else {
        std::cerr << "Unable to open the file for writing: " << filename << std::endl;
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
        std::cerr << "Error: Unable to open the file: " << file_name << std::endl;
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
        std::cerr << "Error: Unable to open the file: " << file_name << std::endl;
        return;
    }

    for (int i = 0; i < container.size(); ++i) {
        output_file << container[i] << std::endl;
    }

    output_file.close();
};


/// <summary>
/// Performs bilinear interpolation on a 3D matrix along the wave direction and frequency axes.
/// This is used to interpolate a scalar value (e.g., excitation magnitude or phase)
/// at a given wave direction and frequency index between grid points.
/// </summary>
/// <typeparam name="MatrixGetter">
/// A callable that takes three integers (i, j, k) and returns the matrix value at that location.
/// </typeparam>
/// <param name="get_val">Function or lambda used to access elements of the 3D matrix.</param>
/// <param name="i">Index of the degree of freedom (DOF) or row in the matrix.</param>
/// <param name="omega_dir_input">Continuous wave direction index (used for interpolation along direction axis).</param>
/// <param name="freq_index_des">Continuous frequency index (used for interpolation along frequency axis).</param>
/// <param name="num_dirs">Total number of discrete wave direction entries in the matrix.</param>
/// <param name="num_freqs">Total number of discrete frequency entries in the matrix.</param>
/// <returns>Interpolated scalar value at the specified DOF, direction, and frequency.</returns>
template <typename MatrixGetter>
double BilinearInterp3D(const MatrixGetter& get_val,
                        int i,
                        double dir_index_des,
                        double freq_index_des,
                        int num_dirs,
                        int num_freqs) {
    int freq_floor    = static_cast<int>(std::floor(freq_index_des));
    int freq_ceil     = freq_floor + 1;
    double freq_alpha = freq_index_des - freq_floor;

    int dir_floor    = static_cast<int>(std::floor(dir_index_des));
    int dir_ceil     = dir_floor + 1;
    double dir_alpha = dir_index_des - dir_floor;

    // Clamp to valid index ranges
    freq_floor = std::max(0, std::min(freq_floor, num_freqs - 2));
    freq_ceil  = freq_floor + 1;

    dir_floor = std::max(0, std::min(dir_floor, num_dirs - 2));
    dir_ceil  = dir_floor + 1;

    // Access values
    double v00 = get_val(i, dir_floor, freq_floor);
    double v10 = get_val(i, dir_ceil, freq_floor);
    double v01 = get_val(i, dir_floor, freq_ceil);
    double v11 = get_val(i, dir_ceil, freq_ceil);

    return (1 - freq_alpha) * (1 - dir_alpha) * v00 + (1 - freq_alpha) * dir_alpha * v10 +
           freq_alpha * (1 - dir_alpha) * v01 + freq_alpha * dir_alpha * v11;
}

}  // end namespace hydroc

#endif