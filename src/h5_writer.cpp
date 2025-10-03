/**
 * @file h5_writer.cpp
 * @brief Implementation of H5Writer and Group.
 *
 * @note Thread-safety: not thread-safe; synchronize externally if shared across threads.
 * @note Strings are written as UTF-8 via HDF5 string type charset setting.
 * @note Datasets are row-major; size must equal product(dims). Mismatches throw std::invalid_argument.
 * @note RequireGroup expects absolute POSIX-style paths (rooted at '/'); empty segments are skipped.
 * @note Dataset creation uses createDataSet and fails if a dataset already exists; choose unique names or manage overwrites.
 * @note Future: chunking/compression and additional data types can be added if needed.
 */

#include <hydroc/h5_writer.h>

#include <stdexcept>
#include <sstream>

 

namespace hydroc {

static H5::H5File OpenFile(const std::string& path, bool overwrite) {
    try {
        if (overwrite) {
            return H5::H5File(path, H5F_ACC_TRUNC);
        }
        return H5::H5File(path, H5F_ACC_EXCL);
    } catch (const H5::FileIException& e) {
        std::ostringstream oss;
        oss << "Failed to open HDF5 file '" << path << "' with mode "
            << (overwrite ? "truncate" : "create-exclusive") << ": ";
        try {
            oss << e.getDetailMsg();
        } catch (...) {
            oss << "(no additional HDF5 details)";
        }
        throw std::runtime_error(oss.str());
    }
}

H5Writer::H5Writer(const std::string& filepath, bool overwrite)
    : file_(OpenFile(filepath, overwrite)), file_path_(filepath) {
    // Suppress global HDF5 error printing; handle failures explicitly via exceptions
    H5::Exception::dontPrint();
}


H5Writer::Group::Group(H5::Group g) : group_(std::move(g)) {}

bool H5Writer::Group::Valid() const noexcept { return group_.getId() >= 0; }

H5Writer::Group H5Writer::Group::CreateGroup(const std::string& name) const {
    if (!Valid()) {
        throw std::logic_error("CreateGroup: group handle is not valid");
    }
    if (name.find('/') != std::string::npos) {
        throw std::invalid_argument("CreateGroup: name must be a single path segment; use RequireGroup for multi-segment paths");
    }
    H5::Group created_group = group_.createGroup(name);
    return Group(created_group);
}

void H5Writer::Group::WriteAttribute(const std::string& name, const std::string& value) const {
    if (!Valid()) {
        throw std::logic_error("WriteAttribute(string): group handle is not valid");
    }
    H5::StrType str_datatype(H5::PredType::C_S1, H5T_VARIABLE);
    str_datatype.setCset(H5T_CSET_UTF8);
    H5::DataSpace data_space(H5S_SCALAR);
    H5::Attribute attribute = group_.createAttribute(name, str_datatype, data_space);
    const char* c_str = value.c_str();
    attribute.write(str_datatype, &c_str);
}

void H5Writer::Group::WriteAttribute(const std::string& name, double value) const {
    if (!Valid()) {
        throw std::logic_error("WriteAttribute(double): group handle is not valid");
    }
    H5::DataSpace data_space(H5S_SCALAR);
    H5::Attribute attribute = group_.createAttribute(name, H5::PredType::NATIVE_DOUBLE, data_space);
    attribute.write(H5::PredType::NATIVE_DOUBLE, &value);
}

void H5Writer::Group::WriteDataset(const std::string& name, const std::string& value) const {
    if (!Valid()) {
        throw std::logic_error("WriteDataset(string): group handle is not valid");
    }
    H5::StrType str_datatype(H5::PredType::C_S1, H5T_VARIABLE);
    str_datatype.setCset(H5T_CSET_UTF8);
    H5::DataSpace data_space(H5S_SCALAR);
    H5::DataSet data_set = group_.createDataSet(name, str_datatype, data_space);
    const char* c_str = value.c_str();
    data_set.write(&c_str, str_datatype);
}

void H5Writer::Group::WriteDataset(const std::string& name, const std::vector<double>& data,
                                   const std::array<hsize_t, 1>& dims) const {
    if (!Valid()) {
        throw std::logic_error("WriteDataset(1D): group handle is not valid");
    }
    if (static_cast<hsize_t>(data.size()) != dims[0]) {
        std::ostringstream oss;
        oss << "WriteDataset(1D, '" << name << "'): size mismatch. data.size()="
            << data.size() << " dims[0]=" << dims[0];
        throw std::invalid_argument(oss.str());
    }
    H5::DataSpace data_space(1, dims.data());
    H5::DataSet data_set = group_.createDataSet(name, H5::PredType::NATIVE_DOUBLE, data_space);
    data_set.write(data.data(), H5::PredType::NATIVE_DOUBLE);
}

void H5Writer::Group::WriteDataset(const std::string& name, const std::vector<double>& data,
                                   const std::array<hsize_t, 2>& dims) const {
    if (!Valid()) {
        throw std::logic_error("WriteDataset(2D): group handle is not valid");
    }
    if (static_cast<hsize_t>(data.size()) != dims[0] * dims[1]) {
        std::ostringstream oss;
        oss << "WriteDataset(2D, '" << name << "'): size mismatch. data.size()="
            << data.size() << " dims product=" << (dims[0] * dims[1]);
        throw std::invalid_argument(oss.str());
    }
    H5::DataSpace data_space(2, dims.data());
    H5::DataSet data_set = group_.createDataSet(name, H5::PredType::NATIVE_DOUBLE, data_space);
    data_set.write(data.data(), H5::PredType::NATIVE_DOUBLE);
}

void H5Writer::Group::WriteStringArray(const std::string& name, const std::vector<std::string>& values) const {
    if (!Valid()) {
        throw std::logic_error("WriteStringArray: group handle is not valid");
    }
    std::vector<const char*> c_strs(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        c_strs[i] = values[i].c_str();
    }
    H5::StrType str_datatype(H5::PredType::C_S1, H5T_VARIABLE);
    str_datatype.setCset(H5T_CSET_UTF8);
    hsize_t dims_1d[1] = { static_cast<hsize_t>(values.size()) };
    H5::DataSpace data_space(1, dims_1d);
    H5::DataSet data_set = group_.createDataSet(name, str_datatype, data_space);
    data_set.write(c_strs.data(), str_datatype);
}

H5Writer::Group H5Writer::Root() const { return Group(file_.openGroup("/")); }

static std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

H5Writer::Group H5Writer::RequireGroup(const std::string& path) const {
    if (path.empty() || path.front() != '/') {
        throw std::invalid_argument("RequireGroup: path must be absolute and start with '/': '" + path + "'");
    }
    H5::Group current_group = file_.openGroup("/");
    // Empty segments are skipped. One open/create per path segment.
    for (const auto& part : Split(path, '/')) {
        try {
            current_group = current_group.openGroup(part);
        } catch (const H5::Exception&) {
            current_group = current_group.createGroup(part);
        }
    }
    return Group(current_group);
}

} // namespace hydroc