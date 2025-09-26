
#ifndef WAVE_TYPES_H
#define WAVE_TYPES_H
/*********************************************************************
 * @file wave_types.h
 *
 * @brief header file for Wavebase and classes inheriting from WaveBase.
 *********************************************************************/
#pragma once

#include <hydroc/h5fileinfo.h>
#include <Eigen/Dense>

// todo move this helper function somewhere else?
Eigen::VectorXd PiersonMoskowitzSpectrumHz(Eigen::VectorXd& f, double Hs, double Tp);

Eigen::VectorXd JONSWAPSpectrumHz(Eigen::VectorXd& f,
                                  double Hs,
                                  double Tp,
                                  double gamma       = 3.3,
                                  bool is_normalized = false);

double GetFreeSurfaceElevation(const Eigen::VectorXd& freqs_hz,
                               const Eigen::VectorXd& spectral_densities,
                               const Eigen::VectorXd& spectral_widths,
                               const Eigen::VectorXd& wave_phases,
                               const Eigen::VectorXd& wavenumbers,
                               const Eigen::Vector3d& position,
                               double time_value,
                               double water_depth);

std::vector<double> GetFreeSurfaceElevationTimeSeries(const Eigen::VectorXd& freqs_hz,
                                                      const Eigen::VectorXd& spectral_densities,
                                                      const Eigen::VectorXd& spectral_widths,
                                                      const Eigen::VectorXd& wave_phases,
                                                      const Eigen::VectorXd& wavenumbers,
                                                      const Eigen::Vector3d& position,
                                                      const Eigen::VectorXd& time_array,
                                                      double water_depth);

enum class WaveMode {
    /// @brief No waves
    noWaveCIC = 0,
    /// @brief Regular waves
    regular = 1,
    /// @brief Irregular waves
    irregular = 2
};

/**
 * @brief  pure virtual (interface) class for wave modes (regular, irregular, etc).
 */
class WaveBase {
  public:
    /**
     * @brief Override Initialize to set up any member variables in derrived class before timestepping occurs.
     */
    virtual void Initialize() = 0;

    /**
     * @brief Override to return the 6N dimensional force vector on hydro bodies.
     *
     * If force changes over time, put calculations
     *
     * @param t the current time to get the force for
     */
    virtual Eigen::VectorXd GetForceAtTime(double t) = 0;
    virtual WaveMode GetWaveMode()                   = 0;

    virtual double GetElevation(const Eigen::Vector3d& position, double time) = 0;

    virtual Eigen::Vector3d GetVelocity(const Eigen::Vector3d& position, double time) = 0;

    virtual Eigen::Vector3d GetAcceleration(const Eigen::Vector3d& position, double time) = 0;

    /// @brief Mean water level
    double mwl_ = 0.0;
    /// @brief Gravitational acceleration
    double g_ = 9.81;
    /// @brief Water depth
    double water_depth_ = 0.0;

protected:
    double GetInterpolatedDirectionIndex(const Eigen::VectorXd& dirs, double dir_input) const;
};

/**
 * @brief class to instantiate WaveBase for no waves.
 */
class NoWave : public WaveBase {
  public:
    NoWave() { num_bodies_ = 1; }
    NoWave(unsigned int num_b) { num_bodies_ = num_b; }
    void Initialize() override {}

    /**
     * @brief fills a vector of 0s for force from NoWave.
     *
     * Overrides WaveBase function.
     *
     * @param t time, not used in NoWave case, here from WaveBase inheritance
     *
     * @return 6N dimensional force vector (Eigen::VectorXd) from waves in NoWave case.
     */
    Eigen::VectorXd GetForceAtTime(double t) override;
    WaveMode GetWaveMode() override { return mode_; }
    double GetElevation(const Eigen::Vector3d& position, double time) override { return 0.0; };
    Eigen::Vector3d GetVelocity(const Eigen::Vector3d& position, double time) override {
        return Eigen::Vector3d(0.0, 0.0, 0.0);
    }
    Eigen::Vector3d GetAcceleration(const Eigen::Vector3d& position, double time) override {
        return Eigen::Vector3d(0.0, 0.0, 0.0);
    }

  private:
    unsigned int num_bodies_;
    const WaveMode mode_ = WaveMode::noWaveCIC;
};

/**
 * @brief class to instantiate WaveBase for regular waves.
 */
