// Reading a game archive: its header and table checked, its files read, inflated and checked.

#include "core/game_archive.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

#include "core/game_archive_layout.hpp"
#include "core/safe_path.hpp"

namespace redgem {

using namespace archive_layout;

namespace {

constexpr char kFolderSeparator = '/';
constexpr std::string_view kCurrentStep = ".";

// What the archive's first bytes say.
struct Header {
  std::uint32_t count = 0;
  std::uint64_t table_offset = 0;
  std::uint64_t table_size = 0;
};

class FileBytes final : public ArchiveBytes {
 public:
  FileBytes(std::filesystem::path file, std::uint64_t size) : file_(std::move(file)), size_(size) {}
  std::uint64_t size() const override { return size_; }
  // (A stream of its own for every read: files are read from more than one thread.)
  bool read_at(std::uint64_t offset, std::size_t count, std::uint8_t* out) const override {
    if (!fits(offset, count, size_)) return false;
    std::ifstream in(file_, std::ios::binary);
    in.seekg(static_cast<std::streamoff>(offset));
    return static_cast<bool>(in.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count)));
  }

 private:
  std::filesystem::path file_;
  std::uint64_t size_;
};

class MemoryBytes final : public ArchiveBytes {
 public:
  explicit MemoryBytes(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
  std::uint64_t size() const override { return bytes_.size(); }
  bool read_at(std::uint64_t offset, std::size_t count, std::uint8_t* out) const override {
    if (!fits(offset, count, bytes_.size())) return false;
    if (count > 0) std::memcpy(out, bytes_.data() + offset, count);
    return true;
  }

 private:
  std::vector<std::uint8_t> bytes_;
};

// Sets `error` and gives nothing: how every refusal ends.
std::nullopt_t refuse(const std::string& why, std::string& error) {
  error = why;
  return std::nullopt;
}

bool fail(const std::string& why, std::string& error) {
  error = why;
  return false;
}

bool has_magic(const std::array<std::uint8_t, kArchiveHeaderBytes>& bytes) {
  return std::equal(std::begin(kArchiveMagic), std::end(kArchiveMagic), bytes.begin());
}

Header header_of(const std::array<std::uint8_t, kArchiveHeaderBytes>& bytes) {
  return {static_cast<std::uint32_t>(little_endian(&bytes[kCountAt], kU32Bytes)),
          little_endian(&bytes[kTableOffsetAt], kU64Bytes), little_endian(&bytes[kTableSizeAt], kU64Bytes)};
}

// Whether the table is where the format puts it, within the bounds, and large enough for its records.
bool table_in_bounds(const Header& header, std::uint64_t archive_size) {
  const bool placed = header.table_offset == kArchiveHeaderBytes && header.table_size <= kMaxArchiveTableBytes;
  const bool room = header.table_size >= std::uint64_t{header.count} * kArchiveRecordBytes;
  return placed && room && fits(header.table_offset, header.table_size, archive_size);
}

std::optional<Header> read_header(const ArchiveBytes& source, std::string& error) {
  std::array<std::uint8_t, kArchiveHeaderBytes> bytes{};
  if (!source.read_at(0, bytes.size(), bytes.data())) return refuse("it is too short to be a game archive", error);
  if (!has_magic(bytes)) return refuse("it is not a game archive (its first bytes are not RGPAK's)", error);
  const std::uint64_t version = little_endian(&bytes[kVersionAt], kU32Bytes);
  if (version != kArchiveVersion) return refuse("its version " + std::to_string(version) + " is not known", error);
  const Header header = header_of(bytes);
  if (header.count > kMaxArchiveEntries) return refuse("it claims too many files", error);
  if (!table_in_bounds(header, source.size())) return refuse("its table is out of bounds", error);
  return header;
}

bool known_method(std::uint8_t method) {
  return method == static_cast<std::uint8_t>(ArchiveMethod::Stored) ||
         method == static_cast<std::uint8_t>(ArchiveMethod::Deflate);
}

void fill_entry(const std::uint8_t* record, ArchiveEntry& entry) {
  entry.method = static_cast<ArchiveMethod>(record[kMethodAt]);
  entry.crc = static_cast<std::uint32_t>(little_endian(record + kCrcAt, kU32Bytes));
  entry.offset = little_endian(record + kOffsetAt, kU64Bytes);
  entry.size = little_endian(record + kSizeAt, kU64Bytes);
  entry.stored_size = little_endian(record + kStoredSizeAt, kU64Bytes);
}

// The record at `at` in the table (moved past it). False, with `error` set, when it does not fit.
bool read_record(const std::vector<std::uint8_t>& table, std::size_t& at, ArchiveEntry& entry, std::string& error) {
  if (!fits(at, kArchiveRecordBytes, table.size())) return fail("its table ends inside a record", error);
  const std::uint8_t* record = table.data() + at;
  const auto name_bytes = static_cast<std::size_t>(little_endian(record + kNameLengthAt, kU16Bytes));
  if (record[kReservedAt] != kReserved || !known_method(record[kMethodAt])) {
    return fail("a record of its table has an unknown method", error);
  }
  fill_entry(record, entry);
  at += kArchiveRecordBytes;
  if (!fits(at, name_bytes, table.size())) return fail("its table ends inside a name", error);
  entry.name.assign(reinterpret_cast<const char*>(table.data() + at), name_bytes);
  at += name_bytes;
  return true;
}

// Whether an entry's data lies in the data part of the archive.
bool data_in_bounds(const ArchiveEntry& entry, std::uint64_t data_start, std::uint64_t archive_size) {
  return entry.offset >= data_start && fits(entry.offset, entry.stored_size, archive_size);
}

// Whether a deflated file's sizes agree: some data, never more than the file itself (a file
// deflate cannot shrink is stored), and no more than kMaxDeflateRatio bytes made of each byte.
bool deflated_sizes_agree(const ArchiveEntry& entry) {
  const bool shrunk = entry.stored_size > 0 && entry.stored_size <= entry.size;
  return shrunk && entry.size / kMaxDeflateRatio <= entry.stored_size;
}

// Whether an entry's sizes agree with its method: stored data is the file itself.
bool sizes_agree(const ArchiveEntry& entry) {
  if (entry.method == ArchiveMethod::Stored) return entry.stored_size == entry.size;
  return deflated_sizes_agree(entry);
}

bool sizes_bounded(const ArchiveEntry& entry) {
  return entry.size <= kMaxArchiveEntryBytes && entry.stored_size <= kMaxArchiveEntryBytes;
}

std::string quoted_name(const std::string& name) { return "'" + printable(name) + "'"; }

// What is wrong with an entry; empty when nothing is.
std::string entry_problem(const ArchiveEntry& entry, std::uint64_t data_start, std::uint64_t archive_size) {
  if (!safe_archive_name(entry.name)) return "the name " + quoted_name(entry.name) + " is not one a game may give";
  if (!sizes_bounded(entry)) return quoted_name(entry.name) + " is too large";
  if (!data_in_bounds(entry, data_start, archive_size))
    return "the data of " + quoted_name(entry.name) + " is out of bounds";
  if (!sizes_agree(entry)) return "the sizes of " + quoted_name(entry.name) + " do not agree";
  return {};
}

// A step of a name ("scripts" in "scripts/a.rb") that names nothing: empty or ".".
bool empty_or_current(std::string_view step) { return step.empty() || step == kCurrentStep; }

bool has_empty_or_current_step(std::string_view name) {
  std::size_t start = 0;
  for (;;) {
    const std::size_t end = name.find(kFolderSeparator, start);
    if (empty_or_current(name.substr(start, end - start))) return true;
    if (end == std::string_view::npos) return false;
    start = end + 1;
  }
}

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.compare(0, prefix.size(), prefix) == 0;
}

