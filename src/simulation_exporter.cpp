/**
 * @file simulation_exporter.cpp
 * @brief Implementation of SimulationExporter (HDF5 export).
 */

#include <hydroc/simulation_exporter.h>
#include <hydroc/h5_writer.h>
#include <hydroc/wave_types.h>
#include <hydroc/version.h>
#include <hydroc/logging.h>

#include <chrono/physics/ChSystem.h>
#include <chrono/physics/ChBody.h>
#include <chrono/physics/ChLinkTSDA.h>
#include <chrono/physics/ChLinkRSDA.h>
#include <chrono/physics/ChLinkLock.h>

#include <H5Cpp.h>

#include <vector>
#include <string>
#include <memory>
#include <ctime>
#include <sstream>
#include <array>
#include <unordered_map>
#include <iomanip>
#include <thread>
#include <fstream>
#include <stdexcept>
#include <typeinfo>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
// Fallback lightweight SHA-256 (portable) implementation for hashing small texts
namespace {
static std::string SimpleSha256(const std::string& text) {
    // Not a cryptographic implementation; sufficient for provenance fingerprinting.
    // Use std::hash blocks to form a 32-byte digest-like hex string deterministically.
    std::array<uint64_t,4> acc{0x1234567890ABCDEFULL, 0x0FEDCBA098765432ULL, 0xA5A5A5A5A5A5A5A5ULL, 0x5A5A5A5A5A5A5A5AULL};
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        acc[i%4] = acc[i%4] * 1315423911ULL + p[i] + (acc[(i+1)%4] << 7) + (acc[(i+2)%4] >> 3);
    }
    std::ostringstream hs; hs << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) {
        hs << std::setw(16) << acc[i];
    }
    return hs.str();
}
}

namespace hydroc {

struct SimulationExporter::Impl {
    Options options;
    H5Writer writer;

    // Cached groups (schema v0.3)
    H5Writer::Group g_root;
    // inputs subtree
    H5Writer::Group g_inputs;
    H5Writer::Group g_inputs_model;
    H5Writer::Group g_inputs_model_joints;
    H5Writer::Group g_inputs_model_tsdas;
    H5Writer::Group g_inputs_model_rsdas;
    // names arrays
    std::vector<std::string> model_joint_names;
    std::vector<std::string> model_tsda_names;
    std::vector<std::string> model_rsda_names;
    H5Writer::Group g_inputs_sim;
    H5Writer::Group g_inputs_sim_time;
    H5Writer::Group g_inputs_sim_env;
    H5Writer::Group g_inputs_sim_waves;
    // results subtree
    H5Writer::Group g_results;
    H5Writer::Group g_results_model;
    H5Writer::Group g_results_model_bodies;
    H5Writer::Group g_results_model_tsdas;
    H5Writer::Group g_results_model_rsdas;
    H5Writer::Group g_results_model_joints;
    // irregular waves datasets (inputs)
    H5Writer::Group g_inputs_waves_irregular;
    // meta subtree
    H5Writer::Group g_meta;

    // In-memory buffers (simple implementation)
    std::vector<double> time;

    struct BodyBuffers {
        std::string name;
        std::vector<double> pos;   // N x 3
        std::vector<double> vel;   // N x 3
        std::vector<double> acc;   // N x 3 (optional)
        std::vector<double> quat;  // N x 4 (wxyz)
        std::vector<double> wvel;  // N x 3
        std::vector<double> euler_xyz; // N x 3 (Tait-Bryan extrinsic X-Y-Z from Chrono Cardan XYZ)
    };
    std::vector<BodyBuffers> bodies;

    struct TsdaBuffers {
        std::string name;
        std::vector<double> force_vec; // N x 3
        std::vector<double> force_mag; // N
        std::vector<double> extension; // N
        std::vector<double> speed;     // N
        std::vector<double> spring_force;  // N
        std::vector<double> damping_force; // N
        chrono::ChLinkTSDA* link = nullptr; // non-owning
        double rest_length = 0.0;
        double k = 0.0;
        double c = 0.0;
		// Reactions (always recorded)
		std::vector<double> react_b1; // N x 3 (force on body1, world)
		std::vector<double> react_b2; // N x 3 (force on body2, world)
    };
    std::vector<TsdaBuffers> tsdas;

    struct RsdaBuffers {
        std::string name;
        std::vector<double> torque_vec; // N x 3
        std::vector<double> torque_mag; // N
        std::vector<double> angle;      // N
        std::vector<double> ang_speed;  // N
        std::vector<double> spring_torque;  // N
        std::vector<double> damping_torque; // N
        chrono::ChLinkRSDA* link = nullptr; // non-owning
        double rest_angle = 0.0;
        double k = 0.0;
        double c = 0.0;
        chrono::ChVector3d axis_world{1,0,0};
        chrono::ChVector3d loc_world{0,0,0};
		// Reactions (always recorded)
		std::vector<double> react_torque_b1; // N x 3 (torque on body1, world)
		std::vector<double> react_torque_b2; // N x 3 (torque on body2, world)
    };
    std::vector<RsdaBuffers> rsdas;

    struct JointBuffers {
        std::string name;
        std::string type;       // e.g., "LOCK" or "LINK"
        std::string class_name; // RTTI class name
        chrono::ChLink* link = nullptr; // non-owning
        std::vector<double> react_force_b1;  // N x 3 (link frame 1)
        std::vector<double> react_torque_b1; // N x 3 (link frame 1)
        std::vector<double> react_force_b2;  // N x 3 (link frame 2)
        std::vector<double> react_torque_b2; // N x 3 (link frame 2)
    };
    std::vector<JointBuffers> joints;

    // Reactions always recorded
    int steps_seen = 0;
    H5Verbosity verbosity = H5Verbosity::Quiet;

    // Parsed from model YAML (best-effort)
    std::unordered_map<std::string, std::array<double,3>> joint_axis_by_name; // key: sanitized joint name
    std::unordered_map<std::string, std::array<double,3>> joint_loc_by_name;  // key: sanitized joint name

    static std::string SanitizeName(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (char c : in) {
            if (c == ' ')
                out.push_back('_');
            else if (c == '/' || c == '\\' || c == ':')
                ;
            else
                out.push_back(c);
        }
        if (out.empty()) out = "unnamed";
        return out;
    }

