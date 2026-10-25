#include "core/message_lines.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace redgem {
namespace {

// How a backtrace line starts, and what follows the place it names.
constexpr const char* kFrom = "from ";
constexpr const char* kIn = ":in";
constexpr char kSeparator = '/';
constexpr char kBackslash = '\\';              // how Windows separates folders
constexpr const char* kBeforePaths = " '\"(";  // what a path in a message may follow

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> lines;
  std::stringstream stream(text);
  for (std::string line; std::getline(stream, line);) lines.push_back(line);
  return lines;
}

// Whether a path starts at `at` in a text: there is nothing before it but a space, a quote or a
// bracket (in "../x", "./" is no path's start).
bool starts_a_path(const std::string& text, std::size_t at) {
  return at == 0 || std::strchr(kBeforePaths, text[at - 1]) != nullptr;
}

// Takes `folder` out of every path of the text that starts with it.
void remove_folder(std::string& text, const std::string& folder) {
  for (std::size_t at = text.find(folder); at != std::string::npos; at = text.find(folder, at)) {
    if (starts_a_path(text, at)) {
      text.erase(at, folder.size());
    } else {
      ++at;
    }
  }
}

// A folder as a path's start: with '/' between its parts and one after it, however it was typed
// ("games/demo/" and "games/demo" are "games/demo/").
std::string folder_start(const std::filesystem::path& folder) {
  std::string text = folder.lexically_normal().generic_string();
  if (text.empty() || text.back() != kSeparator) text += kSeparator;
  return text;
}

// The ways the game's folder is written in what the engine says: as it was given, and in full.
// (Paths are said with '/' whatever the system writes them with.)
std::vector<std::string> folder_prefixes(const std::filesystem::path& game_dir) {
  std::error_code error;
  return {folder_start(std::filesystem::absolute(game_dir, error)), folder_start(game_dir)};
}

std::string with_slashes(std::string text) {
  if (std::filesystem::path::preferred_separator != kSeparator)
    std::replace(text.begin(), text.end(), kBackslash, kSeparator);
  return text;
}

std::string without_folder(const std::string& said, const std::filesystem::path& game_dir) {
  std::string text = with_slashes(said);
  for (const std::string& prefix : folder_prefixes(game_dir)) remove_folder(text, prefix);
  return text;
}

// "from /game/scripts/coin.rb:12:in update" -> "/game/scripts/coin.rb:12".
std::string place_in(const std::string& line) {
  const std::size_t from = line.find(kFrom);
  if (from == std::string::npos) return {};
  const std::size_t start = from + std::string(kFrom).size();
  return line.substr(start, line.find(kIn, start) - start);
}

bool in_game(const std::string& place, const std::filesystem::path& game_dir) {
  return !place.empty() && without_folder(place, game_dir) != with_slashes(place);
}

// The first place of the game's own files a backtrace names; empty when it names none.
std::string first_game_place(const std::vector<std::string>& lines, const std::filesystem::path& game_dir) {
  for (std::size_t line = 1; line < lines.size(); ++line) {
    const std::string place = place_in(lines[line]);
    if (in_game(place, game_dir)) return without_folder(place, game_dir);
  }
  return {};
}

std::string trimmed(const std::string& text) {
  const std::size_t first = text.find_first_not_of(' ');
  return first == std::string::npos ? std::string() : text.substr(first);
}

}  // namespace

std::string compact_message(const std::string& message, const std::filesystem::path& game_dir) {
  const std::vector<std::string> lines = lines_of(message);
  if (lines.empty()) return {};
  const std::string first = without_folder(trimmed(lines.front()), game_dir);
  const std::string place = first_game_place(lines, game_dir);
  return place.empty() ? first : place + ": " + first;
}

}  // namespace redgem
