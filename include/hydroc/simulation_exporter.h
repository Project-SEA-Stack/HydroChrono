/**
 * @file simulation_exporter.h
 * @brief Public interface for exporting HydroChrono simulations to HDF5.
 *
 * Provides a thin, high-level API to record inputs, model description, and
 * time histories into a structured HDF5 file. Does not own the Chrono system.
 *
 * @note Thread-safety: not thread-safe. Use one instance per simulation and
 *       synchronize externally if accessed from multiple threads.
 * @note Encoding & units: strings are UTF-8; time in seconds, distances in meters,
 *       angles in radians unless documented otherwise in attribute names.
 */
#ifndef HYDROC_SIMULATION_EXPORTER_H
#define HYDROC_SIMULATION_EXPORTER_H

#include <memory>
#include <vector>
#include <string>

namespace chrono { class ChSystem; class ChBody; class ChLink; }

namespace hydroc {

class H5Writer;

enum class H5Verbosity { Quiet = 0, Verbose = 1 };

class SimulationExporter {
  public:
    /**
     * @brief Options controlling export paths, provenance, and verbosity.
     */
    struct Options {
      /** Absolute or relative output HDF5 file path */
      std::string output_path;
      /** Full YAML text of the model (optional; used for provenance) */
      std::string model_yaml;
      /** Full YAML text of hydrodynamics config (optional; provenance) */
      std::string hydro_yaml;
      // Provenance
      std::string input_model_file;       // absolute or relative path used
      std::string input_simulation_file;  // absolute or relative path used
      std::string input_hydro_file;       // may be empty if no hydro file
      std::string output_directory;       // resolved output directory
      std::string output_tag;             // optional tag from CLI
      std::string setup_yaml_text;        // exact setup.yaml text
      std::string setup_yaml_path;        // path to setup.yaml used
      // Runtime (filled at end)
      int run_steps = 0;                  // steps simulated
      double run_dt = 0.0;                // seconds
      double run_time_final = 0.0;        // seconds
      std::string run_started_at_utc;  // set by runner
      std::string run_finished_at_utc; // set by runner
      double run_wall_time_s = 0.0;    // set by runner (seconds)

      // Scenario info (filled from already-parsed config)
      std::string scenario_type;          // still | regular | irregular | no_wave
      double scenario_H = 0.0;            // meters (regular)
      double scenario_T = 0.0;            // seconds (regular)
      double scenario_Hs = 0.0;           // meters (irregular)
      double scenario_Tp = 0.0;           // seconds (irregular)
      int scenario_seed = -1;             // irregular; -1 if unset
      H5Verbosity verbosity = H5Verbosity::Quiet;
    };

    /**
     * @brief Construct an exporter.
     * @param opts Export options; must specify a valid `output_path`.
     * @throws std::runtime_error if the HDF5 file cannot be opened/created.
     */
    SimulationExporter(const Options& opts);
    /**
     * @brief Destructor (non-throwing).
     */
    ~SimulationExporter() noexcept;

    // Non-copyable, movable (PIMPL unique ownership)
    SimulationExporter(const SimulationExporter&) = delete;
    SimulationExporter& operator=(const SimulationExporter&) = delete;
    SimulationExporter(SimulationExporter&&) noexcept = default;
    SimulationExporter& operator=(SimulationExporter&&) noexcept = default;

    /**
     * @brief Write simulation metadata and inputs to the file.
     * @param system Non-null Chrono system (world frame provides gravity).
     * @param chrono_version Chrono version string.
     * @param model_name Human-readable model name.
     * @param timestep Integration time step (seconds).
     * @param duration_seconds Planned simulation duration (seconds).
     * @pre system != nullptr
     * @throws H5::Exception or std::runtime_error on I/O failures.
     */
    void WriteSimulationInfo(chrono::ChSystem* system,
                             const std::string& chrono_version,
                             const std::string& model_name,
                             double timestep,
                             double duration_seconds);

    /**
     * @brief Deprecated: initial conditions are captured in WriteSimulationInfo.
     */
    [[deprecated("WriteInitialConditions is a no-op since schema v0.3; use WriteSimulationInfo")]]
    void WriteInitialConditions(chrono::ChSystem* system,
                                double water_density,
                                const std::string& wave_type,
                                double wave_height,
                                double wave_period);

    /**
     * @brief Discover and serialize model structure and initial state.
     * @param system Non-null Chrono system.
     * @pre system != nullptr
     * @throws H5::Exception or std::runtime_error on I/O failures.
     */
    void WriteModel(chrono::ChSystem* system);

    /**
     * @brief Initialize result buffers and record static provenance.
     * @param system Non-null system; used for immediate snapshotting if needed.
     * @param expected_steps Upper bound used to reserve buffers.
     * @pre system != nullptr; expected_steps >= 0
     * @throws std::invalid_argument if preconditions are violated.
     */
    void BeginResults(chrono::ChSystem* system, int expected_steps);
    /**
     * @brief Append one simulation step of state to in-memory buffers.
     * @pre system != nullptr
     * @throws std::invalid_argument if system is null.
     */
    void RecordStep(chrono::ChSystem* system);
    /**
     * @brief Flush all buffered results and close out metadata.
     */
    void Finalize();

    

    // Runtime metadata (written into /meta/run at Finalize)
    /**
     * @brief Record runtime metadata under /meta/run.
     * @param started_at_utc ISO-8601 UTC timestamp
     * @param finished_at_utc ISO-8601 UTC timestamp
     * @param wall_time_s Elapsed wall time (seconds)
     * @param steps Total number of steps
     * @param dt_s Time step (seconds)
     * @param time_final_s Final simulation time (seconds)
     */
    void SetRunMetadata(const std::string& started_at_utc,
                        const std::string& finished_at_utc,
                        double wall_time_s,
                        int steps,
                        double dt_s,
                        double time_final_s);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hydroc

#endif // HYDROC_SIMULATION_EXPORTER_H


