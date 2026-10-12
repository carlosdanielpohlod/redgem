#pragma once

namespace redgem {

// What the program tells whoever started it.
inline constexpr int kExitSuccess = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;  // the command line was not understood

}  // namespace redgem
