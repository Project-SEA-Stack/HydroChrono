// run_hydrochrono.cpp
// Entry point for HydroChrono YAML-based CLI application

#include <hydroc/hydrochrono_runner/run_hydrochrono_from_yaml.h>
#include <iostream>

int main(int argc, char* argv[]) {
    return hydroc::RunHydroChronoFromYAML(argc, argv);
}
