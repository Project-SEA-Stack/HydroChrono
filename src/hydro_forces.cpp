/*********************************************************************
 * @file  hydro_forces.cpp
 *
 * @brief Implementation of TestHydro main class and helper classes
 * ComponentFunc and ForceFunc6d.
 *********************************************************************/

// TODO minimize include statements, move all to header file hydro_forces.h?
#include "hydroc/hydro_forces.h"
#include <hydroc/chloadaddedmass.h>
#include <hydroc/h5fileinfo.h>
#include <hydroc/wave_types.h>

#include <chrono/physics/ChLoad.h>
#include <unsupported/Eigen/Splines>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>  // std::accumulate
#include <random>
#include <stdexcept>
#include <vector>

const int kDofPerBody  = 6;
const int kDofLinOrRot = 3;

/**
 * @brief Generates a vector of evenly spaced numbers over a specified range.
 *
 * This function returns a vector of `num_points` numbers evenly spaced from
 * `start` to `end`. The function utilizes a single loop for this computation,
 * making it efficient for generating large vectors.
 *
 * @param start - The start value of the sequence.
 * @param end - The end value of the sequence.
 * @param num_points - The number of evenly spaced samples to generate.
 * @return std::vector<double> - Vector of evenly spaced numbers.
 * @exception None
 */
std::vector<double> Linspace(double start, double end, int num_points) {
    std::vector<double> result(num_points);
    double step = (end - start) / (num_points - 1);

    for (int i = 0; i < num_points; ++i) {
        result[i] = start + i * step;
    }

    return result;
}

// TODO reorder ComponentFunc implementation functions to match the header order of functions
ComponentFunc::ComponentFunc() {
    base_  = NULL;
    index_ = kDofPerBody;
}

ComponentFunc::ComponentFunc(ForceFunc6d* b, int i) : base_(b), index_(i) {}

ComponentFunc* ComponentFunc::Clone() const {
    return new ComponentFunc(*this);
}

ComponentFunc::ComponentFunc(const ComponentFunc& old) {
    base_  = old.base_;
    index_ = old.index_;
}

double ComponentFunc::GetVal(double x) const {
    if (base_ == NULL) {
        std::cout << "base == Null!" << std::endl;
        return 0;
    }
    return base_->CoordinateFunc(index_);
}

ForceFunc6d::ForceFunc6d() : forces_{{this, 0}, {this, 1}, {this, 2}, {this, 3}, {this, 4}, {this, 5}} {
    for (unsigned i = 0; i < 6; i++) {
        force_ptrs_[i] = std::shared_ptr<ComponentFunc>(forces_ + i, [](ComponentFunc*) {});
        // sets force_ptrs[i] to point to forces[i] but since forces is on the stack, it is faster and it is
        // automatically deallocated...shared pointers typically manage heap pointers, and will try deleting
        // them as soon as done. Doesn't work on stack array (can't delete stack arrays), we overload the
        // default deletion logic to do nothing
        // Also! don't need to worry about deleting this later, because stack arrays are always deleted automatically
    }
    chrono_force_  = chrono_types::make_shared<ChForce>();
    chrono_torque_ = chrono_types::make_shared<ChForce>();
    chrono_force_->SetAlign(ChForce::AlignmentFrame::WORLD_DIR);
    chrono_torque_->SetAlign(ChForce::AlignmentFrame::WORLD_DIR);
    chrono_force_->SetName("hydroforce");
    chrono_torque_->SetName("hydrotorque");
}

ForceFunc6d::ForceFunc6d(std::shared_ptr<ChBody> object, TestHydro* user_all_forces) : ForceFunc6d() {
    body_             = object;
    std::string temp  = body_->GetName();        // remove "body" from "bodyN", convert N to int, get body num
    b_num_            = stoi(temp.erase(0, 4));  // 1 indexed TODO: fix b_num starting here to be 0 indexed
    all_hydro_forces_ = user_all_forces;         // TODO switch to smart pointers? does this use = ?
    if (all_hydro_forces_ == NULL) {
        std::cout << "all hydro forces null " << std::endl;
    }
    SetForce();
    SetTorque();
    ApplyForceAndTorqueToBody();
}