class RegularWave : public WaveBase {
  public:
    /**
     * @brief default constructor for RegularWave.
     *
     * Defaults to 1 body if no argument given. Explicitly separate from constructor with num_b so a default \
     * constructor exists.
     */
    RegularWave();

    /**
     * @brief constructor for RegularWave in multibody cases.
     *
     * @param num_b number of bodies to apply hydro forces to (usually number of bodies in system)
     */
    RegularWave(unsigned int num_b);

    /**
     * @brief Initializes other member variables for timestep calculations later.
     *
     * Needs to be called before RegularWave can calculate any forces at any time. Resizes vectors, calculates \
     * constants, etc from h5 data. Assumes AddH5Data() has been called.
     */
    void Initialize() override;

    /**
     * @brief calculates the force from the regular wave at time t.
     *
     * Note if AdddH5Data and Initialize have not been called first, there will be issues.
     * TODO: add checks that these functions have been called in correct order.
     *
     * @param t time to calculate force at
     *
     * @return 6N dimensional Eigen::VectorXd for force from reg wave at time t, where N is number of bodies with \
     * hydroforces
     */
    Eigen::VectorXd GetForceAtTime(double t) override;

    /**
     * @brief gets wave mode.
     *
     * @return WaveMode enum for RegularWave is regular
     */
    WaveMode GetWaveMode() override { return mode_; }

    // user input variables
    double regular_wave_amplitude_;
    double regular_wave_omega_;
    double regular_wave_phase_ = 0.0;
    double regular_wave_direction_ = 0.0;
    double regular_wave_spread_    = 1;

    /**
     * @brief Initializes other member variables for timestep calculations later.
     *
     * Links the HydroData::RegularWaveInfo chunk to RegularWave for use in calculations.
     * Should be called before Initialize().
     *
     * @param reg_h5_data reference to chunk of h5 data needed for RegularWave calculations
     */
    void AddH5Data(std::vector<HydroData::RegularWaveInfo>& reg_h5_data, HydroData::SimulationParameters& sim_data);

    double GetElevation(const Eigen::Vector3d& position, double time) override;

    Eigen::Vector3d GetVelocity(const Eigen::Vector3d& position, double time) override;

    Eigen::Vector3d GetAcceleration(const Eigen::Vector3d& position, double time) override;

  private:
    unsigned int num_bodies_;
    const WaveMode mode_ = WaveMode::regular;
    std::vector<HydroData::RegularWaveInfo> wave_info_;
    Eigen::VectorXd excitation_force_mag_;
    Eigen::VectorXd excitation_force_phase_;
    Eigen::VectorXd force_;
    double wavenumber_;

    /**
     * @brief Finds omega_max and number of frequencies, then gets omega_max / num_freqs.
     *
     * Helper function for RegularWave calculations.
     *
     * @return omega step size
     */
    double GetOmegaDelta() const;

    double GetInterpolatedDirectionIndex(double dir_input) const;

    /**
     * @brief gets interpolated excitation magnitude value.
     *
     * Helper function for RegWave calculations.
     *
     * @param b body to get matrix from
     * @param i row of matrix to look at
     * @param j column of matrix
     * @param freq_index_des TODO: what to call this
     *
     * @return excitation magnitudes for body b, row i, column j, frequency ix k
     */
    //double GetExcitationMagInterp(int b, int i, int j, double freq_index_des) const;
    double GetExcitationMagInterp(int b, int i, double dir_index_des, double freq_index_des) const;

    /**
     * @brief gets interpolated excitation phase value.
     *
     * Helper function for RegWave calculations.
     *
     * @param b body to get matrix from
     * @param i row of matrix to look at
     * @param j column of matrix
     * @param freq_index_des TODO: what to call this
     *
     * @return excitation phases for body b, row i, column j, frequency ix k
     */
    // double GetExcitationPhaseInterp(int b, int i, int j, double freq_index_des) const;
    double GetExcitationPhaseInterp(int b, int i, double dir_index_des, double freq_index_des) const;
};

