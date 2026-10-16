// Reading a game's translation files (see game_locales.hpp).

#include "script/game_locales.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "script/script_config.hpp"

namespace redgem {
namespace {

namespace fs = std::filesystem;

constexpr const char* kLocaleExtension = ".json";
constexpr std::string_view kLanguageMarks = "-_";  // what a language's name holds besides letters and digits

bool is_language_letter(char letter) {
  const bool alphanumeric = std::isalnum(static_cast<unsigned char>(letter)) != 0;
  return alphanumeric || kLanguageMarks.find(letter) != std::string_view::npos;
}

bool has_language_length(std::string_view name) {
  return !name.empty() && name.size() <= config::kMaxLanguageNameLength;
}

// Whether a file of the locales folder is named for a language: <language>.json.
bool is_locale_file_name(const fs::path& file) {
  return file.extension() == kLocaleExtension && is_language_name(file.stem().string());
}

// Whether a game has a locales folder of its own (not a link to one somewhere else, which
// through_link warns of).
bool has_locales_folder(const fs::path& game_dir) {
  std::error_code fs_error;
  if (!fs::is_directory(game_dir / config::kLocalesDirName, fs_error)) return false;
  return !through_link(game_dir, config::kLocalesDirName);
}

// Why a file of the locales folder is left out before it is read; nothing when it may be read.
std::optional<std::string> refusal(const fs::directory_entry& entry) {
  std::error_code fs_error;
  if (entry.is_symlink(fs_error)) return "it is a symbolic link, which built games do not carry";
  if (!entry.is_regular_file(fs_error)) return "it is not a file";
  if (!is_locale_file_name(entry.path())) return "a translation file is named for its language, such as en.json";
  return std::nullopt;
}

// Adds a file of the locales folder to `texts`, in the language it is named for. Why it was left
// out; nothing when it was added.
std::optional<std::string> add_file(const fs::directory_entry& entry, Texts& texts) {
  if (auto refused = refusal(entry)) return refused;
  const auto bytes = read_file_up_to(entry.path(), config::kMaxLocaleFileBytes);
  if (!bytes) return "it cannot be read, or is larger than " + std::to_string(config::kMaxLocaleFileBytes) + " bytes";
  std::string error;
  const std::string_view json(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  if (!texts.add(entry.path().stem().string(), json, error)) return error;
  return std::nullopt;
}

}  // namespace

bool is_language_name(std::string_view name) {
  return has_language_length(name) && std::all_of(name.begin(), name.end(), is_language_letter);
}

GameLocales read_game_locales(const fs::path& game_dir) {
  GameLocales locales;
  if (!has_locales_folder(game_dir)) return locales;
  std::error_code fs_error;
  for (const fs::directory_entry& entry : fs::directory_iterator(game_dir / config::kLocalesDirName, fs_error)) {
    if (const auto left_out = add_file(entry, locales.texts)) {
      log::warning("left out " + printable(entry.path().string()) + ": " + printable(*left_out));
    } else {
      locales.languages.push_back(entry.path().stem().string());
    }
  }
  std::sort(locales.languages.begin(), locales.languages.end());
  return locales;
}

}  // namespace redgem
