#include "app/raylib_file_reading.hpp"

#include <raylib.h>

#include <cstdlib>
#include <cstring>
#include <limits>

#include "core/game_files.hpp"

namespace redgem {
namespace {

// The most raylib can be handed at once: it counts bytes in an int.
constexpr std::uintmax_t kMostRaylibBytes = static_cast<std::uintmax_t>(std::numeric_limits<int>::max());

// (What raylib is given, it frees with free(): RL_FREE.)
unsigned char* load_file_data(const char* file, int* size) {
  *size = 0;
  const auto bytes = game_files::read(file, kMostRaylibBytes);
  if (!bytes || bytes->empty()) return nullptr;
  auto* copy = static_cast<unsigned char*>(std::malloc(bytes->size()));
  if (copy == nullptr) return nullptr;
  std::memcpy(copy, bytes->data(), bytes->size());
  *size = static_cast<int>(bytes->size());
  return copy;
}

// The text ends with a '\0', as raylib's own reading ends it.
char* load_file_text(const char* file) {
  const auto bytes = game_files::read(file, kMostRaylibBytes - 1);
  if (!bytes) return nullptr;
  auto* text = static_cast<char*>(std::malloc(bytes->size() + 1));
  if (text == nullptr) return nullptr;
  if (!bytes->empty()) std::memcpy(text, bytes->data(), bytes->size());
  text[bytes->size()] = '\0';
  return text;
}

}  // namespace

void read_raylib_files_through_game_files() {
  SetLoadFileDataCallback(load_file_data);
  SetLoadFileTextCallback(load_file_text);
}

}  // namespace redgem
