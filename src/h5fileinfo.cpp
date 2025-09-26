/*********************************************************************
 * @file  h5fileinfo.cpp
 *
 * @brief implementation file of HydroData main class and helper class \
 * H5FileInfo.
 *********************************************************************/
// TODO: this include statement list looks good
#include "hydroc/h5fileinfo.h"
#include "chrono/utils/ChConstants.h"

#include <H5Cpp.h>
#include <filesystem>  // std::filesystem::absolute
#include <cassert>
#include <unsupported/Eigen/CXX11/Tensor>

using namespace chrono;  // TODO narrow this using namespace to specify what we use from chrono or put chrono:: in front
                         // of it all?

H5FileInfo::H5FileInfo(std::string file, int num_bod) {
    h5_file_name_ = file;
    num_bodies_   = num_bod;
    std::cout << "searching for file: " << file << std::endl;
    if (std::filesystem::exists(file)) {
        std::cout << "found file at: " << std::filesystem::absolute(file) << std::endl;
    } else {
        std::cout << "h5 file does not exist, absolute file location: " << std::filesystem::absolute(file) << std::endl;
    }
}

HydroData H5FileInfo::ReadH5Data() {
    // open file with read only access
    H5::H5File userH5File(h5_file_name_, H5F_ACC_RDONLY);
    HydroData data_to_init;
    data_to_init.resize(num_bodies_);

    // simparams first
    InitScalar(userH5File, "simulation_parameters/rho", data_to_init.sim_data_.rho);
    InitScalar(userH5File, "simulation_parameters/g", data_to_init.sim_data_.g);
    InitScalar(userH5File, "simulation_parameters/water_depth", data_to_init.sim_data_.water_depth);
    double rho = data_to_init.sim_data_.rho;
    double g   = data_to_init.sim_data_.g;

    // for each body things
    for (int i = 0; i < num_bodies_; i++) {
        // body data
        data_to_init.body_data_[i].body_name = "body" + std::to_string(i + 1);
        std::string bodyName                 = data_to_init.body_data_[i].body_name;  // shortcut for reading later
        data_to_init.body_data_[i].body_num  = i;

        InitScalar(userH5File, bodyName + "/properties/disp_vol", data_to_init.body_data_[i].disp_vol);
        Init1D(userH5File, bodyName + "/hydro_coeffs/radiation_damping/impulse_response_fun/t",
               data_to_init.body_data_[i].rirf_time_vector);

        // do not need rirf_timestep?
        data_to_init.body_data_[i].rirf_timestep =
            data_to_init.body_data_[i].rirf_time_vector[1] - data_to_init.body_data_[i].rirf_time_vector[0];

        Init1D(userH5File, bodyName + "/properties/cg", data_to_init.body_data_[i].cg);
        Init1D(userH5File, bodyName + "/properties/cb", data_to_init.body_data_[i].cb);
        Init2D(userH5File, bodyName + "/hydro_coeffs/linear_restoring_stiffness",
               data_to_init.body_data_[i].lin_matrix);
        Init2D(userH5File, bodyName + "/hydro_coeffs/added_mass/inf_freq", data_to_init.body_data_[i].inf_added_mass);
        data_to_init.body_data_[i].inf_added_mass *= rho;
        Init3D(userH5File, bodyName + "/hydro_coeffs/radiation_damping/impulse_response_fun/K",
               data_to_init.body_data_[i].rirf_matrix);
        // Init3D(userH5File, bodyName + "/hydro_coeffs/radiation_damping/all",
        //       data_to_init.body_data[i].radiation_damping_matrix);

        // reg wave
        Init1D(userH5File, "simulation_parameters/w", data_to_init.reg_wave_data_[i].freq_list);
        Init1D(userH5File, "simulation_parameters/wave_dir", data_to_init.reg_wave_data_[i].wave_direction_list);
        Init3D(userH5File, bodyName + "/hydro_coeffs/excitation/mag",
               data_to_init.reg_wave_data_[i].excitation_mag_matrix);

        data_to_init.reg_wave_data_[i].wave_direction_list *= CH_DEG_TO_RAD;
        // scale by rho * g
        data_to_init.reg_wave_data_[i].excitation_mag_matrix =
            data_to_init.reg_wave_data_[i].excitation_mag_matrix *
            data_to_init.reg_wave_data_[i].excitation_mag_matrix.constant(rho * g);
        Init3D(userH5File, bodyName + "/hydro_coeffs/excitation/phase",
               data_to_init.reg_wave_data_[i]
                   .excitation_phase_matrix);  // TODO does this also need to be scaled by rho * g?

        // irreg wave
        // Init3D(userH5File, bodyName + "/hydro_coeffs/excitation/re", excitation_re_matrix, re_dims);
        // Init3D(userH5File, bodyName + "/hydro_coeffs/excitation/im", excitation_im_matrix, im_dims);
        Init1D(userH5File, bodyName + "/hydro_coeffs/excitation/impulse_response_fun/t",
               data_to_init.irreg_wave_data_[i].excitation_irf_time);
        // TODO change this to a temp tensor and manip it into a 2d matrix for ecitation_irf_matrix?
        // TODO look up Eigen resize and map to make this temp conversion better
        Eigen::Tensor<double, 3> temp;
        Init3D(userH5File, bodyName + "/hydro_coeffs/excitation/impulse_response_fun/f", temp);
        auto scaled_temp = temp * temp.constant(rho * g); 
        data_to_init.irreg_wave_data_[i].excitation_irf_matrix = (temp * temp.constant(rho * g)).eval();
        data_to_init.irreg_wave_data_[i].wave_direction_list   = data_to_init.reg_wave_data_[i].wave_direction_list;
    }

    userH5File.close();
    // WriteDataToFile(excitation_irf_dims, "excitation_irf_dims.txt");
    // WriteDataToFile(excitation_irf_matrix, "excitation_irf_matrix.txt");
    return data_to_init;
}