ForceFunc6d::ForceFunc6d(const ForceFunc6d& old)
    : forces_{{this, 0}, {this, 1}, {this, 2}, {this, 3}, {this, 4}, {this, 5}} {
    for (unsigned i = 0; i < 6; i++) {
        force_ptrs_[i] = std::shared_ptr<ComponentFunc>(forces_ + i, [](ComponentFunc*) {});
        // sets force_ptrs[i] to point to forces[i] but since forces is on the stack, it is faster and it is
        // automatically deallocated...shared pointers typically manage heap pointers, and will try deleting
        // them as soon as done. Doesn't work on stack array (can't delete stack arrays), we overload the
        // default deletion logic to do nothing
        // Also! don't need to worry about deleting this later, because stack arrays are always deleted automatically
    }
    chrono_force_     = old.chrono_force_;
    chrono_torque_    = old.chrono_torque_;
    body_             = old.body_;
    b_num_            = old.b_num_;
    all_hydro_forces_ = old.all_hydro_forces_;
    SetForce();
    SetTorque();
}

double ForceFunc6d::CoordinateFunc(int i) {
    // b_num is 1 indexed?
    if (i >= kDofPerBody || i < 0) {
        std::cout << "wrong index force func 6d" << std::endl;
        return 0;
    }
    return all_hydro_forces_->CoordinateFuncForBody(
        b_num_, i);  // b_num is 1 indexed here!!!!! TODO: change all b_num to be 0 indexed everywhere
}

void ForceFunc6d::SetForce() {
    if (chrono_force_ == NULL || body_ == NULL) {
        std::cout << "set force null issue" << std::endl;
    }
    chrono_force_->SetF_x(force_ptrs_[0]);
    chrono_force_->SetF_y(force_ptrs_[1]);
    chrono_force_->SetF_z(force_ptrs_[2]);
}

void ForceFunc6d::SetTorque() {
    if (chrono_torque_ == NULL || body_ == NULL) {
        std::cout << "set torque null issue" << std::endl;
    }
    chrono_torque_->SetF_x(force_ptrs_[3]);
    chrono_torque_->SetF_y(force_ptrs_[4]);
    chrono_torque_->SetF_z(force_ptrs_[5]);
    chrono_torque_->SetMode(ChForce::ForceType::TORQUE);
}

void ForceFunc6d::ApplyForceAndTorqueToBody() {
    body_->AddForce(chrono_force_);
    body_->AddForce(chrono_torque_);
}

TestHydro::TestHydro(std::vector<std::shared_ptr<ChBody>> user_bodies,
                     std::string h5_file_name,
                     std::shared_ptr<WaveBase> waves)
    : bodies_(user_bodies),
      num_bodies_(bodies_.size()),
      file_info_(H5FileInfo(h5_file_name, num_bodies_).ReadH5Data()) {
    prev_time = -1;

    // Set up time vector
    rirf_time_vector = file_info_.GetRIRFTimeVector();
    // width array
    rirf_width_vector.resize(rirf_time_vector.size());
    for (int ii = 0; ii < rirf_width_vector.size(); ii++) {
        rirf_width_vector[ii] = 0.0;
        if (ii < rirf_time_vector.size() - 1) {
            rirf_width_vector[ii] += 0.5 * abs(rirf_time_vector[ii + 1] - rirf_time_vector[ii]);
        }
        if (ii > 0) {
            rirf_width_vector[ii] += 0.5 * abs(rirf_time_vector[ii] - rirf_time_vector[ii - 1]);
        }
    }

    // Total degrees of freedom
    int total_dofs = kDofPerBody * num_bodies_;

    // Initialize vectors
    time_history_.clear();
    velocity_history_.clear();
    for (int b = 0; b < num_bodies_; ++b) {
        velocity_history_.push_back(std::vector<std::vector<double>>(0));
    }
    force_hydrostatic_.assign(total_dofs, 0.0);
    force_radiation_damping_.assign(total_dofs, 0.0);
    total_force_.assign(total_dofs, 0.0);
    equilibrium_.assign(total_dofs, 0.0);
    cb_minus_cg_.assign(kDofLinOrRot * num_bodies_, 0.0);

    // Compute equilibrium and cb_minus_cg_
    for (int b = 0; b < num_bodies_; ++b) {
        for (int i = 0; i < kDofLinOrRot; ++i) {
            unsigned eq_idx = i + kDofPerBody * b;
            unsigned c_idx  = i + kDofLinOrRot * b;

            equilibrium_[eq_idx] = file_info_.GetCGVector(b)[i];
            cb_minus_cg_[c_idx]  = file_info_.GetCBVector(b)[i] - file_info_.GetCGVector(b)[i];
        }
    }

    for (int b = 0; b < num_bodies_; ++b) {
        force_per_body_.emplace_back(bodies_[b], this);
    }

    // Handle added mass info
    my_loadcontainer = chrono_types::make_shared<ChLoadContainer>();

    std::vector<std::shared_ptr<ChLoadable>> loadables(bodies_.size());
    for (int i = 0; i < static_cast<int>(bodies_.size()); ++i) {
        loadables[i] = bodies_[i];
    }

    my_loadbodyinertia =
        chrono_types::make_shared<ChLoadAddedMass>(file_info_.GetBodyInfos(), loadables, bodies_[0]->GetSystem());

    bodies_[0]->GetSystem()->Add(my_loadcontainer);
    my_loadcontainer->Add(my_loadbodyinertia);

    // Set up hydro inputs
    user_waves_ = waves;
    AddWaves(user_waves_);
}