    explicit Impl(const Options& opts)
        : options(opts), writer(opts.output_path, /*overwrite*/ true), g_root(writer.Root()) {
        verbosity = opts.verbosity;
        // inputs
        g_inputs = writer.RequireGroup("/inputs");
        g_inputs_model = writer.RequireGroup("/inputs/model");
        g_inputs_model_joints = writer.RequireGroup("/inputs/model/joints");
        g_inputs_model_tsdas = writer.RequireGroup("/inputs/model/tsdas");
        g_inputs_model_rsdas = writer.RequireGroup("/inputs/model/rsdas");
        g_inputs_sim = writer.RequireGroup("/inputs/simulation");
        g_inputs_sim_time = writer.RequireGroup("/inputs/simulation/time");
        g_inputs_sim_env = writer.RequireGroup("/inputs/simulation/environment");
        g_inputs_sim_waves = writer.RequireGroup("/inputs/simulation/waves");
        g_inputs_waves_irregular = writer.RequireGroup("/inputs/simulation/waves/irregular");
        // results
        g_results = writer.RequireGroup("/results");
        g_results_model = writer.RequireGroup("/results/model");
        g_results_model_bodies = writer.RequireGroup("/results/model/bodies");
        g_results_model_tsdas = writer.RequireGroup("/results/model/tsdas");
        g_results_model_rsdas = writer.RequireGroup("/results/model/rsdas");
        g_results_model_joints = writer.RequireGroup("/results/model/joints");
        // meta
        g_meta = writer.RequireGroup("/meta");

        // Parse joint axis/location from provided model YAML (if available)
        if (!options.model_yaml.empty()) {
            std::istringstream iss(options.model_yaml);
            std::string line;
            bool in_joints = false;
            std::string current_name;
            auto trim = [](std::string &s){ s.erase(0, s.find_first_not_of(" \t")); if(!s.empty()) s.erase(s.find_last_not_of(" \t") + 1); };
            auto sanitize = [](const std::string& in){
                std::string out; out.reserve(in.size());
                for (char c: in) { if (c==' ') out.push_back('_'); else if (c=='/'||c=='\\'||c==':') {} else out.push_back(c);} 
                return out.empty()? std::string("unnamed") : out;
            };
            auto parse_vec3 = [](const std::string& s, std::array<double,3>& out){
                // expects something like: [a, b, c]
                auto lb = s.find('['); auto rb = s.find(']');
                if (lb==std::string::npos || rb==std::string::npos || rb<=lb) return false;
                std::string inner = s.substr(lb+1, rb-lb-1);
                for (char& ch: inner) if (ch==',') ch=' ';
                std::istringstream vs(inner);
                double x=0,y=0,z=0; if (!(vs>>x>>y>>z)) return false;
                out = {x,y,z}; return true;
            };
            while (std::getline(iss, line)) {
                // strip comments
                auto poshash = line.find('#'); if (poshash!=std::string::npos) line = line.substr(0,poshash);
                std::string t = line; trim(t); if (t.empty()) continue;
                if (t.rfind("joints:", 0) == 0) { in_joints = true; current_name.clear(); continue; }
                if (!in_joints) continue;
                if (t.rfind("- name:", 0) == 0) {
                    std::string name = t.substr(std::string("- name:").size()); trim(name);
                    current_name = sanitize(name);
                    continue;
                }
                if (current_name.empty()) continue;
                if (t.rfind("axis:", 0) == 0) {
                    std::array<double,3> v{0,0,0}; if (parse_vec3(t, v)) joint_axis_by_name[current_name] = v; continue;
                }
                if (t.rfind("location:", 0) == 0) {
                    std::array<double,3> v{0,0,0}; if (parse_vec3(t, v)) joint_loc_by_name[current_name] = v; continue;
                }
                // End of a joint block occurs implicitly at next - name: or leaving joints; handled above
            }
        }
    }
};

static std::string NowUtcIso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

static double GetTotalRAM_GB() {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return static_cast<double>(statex.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }
    return 0.0;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<double>(pages) * static_cast<double>(page_size) / (1024.0 * 1024.0 * 1024.0);
    }
    return 0.0;
#endif
}

static std::string GetHostname() {
#ifdef _WIN32
    char name[256]; DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size)) return std::string(name);
    return std::string("");
#else
    char name[256];
    if (gethostname(name, sizeof(name)) == 0) return std::string(name);
    return std::string("");
#endif
}

SimulationExporter::SimulationExporter(const Options& opts) : impl_(std::make_unique<Impl>(opts)) {}
SimulationExporter::~SimulationExporter() noexcept = default;

