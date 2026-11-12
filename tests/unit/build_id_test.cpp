// The build id of a program, read from little ELF and PE pictures made here (as a file has them,
// and as the system loads them), and from the programs this build made.

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/build_id.hpp"

using namespace redgem;

namespace {

// Bytes put at chosen places, little-endian, the rest zeros.
struct Picture {
  std::vector<unsigned char> bytes;

  template <typename Value>
  void put(std::size_t at, Value value) {
    if (bytes.size() < at + sizeof value) bytes.resize(at + sizeof value);
    std::memcpy(&bytes[at], &value, sizeof value);
  }
  void put_bytes(std::size_t at, const std::vector<unsigned char>& more) {
    for (std::size_t i = 0; i < more.size(); ++i) put(at + i, more[i]);
  }
  ByteReader reader() const {
    return [this](std::uint64_t at, void* into, std::size_t count) {
      if (at + count > bytes.size()) return false;
      std::memcpy(into, &bytes[at], count);
      return true;
    };
  }
};

const std::vector<unsigned char> kElfId = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
constexpr const char* kElfIdText = "0123456789abcdef";
constexpr std::uint64_t kElfBase = 0x400000;
constexpr std::size_t kElfNotesInFile = 0x200;
constexpr std::size_t kElfNotesLoaded = 0x1300;  // (where the notes are, counted from where the program loads)

// A note: its name's and its content's sizes, its type, the name ("GNU" and its zero), the content.
void put_note(Picture& picture, std::size_t at, std::uint32_t type, const std::vector<unsigned char>& content) {
  constexpr std::uint32_t kNameBytes = 4;
  picture.put<std::uint32_t>(at, kNameBytes);
  picture.put<std::uint32_t>(at + 4, static_cast<std::uint32_t>(content.size()));
  picture.put<std::uint32_t>(at + 8, type);
  picture.put_bytes(at + 12, {'G', 'N', 'U', 0});
  picture.put_bytes(at + 16, content);
}

// A 64-bit ELF with a loaded part at kElfBase and a part of notes: an ABI note, then the build id.
Picture elf_picture(std::size_t notes_at) {
  constexpr std::uint32_t kAbiNote = 1;
  constexpr std::uint32_t kBuildIdNote = 3;
  Picture picture;
  picture.put_bytes(0, {0x7f, 'E', 'L', 'F', 2, 1});
  picture.put<std::uint64_t>(32, 64);  // the program headers, at 64
  picture.put<std::uint16_t>(54, 56);  // each 56 bytes
  picture.put<std::uint16_t>(56, 2);   // two of them
  picture.put<std::uint32_t>(64, 1);   // PT_LOAD at offset 0
  picture.put<std::uint64_t>(64 + 16, kElfBase);
  picture.put<std::uint32_t>(120, 4);  // PT_NOTE
  picture.put<std::uint64_t>(120 + 8, kElfNotesInFile);
  picture.put<std::uint64_t>(120 + 16, kElfBase + kElfNotesLoaded);
  picture.put<std::uint64_t>(120 + 32, 16 + 16 + 16 + kElfId.size());
  put_note(picture, notes_at, kAbiNote, {0, 0, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0});
  put_note(picture, notes_at + 32, kBuildIdNote, kElfId);
  return picture;
}

const std::vector<unsigned char> kPeId = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
                                          0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0x01, 0x00, 0x00, 0x00};
constexpr const char* kPeIdText = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf01000000";
constexpr std::uint64_t kPeBase = 0x140000000;
constexpr std::uint32_t kDebugAddress = 0x2000;  // where the debug directory loads (in the one section)
constexpr std::uint32_t kCodeViewAddress = 0x2100;
constexpr std::uint32_t kSectionInFile = 0x400;  // where that section is in the file
constexpr std::uint32_t kCodeViewInFile = 0x500;

// The headers of a PE32+ program with one section, which holds its debug directory.
void put_pe_headers(Picture& picture) {
  constexpr std::size_t kPe = 0x80;
  constexpr std::size_t kOptional = kPe + 24;
  constexpr std::uint16_t kOptionalBytes = 240;
  constexpr std::size_t kSection = kOptional + kOptionalBytes;
  picture.put_bytes(0, {'M', 'Z'});
  picture.put<std::uint32_t>(0x3c, kPe);
  picture.put_bytes(kPe, {'P', 'E', 0, 0});
  picture.put<std::uint16_t>(kPe + 6, 1);
  picture.put<std::uint16_t>(kPe + 20, kOptionalBytes);
  picture.put<std::uint16_t>(kOptional, 0x20b);
  picture.put<std::uint64_t>(kOptional + 24, kPeBase);
  picture.put<std::uint32_t>(kOptional + 108, 16);  // directories
  picture.put<std::uint32_t>(kOptional + 112 + 6 * 8, kDebugAddress);
  picture.put<std::uint32_t>(kOptional + 112 + 6 * 8 + 4, 28);  // one entry
  picture.put<std::uint32_t>(kSection + 8, 0x1000);             // its size loaded
  picture.put<std::uint32_t>(kSection + 12, kDebugAddress);
  picture.put<std::uint32_t>(kSection + 16, 0x200);  // its size in the file
  picture.put<std::uint32_t>(kSection + 20, kSectionInFile);
}

