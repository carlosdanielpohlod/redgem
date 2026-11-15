// A game's folders of JSON files (see game_json_files.hpp).

#include "script/game_json_files.hpp"

#include <algorithm>
#include <system_error>

#include "core/game_files.hpp"
#include "core/safe_path.hpp"

namespace redgem::game_json {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

// (Folders nest this deep at most: a link that loops would never end.)
constexpr int kMaxFolderDepth = 8;

bool is_json_file(const fs::path& path) { return path.extension() == kFileExtension && game_files::is_file(path); }

void add_files_in(const fs::path& root, const fs::path& folder, std::vector<std::string>& names, int depth);

void add_entry(const fs::path& root, const fs::path& entry, std::vector<std::string>& names, int depth) {
  std::error_code error;
  if (fs::is_symlink(entry, error)) return;
  if (is_json_file(entry)) {
    names.push_back(entry.lexically_relative(root).replace_extension().generic_u8string());
  } else if (game_files::is_folder(entry)) {
    add_files_in(root, entry, names, depth + 1);
  }
}

void add_files_in(const fs::path& root, const fs::path& folder, std::vector<std::string>& names, int depth) {
  if (depth > kMaxFolderDepth) return;
  for (const fs::path& entry : game_files::entries_in(folder)) add_entry(root, entry, names, depth);
}

}  // namespace

Json parsed(std::string_view text) {
  bool too_deep = false;
  const auto watch_depth = [&too_deep](int depth, Json::parse_event_t, Json&) {
    too_deep = too_deep || depth >= kMaxDepth;  // (the outermost value is at depth 0)
    return !too_deep;
  };
  Json json = Json::parse(text, watch_depth, false);
  return too_deep ? Json(Json::value_t::discarded) : json;
}

std::vector<std::string> names_in(const fs::path& game_dir, const char* folder_name) {
  std::vector<std::string> names;
  const fs::path folder = game_dir / folder_name;
  if (!game_files::is_folder(folder) || through_link(game_dir, folder_name)) return names;
  add_files_in(folder, folder, names, 0);
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace redgem::game_json