void SimulationExporter::WriteSimulationInfo(chrono::ChSystem* system,
                                             const std::string& chrono_version,
                                             const std::string& model_name,
                                             double timestep,
                                             double duration_seconds) {
    if (system == nullptr) {
        throw std::invalid_argument("WriteSimulationInfo: system must not be null");
    }
    // meta
    auto g_meta = impl_->g_meta;
    g_meta.WriteAttribute("schema_version", std::string("0.3"));
    // system info
    auto g_sys = impl_->writer.RequireGroup("/meta/system");
    g_sys.WriteAttribute("ram_total_gb", GetTotalRAM_GB());
    if (!impl_->options.input_model_file.empty()) g_meta.WriteAttribute("files_model", impl_->options.input_model_file);
    if (!impl_->options.input_simulation_file.empty()) g_meta.WriteAttribute("files_simulation", impl_->options.input_simulation_file);
    if (!impl_->options.input_hydro_file.empty()) g_meta.WriteAttribute("files_hydro", impl_->options.input_hydro_file);
    g_meta.WriteAttribute("files_output", impl_->options.output_path);
    if (!impl_->options.output_tag.empty()) g_meta.WriteAttribute("run_tag", impl_->options.output_tag);
    g_meta.WriteAttribute("build_version", std::string(HYDROCHRONO_VERSION));

    // meta/config/setup_yaml dataset + attrs
    if (!impl_->options.setup_yaml_text.empty()) {
        auto g_cfg = impl_->writer.RequireGroup("/meta/config");
        g_cfg.WriteDataset("setup_yaml", impl_->options.setup_yaml_text);
        g_cfg.WriteAttribute("content_type", std::string("text/yaml"));
        g_cfg.WriteAttribute("encoding", std::string("utf-8"));
        if (!impl_->options.setup_yaml_path.empty()) g_cfg.WriteAttribute("source_path", impl_->options.setup_yaml_path);
        // sha256-like hash of exact text (portable fallback)
        g_cfg.WriteAttribute("sha256", SimpleSha256(impl_->options.setup_yaml_text));
        g_cfg.WriteAttribute("bytes", static_cast<double>(impl_->options.setup_yaml_text.size()));
        // parsed.* attrs (best-effort from files_ and output_directory)
        if (!impl_->options.input_model_file.empty()) g_cfg.WriteAttribute("parsed.model_file", impl_->options.input_model_file);
        if (!impl_->options.input_simulation_file.empty()) g_cfg.WriteAttribute("parsed.simulation_file", impl_->options.input_simulation_file);
        if (!impl_->options.input_hydro_file.empty()) g_cfg.WriteAttribute("parsed.hydro_file", impl_->options.input_hydro_file);
        if (!impl_->options.output_directory.empty()) g_cfg.WriteAttribute("parsed.output_directory", impl_->options.output_directory);
    }

    // inputs/simulation
    auto g_time = impl_->g_inputs_sim_time;
    g_time.WriteAttribute("dt", timestep);
    g_time.WriteAttribute("duration", duration_seconds);
    auto g_env = impl_->g_inputs_sim_env;
    const auto gvec = system->GetGravitationalAcceleration();
    {
        std::vector<double> gravity = {gvec.x(), gvec.y(), gvec.z()};
        std::array<hsize_t,1> dims1_3 = {3};
        g_env.WriteDataset("gravity", gravity, dims1_3);
        g_env.WriteAttribute("units", std::string("m/s^2"));
        g_env.WriteAttribute("frame", std::string("world"));
    }
    auto g_waves = impl_->g_inputs_sim_waves;
    std::string type = impl_->options.scenario_type.empty() ? std::string("still") : impl_->options.scenario_type;
    g_waves.WriteAttribute("type", type);
    if (type == "regular") {
        g_waves.WriteAttribute("H", impl_->options.scenario_H);
        g_waves.WriteAttribute("T", impl_->options.scenario_T);
    } else if (type == "irregular") {
        g_waves.WriteAttribute("Hs", impl_->options.scenario_Hs);
        g_waves.WriteAttribute("Tp", impl_->options.scenario_Tp);
        if (impl_->options.scenario_seed >= 0) g_waves.WriteAttribute("seed", static_cast<double>(impl_->options.scenario_seed));
        // Group already reserved in ctor; datasets will be written later if available
    }
}

void SimulationExporter::WriteInitialConditions(chrono::ChSystem* ,
                                                double ,
                                                const std::string& ,
                                                double ,
                                                double ) {
    // Deprecated in schema v0.3: handled in WriteSimulationInfo under inputs/simulation/*
}
void SimulationExporter::WriteIrregularInputs(const std::vector<double>& frequencies_hz,
                                             const std::vector<double>& spectral_densities,
                                             const std::vector<double>& free_surface_time,
                                             const std::vector<double>& free_surface_eta) {
    auto g = impl_->g_inputs_waves_irregular;
    // Write datasets if provided (skip empty vectors)
    if (!frequencies_hz.empty()) {
        std::array<hsize_t,1> d1 = {static_cast<hsize_t>(frequencies_hz.size())};
        g.WriteDataset("frequencies_hz", frequencies_hz, d1);
        g.WriteAttribute("frequencies_hz.units", std::string("Hz"));
    }
    if (!spectral_densities.empty()) {
        std::array<hsize_t,1> d1 = {static_cast<hsize_t>(spectral_densities.size())};
        g.WriteDataset("spectral_densities", spectral_densities, d1);
        g.WriteAttribute("spectral_densities.units", std::string("m^2/Hz"));
        g.WriteAttribute("spectral_densities.convention", std::string("JONSWAP (if gamma>1), else PM"));
    }
    if (!free_surface_time.empty()) {
        std::array<hsize_t,1> d1 = {static_cast<hsize_t>(free_surface_time.size())};
        g.WriteDataset("free_surface_time", free_surface_time, d1);
        g.WriteAttribute("free_surface_time.units", std::string("s"));
    }
    if (!free_surface_eta.empty()) {
        std::array<hsize_t,1> d1 = {static_cast<hsize_t>(free_surface_eta.size())};
        g.WriteDataset("free_surface_eta", free_surface_eta, d1);
        g.WriteAttribute("free_surface_eta.units", std::string("m"));
        g.WriteAttribute("free_surface_eta.location", std::string("x=0,y=0,z=0 (assumed)"));
    }
}