// The debug directory's entry, at `at`, and the CodeView record it points at, at `record_at`.
void put_codeview(Picture& picture, std::size_t at, std::size_t record_at) {
  picture.put<std::uint32_t>(at + 12, 2);  // IMAGE_DEBUG_TYPE_CODEVIEW
  picture.put<std::uint32_t>(at + 16, 24);
  picture.put<std::uint32_t>(at + 20, kCodeViewAddress);
  picture.put<std::uint32_t>(at + 24, kCodeViewInFile);
  picture.put_bytes(record_at, {'R', 'S', 'D', 'S'});
  picture.put_bytes(record_at + 4, kPeId);
}

Picture pe_file_picture() {
  Picture picture;
  put_pe_headers(picture);
  put_codeview(picture, kSectionInFile, kCodeViewInFile);
  return picture;
}

Picture pe_loaded_picture() {
  Picture picture;
  put_pe_headers(picture);
  put_codeview(picture, kDebugAddress, kCodeViewAddress);
  return picture;
}

std::filesystem::path written(const Picture& picture, const std::string& name) {
  const std::filesystem::path file = std::filesystem::temp_directory_path() / name;
  std::ofstream(file, std::ios::binary)
      .write(reinterpret_cast<const char*>(picture.bytes.data()), static_cast<std::streamsize>(picture.bytes.size()));
  return file;
}

}  // namespace

TEST_CASE("an ELF program's build id is its GNU note, in its file and loaded") {
  const auto in_file = read_binary_identity(elf_picture(kElfNotesInFile).reader(), BinaryLayout::File);
  REQUIRE(in_file.has_value());
  CHECK(in_file->build_id == kElfIdText);
  CHECK(in_file->preferred_base == kElfBase);
  const auto loaded = read_binary_identity(elf_picture(kElfNotesLoaded).reader(), BinaryLayout::Loaded);
  REQUIRE(loaded.has_value());
  CHECK(loaded->build_id == kElfIdText);
}

TEST_CASE("a PE program's build id is its CodeView record, in its file and loaded") {
  const auto in_file = read_binary_identity(pe_file_picture().reader(), BinaryLayout::File);
  REQUIRE(in_file.has_value());
  CHECK(in_file->build_id == kPeIdText);
  CHECK(in_file->preferred_base == kPeBase);
  const auto loaded = read_binary_identity(pe_loaded_picture().reader(), BinaryLayout::Loaded);
  REQUIRE(loaded.has_value());
  CHECK(loaded->build_id == kPeIdText);
}

TEST_CASE("a program with no build id has none, and what is no program has no identity") {
  Picture plain = elf_picture(kElfNotesInFile);
  plain.put<std::uint32_t>(kElfNotesInFile + 32 + 8, 1);  // the build id note made another kind
  const auto unmarked = read_binary_identity(plain.reader(), BinaryLayout::File);
  REQUIRE(unmarked.has_value());
  CHECK(unmarked->build_id.empty());
  Picture text;
  text.put_bytes(0, {'R', 'e', 'd', 'G', 'e', 'm', ' ', 'c', 'r', 'a', 's', 'h'});
  CHECK_FALSE(read_binary_identity(text.reader(), BinaryLayout::File).has_value());
  CHECK_FALSE(read_binary_identity(Picture().reader(), BinaryLayout::File).has_value());
}

TEST_CASE("a program's file is read for its build id") {
  const std::filesystem::path elf = written(elf_picture(kElfNotesInFile), "redgem-build-id-test-elf");
  const std::filesystem::path pe = written(pe_file_picture(), "redgem-build-id-test-pe.exe");
  CHECK(binary_identity_of_file(elf)->build_id == kElfIdText);
  CHECK(binary_identity_of_file(pe)->build_id == kPeIdText);
  CHECK_FALSE(binary_identity_of_file("no-such-program").has_value());
  std::filesystem::remove(elf);
  std::filesystem::remove(pe);
}

#if !defined(__APPLE__)
TEST_CASE("the player this build made carries a build id, stripped of its debugging information or not") {
  const auto player = binary_identity_of_file(REDGEM_PLAYER_FILE);
  REQUIRE(player.has_value());
  CHECK_FALSE(player->build_id.empty());
}
#endif
