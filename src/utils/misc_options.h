/**
 * @file misc_options.h
 * @brief Hidden miscellaneous CLI options handler (e.g., special outputs)
 */
#pragma once

namespace hydroc::misc {
// Checks argv for hidden flags and prints special outputs if applicable.
// Returns true if a hidden option was triggered (and app should exit).
bool HandleHiddenOptions(int argc, char* argv[]);
}