//// class to instantiate WaveBase for irregular waves
// class IrregularWave : public WaveBase {
//  public:
//    IrregularWave();
//    IrregularWave(unsigned int num_b);
//    void Initialize() override;  // call any set up functions from here
//    Eigen::VectorXd GetForceAtTime(double t) override;
//    WaveMode GetWaveMode() override { return mode; }
//    Eigen::VectorXd SetSpectrumFrequencies(double start, double end, int num_steps);
//    void SetUpWaveMesh(std::string filename = "fse_mesh.obj");
//    std::string GetMeshFile();
//    Eigen::Vector3<double> GetWaveMeshVelocity();
//    // add more helper functions for calculations here:
//
//    double wave_height;
//    double wave_period;
//    double simulation_duration;
//    double simulation_dt;
//    double ramp_duration;
//    Eigen::VectorXd eta;
//
//    void AddH5Data(std::vector<HydroData::IrregularWaveInfo>& irreg_h5_data, HydroData::SimulationParameters&
//    sim_data);
//
//  private:
//    unsigned int num_bodies;
//    const WaveMode mode = WaveMode::irregular;
//    std::vector<HydroData::IrregularWaveInfo> wave_info;
//    HydroData::SimulationParameters sim_data;
//    std::vector<Eigen::MatrixXd> ex_irf_resampled;
//    std::vector<Eigen::VectorXd> ex_irf_time_resampled;
//    Eigen::VectorXd spectrum_frequencies;
//    Eigen::VectorXd spectral_densities;
//    std::string mesh_file_name;
//
//    // Eigen::MatrixXd GetExcitationIRF(int b) const;
//    Eigen::VectorXd ResampleTime(const Eigen::VectorXd& t_old, const double dt_new);
//    Eigen::MatrixXd ResampleVals(const Eigen::VectorXd& t_old, Eigen::MatrixXd& vals_old, const Eigen::VectorXd&
//    t_new); double ExcitationConvolution(int body, int dof, double time);
//
//    void CreateSpectrum();
//    void IrregularWave::CreateFreeSurfaceElevation();
//    friend Eigen::VectorXd PiersonMoskowitzSpectrumHz(Eigen::VectorXd& f, double Hs, double Tp);
//};

struct IrregularWaveParams {
    unsigned int num_bodies_;
    double simulation_dt_;
    double simulation_duration_;
    double ramp_duration_ = 0.0;
    std::string eta_file_path_;
    double wave_height_             = 0.0;
    double wave_period_             = 0.0;
    double frequency_min_           = 0.001;
    double frequency_max_           = 1.0;
    int nfrequencies_               = 0;
    double peak_enhancement_factor_ = 1.0;
    bool is_normalized_             = false;
    int seed_                       = 1;
    bool wave_stretching_           = true;
    std::vector<double> wave_direction_ = {0.0};  
    std::vector<double> wave_spread_    = {1.0};  
};

class IrregularWaves : public WaveBase {
  public:
    IrregularWaves(const IrregularWaveParams& params);
    void Initialize() override {}

    void CreateSpectrum();
    std::vector<double> GetSpectrum();
    Eigen::MatrixXd GetFreeSurfaceElevation();
    std::vector<double> GetEtaTimeData();

    Eigen::VectorXd GetForceAtTime(double t) override;

    /**
     * @brief overloaded function from WaveBase to get the wave mode.
     *
     * @return WaveMode (always irregular for IrregularWave)
     */
    WaveMode GetWaveMode() override { return mode_; }

    /**
     * @brief TODO
     *
     * @param start
     * @param end
     * @param num_steps
     *
     * @return eigen vector
     */
    Eigen::VectorXd SetSpectrumFrequencies(double start, double end, int num_steps);

    /**
     * @brief Makes an obj mesh for the current irregular wave.
     *
     * Computes points and triangles for free surface.
     * This is an optional function for users to call from their main simulation during set up if they want to have the
     * mesh exported and/or used in visualization.
     *
     * @param filename optionally set the file name to export mesh to. Must end in .obj for now. Defaults to
     * "fse_mesh.obj"
     */
    void SetUpWaveMesh(std::string filename = "fse_mesh.obj");

    /**
     * @brief Gives users access to the name of the mesh file generated by SetUpWaveMesh().
     *
     * @return file name from the wave's mesh .obj file.
     */
    std::string GetMeshFile();

    /**
     * @brief Gives users the wave mesh velocity for visualization purposes in simulation.
     *
     * wave mesh velocity is 1 in x direction
     *
     * @return wave mesh velocity as Eigen::Vector3d
     */
    Eigen::Vector3<double> GetWaveMeshVelocity();

