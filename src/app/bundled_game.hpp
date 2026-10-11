#pragma once

#include <filesystem>
#include <optional>

namespace redgem {

// The folder a built game's files are in, next to its executable.
inline constexpr const char* kBundledGameFolder = "game";
// The game a built executable ships with: the game folder next to it, when there is one.
std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable);

}  // namespace redgem
