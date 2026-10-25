#pragma once

// What a new game tells whoever works on it, coding assistants included: AGENTS.md (the name
// such tools look for), with the game's layout, the commands that read, change, play and check
// it, its conventions and the calls used most; and CLAUDE.md, which points at it.

#include <filesystem>
#include <string>

namespace redgem::tools {

// The files of the guide, in a game's own folder. They are for whoever makes the game, and a
// build of it leaves them behind (see game_build.hpp).
inline constexpr const char* kGuideFileName = "AGENTS.md";
inline constexpr const char* kPointerFileName = "CLAUDE.md";  // what one assistant reads instead

// AGENTS.md as a new game gets it: the guide built into the program, with the calls used most
// listed from the API reference (see api_reference.hpp).
std::string assistant_guide();

// Writes AGENTS.md and CLAUDE.md into the game's folder. False, with `error`, when it cannot.
bool write_assistant_guide(const std::filesystem::path& game_dir, std::string& error);

}  // namespace redgem::tools
