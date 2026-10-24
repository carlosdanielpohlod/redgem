#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/translations.hpp"
#include "project/icons/bytes.hpp"
#include "project/icons/exe_icon.hpp"

using namespace redgem;
using bytes::Bytes;
using bytes::read_little;
using bytes::write_little;

namespace {

// A small Windows program, as a linker lays one out, with one section holding its resources: an
// icon group of the sides given, each picture a few bytes saying its side. What the engine's
// player for Windows carries, without the code.
constexpr std::size_t kPeAt = 0x40;  // where the DOS header says the PE header is
constexpr std::size_t kCoffAt = kPeAt + 4;
constexpr std::size_t kOptionalAt = kCoffAt + 20;
constexpr std::size_t kOptionalBytes = 240;  // a 64-bit program's
constexpr std::size_t kSectionsAt = kOptionalAt + kOptionalBytes;
constexpr std::uint32_t kSectionAlignment = 0x1000;
constexpr std::uint32_t kFileAlignment = 0x200;
constexpr std::uint32_t kHeadersBytes = 0x400;
constexpr std::uint32_t kResourcesAddress = 0x1000;
constexpr std::uint32_t kIconType = 3;
constexpr std::uint32_t kGroupType = 14;
constexpr std::uint32_t kLanguage = 1033;  // US English
constexpr std::uint32_t kSubdirectory = 0x80000000;
constexpr std::size_t kDirectoryBytes = 16;
constexpr std::size_t kEntryBytes = 8;
constexpr std::size_t kDataEntryBytes = 16;
constexpr std::size_t kGroupEntryBytes = 14;
constexpr std::size_t kSectionHeaderBytes = 40;

void put(Bytes& out, std::size_t at, std::uint32_t value, std::size_t count) {
  if (out.size() < at + count) out.resize(at + count, 0);
  write_little(out, at, value, count);
}

// A resource directory at `at` with one entry for each id, pointing at `targets` (offsets from the root).
void directory(Bytes& rsrc, std::size_t at, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& entries) {
  put(rsrc, at + 14, static_cast<std::uint32_t>(entries.size()), 2);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    put(rsrc, at + kDirectoryBytes + i * kEntryBytes, entries[i].first, 4);
    put(rsrc, at + kDirectoryBytes + i * kEntryBytes + 4, entries[i].second, 4);
  }
}

Bytes picture_of(int side) { return Bytes(static_cast<std::size_t>(side % 7 + 3), static_cast<std::uint8_t>(side)); }

// The resource section: the root, the icon and group directories, a language directory for each,
// data entries, the group and the pictures.
Bytes resources(const std::vector<int>& sides) {
  const std::size_t count = sides.size();
  const std::size_t icons_dir = kDirectoryBytes + 2 * kEntryBytes;
  const std::size_t group_dir = icons_dir + kDirectoryBytes + count * kEntryBytes;
  const std::size_t languages = group_dir + kDirectoryBytes + kEntryBytes;
  const std::size_t data_entries = languages + (count + 1) * (kDirectoryBytes + kEntryBytes);
  std::size_t data = data_entries + (count + 1) * kDataEntryBytes;
  Bytes rsrc;
  directory(rsrc, 0, {{kIconType, kSubdirectory | icons_dir}, {kGroupType, kSubdirectory | group_dir}});
  std::vector<std::pair<std::uint32_t, std::uint32_t>> icon_entries;
  for (std::size_t i = 0; i <= count; ++i) {
    const std::size_t language = languages + i * (kDirectoryBytes + kEntryBytes);
    directory(rsrc, language, {{kLanguage, static_cast<std::uint32_t>(data_entries + i * kDataEntryBytes)}});
    if (i < count) icon_entries.push_back({static_cast<std::uint32_t>(i + 1), kSubdirectory | language});
  }
  directory(rsrc, icons_dir, icon_entries);
  directory(rsrc, group_dir, {{1, kSubdirectory | (languages + count * (kDirectoryBytes + kEntryBytes))}});
  // The group: a header, then an entry a picture.
  const std::size_t group = data;
  put(rsrc, group + 2, 1, 2);
  put(rsrc, group + 4, static_cast<std::uint32_t>(count), 2);
  data += 6 + count * kGroupEntryBytes;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = group + 6 + i * kGroupEntryBytes;
    const Bytes picture = picture_of(sides[i]);
    put(rsrc, entry, sides[i] >= 256 ? 0 : static_cast<std::uint32_t>(sides[i]), 1);
    put(rsrc, entry + 1, sides[i] >= 256 ? 0 : static_cast<std::uint32_t>(sides[i]), 1);
    put(rsrc, entry + 8, static_cast<std::uint32_t>(picture.size()), 4);
    put(rsrc, entry + 12, static_cast<std::uint32_t>(i + 1), 2);
    put(rsrc, data_entries + i * kDataEntryBytes, kResourcesAddress + static_cast<std::uint32_t>(data), 4);
    put(rsrc, data_entries + i * kDataEntryBytes + 4, static_cast<std::uint32_t>(picture.size()), 4);
    put(rsrc, data + picture.size() - 1, 0, 1);
    std::copy(picture.begin(), picture.end(), rsrc.begin() + static_cast<std::ptrdiff_t>(data));
    data += picture.size();
  }
  put(rsrc, data_entries + count * kDataEntryBytes, kResourcesAddress + static_cast<std::uint32_t>(group), 4);
  put(rsrc, data_entries + count * kDataEntryBytes + 4, static_cast<std::uint32_t>(6 + count * kGroupEntryBytes), 4);
  return rsrc;
}

std::uint32_t aligned(std::size_t value, std::uint32_t alignment) {
  return static_cast<std::uint32_t>((value + alignment - 1) / alignment * alignment);
}

