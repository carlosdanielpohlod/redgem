#include "server/save_migration.hpp"

#include <map>
#include <string>

#include "core/log.hpp"
#include "storage/save_file.hpp"

namespace redgem::server {

namespace {

// Every value of a save file, read as a player's machine reads its own (what is damaged is left out).
std::map<std::string, nlohmann::json> values_of(const std::filesystem::path& file) {
  SaveFile old(file);
  std::map<std::string, nlohmann::json> values;
  for (const std::string& key : old.keys()) values[key] = old.get(key).value_or(nlohmann::json());
  return values;
}

}  // namespace

bool move_into(SaveDatabase& database, const std::filesystem::path& old_save) {
  std::error_code fs_error;
  if (!std::filesystem::exists(old_save, fs_error)) return true;
  const auto values = values_of(old_save);
  if (!database.set_all(values)) return false;
  std::filesystem::rename(old_save, old_save.string() + kMigratedSuffix, fs_error);
  log::info("moved " + std::to_string(values.size()) + " saved value(s) from " + old_save.string() + " into " +
            database.where());
  return !fs_error;
}

}  // namespace redgem::server