// How a folder's files start: "scripts/" (the archive's top folder: nothing).
std::string folder_prefix(const std::string& folder) { return folder.empty() ? folder : folder + kFolderSeparator; }

// The bytes of `packed` inflated (raw deflate) into exactly `size` bytes; nullopt when they are
// not exactly that.
std::optional<std::vector<std::uint8_t>> inflated(const std::vector<std::uint8_t>& packed, std::uint64_t size) {
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::uint8_t none = 0;  // (zlib wants somewhere to write, even nothing)
  z_stream stream{};
  if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return std::nullopt;
  stream.next_in = packed.data();
  stream.avail_in = static_cast<uInt>(packed.size());
  stream.next_out = bytes.empty() ? &none : bytes.data();
  stream.avail_out = static_cast<uInt>(bytes.size());
  const bool whole = inflate(&stream, Z_FINISH) == Z_STREAM_END && stream.avail_out == 0 && stream.avail_in == 0;
  inflateEnd(&stream);
  if (!whole) return std::nullopt;
  return bytes;
}

}  // namespace

bool safe_archive_name(std::string_view name) {
  return name.size() <= kMaxArchiveNameBytes && safe_asset_name(name) && !has_empty_or_current_step(name);
}

// (Every file of an archive is at most kMaxArchiveEntryBytes, which zlib's 32-bit count holds.)
std::uint32_t archive_crc32(const std::uint8_t* bytes, std::size_t size) {
  const uLong start = crc32(0L, Z_NULL, 0);
  return static_cast<std::uint32_t>(size == 0 ? start : crc32(start, bytes, static_cast<uInt>(size)));
}

