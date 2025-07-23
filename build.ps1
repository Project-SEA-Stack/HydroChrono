# Remove old build folder
Remove-Item -Recurse -Force .\build -ErrorAction Ignore

# Recreate and enter build directory
New-Item -ItemType Directory -Path build | Out-Null
Set-Location -Path build

# Run cmake configure (all on one line for simplicity)
cmake .. -DChrono_DIR="C:/code/SEA-Stack/chrono/build/cmake" `
         -DHDF5_DIR="C:/libs/CMake-hdf5-1.10.8/build/HDF5-1.10.8-win64/HDF5-1.10.8-win64/share/cmake" `
         -DPython3_ROOT_DIR="C:/Users/david/.conda/envs/sphinx_docs" `
         -DEIGEN3_INCLUDE_DIR="C:/libs/eigen-3.4.0" `
         -DIrrlicht_ROOT="C:/libs/irrlicht-1.8.4"

# Run cmake build
cmake --build . --config Release