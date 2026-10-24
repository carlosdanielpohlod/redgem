// Building a game: what it is shipped with besides its player and its files (its icon, its menu
// entry), for Windows and Linux. The Mac's app is in game_build_mac.cpp.

#include "project/game_shipping.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/icons/exe_icon.hpp"
#include "project/icons/icon_set.hpp"

namespace redgem {
namespace {

constexpr const char* kDesktopExtension = ".desktop";
// The most a player for Windows is read into memory to be given an icon (it is tens of megabytes).
constexpr std::uintmax_t kMostPlayerBytes = 1024ull * 1024ull * 1024ull;
constexpr const char* kPngExtension = ".png";
constexpr char kEscape = '\\';  // what a .desktop file's values escape themselves with
// What a .desktop value writes escaped, and how (the Desktop Entry Specification's).
constexpr std::array<std::pair<char, char>, 4> kDesktopEscapes = {
    {{'\\', '\\'}, {'\n', 'n'}, {'\t', 't'}, {'\r', 'r'}}};
// A menu entry, by the freedesktop.org Desktop Entry Specification: a game, started without a terminal.
constexpr const char* kDesktopHeader = "[Desktop Entry]\nType=Application\n";
constexpr const char* kDesktopTail = "Categories=Game;\nTerminal=false\n";

// A title as a .desktop value: a backslash, a line break, a tab and a carriage return are written
// as escapes, so the title stays one value on one line.
std::string desktop_value(const std::string& text) {
  std::string value;
  for (const char letter : text) {
    const auto* escape = std::find_if(kDesktopEscapes.begin(), kDesktopEscapes.end(),
                                      [letter](const std::pair<char, char>& known) { return known.first == letter; });
    if (escape == kDesktopEscapes.end()) {
      value.push_back(letter);
    } else {
      value += {kEscape, escape->second};
    }
  }
  return value;
}

bool write_bytes(const std::filesystem::path& file, const bytes::Bytes& data, std::string& error) {
  return write_text_file(file, std::string(data.begin(), data.end()), error);
}

// The icon's file, inside the game; an error when game.json names one that is not there.
bool find_icon(const std::filesystem::path& game_dir, BuildIdentity& identity, std::string& error) {
  if (identity.identity.icon.empty()) return true;
  const auto file = file_inside(game_dir, identity.identity.icon);
  if (file) identity.icon = *file;
  if (!file) error = tr("project.icon.missing", {{"picture", printable(identity.identity.icon.generic_string())}});
  return file.has_value();
}

// Linux's icon beside the executable: the picture at one size menus scale from.
bool write_linux_icon(const std::filesystem::path& icon, const std::filesystem::path& to, std::string& error) {
  if (icon.empty()) {
    std::error_code fs_error;
    std::filesystem::remove(to, fs_error);  // (an earlier build's goes: this game has none now)
    return true;
  }
  const auto icons = scaled_icons(icon, &kLinuxIconSide, 1, error);
  return icons && write_bytes(to, icons->at(kLinuxIconSide), error);
}

bool write_linux_files(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                       const BuildIdentity& identity, std::string& error) {
  const std::string executable = executable_name(game_dir);
  const std::string entry = desktop_entry(identity.title, executable, !identity.icon.empty());
  return write_text_file(out_dir / desktop_entry_file(executable), entry, error) &&
         write_linux_icon(identity.icon, out_dir / desktop_icon_file(executable), error);
}

bool needs_exe_icon(BuildTarget target, const BuildIdentity& identity) {
  return target == BuildTarget::Windows && !identity.icon.empty();
}

}  // namespace

std::optional<BuildIdentity> build_identity(const std::filesystem::path& game_dir, std::string& error) {
  const GameManifest manifest = read_game_manifest(game_dir);
  const GameIdentityReading reading = read_game_identity(game_dir);
  error = reading.error;
  if (!error.empty()) return std::nullopt;
  BuildIdentity identity{manifest.settings.title, manifest.settings.id, reading.identity, {}};
  if (!find_icon(game_dir, identity, error)) return std::nullopt;
  return identity;
}

std::string desktop_entry(const std::string& title, const std::string& executable, bool has_icon) {
  std::string entry = std::string(kDesktopHeader) + "Name=" + desktop_value(title) + "\nExec=" + executable + "\n";
  if (has_icon) entry += "Icon=" + executable + "\n";
  return entry + kDesktopTail;
}

std::string desktop_entry_file(const std::string& executable) { return executable + kDesktopExtension; }

std::string desktop_icon_file(const std::string& executable) { return executable + kPngExtension; }

bool give_exe_icon(const std::filesystem::path& exe, const std::filesystem::path& icon, std::string& error) {
  auto program = read_file_up_to(exe, kMostPlayerBytes);
  if (!program) {
    error = tr("project.icon.cannot_read_player", {{"player", exe.string()}});
    return false;
  }
  const std::vector<int> sides = exe_icon_sides(*program);
  const auto icons = scaled_icons(icon, sides.data(), sides.size(), error);
  return icons && replace_exe_icon(*program, *icons, error) && write_bytes(exe, *program, error);
}

std::vector<std::string> shipping_outputs(const std::filesystem::path& game_dir, BuildTarget target) {
  if (target != BuildTarget::Linux) return {};
  const std::string executable = executable_name(game_dir);
  return {desktop_entry_file(executable), desktop_icon_file(executable)};
}

bool write_shipping_files(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                          BuildTarget target, const BuildIdentity& identity, std::string& error) {
  if (target == BuildTarget::Linux) return write_linux_files(game_dir, out_dir, identity, error);
  if (!needs_exe_icon(target, identity)) return true;
  const std::string executable = platform::executable_file_name(executable_name(game_dir), true);
  return give_exe_icon(out_dir / executable, identity.icon, error);
}

}  // namespace redgem