std::optional<GameArchive> GameArchive::open(const std::filesystem::path& file, std::string& error) {
  std::error_code fs_error;
  const std::uintmax_t size = std::filesystem::file_size(file, fs_error);
  if (fs_error) return refuse("cannot read the game archive " + file.string() + ": " + fs_error.message(), error);
  auto archive = from_source(std::make_shared<FileBytes>(file, size), error);
  if (!archive) error = "the game archive " + file.string() + " is damaged: " + error;
  return archive;
}

std::optional<GameArchive> GameArchive::from_bytes(std::vector<std::uint8_t> bytes, std::string& error) {
  auto archive = from_source(std::make_shared<MemoryBytes>(std::move(bytes)), error);
  if (!archive) error = "the game archive is damaged: " + error;
  return archive;
}

std::optional<GameArchive> GameArchive::from_source(std::shared_ptr<const ArchiveBytes> source, std::string& error) {
  const auto header = read_header(*source, error);
  if (!header) return std::nullopt;
  std::vector<std::uint8_t> table(static_cast<std::size_t>(header->table_size));
  if (!source->read_at(header->table_offset, table.size(), table.data())) return refuse("cannot read its table", error);
  GameArchive archive(std::move(source));
  if (!archive.index(table, header->count, header->table_offset + header->table_size, error)) return std::nullopt;
  return archive;
}

bool GameArchive::index(const std::vector<std::uint8_t>& table, std::uint32_t count, std::uint64_t data_start,
                        std::string& error) {
  std::size_t at = 0;
  for (std::uint32_t i = 0; i < count; ++i) {
    ArchiveEntry entry;
    if (!read_record(table, at, entry, error) || !add(std::move(entry), data_start, error)) return false;
  }
  if (at != table.size()) return fail("its table has bytes no file uses", error);
  return no_file_named_as_folder(error);
}

bool GameArchive::add(ArchiveEntry entry, std::uint64_t data_start, std::string& error) {
  error = entry_problem(entry, data_start, source_->size());
  if (!error.empty()) return false;
  if (!by_name_.emplace(entry.name, entries_.size()).second)
    return fail(quoted_name(entry.name) + " is in it twice", error);
  entries_.push_back(std::move(entry));
  return true;
}

// (A file "a" beside a file "a/b" would make "a" both a file and a folder.)
bool GameArchive::no_file_named_as_folder(std::string& error) const {
  for (const ArchiveEntry& entry : entries_) {
    if (has_folder(entry.name)) return fail(quoted_name(entry.name) + " is both a file and a folder", error);
  }
  return true;
}

const ArchiveEntry* GameArchive::find(const std::string& name) const {
  const auto found = by_name_.find(name);
  return found == by_name_.end() ? nullptr : &entries_[found->second];
}

bool GameArchive::has_folder(const std::string& name) const {
  const std::string prefix = folder_prefix(name);
  const auto first = by_name_.lower_bound(prefix);
  return first != by_name_.end() && starts_with(first->first, prefix);
}

std::vector<std::string> GameArchive::files_in(const std::string& name) const {
  const std::string prefix = folder_prefix(name);
  std::vector<std::string> files;
  for (auto at = by_name_.lower_bound(prefix); at != by_name_.end() && starts_with(at->first, prefix); ++at) {
    const std::string rest = at->first.substr(prefix.size());
    if (rest.find(kFolderSeparator) == std::string::npos) files.push_back(rest);
  }
  return files;
}

std::optional<std::vector<std::uint8_t>> GameArchive::stored_data(const ArchiveEntry& entry, std::string& error) const {
  std::vector<std::uint8_t> stored(static_cast<std::size_t>(entry.stored_size));
  if (source_->read_at(entry.offset, stored.size(), stored.data())) return stored;
  return refuse("cannot read " + quoted_name(entry.name) + " from the game archive", error);
}

std::optional<std::vector<std::uint8_t>> GameArchive::read(const ArchiveEntry& entry, std::uint64_t max_bytes,
                                                           std::string& error) const {
  if (entry.size > max_bytes)
    return refuse(quoted_name(entry.name) + " is larger than " + std::to_string(max_bytes) + " bytes", error);
  auto bytes = stored_data(entry, error);
  if (!bytes) return std::nullopt;
  if (entry.method == ArchiveMethod::Deflate) bytes = inflated(*bytes, entry.size);
  if (!bytes) return refuse("the data of " + quoted_name(entry.name) + " in the game archive does not inflate", error);
  if (archive_crc32(bytes->data(), bytes->size()) != entry.crc) {
    return refuse(quoted_name(entry.name) + " in the game archive is damaged (its CRC32 does not match)", error);
  }
  return bytes;
}

}  // namespace redgem
