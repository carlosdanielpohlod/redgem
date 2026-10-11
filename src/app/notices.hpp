#pragma once

#include <filesystem>
#include <string>

namespace redgem {

// The folder, inside a built game, that holds the licenses below.
inline constexpr const char* kNoticesFolder = "licenses";

// A built game contains the engine, and the engine contains other people's work under licenses
// that ask for their notice to travel with it. Writes the engine's own license and the
// third-party ones (the texts are inside this program) into `folder`/licenses. False, with
// `error` set, when they cannot be written.
bool write_notices(const std::filesystem::path& folder, std::string& error);

}  // namespace redgem