// squeezes the middle dimension of 1 out
Eigen::MatrixXd H5FileInfo::SqueezeMid(Eigen::Tensor<double, 3>& to_be_squeezed) {
    assert(to_be_squeezed.dimension(1));
    int dof  = to_be_squeezed.dimension(0);
    int size = to_be_squeezed.dimension(2);
    Eigen::MatrixXd squozen(dof, size);
    for (int i = 0; i < dof; i++) {
        for (int j = 0; j < size; j++) {
            // squeeze 6x1x1000 or whatever into 6x1000 matrix
            squozen(i, j) = to_be_squeezed(i, 0, j);
        }
    }
    return squozen;
}

void H5FileInfo::InitScalar(H5::H5File& file, std::string data_name, double& var) {
    H5::DataSet dataset   = file.openDataSet(data_name);
    H5::DataType datatype = dataset.getDataType();

    if (H5::PredType::NATIVE_FLOAT == datatype || H5::PredType::NATIVE_DOUBLE == datatype) {
        H5::DataSpace filespace = dataset.getSpace();
        hsize_t dims[2]         = {0, 0};
        int rank                = filespace.getSimpleExtentDims(dims);
        H5::DataSpace mspace1   = H5::DataSpace(rank, dims);
        dataset.read(&var, H5::PredType::NATIVE_DOUBLE, mspace1, filespace);
    } else if (H5::PredType::C_S1 == datatype) {
        H5::DataSpace filespace = dataset.getSpace();
        hsize_t size            = dataset.getStorageSize();
        char* temp              = new char[size + 1];
        dataset.read(temp, datatype, filespace, filespace);
        temp[size] = '\0';
        std::string str(temp);
        delete[] temp;

        if (str == "infinite") {
            var = std::numeric_limits<double>::infinity();
        } else {
            // Handle unexpected string values if necessary
        }
    } else {
        // Handle unexpected data types if necessary
    }

    dataset.close();
}

void H5FileInfo::Init1D(H5::H5File& file, std::string data_name, Eigen::VectorXd& var) {
    // open specific dataset
    H5::DataSet dataset = file.openDataSet(data_name);
    // Get filespace for rank and dimension
    H5::DataSpace filespace = dataset.getSpace();
    // Get number of dimensions in the file dataspace
    // Get and print the dimension sizes of the file dataspace
    hsize_t dims[2] = {0, 0};  // dataset dimensions
    int rank        = filespace.getSimpleExtentDims(dims);
    // read file into data_out 2d array
    H5::DataSpace mspace1(rank, dims);
    double* temp = new double[dims[0] * dims[1]];
    // read file info into current_pos
    dataset.read(temp, H5::PredType::NATIVE_DOUBLE, mspace1, filespace);
    var.resize(dims[0] * dims[1]);
    for (int i = 0; i < dims[0] * dims[1]; i++) {
        var[i] = temp[i];
    }
    dataset.close();
    delete[] temp;
}

