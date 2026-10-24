// game.json's version, pictures and network flag: what a game is when it ships.

#include "app/game_identity.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "app/json_file.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

// What game.json says.
constexpr const char* kVersionKey = "version";
constexpr const char* kIconKey = "icon";
constexpr const char* kSplashKey = "splash";
constexpr const char* kNetworkKey = "network";
constexpr const char* kPictureExtension = ".png";  // the only kind of picture icons and splashes are made from
// How a semantic version is put together.
constexpr char kPartSeparator = '.';
constexpr char kPreReleaseMark = '-';
constexpr char kBuildMark = '+';
constexpr char kLeadingZero = '0';
constexpr std::size_t kVersionNumbers = 3;  // MAJOR.MINOR.PATCH

std::vector<std::string_view> parts_of(std::string_view text) {
  std::vector<std::string_view> parts;
  for (std::size_t dot = text.find(kPartSeparator); dot != std::string_view::npos; dot = text.find(kPartSeparator)) {
    parts.push_back(text.substr(0, dot));
    text.remove_prefix(dot + 1);
  }
  parts.push_back(text);
  return parts;
}

bool digit(unsigned char letter) { return std::isdigit(letter) != 0; }
bool identifier_letter(unsigned char letter) { return std::isalnum(letter) != 0 || letter == kPreReleaseMark; }

bool all_digits(std::string_view part) { return std::all_of(part.begin(), part.end(), digit); }

// A number as a version writes it: digits, and no zero in front of others.
bool no_leading_zero(std::string_view part) { return part.size() == 1 || part.front() != kLeadingZero; }
bool plain_number(std::string_view part) { return !part.empty() && all_digits(part) && no_leading_zero(part); }

bool build_part(std::string_view part) {
  return !part.empty() && std::all_of(part.begin(), part.end(), identifier_letter);
}

// A pre-release part is a build part, and a number when it is all digits.
bool pre_release_part(std::string_view part) { return build_part(part) && (!all_digits(part) || plain_number(part)); }

template <typename Check>
bool every_part(std::string_view text, Check check) {
  const std::vector<std::string_view> parts = parts_of(text);
  return std::all_of(parts.begin(), parts.end(), check);
}

bool version_core(std::string_view core) {
  return parts_of(core).size() == kVersionNumbers && every_part(core, plain_number);
}

// What follows the three numbers: nothing, or "-" and pre-release parts, then "+" and build parts.
bool version_tail(std::string_view pre_release, std::string_view build, bool has_pre_release, bool has_build) {
  const bool pre_release_fine = !has_pre_release || every_part(pre_release, pre_release_part);
  return pre_release_fine && (!has_build || every_part(build, build_part));
}

bool sized_as_version(const std::string& version) { return !version.empty() && version.size() <= kMaxVersionLength; }

std::string place(const char* key) { return std::string(kGameSettingsFile) + ": " + key; }

bool png_name(const std::string& name) {
  std::string extension = std::filesystem::path(name).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return extension == kPictureExtension;
}

// A picture game.json names: a PNG inside the game's folder.
bool usable_picture(const nlohmann::json& json, const char* key) {
  return holds_text(json, key) && safe_asset_name(json[key].get<std::string>()) &&
         png_name(json[key].get<std::string>());
}

// The first thing wrong is the one said: `error` keeps it.
void note(std::string& error, const std::string& wrong) {
  if (error.empty()) error = wrong;
}

void read_version(const nlohmann::json& json, GameIdentityReading& reading) {
  if (!json.contains(kVersionKey)) return;
  if (holds_text(json, kVersionKey) && valid_game_version(json[kVersionKey].get<std::string>())) {
    reading.identity.version = json[kVersionKey].get<std::string>();
    return;
  }
  note(reading.error, tr("game.identity.bad_version", {{"place", place(kVersionKey)}}));
}

void read_picture(const nlohmann::json& json, const char* key, std::filesystem::path& into, std::string& error) {
  if (!json.contains(key)) return;
  if (usable_picture(json, key)) {
    into = std::filesystem::path(json[key].get<std::string>());
    return;
  }
  note(error, tr("game.identity.bad_picture", {{"place", place(key)}}));
}

void read_network(const nlohmann::json& json, GameIdentityReading& reading) {
  if (!json.contains(kNetworkKey)) return;
  if (json[kNetworkKey].is_boolean()) {
    reading.identity.network = json[kNetworkKey].get<bool>();
    return;
  }
  note(reading.error, tr("game.identity.bad_network", {{"place", place(kNetworkKey)}}));
}

}  // namespace

GameIdentityReading read_game_identity(const std::filesystem::path& game_dir) {
  GameIdentityReading reading;
  const nlohmann::json json = read_json_file(game_dir / kGameSettingsFile);
  if (!json.is_object()) return reading;
  read_version(json, reading);
  read_picture(json, kIconKey, reading.identity.icon, reading.error);
  read_picture(json, kSplashKey, reading.identity.splash, reading.error);
  read_network(json, reading);
  return reading;
}

bool valid_game_version(const std::string& version) {
  if (!sized_as_version(version)) return false;
  const std::string_view text = version;
  const std::size_t build_at = text.find(kBuildMark);
  const std::string_view before_build = text.substr(0, build_at);
  const std::size_t pre_release_at = before_build.find(kPreReleaseMark);
  const bool has_build = build_at != std::string_view::npos;
  const bool has_pre_release = pre_release_at != std::string_view::npos;
  const std::string_view build = has_build ? text.substr(build_at + 1) : std::string_view();
  const std::string_view pre_release = has_pre_release ? before_build.substr(pre_release_at + 1) : std::string_view();
  return version_core(before_build.substr(0, pre_release_at)) &&
         version_tail(pre_release, build, has_pre_release, has_build);
}

std::string version_numbers(const std::string& version) {
  return version.substr(0, std::min(version.find(kPreReleaseMark), version.find(kBuildMark)));
}

}  // namespace redgem
