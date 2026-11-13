// A game's JSON save: a value of a few hundred entries written to the file (whole, beside its
// backup, as every Save.set does) and read back by a store that opens the file anew. The disk is
// not flushed to: on a shared CI machine its time is the disk's, not the engine's.

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "bench/bench_case.hpp"
#include "storage/save_file.hpp"

namespace redgem::bench {

namespace {

constexpr int kEntries = 256;
constexpr int kWritesPerSample = 20;
constexpr int kReadsPerSample = 20;
constexpr double kScoreStep = 12.5;
constexpr const char* kKey = "progress";

bool no_flush(std::FILE* /*file*/) { return true; }

// A save of the kind a game keeps: levels, each with a few numbers, flags and a name.
nlohmann::json progress(int revision) {
  nlohmann::json levels = nlohmann::json::array();
  for (int i = 0; i < kEntries; ++i) {
    levels.push_back({{"name", "level-" + std::to_string(i)},
                      {"best", i * kScoreStep + revision},
                      {"done", i % 2 == 0},
                      {"coins", {i, i + 1, i + 2}}});
  }
  return {{"revision", revision}, {"levels", std::move(levels)}};
}

// A folder of its own among the temporary files, removed with the last sampler that uses it.
struct SaveFolder {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               ("redgem-bench-" + std::to_string(::getpid()) + "-" + std::to_string(next++));
  SaveFolder() { std::filesystem::create_directories(path); }
  ~SaveFolder() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
  SaveFolder(const SaveFolder&) = delete;
  SaveFolder& operator=(const SaveFolder&) = delete;
  std::filesystem::path file() const { return path / "save.json"; }

  static inline int next = 0;
};

Sampler write_sampler() {
  auto folder = std::make_shared<SaveFolder>();
  auto store = std::make_shared<SaveFile>(folder->file(), no_flush);
  // Two saves that differ, written in turn: every write changes the file, and none times making the value.
  auto saves = std::make_shared<std::vector<nlohmann::json>>(std::vector<nlohmann::json>{progress(0), progress(1)});
  auto turn = std::make_shared<std::size_t>(0);
  return [folder, store, saves, turn] {
    return microseconds_each(kWritesPerSample, [&] {
      const nlohmann::json& save = (*saves)[(*turn)++ % saves->size()];
      require(store->set(kKey, save), "writing the save");
    });
  };
}

void read_once(const std::filesystem::path& file) {
  SaveFile store(file, no_flush);
  const auto value = store.get(kKey);
  require(value.has_value(), "reading the save");
  keep(value->size());
}

Sampler read_sampler() {
  auto folder = std::make_shared<SaveFolder>();
  require(SaveFile(folder->file(), no_flush).set(kKey, progress(0)), "writing the save to read");
  return [folder] { return microseconds_each(kReadsPerSample, [&folder] { read_once(folder->file()); }); };
}

std::string save_words() { return " a save of " + std::to_string(kEntries) + " levels"; }

}  // namespace

std::vector<BenchCase> save_cases() {
  return {{"save.json_write", "writing" + save_words(), write_sampler},
          {"save.json_read", "opening and reading" + save_words(), read_sampler}};
}

}  // namespace redgem::bench
