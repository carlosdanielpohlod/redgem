// Building a game for the Mac: an .app with the player, the game, its icon and its description.

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string_view>
#include <vector>

#include "app/bundled_game.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "core/xml_writer.hpp"
#include "project/build_record.hpp"
#include "project/game_shipping.hpp"
#include "project/icons/icon_set.hpp"
#include "project/notices.hpp"

namespace redgem {
namespace {

constexpr const char* kAppExtension = ".app";
constexpr const char* kIcnsExtension = ".icns";
constexpr const char* kInfoFile = "Info.plist";
constexpr const char* kBundleIdPrefix = "com.redgem.";
constexpr char kUnderscore = '_';
constexpr char kBundleIdJoiner = '-';            // what a bundle id has instead of '_'
constexpr const char* kNotInFileNames = "/\\:";  // what a folder's name cannot hold on a Mac or elsewhere
constexpr const char* kTrimmed = " .";           // a name starting with a dot is hidden; spaces at its ends are lost
constexpr const char* kMinimumMacOs = "11.0";    // the oldest macOS the player is built for
constexpr const char* kAdHocIdentity = "-";
constexpr unsigned char kFirstPrintable = 0x20;  // the first character that is no control character
constexpr const char* kAllowedControls =
    "\t\n\r";  // the control characters XML 1.0 allows      // codesign's name for "no identity"
// The start and the end of every Info.plist (Apple's "Information Property List").
constexpr const char* kPlistHead =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    "<plist version=\"1.0\">\n<dict>\n";
// What an Info.plist says, by the names Apple gives it.
namespace key {
constexpr const char* kName = "CFBundleName";
constexpr const char* kDisplayName = "CFBundleDisplayName";
constexpr const char* kIdentifier = "CFBundleIdentifier";
constexpr const char* kExecutable = "CFBundleExecutable";
constexpr const char* kPackageType = "CFBundlePackageType";
constexpr const char* kDictionaryVersion = "CFBundleInfoDictionaryVersion";
constexpr const char* kShortVersion = "CFBundleShortVersionString";
constexpr const char* kVersion = "CFBundleVersion";
constexpr const char* kMinimumSystem = "LSMinimumSystemVersion";
constexpr const char* kIconFile = "CFBundleIconFile";
}  // namespace key
constexpr const char* kApplicationType = "APPL";   // the bundle is an application
constexpr const char* kDictionaryVersion = "6.0";  // the only version of the format there is
constexpr const char* kPlistTail = "  <key>NSHighResolutionCapable</key>\n  <true/>\n</dict>\n</plist>\n";

// Whether XML 1.0 lets a text hold a character: every control character but tab, line feed and
// carriage return is forbidden.
bool allowed_control(char letter) { return std::string_view(kAllowedControls).find(letter) != std::string_view::npos; }

bool xml_character(char letter) {
  return static_cast<unsigned char>(letter) >= kFirstPrintable || allowed_control(letter);
}

// A value as an Info.plist can hold it: without what XML forbids, escaped.
std::string plist_text(const std::string& value) {
  std::string text;
  std::copy_if(value.begin(), value.end(), std::back_inserter(text), xml_character);
  return xml_escaped(text);
}

std::string plist_entry(const char* key, const std::string& value) {
  return std::string("  <key>") + key + "</key>\n  <string>" + plist_text(value) + "</string>\n";
}

std::string version_entries(const std::string& version) {
  // (The Mac takes numbers only: a pre-release's version is its numbers.)
  const std::string numbers = version.empty() ? kDefaultGameVersion : version_numbers(version);
  return plist_entry(key::kShortVersion, numbers) + plist_entry(key::kVersion, numbers);
}

bool kept_in_folder_name(char letter) { return std::string(kNotInFileNames).find(letter) == std::string::npos; }

std::string trimmed(const std::string& name) {
  const std::size_t first = name.find_first_not_of(kTrimmed);
  if (first == std::string::npos) return {};
  return name.substr(first, name.find_last_not_of(kTrimmed) - first + 1);
}

std::filesystem::path contents_of(const std::filesystem::path& app) { return app / kMacContentsFolder; }

// What goes in Contents/Resources besides the game: its icon and the licenses.
bool write_resources(const std::filesystem::path& resources, const BuildIdentity& identity,
                     const std::string& executable, std::string& error) {
  if (!write_notices(resources, error)) return false;
  if (identity.icon.empty()) return true;
  const auto icons = icon_set_from(identity.icon, kMacIconSides, error);
  if (!icons) return false;
  const bytes::Bytes icns = icns_file(*icons);
  return write_bytes_file(resources / (executable + kIcnsExtension), icns, error);
}

MacApp app_of(const BuildIdentity& identity, const std::string& executable) {
  return MacApp{identity.title, executable, mac_bundle_id(identity.id, executable), identity.identity.version,
                !identity.icon.empty()};
}

bool fill_app(const std::filesystem::path& game_dir, const std::filesystem::path& app,
              const std::filesystem::path& player, const BuildIdentity& identity, std::string& error) {
  const std::string executable = executable_name(game_dir);
  const std::filesystem::path contents = contents_of(app);
  const std::filesystem::path resources = contents / kMacResourcesFolder;
  std::error_code fs_error;
  std::filesystem::create_directories(contents / kMacProgramFolder, fs_error);
  return copy_player(player, contents / kMacProgramFolder / executable, error) &&
         copy_game(game_dir, resources / kBundledGameFolder, error) &&
         write_resources(resources, identity, executable, error) &&
         write_text_file(contents / kInfoFile, mac_info_plist(app_of(identity, executable)), error);
}

}  // namespace

std::string mac_info_plist(const MacApp& app) {
  std::string plist = kPlistHead;
  plist += plist_entry(key::kName, app.title) + plist_entry(key::kDisplayName, app.title);
  plist += plist_entry(key::kIdentifier, app.identifier) + plist_entry(key::kExecutable, app.executable);
  plist += plist_entry(key::kPackageType, kApplicationType) + plist_entry(key::kDictionaryVersion, kDictionaryVersion);
  plist += version_entries(app.version) + plist_entry(key::kMinimumSystem, kMinimumMacOs);
  if (app.icon) plist += plist_entry(key::kIconFile, app.executable + kIcnsExtension);
  return plist + kPlistTail;
}

std::string mac_bundle_id(const std::string& game_id, const std::string& executable) {
  std::string id = game_id.empty() ? kBundleIdPrefix + executable : game_id;
  for (char& letter : id) {
    if (letter == kUnderscore) letter = kBundleIdJoiner;
  }
  return id;
}

std::string mac_app_folder(const std::string& title, const std::string& executable) {
  std::string name;
  for (const char letter : title) {
    if (kept_in_folder_name(letter)) name.push_back(letter);
  }
  name = trimmed(name);
  return (name.empty() ? executable : name) + kAppExtension;
}

std::optional<std::filesystem::path> build_mac_app(const std::filesystem::path& game_dir,
                                                   const std::filesystem::path& out_dir,
                                                   const std::filesystem::path& player, const BuildIdentity& identity,
                                                   std::string& error) {
  const std::vector<std::string> outputs = {mac_app_folder(identity.title, executable_name(game_dir))};
  std::filesystem::path app = out_dir / outputs.front();
  // (The app's folder is emptied first: it must not hold the game it is made of.)
  if (!game_outside(game_dir, app, error)) return std::nullopt;
  std::error_code fs_error;
  std::filesystem::create_directories(out_dir, fs_error);
  BuildRecord record = BuildRecord::of(out_dir);
  if (!record.may_write(outputs, error) || !record.keep(outputs, error)) return std::nullopt;
  if (!fresh_bundle(app, error) || !fill_app(game_dir, app, player, identity, error)) return std::nullopt;
  return app;
}

bool sign_mac_app(const std::filesystem::path& app, std::string& error) {
  const std::string command = "codesign --force --deep --sign " + platform::posix_shell_word(kAdHocIdentity) + " " +
                              platform::posix_shell_word(app) + " >/dev/null 2>&1";
  // (The app's path reaches the shell as one quoted word.)
  if (std::system(command.c_str()) == 0) return true;  // NOLINT(bugprone-command-processor)
  error = tr("project.build.cannot_sign_app", {{"app", app.string()}});
  return false;
}

}  // namespace redgem
