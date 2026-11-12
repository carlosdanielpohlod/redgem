// Writing a game archive: the header and table first (written last, once every file's place is
// known), then each file's data, compressed when that makes it smaller.

#include <zlib.h>

#include <fstream>
#include <iterator>
#include <set>
#include <utility>

#include "core/game_archive.hpp"
#include "core/game_archive_layout.hpp"
#include "core/safe_path.hpp"

namespace redgem {

using namespace archive_layout;

namespace {

constexpr int kDeflateMemoryLevel = 8;  // zlib's default

// A file's data as it goes into the archive.
struct Packed {
  std::vector<std::uint8_t> data;
  ArchiveMethod method = ArchiveMethod::Stored;
};

bool fail(const std::string& why, std::string& error) {
  error = why;
  return false;
}

// `bytes` as raw deflate; nullopt when zlib cannot.
std::optional<std::vector<std::uint8_t>> deflated(const std::vector<std::uint8_t>& bytes) {
  z_stream stream{};
  const int level = Z_DEFAULT_COMPRESSION;
  if (deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS, kDeflateMemoryLevel, Z_DEFAULT_STRATEGY) != Z_OK) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> packed(deflateBound(&stream, static_cast<uLong>(bytes.size())));
  stream.next_in = bytes.data();
  stream.avail_in = static_cast<uInt>(bytes.size());
  stream.next_out = packed.data();
  stream.avail_out = static_cast<uInt>(packed.size());
  const bool done = deflate(&stream, Z_FINISH) == Z_STREAM_END;
  packed.resize(stream.total_out);
  deflateEnd(&stream);
  if (!done) return std::nullopt;
  return packed;
}

// Whether a file is worth compressing: something to compress, when compression is asked for.
bool may_compress(const std::vector<std::uint8_t>& bytes, ArchiveCompression compression) {
  return compression == ArchiveCompression::Deflate && !bytes.empty();
}

// A file's data: deflated when that is smaller, else as it is.
Packed packed_for(std::vector<std::uint8_t> bytes, ArchiveCompression compression) {
  if (may_compress(bytes, compression)) {
    auto packed = deflated(bytes);
    if (packed && packed->size() < bytes.size()) return {std::move(*packed), ArchiveMethod::Deflate};
  }
  return {std::move(bytes), ArchiveMethod::Stored};
}

std::uint64_t table_bytes(const std::vector<ArchiveInput>& inputs) {
  std::uint64_t bytes = 0;
  for (const ArchiveInput& input : inputs) bytes += kArchiveRecordBytes + input.name.size();
  return bytes;
}

// Whether every name may go into an archive, once, and the table stays within its bounds.
bool names_fit(const std::vector<ArchiveInput>& inputs, std::string& error) {
  if (inputs.size() > kMaxArchiveEntries) return fail("a game archive holds at most so many files", error);
  if (table_bytes(inputs) > kMaxArchiveTableBytes) return fail("the names of the game's files are too long", error);
  std::set<std::string> names;
  for (const ArchiveInput& input : inputs) {
    if (!safe_archive_name(input.name))
      return fail("'" + printable(input.name) + "' cannot go into a game archive", error);
    if (!names.insert(input.name).second) return fail("'" + printable(input.name) + "' is given twice", error);
  }
  return true;
}

std::vector<std::uint8_t> header_bytes(std::uint32_t count, std::uint64_t table_size) {
  std::vector<std::uint8_t> header(std::begin(kArchiveMagic), std::end(kArchiveMagic));
  put_little_endian(header, kArchiveVersion, kU32Bytes);
  put_little_endian(header, count, kU32Bytes);
  put_little_endian(header, kArchiveHeaderBytes, kU64Bytes);
  put_little_endian(header, table_size, kU64Bytes);
  return header;
}

void put_record(std::vector<std::uint8_t>& table, const ArchiveEntry& entry) {
  put_little_endian(table, entry.name.size(), kU16Bytes);
  table.push_back(static_cast<std::uint8_t>(entry.method));
  table.push_back(kReserved);
  put_little_endian(table, entry.crc, kU32Bytes);
  put_little_endian(table, entry.offset, kU64Bytes);
  put_little_endian(table, entry.size, kU64Bytes);
  put_little_endian(table, entry.stored_size, kU64Bytes);
  table.insert(table.end(), entry.name.begin(), entry.name.end());
}

void write_bytes(std::ofstream& out, const std::vector<std::uint8_t>& bytes) {
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// An archive being written: the space of its header and table kept, its files' data appended.
class ArchiveWriter {
 public:
  ArchiveWriter(const std::filesystem::path& file, std::uint64_t table_size)
      : out_(file, std::ios::binary | std::ios::trunc), table_size_(table_size), at_(kArchiveHeaderBytes + table_size) {
    write_bytes(out_, std::vector<std::uint8_t>(static_cast<std::size_t>(at_)));
  }

  bool add(const ArchiveInput& input, ArchiveCompression compression, std::string& error) {
    auto bytes = input.bytes();
    if (!bytes) return fail("cannot read " + printable(input.name) + " (or it is too large)", error);
    ArchiveEntry entry{
        input.name, at_, bytes->size(), 0, ArchiveMethod::Stored, archive_crc32(bytes->data(), bytes->size())};
    const Packed packed = packed_for(std::move(*bytes), compression);
    entry.method = packed.method;
    entry.stored_size = packed.data.size();
    write_bytes(out_, packed.data);
    at_ += entry.stored_size;
    entries_.push_back(std::move(entry));
    return static_cast<bool>(out_) || fail("cannot write into the game archive", error);
  }

  bool finish() {
    std::vector<std::uint8_t> head = header_bytes(static_cast<std::uint32_t>(entries_.size()), table_size_);
    for (const ArchiveEntry& entry : entries_) put_record(head, entry);
    out_.seekp(0);
    write_bytes(out_, head);
    out_.close();
    return !out_.fail();
  }

 private:
  std::ofstream out_;
  std::uint64_t table_size_;
  std::uint64_t at_;
  std::vector<ArchiveEntry> entries_;
};

bool write_all(const std::filesystem::path& file, const std::vector<ArchiveInput>& inputs,
               ArchiveCompression compression, std::string& error) {
  ArchiveWriter writer(file, table_bytes(inputs));
  for (const ArchiveInput& input : inputs) {
    if (!writer.add(input, compression, error)) return false;
  }
  return writer.finish() || fail("cannot write the game archive " + file.string(), error);
}

}  // namespace

ArchiveInput archive_input_from_file(const std::string& name, const std::filesystem::path& file) {
  return {name, [file]() { return read_file_up_to(file, kMaxArchiveEntryBytes); }};
}

ArchiveInput archive_input_from_bytes(const std::string& name, std::vector<std::uint8_t> bytes) {
  return {name, [bytes = std::move(bytes)]() { return std::optional<std::vector<std::uint8_t>>(bytes); }};
}

bool write_game_archive(const std::filesystem::path& file, const std::vector<ArchiveInput>& inputs,
                        ArchiveCompression compression, std::string& error) {
  if (!names_fit(inputs, error)) return false;
  if (write_all(file, inputs, compression, error)) return true;
  std::error_code fs_error;
  std::filesystem::remove(file, fs_error);  // (half an archive is no archive)
  return false;
}

}  // namespace redgem
