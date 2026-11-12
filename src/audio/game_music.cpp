#include "audio/game_music.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "core/game_files.hpp"

namespace redgem {
namespace {

// The most of a piece of music read into memory to be streamed from there.
constexpr std::uintmax_t kMaxMusicBytes = 256ull * 1024ull * 1024ull;

// What each stream from memory is streamed from, by the decoder raylib made for it.
struct StreamedBytes {
  std::mutex guard;
  std::map<const void*, std::vector<std::uint8_t>> by_stream;
};

StreamedBytes& streamed() {
  static StreamedBytes bytes;
  return bytes;
}

void keep(const Music& music, std::vector<std::uint8_t> bytes) {
  StreamedBytes& kept = streamed();
  const std::lock_guard<std::mutex> lock(kept.guard);
  kept.by_stream[music.ctxData] = std::move(bytes);
}

void forget(const Music& music) {
  StreamedBytes& kept = streamed();
  const std::lock_guard<std::mutex> lock(kept.guard);
  kept.by_stream.erase(music.ctxData);
}

// The kind of a music file as raylib names it from memory: its extension, in lower case (raylib
// compares it as it is, where its own file loading ignores the case).
std::string music_type(const std::filesystem::path& file) {
  std::string type = file.extension().string();
  std::transform(type.begin(), type.end(), type.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return type;
}

Music music_from_archive(const std::filesystem::path& file) {
  auto bytes = game_files::read(file, kMaxMusicBytes);
  if (!bytes || bytes->empty()) return Music{};
  const std::string type = music_type(file);
  const Music music = LoadMusicStreamFromMemory(type.c_str(), bytes->data(), static_cast<int>(bytes->size()));
  if (music.ctxData != nullptr) keep(music, std::move(*bytes));
  return music;
}

}  // namespace

Music load_game_music(const std::filesystem::path& file) {
  if (game_files::in_archive(file)) return music_from_archive(file);
  return LoadMusicStream(file.string().c_str());
}

void unload_game_music(Music music) {
  UnloadMusicStream(music);
  forget(music);
}

}  // namespace redgem