// The program, its one section's raw data starting at `first_section` in the file.
Bytes program(const std::vector<int>& sides, std::uint32_t first_section = kHeadersBytes) {
  Bytes exe;
  put(exe, 0, 0x5A4D, 2);  // "MZ"
  put(exe, 0x3C, kPeAt, 4);
  put(exe, kPeAt, 0x00004550, 4);  // "PE\0\0"
  put(exe, kCoffAt, 0x8664, 2);    // x86-64
  put(exe, kCoffAt + 2, 1, 2);
  put(exe, kCoffAt + 16, kOptionalBytes, 2);
  put(exe, kOptionalAt, 0x20B, 2);  // a 64-bit program
  put(exe, kOptionalAt + 32, kSectionAlignment, 4);
  put(exe, kOptionalAt + 36, kFileAlignment, 4);
  put(exe, kOptionalAt + 60, kHeadersBytes, 4);
  put(exe, kOptionalAt + 64, 0x1234, 4);  // a checksum, which a patched program no longer has
  const Bytes rsrc = resources(sides);
  put(exe, kOptionalAt + 112 + 2 * 8, kResourcesAddress, 4);
  put(exe, kOptionalAt + 112 + 2 * 8 + 4, static_cast<std::uint32_t>(rsrc.size()), 4);
  put(exe, kOptionalAt + 56, aligned(kResourcesAddress + rsrc.size(), kSectionAlignment), 4);
  put(exe, kSectionsAt + 8, static_cast<std::uint32_t>(rsrc.size()), 4);
  put(exe, kSectionsAt + 12, kResourcesAddress, 4);
  put(exe, kSectionsAt + 16, aligned(rsrc.size(), kFileAlignment), 4);
  put(exe, kSectionsAt + 20, first_section, 4);
  exe.resize(first_section, 0);
  exe.insert(exe.end(), rsrc.begin(), rsrc.end());
  exe.resize(first_section + aligned(rsrc.size(), kFileAlignment), 0);
  return exe;
}

const std::vector<int> kPlayerSides = {256, 16, 48, 32};

IconSet new_icons() {
  IconSet icons;
  for (const int side : kPlayerSides) icons[side] = Bytes(static_cast<std::size_t>(side), 0xAB);
  return icons;
}

}  // namespace

TEST_CASE("the icon a Windows program carries is read from its resources") {
  const Bytes exe = program(kPlayerSides);
  CHECK(exe_icon_sides(exe) == std::vector<int>{16, 32, 48, 256});
  const IconSet icons = exe_icons(exe);
  REQUIRE(icons.size() == kPlayerSides.size());
  for (const int side : kPlayerSides) CHECK(icons.at(side) == picture_of(side));
}

TEST_CASE("a game's icon takes the place of the player's, in a section of its own") {
  Bytes exe = program(kPlayerSides);
  const Bytes before = exe;
  std::string error;
  REQUIRE_MESSAGE(replace_exe_icon(exe, new_icons(), error), error);
  CHECK(exe_icons(exe) == new_icons());
  CHECK(exe_icon_sides(exe) == std::vector<int>{16, 32, 48, 256});
  // One section more, the program bigger once loaded by it, and no checksum left to be wrong.
  CHECK(read_little(exe, kCoffAt + 2, 2) == 2);
  const std::size_t added = kSectionsAt + kSectionHeaderBytes;
  CHECK(std::string(exe.begin() + added, exe.begin() + added + 6) == ".icons");
  const std::uint32_t address = read_little(exe, added + 12, 4);
  CHECK(address % kSectionAlignment == 0);
  CHECK(address >= kResourcesAddress + kSectionAlignment);
  CHECK(read_little(exe, kOptionalAt + 56, 4) == aligned(address + read_little(exe, added + 8, 4), kSectionAlignment));
  CHECK(read_little(exe, kOptionalAt + 64, 4) == 0);
  CHECK(read_little(exe, added + 20, 4) == before.size());  // the pictures come after all there was
  CHECK(exe.size() % kFileAlignment == 0);
  // Nothing that was there moved: the player's own pictures are still where they were, unused.
  for (const int side : kPlayerSides) {
    const Bytes old = picture_of(side);
    CHECK(std::search(exe.begin(), exe.begin() + static_cast<std::ptrdiff_t>(before.size()), old.begin(), old.end()) !=
          exe.begin() + static_cast<std::ptrdiff_t>(before.size()));
  }
}

TEST_CASE("a program that cannot take the game's icon is left as it was, and the build says why") {
  std::string error;
  Bytes not_a_program(1024, 7);
  CHECK_FALSE(replace_exe_icon(not_a_program, new_icons(), error));
  CHECK(error == tr("project.icon.no_exe_icon"));
  CHECK(exe_icon_sides(not_a_program).empty());
  // No room for another section header before the first section starts.
  Bytes full = program(kPlayerSides, kSectionsAt + kSectionHeaderBytes);
  const Bytes before = full;
  CHECK_FALSE(replace_exe_icon(full, new_icons(), error));
  CHECK(error == tr("project.icon.no_room"));
  CHECK(full == before);
  // An icon missing a side the player has.
  Bytes exe = program(kPlayerSides);
  IconSet fewer = new_icons();
  fewer.erase(48);
  CHECK_FALSE(replace_exe_icon(exe, fewer, error));
  CHECK(error == tr("project.icon.missing_side"));
  // A file cut short anywhere is no program.
  Bytes cut = program(kPlayerSides);
  cut.resize(kSectionsAt);
  CHECK_FALSE(replace_exe_icon(cut, new_icons(), error));
}