void TestHydro::AddWaves(std::shared_ptr<WaveBase> waves) {
    user_waves_ = waves;

    switch (user_waves_->GetWaveMode()) {
        case WaveMode::regular: {
            auto reg = std::static_pointer_cast<RegularWave>(user_waves_);
            reg->AddH5Data(file_info_.GetRegularWaveInfos(), file_info_.GetSimulationInfo());
            break;
        }
        case WaveMode::irregular: {
            auto irreg = std::static_pointer_cast<IrregularWaves>(user_waves_);
            irreg->AddH5Data(file_info_.GetIrregularWaveInfos(), file_info_.GetSimulationInfo());
            break;
        }
    }

    user_waves_->Initialize();
}

std::vector<double> TestHydro::ComputeForceHydrostatics() {
    assert(num_bodies_ > 0);

    const double rho = file_info_.GetRhoVal();
    const auto gravitational_acceleration = bodies_[0]->GetSystem()->GetGravitationalAcceleration();  // same system for all bodies
    const double rho_times_g = rho * gravitational_acceleration.Length();

    for (int b = 0; b < num_bodies_; ++b) {
        const auto& body = bodies_[b];

        const int body_offset = kDofPerBody * b;
        double* const body_force_hydrostatic = &force_hydrostatic_[body_offset];
        const double* const body_equilibrium = &equilibrium_[body_offset];

        // Current pose
        const chrono::ChVector3d position_world = body->GetPos();
        const chrono::ChVector3d rotation_rpy   = body->GetRot().GetCardanAnglesXYZ();

        // 6-DOF displacement from equilibrium (translation xyz, rotation rpy)
        Eigen::Matrix<double, kDofPerBody, 1> displacement_from_equilibrium;
        displacement_from_equilibrium[0] = position_world.x() - body_equilibrium[0];
        displacement_from_equilibrium[1] = position_world.y() - body_equilibrium[1];
        displacement_from_equilibrium[2] = position_world.z() - body_equilibrium[2];
        displacement_from_equilibrium[3] = rotation_rpy.x()   - body_equilibrium[3];
        displacement_from_equilibrium[4] = rotation_rpy.y()   - body_equilibrium[4];
        displacement_from_equilibrium[5] = rotation_rpy.z()   - body_equilibrium[5];

        // Linear hydrostatic restoring force/torque
        const Eigen::MatrixXd restoring_stiffness_matrix = file_info_.GetLinMatrix(b);
        const Eigen::Matrix<double, kDofPerBody, 1> restoring_force_torque =
            -rho_times_g * (restoring_stiffness_matrix * displacement_from_equilibrium);
        for (int i = 0; i < kDofPerBody; ++i) {
            body_force_hydrostatic[i] += restoring_force_torque[i];
        }

        // Buoyancy force at equilibrium: F = rho * (-g) * displaced_volume
        const double displaced_volume = file_info_.GetDispVolVal(b);
        const chrono::ChVector3d buoyancy_force = rho * (-gravitational_acceleration) * displaced_volume;
        body_force_hydrostatic[0] += buoyancy_force.x();
        body_force_hydrostatic[1] += buoyancy_force.y();
        body_force_hydrostatic[2] += buoyancy_force.z();

        // Buoyancy-induced moment about CG: (r_CB - r_CG) x buoyancy
        const int rotation_offset = kDofLinOrRot * b;
        const chrono::ChVector3d cg_to_cb(
            cb_minus_cg_[rotation_offset + 0],
            cb_minus_cg_[rotation_offset + 1],
            cb_minus_cg_[rotation_offset + 2]
        );
        const chrono::ChVector3d buoyancy_torque = cg_to_cb % buoyancy_force;
        body_force_hydrostatic[3] += buoyancy_torque.x();
        body_force_hydrostatic[4] += buoyancy_torque.y();
        body_force_hydrostatic[5] += buoyancy_torque.z();
    }

    return force_hydrostatic_;
}

