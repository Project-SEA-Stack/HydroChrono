#ifndef H5FILEINFO_H
#define H5FILEINFO_H
/*********************************************************************
 * @file  h5fileinfo.h
 *
 * @brief header file of HydroData main class and helper class \
 * H5FileInfo.
 *********************************************************************/
// TODO: clean up include statements
#pragma once

#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace H5 {
class H5File;
}

#include <chrono/core/ChMatrix.h>
#include <unsupported/Eigen/CXX11/Tensor>

/** @brief Extract bemio formated hdf5 data
 *
 * https://wec-sim.github.io/bemio/_modules/bemio/io/output.html
 */

class H5FileInfo;

// TODO separate these 2 classes into 2 files? (and corresponding .cpp)

// contains "chunked" data from the h5 file, generated from H5FileInfor class
class HydroData {
  public:
    struct BodyInfo {
        std::string body_name;
        int body_num;
        double disp_vol;
        Eigen::VectorXd rirf_time_vector;
        double rirf_timestep;
        Eigen::VectorXd cg;
        Eigen::VectorXd cb;
        Eigen::MatrixXd lin_matrix;
        Eigen::MatrixXd inf_added_mass;
        Eigen::Tensor<double, 3> rirf_matrix;
        // Eigen::Tensor<double, 3> radiation_damping_matrix;
    };
    struct SimulationParameters {
        std::string h5_file_name;
        double rho;
        double g;
        double water_depth;
    };
    struct RegularWaveInfo {
        Eigen::VectorXd freq_list;
        Eigen::VectorXd wave_direction_list;
        Eigen::Tensor<double, 3> excitation_mag_matrix;
        Eigen::Tensor<double, 3> excitation_phase_matrix;
    };
    struct IrregularWaveInfo {
        // Eigen::Tensor<double,3> excitation_re_matrix;
        // Eigen::Vector3i re_dims;
        // Eigen::Tensor<double, 3> excitation_im_matrix;
        // Eigen::Vector3i im_dims;
        Eigen::VectorXd excitation_irf_time;
        Eigen::Tensor<double, 3> excitation_irf_matrix;

        // see std::optional documentation for how to use
        std::optional<Eigen::MatrixXd> excitation_irf_resampled;  // TODO needs to be tensor?
        std::optional<Eigen::MatrixXd> excitation_irf_time_resampled;
        Eigen::VectorXd wave_direction_list;
    };

  private:
    // a vector of BodyInfo objects, one for each hydro body in system
    std::vector<BodyInfo> body_data_;
    // a variable for simulation parameters, assumed the same for each body, only  1 per system
    SimulationParameters sim_data_;
    // a vector of RegularWaveInfo, one for each hydro body in system
    // is empty if regular waves are not used
    std::vector<RegularWaveInfo> reg_wave_data_;
    // a vector of IrregularWaveInfo, one for each hydro body in system
    // is empty if irregular waves are not used
    std::vector<IrregularWaveInfo> irreg_wave_data_;
    friend H5FileInfo;
    void resize(int num_bodies);
    HydroData() = default;

  public:
    // getter function naming conventions: put Matrix, Vector, or Val at end to denote return type, always put body_num
    // argument first, body_num is always 0 indexed, one line return types can be defined here and not in cpp

    /**
     * @brief returns the matrix for added mass at infinite frequency for the given body.
     *
     * Matrix is scaled by rho when initialized, does not need to be scaled here.
     *
     * @param b body number, 0 indexed
     *
     * @return added mass matrix for the body from h5file
     */
    Eigen::MatrixXd GetInfAddedMassMatrix(int b) const;

    /**
     * @brief Get specific value of the linear restoring stiffness matrix for body b, row i , column j.
     *
     * @param b which body's matrix to look at
     * @param i the row of the matrix
     * @param j the column of the matrix
     *
     * @return value in linear restoring stiffness matrix b at row i, column j
     */
    double GetHydrostaticStiffnessVal(int b, int i, int j) const;

    /**
     * @brief Getter function for linear restoring stiffness matrix.
     *
     * @param b which body to get matrix from, 0 indexed
     *
     * @return the full linear restoring stiffness matrix for body b
     */
    Eigen::MatrixXd GetLinMatrix(int b) const;

    /**
     * @brief Getter function for value in RIRF matrix.
     *
     * @param b which body in system to get matrix from, 0 indexed
     * @param dof DoF: 0,...,5
     * @param col col: 0,...,6N-1 for N bodies in system
     * @param s s: 0,...,1001 (or size of rirfdims[2])
     *
     * @return rirf val for specified place in matrix
     */
    double GetRIRFVal(int b, int dof, int col, int s) const;

    /**
     * @brief Get scalar constant displaced volume for a body.
     *
     * @param b which body
     *
     * @return displaced volume from h5file
     */
    double GetDispVolVal(int b) const { return body_data_[b].disp_vol; }

    /**
     * @brief Get cg vector constant for a body.
     *
     * @param b which body
     *
     * @return cg vector from h5file
     */
    Eigen::VectorXd GetCGVector(int b) const { return body_data_[b].cg; }

    /**
     * @brief Get cb vector constant for a body.
     *
     * @param b which body
     *
     * @return cb vector from h5file
     */
    Eigen::VectorXd GetCBVector(int b) const { return body_data_[b].cb; }

    double GetExcitationIRFVal(int b, int dof, int s) const;  // TODO if this isn't used get rid of it
    Eigen::MatrixXd GetExcitationIRF(int b) const;            // TODO if this isn't used get rid of it

