// Building a game: which of its files go into a build.

#include "project/game_build.hpp"

#include <algorithm>
#include <cctype>

#include "app/game_settings.hpp"

namespace redgem {
namespace {

// Of files that hold keys or certificates to sign apps with, in lowercase (they are compared so).
constexpr const char* kSigningKeyExtensions[] = {".keystore", ".jks", ".p12", ".pfx", ".pem", ".key"};
constexpr char kHiddenPrefix = '.';  // of files and folders that are not part of a game (.git, .env)
constexpr char kBackupSuffix = '~';  // of the copies editors leave behind

// Whether a file or folder of a game goes into a build of it. A copy of a game goes to other
// people: not what a link points at (that can be anything on the machine that builds), not hidden
// files and folders, not editors' backups, not a signing key left in the folder.
// The file the game's own settings sign its app with, whatever it is called. (A game.json with no
// keystore names nothing, which no file is.)
struct Shipping {
  std::filesystem::path game_dir;
  std::filesystem::path keystore = read_game_settings(game_dir).android.keystore.lexically_normal();

  bool is_the_keystore(const std::filesystem::path& file) const {
    return !keystore.empty() && file.lexically_relative(game_dir).lexically_normal() == keystore;
  }
};

bool is_leftover(const std::string& name) {
  return !name.empty() && (name.front() == kHiddenPrefix || name.back() == kBackupSuffix);
}

bool ships(const std::filesystem::directory_entry& entry, const Shipping& shipping) {
  std::error_code fs_error;
  if (entry.is_symlink(fs_error) || is_leftover(entry.path().filename().string())) return false;
  return !signing_key_file(entry.path()) && !shipping.is_the_keystore(entry.path());
}

std::string lowercase(std::string text) {
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

// Whether a walk of a folder has more to look at (one that cannot go on is over).
bool still_walking(const std::filesystem::recursive_directory_iterator& walk, const std::error_code& fs_error) {
  return !fs_error && walk != std::filesystem::end(walk);
}

}  // namespace

std::vector<std::filesystem::path> shipped_files(const std::filesystem::path& game_dir) {
  std::vector<std::filesystem::path> files;
  std::error_code fs_error;
  std::error_code unreadable;  // (one entry that cannot be looked at does not end the walk)
  const Shipping shipping{game_dir};
  std::filesystem::recursive_directory_iterator walk(game_dir, fs_error);
  for (; still_walking(walk, fs_error); walk.increment(fs_error)) {
    if (!ships(*walk, shipping)) {
      walk.disable_recursion_pending();  // nothing inside a folder that stays behind is looked at
    } else if (walk->is_regular_file(unreadable)) {
      files.push_back(walk->path().lexically_relative(game_dir));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool signing_key_file(const std::filesystem::path& file) {
  const std::string extension = lowercase(file.extension().string());
  return std::any_of(std::begin(kSigningKeyExtensions), std::end(kSigningKeyExtensions),
                     [&extension](const char* known) { return extension == known; });
}

}  // namespace redgem
