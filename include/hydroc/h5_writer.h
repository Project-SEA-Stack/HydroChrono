#ifndef HYDROC_H5_WRITER_H
#define HYDROC_H5_WRITER_H

/**
 * @file h5_writer.h
 * @brief Lightweight RAII writer over the HDF5 C++ API (H5Cpp.h).
 *
 * Provides a compact, easy-to-use interface to write groups, attributes, and
 * datasets required by HydroChrono exports.
 *
 * @note File overwrite: overwrite=true (default) truncates existing files; overwrite=false fails if the file exists.
 * @note Paths: POSIX-style with '/' separators; root is "/". Missing intermediate groups are created by RequireGroup().
 * @note Types: strings are written as variable-length UTF-8; numeric datasets use native double.
 * @note Errors: global HDF5 error printing is disabled; failures surface as HDF5 exceptions or std::runtime_error on open.
 * @note Thread-safety: not thread-safe; synchronize externally if shared across threads.
 */

#include <H5Cpp.h>
#include <string>
#include <vector>
#include <array>

namespace hydroc {

//-----------------------------------------------------------------------------
// H5Writer
//-----------------------------------------------------------------------------
class H5Writer {
  public:
    /**
     * @brief Open or create an HDF5 file for writing.
     * @param filepath Destination path for the HDF5 file.
     * @param overwrite If true, truncate existing file; if false, fail if file exists.
     * @throws std::runtime_error on failure to create/open the file.
     */
    explicit H5Writer(const std::string& filepath, bool overwrite = true);
    ~H5Writer() noexcept = default;
    // Note: copy and move semantics follow HDF5 C++ handle behavior.
    // We keep default copyability to avoid breaking existing usages.
    /** @return Original file path passed to the constructor. */
    [[nodiscard]] const std::string& GetPath() const noexcept { return file_path_; }

    //-------------------------------------------------------------------------
    // Group
    //-------------------------------------------------------------------------
    class Group {
      public:
        // Note: copy and move semantics follow HDF5 C++ handle behavior.
        // We keep default copyability to avoid breaking existing usages.
        /**
         * @brief Lightweight handle to an HDF5 group.
         *
         * Default-constructed instances are invalid/empty and become valid when
         * assigned from Root(), RequireGroup(), or CreateGroup().
         */
        explicit Group(H5::Group g);
        Group() = default;

        /** @return True if this handle refers to a valid underlying HDF5 group. */
        [[nodiscard]] bool Valid() const noexcept;

         /**
          * @brief Create a direct child group of this group. Requires: Valid().
          * @param name Child group name (single path segment).
          * @throws std::logic_error if this Group handle is not valid.
          * @throws std::invalid_argument if name contains '/'; use RequireGroup for multi-segment paths.
          * @throws H5::Exception if HDF5 group creation fails.
          */
        [[nodiscard]] Group CreateGroup(const std::string& name) const;
         /**
          * @brief Write a string attribute on this group. Requires: Valid().
          * @param name Attribute name
          * @param value UTF-8 string value
          * @throws std::logic_error if this Group handle is not valid.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteAttribute(const std::string& name, const std::string& value) const;
         /**
          * @brief Write a scalar double attribute on this group (SI units recommended). Requires: Valid().
          * @param name Attribute name
          * @param value Value to write (native double)
          * @throws std::logic_error if this Group handle is not valid.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteAttribute(const std::string& name, double value) const;

         /**
          * @brief Write a scalar string dataset under this group. Requires: Valid().
          * @param name Dataset name
          * @param value UTF-8 string value
          * @throws std::logic_error if this Group handle is not valid.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteDataset(const std::string& name, const std::string& value) const;
         /**
          * @brief Write a 1D dataset of doubles under this group. Requires: Valid().
          * @pre dims[0] == data.size(). The dataset layout is row-major.
          * @param name Dataset name
          * @param data Contiguous row-major values
          * @param dims dims = {data.size()}
          * @throws std::logic_error if this Group handle is not valid.
          * @throws std::invalid_argument if dims and data.size() mismatch.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteDataset(const std::string& name, const std::vector<double>& data,
                          const std::array<hsize_t, 1>& dims) const;
         /**
          * @brief Write a 2D dataset of doubles under this group (row-major). Requires: Valid().
          * @pre dims[0] * dims[1] == data.size().
          * @param name Dataset name
          * @param data Contiguous row-major values
          * @param dims {rows, cols}; product equals data.size()
          * @throws std::logic_error if this Group handle is not valid.
          * @throws std::invalid_argument if dims product and data.size() mismatch.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteDataset(const std::string& name, const std::vector<double>& data,
                          const std::array<hsize_t, 2>& dims) const;
         /**
          * @brief Write an array of strings as a 1D variable-length string dataset. Requires: Valid().
          * @param name Dataset name
          * @param values Vector of UTF-8 strings
          * @throws std::logic_error if this Group handle is not valid.
          * @throws H5::Exception on HDF5 failures
          */
        void WriteStringArray(const std::string& name, const std::vector<std::string>& values) const;

      private:
        H5::Group group_;
        friend class H5Writer;
    }; // class Group

    /** @brief Return the root group ('/'). */
    [[nodiscard]] Group Root() const;
    /**
     * @brief Ensure a group exists at the given absolute path and return it.
     * @param path Absolute POSIX-style group path (e.g., "/results/bodies").
     * Missing intermediate groups are created as needed. Skips empty segments.
     * @throws std::invalid_argument if path is empty or does not start with '/'.
     * @throws H5::Exception on HDF5 failures while creating/opening a group.
     */
    [[nodiscard]] Group RequireGroup(const std::string& path) const;

  private:
    H5::H5File file_;
    std::string file_path_;
};

} // namespace hydroc

#endif // HYDROC_H5_WRITER_H