    // things that are the same no matter the body, don't need body argument
    /**
     * @brief returns the i-th component of the dimensions of radiation_damping_matrix
     *
     * @param i i = [0,1,2] -> [number of rows, number of columns, number of matrices]
     *
     * @return integer for how many rows, columns, or matrices are in RIRF
     */
    int GetRIRFDims(int i) const;

    /**
     * @brief Getter function for time vector for RIRF in radiation damping calculations.
     *
     * @return the Eigen::VectorXd of rirf_time_vector from h5 file
     */
    Eigen::VectorXd GetRIRFTimeVector() const;

    /**
     * @brief Get water density rho.
     *
     * @return density rho
     */
    double GetRhoVal() const { return sim_data_.rho; }

    // getters for individual chunks of data
    /**
     * @brief Get chunk of data corresponding to the BodyInfo struct in this class.
     *
     * BodyInfo contains information for non-wave hydro forces.
     *
     * @return vector containing BodyInfo classes info for each body in system with hydro forces on it
     */
    std::vector<BodyInfo>& GetBodyInfos() { return body_data_; }

    /**
     * @brief Get chunk of data corresponding to the SimulationParameters struct in this class.
     *
     * SimulationParameters contains information for the system, and doesn't correspond to any individual body.
     *
     * @return SimulationParameters info for the system
     */
    SimulationParameters& GetSimulationInfo() { return sim_data_; }

    /**
     * @brief Get chunk of data corresponding to the RegularWaveInfo struct in this class.
     *
     * RegularWaveInfo contains information for hydro forces from regular waves.
     *
     * @return vector containing RegularWaveInfo classes info for each body in system with hydro forces on it
     */
    std::vector<RegularWaveInfo>& GetRegularWaveInfos() { return reg_wave_data_; }

    /**
     * @brief Get chunk of data corresponding to the IrregularWaveInfo struct in this class.
     *
     * IrregularWaveInfo contains information for hydro forces from irregular waves.
     *
     * @return vector containing IrregularWaveInfo classes info for each body in system with hydro forces on it
     */
    std::vector<IrregularWaveInfo>& GetIrregularWaveInfos() { return irreg_wave_data_; }
};

// TODO change name to LoadH5File or ReadH5File or H5Init or something similar to give better description of
// functionality used only to initialize everything in HydroData from the h5 file
class H5FileInfo {
  public:
    bool printed = false;  // TODO remove this, dont use here

    /**
     * @brief prepares for h5 file reading, checks file exists.
     *
     * @param file string containing file name for h5 hydro data file
     * @param num_bod number of hydro bodies in system = number of bodies in h5 file \
     * note, hydrobodies should be added to system before any non hydrobodies
     */
    H5FileInfo(std::string file, int num_bod = 1);
    H5FileInfo() = delete;

    H5FileInfo(const H5FileInfo& old) = default;
    H5FileInfo& operator=(const H5FileInfo& rhs) = default;

    H5FileInfo(H5FileInfo&&) = default;
    H5FileInfo& operator=(H5FileInfo&& rhs) = default;

    ~H5FileInfo();

    /**
     * @brief Creates HydroData object and populates it with info from h5 file.
     *
     * h5_file_name needs to be set before readH5Data called (usually set in constructor).
     * calls Initialize functions to read h5 file information into  member variables.]
     *
     * @return newly initialized HydroData object
     */
    HydroData ReadH5Data();  // TODO: eventually pass user input struct here? instead of making it in function?

  private:
    std::string h5_file_name_;
    int num_bodies_;

    /**
     * @brief helper function for readH5Data() to initialize any scalars.
     *
     * @param[in] file open h5 file reference to read data from
     * @param[in] data_name data name within file to extract value from
     * @param[out] var variable to be set from h5 info
     */
    void InitScalar(H5::H5File& file, std::string data_name, double& var);

    /**
     * @brief helper function for readH5Data() to initialize any 1D data (vectors, lists etc).
     *
     * @param[in] file open h5 file reference to read data from
     * @param[in] data_name data name within file to extract value from
     * @param[out] var variable to be set from h5 info
     */
    void Init1D(H5::H5File& file, std::string data_name, Eigen::VectorXd& var);

    /**
     * @brief helper function for readH5Data() to initialize any 2D data (matrices)
     *
     * @param[in] file open h5 file reference to read data from
     * @param[in] data_name data name within file to extract value from
     * @param[out] var variable to be set from h5 info
     */
    void Init2D(H5::H5File& file, std::string data_name, Eigen::MatrixXd& var);

    /**
     * @brief helper function for readH5Data() to initialize any 3D data.
     *
     * lists of matrices, or lists of vectors (yes this looks like 2d but is stored weird in h5 file, see squeeze_mid)
     *
     * @param[in] file open h5 file reference to read data from
     * @param[in] data_name data name within file to extract value from
     * @param[out] var variable to be set from h5 info
     */
    void Init3D(H5::H5File& file, std::string data_name, Eigen::Tensor<double, 3>& var /*, std::vector<int>& dims*/);

    /**
     * @brief helper function for readH5Data() to remove the middle index of 3D data if that index is 1.
     *
     * Some data in the h5 file is a list of 1D vectors that should be 2D data, but needs to be read in as 3D data \
     * Should operate similarly to a MatLab squeeze function.
     *
     * @param to_be_squeezed is the 3D data (Eigen::Tensor) that the middle index needs to be removed from
     *
     * @return 2D matrix representing same data as to_be_squeezed, but in Eigen::Matrix, it is much easier to handle
     */
    Eigen::MatrixXd SqueezeMid(Eigen::Tensor<double, 3>& to_be_squeezed);
};

#endif