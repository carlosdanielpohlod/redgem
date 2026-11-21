#pragma once

// The game's own JSON files as MCP's project tool reads and writes them: game.json, locales/,
// dialogues/, quests/ and items/. A write is checked as `redgem check` checks the game, and put
// back as it was when it brings a problem the game did not have.

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "project/mcp_protocol.hpp"

namespace redgem::tools::project_files {

using Lines = std::vector<std::string>;
// What a check of the game finds: errors refuse a write, warnings are told.
struct Findings {
  Lines errors;
  Lines warnings;
};
using Check = std::function<Findings()>;

// The object a file holds: {} when there is no such file. Throws std::invalid_argument when it is
// not a JSON object.
mcp::Json read_object(const std::filesystem::path& file);

// Writes `value` as the file (two spaces of indentation, keys sorted), runs `check`, and puts the
// file back as it was (or takes it away, when it was not there) when it finds an error it did not
// find before (thrown as std::invalid_argument, a line each) or throws. Returns the warnings it did
// not find before.
Lines write_checked(const std::filesystem::path& file, const mcp::Json& value, const Check& check);

// Whether a name can be a file of those folders: small letters, digits, '_' and '-', in folders
// separated by '/' ("npcs/baker").
bool valid_file_name(const std::string& name);
// Whether a text names a language as locales/ does: "en", "pt-BR".
bool valid_language(const std::string& language);

}  // namespace redgem::tools::project_files