    // user input // TODO add default values in case user doesn't initialize these?
    // double wave_height_;
    // double wave_period_;
    // double simulation_duration_;
    // double simulation_dt_;
    // double ramp_duration_;
    // Eigen::VectorXd eta_;  // public for mesh printing functions, TODO maybe make those friends, so this can be
    // private?

    /**
     * @brief Initializes other member variables for timestep calculations later.
     *
     * Links the HydroData::IrregularWaveInfo chunk to IrregularWave for use in calculations.
     * Should be called before Initialize().
     *
     * @param irreg_h5_data reference to chunk of h5 data needed for IrregularWave calculations
     */
    void AddH5Data(std::vector<HydroData::IrregularWaveInfo>& irreg_h5_data, HydroData::SimulationParameters& sim_data);

    double GetElevation(const Eigen::Vector3d& position, double time) override;

    Eigen::Vector3d GetVelocity(const Eigen::Vector3d& position, double time) override;

    Eigen::Vector3d GetAcceleration(const Eigen::Vector3d& position, double time) override;

  private:
    IrregularWaveParams params_;
    std::vector<double> spectrum_;
    std::vector<double> time_data_;
    // std::vector<double> free_surface_elevation_sampled_;
    Eigen::MatrixXd free_surface_elevation_sampled_;
    std::vector<double> free_surface_time_sampled_;
    bool spectrumCreated_;

    const WaveMode mode_ = WaveMode::irregular;
    // unsigned int num_bodies_;
    // const WaveMode mode_ = WaveMode::irregular;
    std::vector<HydroData::IrregularWaveInfo> wave_info_;
    std::vector<Eigen::Tensor<double, 3>> ex_irf_sampled_;
    std::vector<Eigen::VectorXd> ex_irf_time_sampled_;
    std::vector<Eigen::VectorXd> ex_irf_width_sampled_;
    Eigen::VectorXd spectrum_frequencies_;
    Eigen::VectorXd spectral_densities_;
    Eigen::VectorXd spectral_widths_;
    Eigen::VectorXd wavenumbers_;
    Eigen::MatrixXd wave_phases_;
    std::string mesh_file_name_;

    void InitializeIRFVectors();
    void ReadEtaFromFile();
    void CreateFreeSurfaceElevation();

    Eigen::Tensor<double, 3> GetExcitationIRF(int b) const;

    /** @brief Resamples IRF time, widths, and values.
     *
     * @param dt Time step value to resample
     */
    void ResampleIRF(double dt);

    /** @brief Calculates width (used for excitation convolution).
     */
    void CalculateWidthIRF();

    /**
     * @brief Calculates the component of force from Convolution integral for specified body, dof, time.
     *
     * The discretization uses the time series of the of the IRF relative to the current time step.
     * Linear interpolation is done for the free surface elevation if time_sim-time_irf is between two
     * values of the time series of the precomputed free surface elevation.
     * Trapezoidal integration is used to compute the force.
     *
     * @param body which body currently calculating for
     * @param dof which degree of freedom to calculate force value for
     * @param time the time to compute force for
     *
     * @return value of force vector at t time in component corresponding to body and dof
     */
    double ExcitationConvolution(int body, int dof, double time);
};

/**
 * @brief Opens obj file and writes points and triangles to file.
 *
 * @param points vector of tuples representing vertices of triangles
 * @param triangles list of faces each face is 3 indices of points
 * @param file_name name of obj file to write points and triangles to
 */
void WriteFreeSurfaceMeshObj(const std::vector<std::array<double, 3>>& points,
                             const std::vector<std::array<size_t, 3>>& triangles,
                             const std::string& file_name);

/**
 * @brief Makes triangles for mesh (visible from both sides).
 *
 * @param eta TODO
 *
 * @return vector of tuples representing triangles for mesh
 */
std::vector<std::array<size_t, 3>> CreateFreeSurfaceTriangles(size_t eta_size);

/**
 * @brief vertices for mesh.
 *
 * @param eta TODO
 * @param t_vec vector of times at each timestep in simulation, evenly spaced in most cases
 *
 * @return vector of tuples representing vertices for mesh
 */
std::vector<std::array<double, 3>> CreateFreeSurface3DPts(const Eigen::VectorXd& eta, const Eigen::VectorXd& t_vec);

#endif
