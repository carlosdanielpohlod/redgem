// Packing an Android app: the id the app goes by on a phone.

#include "project/android_package.hpp"

#include <algorithm>
#include <cctype>

#include "core/files.hpp"
#include "core/names.hpp"

namespace redgem {

namespace {

constexpr const char* kPackagePrefix = "com.redgem.";
constexpr const char* kFallbackPackage = "game";
constexpr char kPackageJoiner = '_';
constexpr std::size_t kDigitsLeftOut = 2;  // of a name's eight: an app's id carries the last six

// A folder's name with only its letters and digits, in small letters.
std::string letters_and_digits(const std::string& folder) {
  std::string name;
  for (const char c : folder) {
    const auto letter = static_cast<unsigned char>(c);
    if (std::isalnum(letter) != 0) name.push_back(static_cast<char>(std::tolower(letter)));
  }
  return name;
}

// A few digits that tell names apart once what an app's id cannot hold was dropped from them.
std::string id_digits(const std::string& folder) { return name_digits(folder).substr(kDigitsLeftOut); }

// Whether a name can start a part of an app's id: with a letter.
bool starts_a_part(const std::string& part) {
  return !part.empty() && std::isalpha(static_cast<unsigned char>(part.front())) != 0;
}

}  // namespace

std::string android_package_from(const std::string& name) {
  std::string part = letters_and_digits(name);
  if (!starts_a_part(part)) part = kFallbackPackage + part;
  // Nothing but small letters and digits: the name is the id as it is.
  if (std::all_of(name.begin(), name.end(), small_letter_or_digit)) return kPackagePrefix + part;
  // "My Game", "my-game" and "mygame" are three games: what was dropped from the name comes
  // back as a few digits, so one does not install over another.
  return kPackagePrefix + part + kPackageJoiner + id_digits(name);
}

std::string android_package_name(const std::filesystem::path& game_dir) {
  return android_package_from(normalized_folder(game_dir).filename().string());
}

std::string android_package_for(const GameSettings& settings, const std::filesystem::path& game_dir) {
  if (!settings.android.package.empty()) return settings.android.package;
  if (settings.id.empty()) return android_package_name(game_dir);
  return valid_android_package(settings.id) ? settings.id : android_package_from(settings.id);
}

}  // namespace redgem
