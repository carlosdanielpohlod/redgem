#pragma once

#include <filesystem>

#include "server/save_database.hpp"

namespace redgem::server {

// What a server's old save file is kept as once its values were moved into the database, beside
// where it was ("save.json" -> "save.json.migrated").
inline constexpr const char* kMigratedSuffix = ".migrated";

// A server's save from before it kept one in a database (a whole JSON file, as a player's machine
// keeps its own) is moved into `database` the first time the server starts: read once, written in
// one transaction, and the file renamed out of the way. Nothing to move is no failure. False when
// the values could not be written (the file stays, for the next start to try again) or the file
// could not be renamed.
bool move_into(SaveDatabase& database, const std::filesystem::path& old_save);

}  // namespace redgem::server