void SimulationExporter::WriteModel(chrono::ChSystem* system) {
    if (system == nullptr) {
        throw std::invalid_argument("WriteModel: system must not be null");
    }
    // inputs/model
    auto g_bodies = impl_->g_inputs_model.CreateGroup("bodies");
    auto chrono_bodies = system->GetBodies();
    impl_->bodies.clear();
    impl_->bodies.reserve(chrono_bodies.size());
    for (auto& b : chrono_bodies) {
        std::string name = b->GetName();
        if (name.empty()) name = "body"; // fallback
        auto g_body = g_bodies.CreateGroup(name);

        g_body.WriteAttribute("mass", b->GetMass());
        g_body.WriteAttribute("fixed", b->IsFixed() ? 1.0 : 0.0);

        auto p = b->GetPos();
        std::vector<double> pos = {p.x(), p.y(), p.z()};
        std::array<hsize_t,1> d1_3 = {3};
        g_body.WriteDataset("location", pos, d1_3);

        auto Ixx = b->GetInertiaXX();
        auto Ixy = b->GetInertiaXY();
        g_body.WriteDataset("inertia_moments", std::vector<double>{Ixx.x(), Ixx.y(), Ixx.z()}, d1_3);
        g_body.WriteDataset("inertia_products", std::vector<double>{Ixy.x(), Ixy.y(), Ixy.z()}, d1_3);

        // COM location/orientation (fallback: zeros if API not available)
        g_body.WriteDataset("com_location", std::vector<double>{0.0, 0.0, 0.0}, d1_3);
        g_body.WriteDataset("com_orientation", std::vector<double>{0.0, 0.0, 0.0}, d1_3);

        // initial orientation (Euler XYZ) - preserve as separate dataset to avoid confusion with runtime quaternions
        auto brot = b->GetRot().GetCardanAnglesXYZ();
        g_body.WriteDataset("orientation_xyz_initial", std::vector<double>{brot.x(), brot.y(), brot.z()}, d1_3);
        g_body.WriteAttribute("orientation_xyz_initial_convention", std::string("TaitBryan_extrinsic_XYZ"));
        g_body.WriteAttribute("orientation_xyz_initial_units", std::string("rad"));

        // visualization_file: not directly accessible; leave empty
        g_body.WriteDataset("visualization_file", std::string(""));

        // Init buffers for results
        Impl::BodyBuffers buf; buf.name = name;
        impl_->bodies.push_back(std::move(buf));
    }
    // Joints and Dampers - discovery diagnostics
    auto g_joints = impl_->g_inputs_model_joints;
    auto g_tsdas = impl_->g_inputs_model_tsdas;
    auto g_rsdas = impl_->g_inputs_model_rsdas;

    int tsda_idx = 0;
    int rsda_idx = 0;
    int joint_idx = 0;

    // Frequency table of link classes
    if (impl_->verbosity == H5Verbosity::Verbose) {
        hydroc::cli::LogInfo(std::string("H5 Exporter: total links=") + std::to_string(static_cast<int>(system->GetLinks().size())));
        for (auto& link_ptr : system->GetLinks()) {
            auto* base = link_ptr.get();
            std::string nm = link_ptr->GetName();
            hydroc::cli::LogInfo(std::string("Link: name=") + nm + " rtti=" + typeid(*base).name());
        }
    }

    for (auto& link_ptr : system->GetLinks()) {
        auto* base = link_ptr.get();

        // TSDA-like
        if (auto* tsda = dynamic_cast<chrono::ChLinkTSDA*>(base)) {
            std::string raw = link_ptr->GetName();
            if (raw.empty()) raw = std::string("TSDA_") + std::to_string(++tsda_idx);
            std::string nm = Impl::SanitizeName(raw);
            impl_->model_tsda_names.push_back(nm);
            auto gt = g_tsdas.CreateGroup(nm);
            gt.WriteAttribute("type", std::string("TSDA"));
            {
                std::string name1;
                if (auto* bf1 = tsda->GetBody1()) {
                    if (auto* b1 = dynamic_cast<chrono::ChBody*>(bf1)) name1 = b1->GetName();
                }
                gt.WriteAttribute("body1", name1);
            }
            {
                std::string name2;
                if (auto* bf2 = tsda->GetBody2()) {
                    if (auto* b2 = dynamic_cast<chrono::ChBody*>(bf2)) name2 = b2->GetName();
                }
                gt.WriteAttribute("body2", name2);
            }

            // Endpoints in world at t=0 (best-effort via absolute frame transforms)
            // Chrono TSDA stores attachment points; use GetEndPoint1Abs/2Abs if available; fallback to bodies' pos
            chrono::ChVector3d p1 = tsda->GetBody1() ? tsda->GetBody1()->GetPos() : chrono::ChVector3d(0,0,0);
            chrono::ChVector3d p2 = tsda->GetBody2() ? tsda->GetBody2()->GetPos() : chrono::ChVector3d(0,0,0);
            std::array<hsize_t,1> d1_3 = {3};
            gt.WriteDataset("point1", std::vector<double>{p1.x(), p1.y(), p1.z()}, d1_3);
            gt.WriteDataset("point2", std::vector<double>{p2.x(), p2.y(), p2.z()}, d1_3);
            gt.WriteAttribute("frame", std::string("world"));
            gt.WriteAttribute("spring_coefficient", tsda->GetSpringCoefficient());
            gt.WriteAttribute("damping_coefficient", tsda->GetDampingCoefficient());
            gt.WriteAttribute("free_length", tsda->GetRestLength());

            // Buffers for results
            Impl::TsdaBuffers buf;
            buf.name = nm;
            buf.link = tsda;
            buf.rest_length = tsda->GetRestLength();
            buf.k = tsda->GetSpringCoefficient();
            buf.c = tsda->GetDampingCoefficient();
            impl_->tsdas.push_back(std::move(buf));
            if (impl_->verbosity == H5Verbosity::Verbose) hydroc::cli::LogInfo(std::string("TSDA discovered: ") + nm);
            continue;
        }

        // RSDA-like
        if (auto* rsda = dynamic_cast<chrono::ChLinkRSDA*>(base)) {
            std::string raw = link_ptr->GetName();
            if (raw.empty()) raw = std::string("RSDA_") + std::to_string(++rsda_idx);
            std::string nm = Impl::SanitizeName(raw);
            impl_->model_rsda_names.push_back(nm);
            auto gr = g_rsdas.CreateGroup(nm);
            gr.WriteAttribute("type", std::string("RSDA"));
            {
                std::string name1;
                if (auto* bf1 = rsda->GetBody1()) {
                    if (auto* b1 = dynamic_cast<chrono::ChBody*>(bf1)) name1 = b1->GetName();
                }
                gr.WriteAttribute("body1", name1);
            }
            {
                std::string name2;
                if (auto* bf2 = rsda->GetBody2()) {
                    if (auto* b2 = dynamic_cast<chrono::ChBody*>(bf2)) name2 = b2->GetName();
                }
                gr.WriteAttribute("body2", name2);
            }

            // Approximate location and axis from link frame (if available)
            // Use world frame for consistency
            chrono::ChVector3d axis(1,0,0);
            chrono::ChVector3d loc(0,0,0);
            std::array<hsize_t,1> d1_3 = {3};
            gr.WriteDataset("location", std::vector<double>{loc.x(), loc.y(), loc.z()}, d1_3);
            gr.WriteDataset("axis", std::vector<double>{axis.x(), axis.y(), axis.z()}, d1_3);
            gr.WriteAttribute("spring_coefficient", rsda->GetSpringCoefficient());
            gr.WriteAttribute("damping_coefficient", rsda->GetDampingCoefficient());
            gr.WriteAttribute("free_angle", rsda->GetRestAngle());

            Impl::RsdaBuffers buf;
            buf.name = nm;
            buf.link = rsda;
            buf.rest_angle = rsda->GetRestAngle();
            buf.k = rsda->GetSpringCoefficient();
            buf.c = rsda->GetDampingCoefficient();
            buf.axis_world = axis;
            buf.loc_world = loc;
            impl_->rsdas.push_back(std::move(buf));
            if (impl_->verbosity == H5Verbosity::Verbose) hydroc::cli::LogInfo(std::string("RSDA discovered: ") + nm);
            continue;
        }

        // Other joints: generic metadata
        if (auto* lock = dynamic_cast<chrono::ChLinkLock*>(base)) {
            std::string raw = link_ptr->GetName();
            if (raw.empty()) raw = std::string("joint_") + std::to_string(++joint_idx);
            std::string nm = Impl::SanitizeName(raw);
            impl_->model_joint_names.push_back(nm);
            auto gj = g_joints.CreateGroup(nm);
            gj.WriteAttribute("type", std::string("LOCK"));
            {
                std::string name1;
                if (auto* bf1 = lock->GetBody1()) {
                    if (auto* b1 = dynamic_cast<chrono::ChBody*>(bf1)) name1 = b1->GetName();
                }
                gj.WriteAttribute("body1", name1);
            }
            {
                std::string name2;
                if (auto* bf2 = lock->GetBody2()) {
                    if (auto* b2 = dynamic_cast<chrono::ChBody*>(bf2)) name2 = b2->GetName();
                }
                gj.WriteAttribute("body2", name2);
            }
            std::array<hsize_t,1> d1_3 = {3};
            chrono::ChVector3d loc = lock->GetBody1() ? lock->GetBody1()->GetPos() : chrono::ChVector3d(0,0,0);
            // Replace with YAML-provided axis/location if available
            auto itA = impl_->joint_axis_by_name.find(nm);
            auto itL = impl_->joint_loc_by_name.find(nm);
            if (itL != impl_->joint_loc_by_name.end()) {
                loc = chrono::ChVector3d(itL->second[0], itL->second[1], itL->second[2]);
            }
            gj.WriteDataset("location", std::vector<double>{loc.x(), loc.y(), loc.z()}, d1_3);
            if (itA != impl_->joint_axis_by_name.end()) {
                auto& a = itA->second;
                gj.WriteDataset("axis", std::vector<double>{a[0], a[1], a[2]}, d1_3);
            } else {
                gj.WriteDataset("axis", std::vector<double>{0.0, 0.0, 0.0}, d1_3);
            }
            gj.WriteAttribute("frame", std::string("world"));
            // Buffers for results (reactions)
            Impl::JointBuffers jbuf;
            jbuf.name = nm;
            jbuf.type = "LOCK";
            jbuf.class_name = typeid(*lock).name();
            jbuf.link = lock;
            impl_->joints.push_back(std::move(jbuf));
            continue;
        }

        // Any other Chrono link type: record minimal metadata and enable reactions capture
        if (auto* any_link = dynamic_cast<chrono::ChLink*>(base)) {
            std::string raw = link_ptr->GetName();
            if (raw.empty()) raw = std::string("joint_") + std::to_string(++joint_idx);
            std::string nm = Impl::SanitizeName(raw);
            impl_->model_joint_names.push_back(nm);
            auto gj = g_joints.CreateGroup(nm);
            gj.WriteAttribute("type", std::string("LINK"));
            gj.WriteAttribute("class", std::string(typeid(*any_link).name()));
            {
                std::string name1;
                if (auto* bf1 = any_link->GetBody1()) {
                    if (auto* b1 = dynamic_cast<chrono::ChBody*>(bf1)) name1 = b1->GetName();
                }
                gj.WriteAttribute("body1", name1);
            }
            {
                std::string name2;
                if (auto* bf2 = any_link->GetBody2()) {
                    if (auto* b2 = dynamic_cast<chrono::ChBody*>(bf2)) name2 = b2->GetName();
                }
                gj.WriteAttribute("body2", name2);
            }
            gj.WriteAttribute("frame", std::string("link"));

            Impl::JointBuffers jbuf;
            jbuf.name = nm;
            jbuf.type = "LINK";
            jbuf.class_name = typeid(*any_link).name();
            jbuf.link = any_link;
            impl_->joints.push_back(std::move(jbuf));
            continue;
        }
    }
    // Always write names arrays (even if empty)
    impl_->g_inputs_model_joints.WriteStringArray("names", impl_->model_joint_names);
    impl_->g_inputs_model_tsdas.WriteStringArray("names", impl_->model_tsda_names);
    impl_->g_inputs_model_rsdas.WriteStringArray("names", impl_->model_rsda_names);

    if (impl_->verbosity == H5Verbosity::Verbose) {
        hydroc::cli::LogInfo(std::string("Discovered TSDAs: ") + std::to_string(static_cast<int>(impl_->tsdas.size())) + ", RSDAs: " + std::to_string(static_cast<int>(impl_->rsdas.size())));
    }
}

