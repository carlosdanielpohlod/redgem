// game.json's "android" section: what a game says about its Android app.

#include "app/android/android_settings.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "app/json_file.hpp"
#include "core/names.hpp"

namespace redgem {
namespace {

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

// A part of an app's id: a small letter followed by small letters, digits or underscores.
bool starts_with_small_letter(std::string_view part) {
  return !part.empty() && std::islower(static_cast<unsigned char>(part.front())) != 0;
}

bool package_letter(unsigned char letter) { return small_letter_or_digit(letter) || letter == kPackageJoiner; }

bool valid_package_part(std::string_view part) {
  return starts_with_small_letter(part) && std::all_of(part.begin() + 1, part.end(), package_letter);
}

// Whether a text is as long as an app's id may be, and in parts.
bool sized_as_package(const std::string& package) {
  return package.size() <= kMaxPackageLength && package.find(kPackageSeparator) != std::string::npos;
}

std::string settings_place() { return std::string(kGameSettingsFile) + ": android."; }

// A text the section may hold under `key`, left as it is when it does not.
bool read_text(const nlohmann::json& section, const char* key, std::string& into, std::string& error) {
  if (!section.contains(key)) return true;
  if (!holds_text(section, key)) {
    error = settings_place() + key + " must be a text";
    return false;
  }
  into = section[key].get<std::string>();
  return true;
}

bool is_version_code(const nlohmann::json& code) { return code.is_number_integer() && code >= 1; }

bool read_version_code(const nlohmann::json& section, int& into, std::string& error) {
  if (!section.contains(kVersionCodeKey)) return true;
  const nlohmann::json& code = section[kVersionCodeKey];
  if (!is_version_code(code)) {
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
  const bool texts_read = read_text(section, kPackageKey, settings.package, error) &&
                          read_text(section, kVersionNameKey, settings.version_name, error) &&
                          read_text(section, kKeystoreKey, keystore, error) &&
                          read_text(section, kKeyAliasKey, settings.key_alias, error);
  if (!texts_read) return false;
  settings.keystore = keystore;
  return read_version_code(section, settings.version_code, error);
}

bool names_unusable_package(const AndroidSettings& settings) {
  return !settings.package.empty() && !valid_android_package(settings.package);
}

bool keystore_without_alias(const AndroidSettings& settings) {
  return !settings.keystore.empty() && settings.key_alias.empty();
}

// Whether what was read can be used together.
bool check_settings(const AndroidSettings& settings, std::string& error) {
  if (names_unusable_package(settings)) {
    error = settings_place() +
            "package must look like com.mystudio.mygame (small letters, digits and _, parts joined by dots)";
    return false;
  }
  if (keystore_without_alias(settings)) {
    error = settings_place() + "key_alias is needed with a keystore: the name of the key inside it";
    return false;
  }
  return true;
}

}  // namespace

std::string read_android_section(const nlohmann::json& json, AndroidSettings& settings) {
  if (!json.contains(kAndroidSection)) return {};
  std::string error;
  const bool usable = read_section(json[kAndroidSection], settings, error) && check_settings(settings, error);
  return usable ? std::string() : error;
}

bool valid_android_package(const std::string& package) {
  if (!sized_as_package(package)) return false;
  std::string_view rest = package;
  for (std::size_t dot = rest.find(kPackageSeparator); dot != std::string_view::npos;
       dot = rest.find(kPackageSeparator)) {
    if (!valid_package_part(rest.substr(0, dot))) return false;
    rest.remove_prefix(dot + 1);
  }
  return valid_package_part(rest);
}

}  // namespace redgem
