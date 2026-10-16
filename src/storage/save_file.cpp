#include "storage/save_store.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>

#include "core/log.hpp"

namespace redgem {
namespace {

constexpr const char* kPartialSuffix = ".part";  // of a save still being written
constexpr int kIndent = 2;

// Arrays in hashes in arrays...: deeper than anything Save is given. A file nested deeper is not a save.
constexpr int kMaxDepth = 32;
constexpr double kLargestWhole = 9007199254740992.0;  // 2^53: whole numbers a double holds exactly

bool is_whole(double number) { return std::fabs(number) <= kLargestWhole && std::floor(number) == number; }

bool is_whole_float(const nlohmann::json& value) { return value.is_number_float() && is_whole(value.get<double>()); }

std::optional<nlohmann::json> kept(const nlohmann::json& value, int depth);

// An array or a hash, each of its members as it is kept.
std::optional<nlohmann::json> kept_members(const nlohmann::json& value, int depth) {
  if (depth >= kMaxDepth) return std::nullopt;
  nlohmann::json members = value;
  for (nlohmann::json& member : members) {
    const auto inner = kept(member, depth + 1);
    if (!inner) return std::nullopt;
    member = *inner;
  }
  return members;
}

// A value as it is kept: whole numbers as integers; nullopt for one nested too deep.
std::optional<nlohmann::json> kept(const nlohmann::json& value, int depth) {
  if (value.is_structured()) return kept_members(value, depth);
  if (is_whole_float(value)) return nlohmann::json(value.get<std::int64_t>());
  return value;
}

// What a key of a save file holds, as it is kept; nothing for null (which stands for nothing) or
// for what nests too deep.
std::optional<nlohmann::json> saved_entry(const nlohmann::json& value) {
  if (value.is_null()) return std::nullopt;
  return kept(value, 0);
}

nlohmann::json as_json(const std::map<std::string, nlohmann::json>& values) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : values) json[key] = value;
  return json;
}

// False when not all of the text reached the disk.
bool write_whole(const std::filesystem::path& file, const std::string& text) {
  std::ofstream out(file);
  if (!out) return false;
  out << text;
  out.close();  // a full disk shows here: the file being replaced must not be lost to half a one
  return static_cast<bool>(out);
}

}  // namespace

void SaveStore::load() {
  if (loaded_ || in_memory()) return;
  loaded_ = true;
  std::ifstream in(file_);
  if (!in) return;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object()) {
    log::warning("the save file " + file_.string() + " is damaged: starting from nothing");
    return;
  }
  for (const auto& [key, value] : json.items()) {
    if (const auto keeping = saved_entry(value)) values_[key] = *keeping;
  }
}

bool SaveStore::write() const {
  if (in_memory()) return true;
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  // Written beside the file and moved over it: a crash mid-write never leaves half a save.
  const std::filesystem::path partial = file_.string() + kPartialSuffix;
  // (Text that is not valid UTF-8 is saved with the broken parts replaced, instead of not at all.)
  const std::string text = as_json(values_).dump(kIndent, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
  if (!write_whole(partial, text)) return false;
  std::filesystem::rename(partial, file_, fs_error);
  return !fs_error;
}

std::optional<nlohmann::json> SaveStore::get(const std::string& key) {
  load();
  const auto found = values_.find(key);
  if (found == values_.end()) return std::nullopt;
  return found->second;
}

bool SaveStore::set(const std::string& key, const nlohmann::json& value) {
  if (value.is_null()) return remove(key);
  load();
  values_[key] = kept(value, 0).value_or(nlohmann::json());
  return write();
}

bool SaveStore::remove(const std::string& key) {
  load();
  if (values_.erase(key) == 0) return true;
  return write();
}

bool SaveStore::clear() {
  load();
  values_.clear();
  return write();
}

std::vector<std::string> SaveStore::keys() {
  load();
  std::vector<std::string> names;
  names.reserve(values_.size());
  for (const auto& [key, value] : values_) names.push_back(key);
  return names;
}

}  // namespace redgem
