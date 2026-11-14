#pragma once

// The scripting API as Ruby type signatures (RBS), for the language servers of editors and the
// type checkers that read them (Steep, Solargraph and the like): completion, hover help and type
// checks for a game's scripts. Made from the API reference (api_reference.hpp), so they say what
// it says: docs/scripting/redgem.rbs is this text (`redgem api --rbs`), and new games get it as
// sig/redgem.rbs.

#include <filesystem>
#include <string>

#include "project/api_reference.hpp"

namespace redgem::tools {

// Where a game keeps them, in its own folder (the folder RBS tools look in), beside any of its
// own. For whoever makes the game: a build of it leaves the whole folder behind (see
// game_build.hpp).
inline constexpr const char* kSignaturesFolder = "sig";
inline constexpr const char* kSignaturesFile = "sig/redgem.rbs";

// The signatures of every class and method of the reference, each with its line as a comment.
std::string api_rbs(const ApiReference& reference);

// Writes the built-in reference's into the game's folder (sig/redgem.rbs). False, with `error`,
// when it cannot.
bool write_type_signatures(const std::filesystem::path& game_dir, std::string& error);

}  // namespace redgem::tools