std::vector<double> TestHydro::ComputeForceRadiationDampingConv() {
    const int rirf_steps = file_info_.GetRIRFDims(2);
    const int total_dofs = kDofPerBody * num_bodies_;

    assert(total_dofs > 0 && rirf_steps > 0);

    // Current time and minimum history time window required
    const double simulation_time = bodies_[0]->GetChTime();
    const int rirf_last_index = static_cast<int>(rirf_time_vector.size()) - 1;
    const double history_min_time = simulation_time - (rirf_last_index >= 0 ? rirf_time_vector[rirf_last_index] : 0.0);

    // Prevent duplicate computation within same step
    if (!time_history_.empty() && simulation_time == time_history_.front()) {
        throw std::runtime_error("Tried to compute the radiation damping convolution twice within the same time step!");
    }

    // Record current time at the front (most recent first)
    time_history_.insert(time_history_.begin(), simulation_time);

    // Record current velocities per body at the front (matching time_history_ ordering)
    for (int b = 0; b < num_bodies_; ++b) {
        auto& body = bodies_[b];
        auto& velocity_history_body = velocity_history_[b];

        const auto linear_velocity_world  = body->GetPosDt();
        const auto angular_velocity_world = body->GetAngVelParent();
        std::vector<double> velocity_dof_vector = {
            linear_velocity_world[0], linear_velocity_world[1], linear_velocity_world[2],
            angular_velocity_world[0], angular_velocity_world[1], angular_velocity_world[2]
        };
        velocity_history_body.insert(velocity_history_body.begin(), std::move(velocity_dof_vector));
    }

    // Prune history older than the max RIRF time span
    if (time_history_.size() > 1) {
        while (time_history_.size() > 1 && time_history_[time_history_.size() - 2] < history_min_time) {
            time_history_.pop_back();
            for (int b = 0; b < num_bodies_; ++b) {
                auto& velocity_history_body = velocity_history_[b];
                if (!velocity_history_body.empty()) {
                    velocity_history_body.pop_back();
                }
            }
        }
    }

    // Nothing to convolve with if we don't yet have at least 2 time points
    if (time_history_.size() <= 1) {
        return force_radiation_damping_;
    }

    // Walk through RIRF steps and accumulate convolution
    size_t history_index = 0;  // index into descending time_history_ (front is most recent)
    for (int step = 0; step < rirf_steps; ++step) {
        const double rirf_query_time = simulation_time - rirf_time_vector[step];

        // Advance history_index until [history_index, history_index+1] brackets rirf_query_time, or we run out
        while ((history_index + 1) < time_history_.size() && time_history_[history_index + 1] > rirf_query_time) {
            ++history_index;
        }
        if ((history_index + 1) >= time_history_.size()) {
            break;  // not enough older history to interpolate
        }

        const double newer_time = time_history_[history_index];
        const double older_time = time_history_[history_index + 1];

        // For each body, interpolate 6-DOF velocity at rirf_query_time and apply convolution
        for (int body_index = 0; body_index < num_bodies_; ++body_index) {
            auto& velocity_history_body = velocity_history_[body_index];

            // Ensure history is consistent across bodies
            if (velocity_history_body.size() <= history_index) {
                continue;  // skip if inconsistent; should not happen in normal flow
            }

            // Interpolate velocities
            double interpolated_velocity_dof[kDofPerBody];
            if (rirf_query_time == older_time) {
                const auto& older_velocity = velocity_history_body[history_index + 1];
                interpolated_velocity_dof[0] = older_velocity[0];
                interpolated_velocity_dof[1] = older_velocity[1];
                interpolated_velocity_dof[2] = older_velocity[2];
                interpolated_velocity_dof[3] = older_velocity[3];
                interpolated_velocity_dof[4] = older_velocity[4];
                interpolated_velocity_dof[5] = older_velocity[5];
            } else if (rirf_query_time == newer_time) {
                const auto& newer_velocity = velocity_history_body[history_index];
                interpolated_velocity_dof[0] = newer_velocity[0];
                interpolated_velocity_dof[1] = newer_velocity[1];
                interpolated_velocity_dof[2] = newer_velocity[2];
                interpolated_velocity_dof[3] = newer_velocity[3];
                interpolated_velocity_dof[4] = newer_velocity[4];
                interpolated_velocity_dof[5] = newer_velocity[5];
            } else if (rirf_query_time > older_time && rirf_query_time < newer_time) {
                const double time_delta = (newer_time - older_time);
                const double weight_older = (time_delta != 0.0) ? ((newer_time - rirf_query_time) / time_delta) : 0.0;
                const double weight_newer = 1.0 - weight_older;
                const auto& older_velocity = velocity_history_body[history_index + 1];
                const auto& newer_velocity = velocity_history_body[history_index];
                interpolated_velocity_dof[0] = weight_older * older_velocity[0] + weight_newer * newer_velocity[0];
                interpolated_velocity_dof[1] = weight_older * older_velocity[1] + weight_newer * newer_velocity[1];
                interpolated_velocity_dof[2] = weight_older * older_velocity[2] + weight_newer * newer_velocity[2];
                interpolated_velocity_dof[3] = weight_older * older_velocity[3] + weight_newer * newer_velocity[3];
                interpolated_velocity_dof[4] = weight_older * older_velocity[4] + weight_newer * newer_velocity[4];
                interpolated_velocity_dof[5] = weight_older * older_velocity[5] + weight_newer * newer_velocity[5];
            } else {
                throw std::runtime_error(
                    "Radiation convolution: interpolation error; rirf_query_time not bracketed by adjacent history.");
            }

            // Apply convolution: for each DOF column, add RIRF[:, col, step] * vel[dof] * width
            const double step_width = rirf_width_vector[step];
            const int body_col_offset = body_index * kDofPerBody;
            for (int dof = 0; dof < kDofPerBody; ++dof) {
                const int col = body_col_offset + dof;
                const double contribution_scale = interpolated_velocity_dof[dof] * step_width;
                if (contribution_scale == 0.0) {
                    continue;
                }
                for (int row = 0; row < total_dofs; ++row) {
                    force_radiation_damping_[row] += GetRIRFval(row, col, step) * contribution_scale;
                }
            }
        }
    }

    return force_radiation_damping_;
}