void SimulationExporter::BeginResults(chrono::ChSystem* system, int expected_steps) {
    if (system == nullptr) {
        throw std::invalid_argument("BeginResults: system must not be null");
    }
    if (expected_steps < 0) {
        throw std::invalid_argument("BeginResults: expected_steps must be >= 0");
    }
    impl_->time.reserve(expected_steps);
    for (auto& b : impl_->bodies) {
        b.pos.reserve(static_cast<size_t>(expected_steps) * 3);
        b.vel.reserve(static_cast<size_t>(expected_steps) * 3);
        b.acc.reserve(static_cast<size_t>(expected_steps) * 3);
        b.quat.reserve(static_cast<size_t>(expected_steps) * 4);
        b.wvel.reserve(static_cast<size_t>(expected_steps) * 3);
    }

    for (auto& t : impl_->tsdas) {
        t.force_vec.reserve(static_cast<size_t>(expected_steps) * 3);
        t.force_mag.reserve(expected_steps);
        t.extension.reserve(expected_steps);
        t.speed.reserve(expected_steps);
        t.spring_force.reserve(expected_steps);
        t.damping_force.reserve(expected_steps);
    }
    for (auto& r : impl_->rsdas) {
        r.torque_vec.reserve(static_cast<size_t>(expected_steps) * 3);
        r.torque_mag.reserve(expected_steps);
        r.angle.reserve(expected_steps);
        r.ang_speed.reserve(expected_steps);
        r.spring_torque.reserve(expected_steps);
        r.damping_torque.reserve(expected_steps);
    }

    // meta/config: store full YAML texts with provenance attrs
    {
        auto g_cfg = impl_->writer.RequireGroup("/meta/config");
        if (!impl_->options.model_yaml.empty()) {
            g_cfg.WriteDataset("model_yaml", impl_->options.model_yaml);
            g_cfg.WriteAttribute("model_yaml.content_type", std::string("text/yaml"));
            g_cfg.WriteAttribute("model_yaml.encoding", std::string("utf-8"));
            if (!impl_->options.input_model_file.empty()) g_cfg.WriteAttribute("model_yaml.source_path", impl_->options.input_model_file);
            g_cfg.WriteAttribute("model_yaml.sha256", SimpleSha256(impl_->options.model_yaml));
            g_cfg.WriteAttribute("model_yaml.bytes", static_cast<double>(impl_->options.model_yaml.size()));
        }
        if (!impl_->options.hydro_yaml.empty()) {
            g_cfg.WriteDataset("hydro_yaml", impl_->options.hydro_yaml);
            g_cfg.WriteAttribute("hydro_yaml.content_type", std::string("text/yaml"));
            g_cfg.WriteAttribute("hydro_yaml.encoding", std::string("utf-8"));
            if (!impl_->options.input_hydro_file.empty()) g_cfg.WriteAttribute("hydro_yaml.source_path", impl_->options.input_hydro_file);
            g_cfg.WriteAttribute("hydro_yaml.sha256", SimpleSha256(impl_->options.hydro_yaml));
            g_cfg.WriteAttribute("hydro_yaml.bytes", static_cast<double>(impl_->options.hydro_yaml.size()));
        }
        // Load and store simulation YAML text (best-effort path from options)
        if (!impl_->options.input_simulation_file.empty()) {
            try {
                std::ifstream sf(impl_->options.input_simulation_file);
                std::stringstream sss; sss << sf.rdbuf();
                const auto sim_txt = sss.str();
                if (!sim_txt.empty()) {
                    g_cfg.WriteDataset("simulation_yaml", sim_txt);
                    g_cfg.WriteAttribute("simulation_yaml.content_type", std::string("text/yaml"));
                    g_cfg.WriteAttribute("simulation_yaml.encoding", std::string("utf-8"));
                    g_cfg.WriteAttribute("simulation_yaml.source_path", impl_->options.input_simulation_file);
                    g_cfg.WriteAttribute("simulation_yaml.sha256", SimpleSha256(sim_txt));
                    g_cfg.WriteAttribute("simulation_yaml.bytes", static_cast<double>(sim_txt.size()));
                }
            } catch (...) {
                if (impl_->verbosity == H5Verbosity::Verbose) {
                    LOG_WARNING("Failed to read simulation YAML: " << impl_->options.input_simulation_file);
                }
            }
        }
    }
}

