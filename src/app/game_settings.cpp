#include "app/game_settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string_view>

#include <nlohmann/json.hpp>

#include "app/app_config.hpp"
#include "core/files.hpp"
#include "core/safe_path.hpp"

namespace redgem {
namespace {

// What game.json says.
constexpr const char* kTitleKey = "title";
constexpr const char* kWindowWidthKey = "window_width";
constexpr const char* kWindowHeightKey = "window_height";
constexpr int kJsonIndent = 2;
constexpr const char* kAndroidSection = "android";
// What the section says.
constexpr const char* kPackageKey = "package";
constexpr const char* kVersionCodeKey = "version_code";
constexpr const char* kVersionNameKey = "version_name";
constexpr const char* kKeystoreKey = "keystore";
constexpr const char* kKeyAliasKey = "key_alias";
constexpr char kPackageSeparator = '.';
constexpr char kPackageJoiner = '_';
constexpr std::size_t kMaxPackageLength = 150;

// What a JSON file holds; a discarded value (no object, no array) when it is missing or broken.
nlohmann::json read_json(const std::filesystem::path& file) {
  std::ifstream in(file);
  if (!in) return nlohmann::json(nlohmann::json::value_t::discarded);
  return nlohmann::json::parse(in, nullptr, false);
}

// A side of the game's window as game.json gives it, kept to what a window can be; `fallback`
// when the file gives none, or something that is not a whole number.
int window_size(const nlohmann::json& json, const char* key, int fallback) {
  if (!json.contains(key) || !json[key].is_number_integer()) return fallback;
  const auto asked = json[key].get<std::int64_t>();
  return static_cast<int>(std::clamp<std::int64_t>(asked, config::kMinWindowSide, config::kMaxWindowSide));
}

bool small_letter_or_digit(unsigned char letter) { return std::islower(letter) != 0 || std::isdigit(letter) != 0; }

// A part of an app's id: a small letter followed by small letters, digits or underscores.
bool valid_package_part(std::string_view part) {
  if (part.empty() || std::islower(static_cast<unsigned char>(part.front())) == 0) return false;
  return std::all_of(part.begin() + 1, part.end(),
                     [](unsigned char letter) { return small_letter_or_digit(letter) || letter == kPackageJoiner; });
}

// An app's id is at least two parts joined by dots, each a small letter followed by small
// letters, digits or underscores ("com.mystudio.my_game").
bool valid_package(const std::string& package) {
  if (package.size() > kMaxPackageLength || package.find(kPackageSeparator) == std::string::npos) return false;
  std::string_view rest = package;
  for (std::size_t dot = rest.find(kPackageSeparator); dot != std::string_view::npos;
       dot = rest.find(kPackageSeparator)) {
    if (!valid_package_part(rest.substr(0, dot))) return false;
    rest.remove_prefix(dot + 1);
  }
  return valid_package_part(rest);
}

std::string settings_place() { return std::string(kGameSettingsFile) + ": android."; }

// A text the section may hold under `key`, left as it is when it does not.
bool read_text(const nlohmann::json& section, const char* key, std::string& into, std::string& error) {
  if (!section.contains(key)) return true;
  if (!section[key].is_string()) {
    error = settings_place() + key + " must be a text";
    return false;
  }
  into = section[key].get<std::string>();
  return true;
}

bool read_version_code(const nlohmann::json& section, int& into, std::string& error) {
  if (!section.contains(kVersionCodeKey)) return true;
  const nlohmann::json& code = section[kVersionCodeKey];
  if (!code.is_number_integer() || code < 1) {
    error = settings_place() + "version_code must be a whole number, 1 or more";
    return false;
  }
  into = code.get<int>();
  return true;
}

bool read_section(const nlohmann::json& section, AndroidSettings& settings, std::string& error) {
  if (!section.is_object()) {
    error = std::string(kGameSettingsFile) + ": android must be a section ({ ... })";
    return false;
  }
  std::string keystore;
  if (!read_text(section, kPackageKey, settings.package, error) ||
      !read_text(section, kVersionNameKey, settings.version_name, error) ||
      !read_text(section, kKeystoreKey, keystore, error) ||
      !read_text(section, kKeyAliasKey, settings.key_alias, error)) {
    return false;
  }
  settings.keystore = keystore;
  return read_version_code(section, settings.version_code, error);
}

// Whether what was read can be used together.
bool check_settings(const AndroidSettings& settings, std::string& error) {
  if (!settings.package.empty() && !valid_package(settings.package)) {
    error = settings_place() +
            "package must look like com.mystudio.mygame (small letters, digits and _, parts joined by dots)";
    return false;
  }
  if (!settings.keystore.empty() && settings.key_alias.empty()) {
    error = settings_place() + "key_alias is needed with a keystore: the name of the key inside it";
    return false;
  }
  return true;
}

// The title and the window's size: each is used only when it is what it should be.
void read_window(const nlohmann::json& json, GameSettings& settings) {
  if (json.contains(kTitleKey) && json[kTitleKey].is_string())
    settings.title = printable(json[kTitleKey].get<std::string>());
  settings.window_width = window_size(json, kWindowWidthKey, settings.window_width);
  settings.window_height = window_size(json, kWindowHeightKey, settings.window_height);
}

// The "android" section, when the file has one. What is wrong with it; empty when nothing is.
std::string read_android(const nlohmann::json& json, AndroidSettings& settings) {
  if (!json.contains(kAndroidSection)) return {};
  std::string error;
  if (read_section(json[kAndroidSection], settings, error) && check_settings(settings, error)) return {};
  return error;
}

}  // namespace

GameManifest read_game_manifest(const std::filesystem::path& game_dir) {
  GameManifest manifest;
  manifest.settings.title = game_dir.filename().string();
  manifest.settings.window_width = config::kWindowWidth;
  manifest.settings.window_height = config::kWindowHeight;
  const nlohmann::json json = read_json(game_dir / kGameSettingsFile);
  if (!json.is_object()) return manifest;
  read_window(json, manifest.settings);
  manifest.android_error = read_android(json, manifest.settings.android);
  return manifest;
}

GameSettings read_game_settings(const std::filesystem::path& game_dir) { return read_game_manifest(game_dir).settings; }

bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error) {
  // What else the file holds (its "android" section...) stays as it is.
  nlohmann::json json = read_json(game_dir / kGameSettingsFile);
  if (!json.is_object()) json = nlohmann::json::object();
  json[kTitleKey] = settings.title;
  json[kWindowWidthKey] = settings.window_width;
  json[kWindowHeightKey] = settings.window_height;
  return write_text_file(game_dir / kGameSettingsFile, json.dump(kJsonIndent) + "\n", error);
}

}  // namespace redgem