double TestHydro::GetRIRFval(int row, int col, int st) {
    if (row < 0 || row >= kDofPerBody * num_bodies_ || col < 0 || col >= kDofPerBody * num_bodies_ || st < 0 ||
        st >= file_info_.GetRIRFDims(2)) {
        throw std::out_of_range("rirfval index out of range in TestHydro");
    }

    int body_index = row / kDofPerBody;
    int col_dof    = col % kDofPerBody;
    int row_dof    = row % kDofPerBody;

    return file_info_.GetRIRFVal(body_index, row_dof, col, st);
}

Eigen::VectorXd TestHydro::ComputeForceWaves() {
    // Ensure bodies_ is not empty
    if (bodies_.empty()) {
        throw std::runtime_error("bodies_ array is empty in ComputeForceWaves");
    }

    force_waves_ = user_waves_->GetForceAtTime(bodies_[0]->GetChTime());

    // TODO: Add size check for force_waves_ if needed
    // Example:
    // if (force_waves_.size() != expected_size) {
    //     throw std::runtime_error("Mismatched size in ComputeForceWaves");
    // }

    return force_waves_;
}

double TestHydro::CoordinateFuncForBody(int b, int dof_index) {
    if (dof_index < 0 || dof_index >= kDofPerBody || b < 1 || b > num_bodies_) {
        throw std::out_of_range("Invalid index in CoordinateFuncForBody");
    }

    // Adjusting for 1-indexed body number
    const int body_num_offset = kDofPerBody * (b - 1);
    const int total_dofs      = kDofPerBody * num_bodies_;

    // Ensure the bodies_ vector isn't empty and the first element isn't null
    if (bodies_.empty() || !bodies_[0]) {
        throw std::runtime_error("bodies_ array is empty or invalid in CoordinateFuncForBody");
    }

    // Check if the forces for this time step have already been computed
    if (bodies_[0]->GetChTime() == prev_time) {
        return total_force_[body_num_offset + dof_index];
    }

    // Update time and reset forces for this time step
    prev_time = bodies_[0]->GetChTime();
    std::fill(total_force_.begin(), total_force_.end(), 0.0);
    std::fill(force_hydrostatic_.begin(), force_hydrostatic_.end(), 0.0);
    std::fill(force_radiation_damping_.begin(), force_radiation_damping_.end(), 0.0);
    std::fill(force_waves_.begin(), force_waves_.end(), 0.0);

    force_hydrostatic_       = ComputeForceHydrostatics();
    force_radiation_damping_ = ComputeForceRadiationDampingConv();
    force_waves_             = ComputeForceWaves();

    // Accumulate total force (consider converting forces to Eigen::VectorXd in the future for direct addition)
    for (int index = 0; index < total_dofs; index++) {
        total_force_[index] = force_hydrostatic_[index] - force_radiation_damping_[index] + force_waves_[index];
    }

    if (body_num_offset + dof_index < 0 || body_num_offset >= total_dofs) {
        throw std::out_of_range("Accessing out-of-bounds index in CoordinateFuncForBody");
    }

    return total_force_[body_num_offset + dof_index];
}