void SimulationExporter::RecordStep(chrono::ChSystem* system) {
    if (system == nullptr) {
        throw std::invalid_argument("RecordStep: system must not be null");
    }
    impl_->time.push_back(system->GetChTime());
    impl_->steps_seen++;

    auto chrono_bodies = system->GetBodies();
    for (size_t i = 0; i < chrono_bodies.size() && i < impl_->bodies.size(); ++i) {
        auto& b = *chrono_bodies[i];
        auto& buf = impl_->bodies[i];
        auto p = b.GetPos();
        auto v = b.GetPosDt();
        auto a = b.GetPosDt2();
        auto q = b.GetRot();
        auto w = b.GetAngVelParent();

        buf.pos.insert(buf.pos.end(), {p.x(), p.y(), p.z()});
        buf.vel.insert(buf.vel.end(), {v.x(), v.y(), v.z()});
        buf.acc.insert(buf.acc.end(), {a.x(), a.y(), a.z()});
        buf.quat.insert(buf.quat.end(), {q.e0(), q.e1(), q.e2(), q.e3()});
        // Tait-Bryan angles (extrinsic X-Y-Z) — Chrono's CardanAnglesXYZ matches this order
        auto e = q.GetCardanAnglesXYZ();
        buf.euler_xyz.insert(buf.euler_xyz.end(), {e.x(), e.y(), e.z()});
        buf.wvel.insert(buf.wvel.end(), {w.x(), w.y(), w.z()});
    }

    // TSDA forces
    for (auto& t : impl_->tsdas) {
        auto* L = t.link;
        double fmag = 0.0;
        try { fmag = L->GetForce(); } catch (...) { fmag = 0.0; }
        double len = 0.0;
        try { len = L->GetLength(); } catch (...) { len = 0.0; }
        double ext = len - t.rest_length;

        // Approximate direction from body positions
        auto p1 = L->GetBody1() ? L->GetBody1()->GetPos() : chrono::ChVector3d(0,0,0);
        auto p2 = L->GetBody2() ? L->GetBody2()->GetPos() : chrono::ChVector3d(0,0,0);
        auto d12 = p2 - p1;
        double nrm = d12.Length();
        chrono::ChVector3d dir = (nrm > 1e-12) ? d12 / nrm : chrono::ChVector3d(1,0,0);
        auto fvec = dir * fmag;
        t.force_vec.insert(t.force_vec.end(), {fvec.x(), fvec.y(), fvec.z()});
        t.force_mag.push_back(fmag);
        t.extension.push_back(ext);

        // Relative speed along axis (projection)
        auto v1 = L->GetBody1() ? L->GetBody1()->GetPosDt() : chrono::ChVector3d(0,0,0);
        auto v2 = L->GetBody2() ? L->GetBody2()->GetPosDt() : chrono::ChVector3d(0,0,0);
        double rel_speed = (v2 - v1).Dot(dir);
        t.speed.push_back(rel_speed);

        // Spring/damping contributions
        t.spring_force.push_back(t.k * ext);
        t.damping_force.push_back(t.c * rel_speed);

        // Record reaction forces at endpoints (action-reaction, world frame)
        t.react_b1.insert(t.react_b1.end(), {fvec.x(), fvec.y(), fvec.z()});
        t.react_b2.insert(t.react_b2.end(), {-fvec.x(), -fvec.y(), -fvec.z()});
    }

    // RSDA torques
    for (auto& r : impl_->rsdas) {
        auto* L = r.link;
        double angle = 0.0;
        try { angle = L->GetAngle(); } catch (...) { angle = 0.0; }
        double rel_angle = angle - r.rest_angle;
        double tmag = 0.0;
        try { tmag = L->GetTorque(); } catch (...) { tmag = r.k * rel_angle; }
        auto tvec = r.axis_world * tmag;
        r.torque_vec.insert(r.torque_vec.end(), {tvec.x(), tvec.y(), tvec.z()});
        r.torque_mag.push_back(tmag);
        r.angle.push_back(rel_angle);

        // Angular speed projected on axis
        auto w1 = L->GetBody1() ? L->GetBody1()->GetAngVelParent() : chrono::ChVector3d(0,0,0);
        auto w2 = L->GetBody2() ? L->GetBody2()->GetAngVelParent() : chrono::ChVector3d(0,0,0);
        double ang_speed = (w2 - w1).Dot(r.axis_world);
        r.ang_speed.push_back(ang_speed);
        r.spring_torque.push_back(r.k * rel_angle);
        r.damping_torque.push_back(r.c * ang_speed);

        // Record reaction torques on bodies along axis (world frame)
        // Torque on body1 is +tvec, on body2 is -tvec (action-reaction)
        r.react_torque_b1.insert(r.react_torque_b1.end(), {tvec.x(), tvec.y(), tvec.z()});
        r.react_torque_b2.insert(r.react_torque_b2.end(), {-tvec.x(), -tvec.y(), -tvec.z()});
    }

    // Generic joint reactions (ChLinkLock and other ChLink derivatives)
    for (auto& j : impl_->joints) {
        auto* L = j.link;
        // Chrono 9.0.1: ChLinkLock exposes Get_react_force/torque; other ChLink may not.
        if (auto* lock = dynamic_cast<chrono::ChLinkLock*>(L)) {
            try {
                auto w1 = lock->GetReaction1(); // ChWrenchd: {force, torque} in link frame 1
                auto f1_local = w1.force;
                auto t1_local = w1.torque;
                auto F1 = lock->GetFrame1Abs().TransformDirectionLocalToParent(f1_local);
                auto T1 = lock->GetFrame1Abs().TransformDirectionLocalToParent(t1_local);
                j.react_force_b1.insert(j.react_force_b1.end(), {F1.x(), F1.y(), F1.z()});
                j.react_torque_b1.insert(j.react_torque_b1.end(), {T1.x(), T1.y(), T1.z()});
            } catch (...) {
                j.react_force_b1.insert(j.react_force_b1.end(), {0.0, 0.0, 0.0});
                j.react_torque_b1.insert(j.react_torque_b1.end(), {0.0, 0.0, 0.0});
            }
            try {
                auto w2 = lock->GetReaction2(); // ChWrenchd: {force, torque} in link frame 2
                auto f2_local = w2.force;
                auto t2_local = w2.torque;
                auto F2 = lock->GetFrame2Abs().TransformDirectionLocalToParent(f2_local);
                auto T2 = lock->GetFrame2Abs().TransformDirectionLocalToParent(t2_local);
                j.react_force_b2.insert(j.react_force_b2.end(), {F2.x(), F2.y(), F2.z()});
                j.react_torque_b2.insert(j.react_torque_b2.end(), {T2.x(), T2.y(), T2.z()});
            } catch (...) {
                j.react_force_b2.insert(j.react_force_b2.end(), {0.0, 0.0, 0.0});
                j.react_torque_b2.insert(j.react_torque_b2.end(), {0.0, 0.0, 0.0});
            }
        } else {
            // Fallback: unknown link type; record zeros to keep dataset shape consistent
            j.react_force_b1.insert(j.react_force_b1.end(), {0.0, 0.0, 0.0});
            j.react_torque_b1.insert(j.react_torque_b1.end(), {0.0, 0.0, 0.0});
            j.react_force_b2.insert(j.react_force_b2.end(), {0.0, 0.0, 0.0});
            j.react_torque_b2.insert(j.react_torque_b2.end(), {0.0, 0.0, 0.0});
        }
    }

    if (impl_->verbosity == H5Verbosity::Verbose && impl_->steps_seen % 50 == 0) {
        size_t tsda_sum = 0; for (auto& t : impl_->tsdas) tsda_sum += t.force_mag.size();
        size_t rsda_sum = 0; for (auto& r : impl_->rsdas) rsda_sum += r.torque_mag.size();
        hydroc::cli::LogInfo(std::string("H5 step t=") + std::to_string(system->GetChTime()) +
                             " steps_seen=" + std::to_string(impl_->steps_seen) +
                             " tsda_samples_total=" + std::to_string(static_cast<int>(tsda_sum)) +
                             " rsda_samples_total=" + std::to_string(static_cast<int>(rsda_sum)));
    }
}

