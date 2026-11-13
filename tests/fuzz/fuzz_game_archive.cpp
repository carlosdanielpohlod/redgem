// A built game's archive (game.rgpak) as it could arrive damaged or made up: its header and table
// read and checked, then every file it lists read, inflated and checked against its CRC32.
#include <cstdint>
#include <string>
#include <vector>

#include "core/game_archive.hpp"
#include "fuzz/fuzz_target.hpp"

namespace {

// The most of one file read: room for what a small input can inflate to, little enough to stay quick.
constexpr std::uint64_t kMostFileBytes = 16ull * 1024ull * 1024ull;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string error;
  const auto archive = redgem::GameArchive::from_bytes(std::vector<std::uint8_t>(data, data + size), error);
  if (!archive) return redgem::fuzz::kKeepInput;
  for (const redgem::ArchiveEntry& entry : archive->entries()) {
    archive->read(entry, kMostFileBytes, error);
    archive->has_folder(entry.name);
  }
  archive->files_in("");
  return redgem::fuzz::kKeepInput;
}
