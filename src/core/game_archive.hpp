#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A built game's files packed into one file (game.rgpak): its scripts, scenes, pictures, sounds,
// fonts and texts, read-only. The format (all numbers little-endian):
//
//   header   (32 bytes)
//     magic         8 bytes   "RGPAK", 0x1A, '\n', 0
//     version       u32       kArchiveVersion
//     entry count   u32
//     table offset  u64       where the table starts (right after the header)
//     table size    u64       its bytes
//   table    (one record per file, in the order they were written)
//     name length   u16       bytes of the name that follows
//     method        u8        0 stored as it is, 1 raw deflate
//     reserved      u8        0
//     crc32         u32       of the file's own bytes (uncompressed)
//     offset        u64       where its data starts, from the start of the archive
//     size          u64       the file's own bytes
//     stored size   u64       the bytes its data takes in the archive (deflated: never more than size;
//                             a file deflate cannot shrink is stored)
//     name          UTF-8, '/' between folders, relative to the game's folder ("scripts/player.rb")
//   data     every file's data, one after another
//
// What an archive says is never trusted: every size is bounded, every name must be one a game may
// name (see safe_archive_name), and a file's bytes are checked against its CRC32 when read. An
// archive that breaks any rule is refused whole, with a reason.
namespace redgem {

inline constexpr std::size_t kArchiveMagicBytes = 8;
inline constexpr std::uint8_t kArchiveMagic[kArchiveMagicBytes] = {'R', 'G', 'P', 'A', 'K', 0x1A, '\n', 0};
inline constexpr std::uint32_t kArchiveVersion = 1;
inline constexpr std::uint64_t kArchiveHeaderBytes = 32;
inline constexpr std::uint64_t kArchiveRecordBytes = 32;  // a table record without its name
// The bounds every archive is held to.
inline constexpr std::uint32_t kMaxArchiveEntries = 100000;
inline constexpr std::size_t kMaxArchiveNameBytes = 1024;
inline constexpr std::uint64_t kMaxArchiveTableBytes = 64ull * 1024ull * 1024ull;
inline constexpr std::uint64_t kMaxArchiveEntryBytes = 1024ull * 1024ull * 1024ull;  // fits zlib's 32-bit counts
// Deflate packs at most this many bytes into one (its longest match in its shortest code): an
// entry that claims more is refused before anything is allocated for it.
inline constexpr std::uint64_t kMaxDeflateRatio = 1032;
// The file a built game's files are packed into, beside its executable (see app/bundled_game.hpp).
inline constexpr const char* kArchiveFileName = "game.rgpak";
// An archive is written as <file><kArchivePartialSuffix> beside it, then renamed over it.
inline constexpr const char* kArchivePartialSuffix = ".partial";

enum class ArchiveMethod : std::uint8_t { Stored = 0, Deflate = 1 };

struct ArchiveEntry {
  std::string name;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint64_t stored_size = 0;
  ArchiveMethod method = ArchiveMethod::Stored;
  std::uint32_t crc = 0;  // the CRC32 of the file's own bytes (zlib names a macro crc32)
};

// Whether `name` may name a file in an archive: a name a game may give (core/safe_path.hpp: not
// absolute, no "..", no backslash, no ':'), with no empty or "." step, at most
// kMaxArchiveNameBytes long.
bool safe_archive_name(std::string_view name);

// The CRC32 (zlib's) of some bytes.
std::uint32_t archive_crc32(const std::uint8_t* bytes, std::size_t size);

// Where an archive's bytes come from: a file on disk, or memory (tests, fuzzing).
class ArchiveBytes {
 public:
  virtual ~ArchiveBytes() = default;
  virtual std::uint64_t size() const = 0;
  // Exactly `count` bytes from `offset` into `out`; false when they cannot be read.
  virtual bool read_at(std::uint64_t offset, std::size_t count, std::uint8_t* out) const = 0;
};

class GameArchive {
 public:
  // An archive file, its table read and checked; nullopt with `error` saying why when it is not one.
  static std::optional<GameArchive> open(const std::filesystem::path& file, std::string& error);
  // The same, for an archive held in memory.
  static std::optional<GameArchive> from_bytes(std::vector<std::uint8_t> bytes, std::string& error);

  const std::vector<ArchiveEntry>& entries() const { return entries_; }
  // The entry of a file; nullptr when the archive has none of that name.
  const ArchiveEntry* find(const std::string& name) const;
  // Whether some file of the archive is inside the folder `name` ("scripts").
  bool has_folder(const std::string& name) const;
  // The names of the files right inside the folder `name` (not in its subfolders), sorted.
  std::vector<std::string> files_in(const std::string& name) const;
  // A file's bytes, inflated and checked against its CRC32; nullopt with `error` saying why when
  // it is larger than `max_bytes`, cannot be read, or does not match.
  std::optional<std::vector<std::uint8_t>> read(const ArchiveEntry& entry, std::uint64_t max_bytes,
                                                std::string& error) const;

 private:
  static std::optional<GameArchive> from_source(std::shared_ptr<const ArchiveBytes> source, std::string& error);
  explicit GameArchive(std::shared_ptr<const ArchiveBytes> source) : source_(std::move(source)) {}
  bool index(const std::vector<std::uint8_t>& table, std::uint32_t count, std::uint64_t data_start, std::string& error);
  bool add(ArchiveEntry entry, std::uint64_t data_start, std::string& error);
  bool no_file_named_as_folder(std::string& error) const;
  std::optional<std::vector<std::uint8_t>> stored_data(const ArchiveEntry& entry, std::string& error) const;

  std::shared_ptr<const ArchiveBytes> source_;
  std::vector<ArchiveEntry> entries_;
  std::map<std::string, std::size_t> by_name_;
};

// What goes into an archive: a file's name in it, and how its bytes are got (read from disk when
// it is written, one file at a time). nullopt from `bytes` fails the archive.
struct ArchiveInput {
  std::string name;
  std::function<std::optional<std::vector<std::uint8_t>>()> bytes;
};
ArchiveInput archive_input_from_file(const std::string& name, const std::filesystem::path& file);
ArchiveInput archive_input_from_bytes(const std::string& name, std::vector<std::uint8_t> bytes);

// Whether files are compressed: each one that deflate makes smaller is, the others are stored.
enum class ArchiveCompression { None, Deflate };

// Writes `inputs` into the archive `file` (replaced). False, with `error` saying why, for a name an
// archive may not hold, a name given twice, a file too large or unreadable, or a file that cannot
// be written.
bool write_game_archive(const std::filesystem::path& file, const std::vector<ArchiveInput>& inputs,
                        ArchiveCompression compression, std::string& error);

}  // namespace redgem