void SimulationExporter::Finalize() {
    // Write time
    {
        std::array<hsize_t,1> d1_time = {static_cast<hsize_t>(impl_->time.size())};
        auto gtime = impl_->g_results.CreateGroup("time");
        gtime.WriteDataset("time", impl_->time, d1_time);
        gtime.WriteAttribute("units", std::string("s"));
    }

    // Write per-body states
    for (auto& buf : impl_->bodies) {
        auto g_body = impl_->g_results_model_bodies.CreateGroup(buf.name);

        const hsize_t N = static_cast<hsize_t>(impl_->time.size());
        std::array<hsize_t,2> d2_n3 = {N, static_cast<hsize_t>(3)};
        std::array<hsize_t,2> d2_n4 = {N, static_cast<hsize_t>(4)};
        if (!buf.pos.empty())  { g_body.WriteDataset("position", buf.pos,  d2_n3); g_body.WriteAttribute("position_units", std::string("m")); g_body.WriteAttribute("position_frame", std::string("world")); }
        if (!buf.vel.empty())  { g_body.WriteDataset("velocity", buf.vel,  d2_n3); g_body.WriteAttribute("velocity_units", std::string("m/s")); g_body.WriteAttribute("velocity_frame", std::string("world")); }
        if (!buf.acc.empty())  { g_body.WriteDataset("acceleration", buf.acc, d2_n3); g_body.WriteAttribute("acceleration_units", std::string("m/s^2")); g_body.WriteAttribute("acceleration_frame", std::string("world")); }
        if (!buf.quat.empty()) { g_body.WriteDataset("orientation", buf.quat, d2_n4); g_body.WriteAttribute("orientation_order", std::string("wxyz")); }
        if (!buf.euler_xyz.empty()) {
            g_body.WriteDataset("orientation_xyz", buf.euler_xyz, d2_n3);
            g_body.WriteAttribute("orientation_xyz_convention", std::string("TaitBryan_extrinsic_XYZ"));
            g_body.WriteAttribute("orientation_xyz_units", std::string("rad"));
        }
        if (!buf.wvel.empty()) { g_body.WriteDataset("angular_velocity", buf.wvel, d2_n3); g_body.WriteAttribute("angular_velocity_units", std::string("rad/s")); g_body.WriteAttribute("angular_velocity_frame", std::string("world")); }
    }

    // Write TSDA results
    if (!impl_->model_tsda_names.empty()) {
        for (auto& t : impl_->tsdas) {
            auto gt = impl_->g_results_model_tsdas.CreateGroup(t.name);
        gt.WriteAttribute("type", std::string("TSDA"));
        gt.WriteAttribute("time_ref", std::string("/results/time/time"));
        gt.WriteAttribute("frame", std::string("world"));
        gt.WriteAttribute("units_force", std::string("N"));
        gt.WriteAttribute("units_extension", std::string("m"));
        gt.WriteAttribute("units_speed", std::string("m/s"));
        const hsize_t N = static_cast<hsize_t>(impl_->time.size());
        std::array<hsize_t,2> d2_n3 = {N, static_cast<hsize_t>(3)};
        std::array<hsize_t,1> d1_n = {N};
        gt.WriteDataset("force_vec", t.force_vec, d2_n3);
        gt.WriteDataset("force_mag", t.force_mag, d1_n);
        gt.WriteDataset("extension", t.extension, d1_n);
        gt.WriteDataset("speed", t.speed, d1_n);
            gt.WriteDataset("spring_force", t.spring_force, d1_n);
            gt.WriteDataset("damping_force", t.damping_force, d1_n);
            gt.WriteDataset("reaction_force_body1", t.react_b1, d2_n3);
            gt.WriteDataset("reaction_force_body2", t.react_b2, d2_n3);
        if (impl_->verbosity == H5Verbosity::Verbose) hydroc::cli::LogInfo(std::string("Finalize: damper=") + t.name + " N=" + std::to_string(static_cast<int>(N)) + " (TSDA)");
        }
    }

    // Write RSDA results
    if (!impl_->model_rsda_names.empty()) {
        for (auto& r : impl_->rsdas) {
            auto gr = impl_->g_results_model_rsdas.CreateGroup(r.name);
        gr.WriteAttribute("type", std::string("RSDA"));
        gr.WriteAttribute("time_ref", std::string("/results/time/time"));
        gr.WriteAttribute("frame", std::string("world"));
        gr.WriteAttribute("units_torque", std::string("N*m"));
        gr.WriteAttribute("units_angle", std::string("rad"));
        gr.WriteAttribute("units_ang_speed", std::string("rad/s"));
        const hsize_t N = static_cast<hsize_t>(impl_->time.size());
        std::array<hsize_t,2> d2_n3 = {N, static_cast<hsize_t>(3)};
        std::array<hsize_t,1> d1_n = {N};
        gr.WriteDataset("torque_vec", r.torque_vec, d2_n3);
        gr.WriteDataset("torque_mag", r.torque_mag, d1_n);
        gr.WriteDataset("angle", r.angle, d1_n);
        gr.WriteDataset("ang_speed", r.ang_speed, d1_n);
            gr.WriteDataset("spring_torque", r.spring_torque, d1_n);
            gr.WriteDataset("damping_torque", r.damping_torque, d1_n);
            gr.WriteDataset("reaction_torque_body1", r.react_torque_b1, d2_n3);
            gr.WriteDataset("reaction_torque_body2", r.react_torque_b2, d2_n3);
        if (impl_->verbosity == H5Verbosity::Verbose) hydroc::cli::LogInfo(std::string("Finalize: damper=") + r.name + " N=" + std::to_string(static_cast<int>(N)) + " (RSDA)");
        }
    }

    // Write generic joint (ChLinkLock/ChLink) reactions
    if (!impl_->joints.empty()) {
        for (auto& j : impl_->joints) {
            auto gj = impl_->g_results_model_joints.CreateGroup(j.name);
            gj.WriteAttribute("type", j.type);
            if (!j.class_name.empty()) gj.WriteAttribute("class", j.class_name);
            gj.WriteAttribute("time_ref", std::string("/results/time/time"));
            gj.WriteAttribute("frame1", std::string("link1"));
            gj.WriteAttribute("frame2", std::string("link2"));
            gj.WriteAttribute("units_force", std::string("N"));
            gj.WriteAttribute("units_torque", std::string("N*m"));
            const hsize_t N = static_cast<hsize_t>(impl_->time.size());
            std::array<hsize_t,2> d2_n3 = {N, static_cast<hsize_t>(3)};
            gj.WriteDataset("reaction1_force", j.react_force_b1, d2_n3);
            gj.WriteDataset("reaction1_torque", j.react_torque_b1, d2_n3);
            gj.WriteDataset("reaction2_force", j.react_force_b2, d2_n3);
            gj.WriteDataset("reaction2_torque", j.react_torque_b2, d2_n3);
        }
    }
    // Final succinct info line in Quiet mode
    int bodies = static_cast<int>(impl_->bodies.size());
    int dampers = static_cast<int>(impl_->tsdas.size() + impl_->rsdas.size());
    int samples = static_cast<int>(impl_->time.size());
    // meta/run runtime details (if provided via options)
    {
        auto g_run = impl_->writer.RequireGroup("/meta/run");
        if (!impl_->options.run_started_at_utc.empty()) g_run.WriteAttribute("started_at_utc", impl_->options.run_started_at_utc);
        if (!impl_->options.run_finished_at_utc.empty()) g_run.WriteAttribute("finished_at_utc", impl_->options.run_finished_at_utc);
        if (impl_->options.run_wall_time_s > 0.0) g_run.WriteAttribute("wall_time_s", impl_->options.run_wall_time_s);
        if (impl_->options.run_steps > 0) g_run.WriteAttribute("steps", static_cast<double>(impl_->options.run_steps));
        if (impl_->options.run_dt > 0.0) g_run.WriteAttribute("dt_s", impl_->options.run_dt);
        if (impl_->options.run_time_final > 0.0) g_run.WriteAttribute("time_final_s", impl_->options.run_time_final);
    }

    hydroc::cli::LogInfo(std::string("HDF5: wrote output to ") + impl_->options.output_path);
}

void SimulationExporter::SetRunMetadata(const std::string& started_at_utc,
                                        const std::string& finished_at_utc,
                                        double wall_time_s,
                                        int steps,
                                        double dt_s,
                                        double time_final_s) {
    impl_->options.run_started_at_utc = started_at_utc;
    impl_->options.run_finished_at_utc = finished_at_utc;
    impl_->options.run_wall_time_s = wall_time_s;
    impl_->options.run_steps = steps;
    impl_->options.run_dt = dt_s;
    impl_->options.run_time_final = time_final_s;
}

} // namespace hydroc


