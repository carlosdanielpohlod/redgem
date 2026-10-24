#pragma once

// What to do about the errors games run into most, said with them: a method mistyped on one of
// the engine's classes gets the names near it, a class no script defines gets the file it is
// looked for in, a scene the game lacks gets the scenes it has.

#include <filesystem>
#include <string>

namespace redgem::tools {

// A hint for an error, as a short line (see core/message_lines.hpp) says it; empty when there is
// nothing to add.
std::string hint_for(const std::string& message, const std::filesystem::path& game_dir);

}  // namespace redgem::tools