void H5FileInfo::Init2D(H5::H5File& file, std::string data_name, Eigen::MatrixXd& var) {
    // data_name = bodyName + "/hydro_coeffs/radiation_damping/impulse_response_fun/K";
    H5::DataSet dataset     = file.openDataSet(data_name);
    H5::DataSpace filespace = dataset.getSpace();
    hsize_t dims[2]         = {0, 0};
    int rank                = filespace.getSimpleExtentDims(dims);
    // read file into data_out 2d array
    H5::DataSpace mspace(rank, dims);
    // rirf_dims[0] is number of rows, rirf_dims[1] is number of columns, rirf_dims[2] is number of matrices
    double* temp = new double[dims[0] * dims[1]];
    // read file info into data_out, a 2d array
    dataset.read(temp, H5::PredType::NATIVE_DOUBLE, mspace, filespace);
    // set var here
    var.resize(dims[0], dims[1]);
    for (int i = 0; i < dims[0]; i++) {
        for (int j = 0; j < dims[1]; j++) {
            var(i, j) = temp[i * dims[1] + j];
        }
    }
    dataset.close();
    delete[] temp;
}

void H5FileInfo::Init3D(H5::H5File& file, std::string data_name, Eigen::Tensor<double, 3>& var) {
    // open specific dataset
    H5::DataSet dataset = file.openDataSet(data_name);
    // Get filespace for rank and dimension
    H5::DataSpace filespace = dataset.getSpace();
    hsize_t dims[3]         = {0, 0, 0};
    int rank                = filespace.getSimpleExtentDims(dims);
    // read file into data_out 2d array
    H5::DataSpace mspace(rank, dims);
    // rirf_dims[0] is number of rows, rirf_dims[1] is number of columns, rirf_dims[2] is number of matrices
    double* temp = new double[dims[0] * dims[1] * dims[2]];
    // read file info into data_out, a 2d array
    dataset.read(temp, H5::PredType::NATIVE_DOUBLE, mspace, filespace);
    // set var here
    var.resize((int64_t)dims[0], (int64_t)dims[1], (int64_t)dims[2]);
    for (int i = 0; i < dims[0]; i++) {
        for (int j = 0; j < dims[1]; j++) {
            for (int k = 0; k < dims[2]; k++) {
                int index    = k + dims[2] * (j + i * dims[1]);
                var(i, j, k) = temp[index];
            }
        }
    }
    dataset.close();
    delete[] temp;
}

H5FileInfo::~H5FileInfo() {}

// TODO check order of function definitions here matches order in .h file
void HydroData::resize(int num_bodies) {
    body_data_.resize(num_bodies);
    reg_wave_data_.resize(num_bodies);
    irreg_wave_data_.resize(num_bodies);
}

Eigen::MatrixXd HydroData::GetInfAddedMassMatrix(int b) const {
    return body_data_[b].inf_added_mass;
}

double HydroData::GetHydrostaticStiffnessVal(int b, int i, int j) const {
    return body_data_[b].lin_matrix(i, j) * sim_data_.rho * sim_data_.g;
}

Eigen::MatrixXd HydroData::GetLinMatrix(int b) const {
    return body_data_[b].lin_matrix;
}

double HydroData::GetRIRFVal(int b, int dof, int col, int s) const {
    return body_data_[b].rirf_matrix(dof, col, s) * sim_data_.rho;  // scale radiation force by rho
}

int HydroData::GetRIRFDims(int i) const {
    return body_data_[0].rirf_matrix.dimension(i);
}

Eigen::VectorXd HydroData::GetRIRFTimeVector() const {
    double tol = 1e-10;
    // check if all time vectors are the same within tolerance
    auto& rirf_time_vector = body_data_[0].rirf_time_vector;
    for (size_t ii = 1; ii < body_data_.size(); ii++) {
        for (size_t jj = 0; jj < body_data_[ii].rirf_time_vector.size(); jj++) {
            if (abs(body_data_[ii].rirf_time_vector[jj] - rirf_time_vector[jj]) > tol) {
                throw std::runtime_error(
                    "RIRF time vectors have to be exactly the same for all bodies. Difference found in body " +
                    std::to_string(jj) + " at time index " + std::to_string(jj) + ".");
            }
        }
    }
    return rirf_time_vector;
}