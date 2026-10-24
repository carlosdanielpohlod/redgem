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
constexpr std::size_t kDirectoryCount = 16;  // what linkers write
constexpr std::size_t kResources = 2;        // data directories, by their place
constexpr std::size_t kCertificates = 4;
constexpr std::size_t kBoundImports = 11;
constexpr std::size_t kCertificateBytes = 64;
constexpr std::size_t directory_at(std::size_t index) { return kOptionalAt + 112 + index * 8; }

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

// How the test's program is made.
struct Layout {
  std::vector<int> sides;
  std::vector<std::uint32_t> languages = {kLanguage};  // each picture is there in each of them
  std::size_t groups = 1;                              // icon groups, each naming every picture
  std::uint32_t first_section = kHeadersBytes;         // where its one section's data starts
  std::size_t directory_count = kDirectoryCount;       // NumberOfRvaAndSizes
  std::size_t optional_bytes = kOptionalBytes;         // SizeOfOptionalHeader
};

// The resource section, laid out as it is asked for, one piece after the other.
struct Resources {
  Bytes rsrc;
  std::vector<std::size_t> picture_entries;  // the data entries of the pictures, every language
  std::vector<std::size_t> groups;
  std::size_t take(std::size_t count) {
    const std::size_t at = rsrc.size();
    rsrc.resize(at + count, 0);
    return at;
  }
  std::size_t directory_of(std::size_t count) { return take(kDirectoryBytes + count * kEntryBytes); }
  // A data entry pointing at `count` bytes at `at` in the section.
  std::size_t data_entry(std::size_t at, std::size_t count) {
    const std::size_t entry = take(kDataEntryBytes);
    put(rsrc, entry, kResourcesAddress + static_cast<std::uint32_t>(at), 4);
    put(rsrc, entry + 4, static_cast<std::uint32_t>(count), 4);
    return entry;
  }
  // A language directory with a data entry for each language, all for the same `count` bytes at `at`.
  std::size_t languages(const std::vector<std::uint32_t>& names, std::size_t at, std::size_t count) {
    const std::size_t dir = directory_of(names.size());
    std::vector<std::pair<std::uint32_t, std::uint32_t>> entries;
    for (const std::uint32_t name : names) entries.push_back({name, static_cast<std::uint32_t>(data_entry(at, count))});
    directory(rsrc, dir, entries);
    last_entries.clear();
    for (const auto& entry : entries) last_entries.push_back(entry.second);
    return dir;
  }
  std::vector<std::size_t> last_entries;
  std::size_t picture(int side) {
    const Bytes bytes = picture_of(side);
    const std::size_t at = take(bytes.size());
    std::copy(bytes.begin(), bytes.end(), rsrc.begin() + static_cast<std::ptrdiff_t>(at));
    return at;
  }
  std::size_t group(const std::vector<int>& sides) {
    const std::size_t at = take(6 + sides.size() * kGroupEntryBytes);
    put(rsrc, at + 2, 1, 2);
    put(rsrc, at + 4, static_cast<std::uint32_t>(sides.size()), 2);
    for (std::size_t i = 0; i < sides.size(); ++i) {
      const std::size_t entry = at + 6 + i * kGroupEntryBytes;
      const std::uint32_t side = sides[i] >= 256 ? 0 : static_cast<std::uint32_t>(sides[i]);
      put(rsrc, entry, side, 1);
      put(rsrc, entry + 1, side, 1);
      put(rsrc, entry + 8, static_cast<std::uint32_t>(picture_of(sides[i]).size()), 4);
      put(rsrc, entry + 12, static_cast<std::uint32_t>(i + 1), 2);
    }
    return at;
  }
};

// The root, a directory of pictures and one of groups, a language directory for each, data
// entries, then the groups and the pictures.
Resources resources(const Layout& layout) {
  Resources made;
  const std::size_t root = made.directory_of(2);
  const std::size_t icons_dir = made.directory_of(layout.sides.size());
  const std::size_t groups_dir = made.directory_of(layout.groups);
  std::vector<std::pair<std::uint32_t, std::uint32_t>> icons, groups;
  for (std::size_t i = 0; i < layout.sides.size(); ++i) {
    const std::size_t picture = made.picture(layout.sides[i]);
    const std::size_t dir = made.languages(layout.languages, picture, picture_of(layout.sides[i]).size());
    icons.push_back({static_cast<std::uint32_t>(i + 1), kSubdirectory | static_cast<std::uint32_t>(dir)});
    made.picture_entries.insert(made.picture_entries.end(), made.last_entries.begin(), made.last_entries.end());
  }
  for (std::size_t g = 0; g < layout.groups; ++g) {
    const std::size_t group = made.group(layout.sides);
    made.groups.push_back(group);
    const std::size_t dir = made.languages({kLanguage}, group, 6 + layout.sides.size() * kGroupEntryBytes);
    groups.push_back({static_cast<std::uint32_t>(g + 1), kSubdirectory | static_cast<std::uint32_t>(dir)});
  }
  directory(made.rsrc, root,
            {{kIconType, kSubdirectory | static_cast<std::uint32_t>(icons_dir)},
             {kGroupType, kSubdirectory | static_cast<std::uint32_t>(groups_dir)}});
  directory(made.rsrc, icons_dir, icons);
  directory(made.rsrc, groups_dir, groups);
  return made;
}

std::uint32_t aligned(std::size_t value, std::uint32_t alignment) {
  return static_cast<std::uint32_t>((value + alignment - 1) / alignment * alignment);
}

void headers(Bytes& exe, const Layout& layout) {
  put(exe, 0, 0x5A4D, 2);  // "MZ"
  put(exe, 0x3C, kPeAt, 4);
  put(exe, kPeAt, 0x00004550, 4);  // "PE\0\0"
  put(exe, kCoffAt, 0x8664, 2);    // x86-64
  put(exe, kCoffAt + 2, 1, 2);
  put(exe, kCoffAt + 16, static_cast<std::uint32_t>(layout.optional_bytes), 2);
  put(exe, kOptionalAt, 0x20B, 2);  // a 64-bit program
  put(exe, kOptionalAt + 32, kSectionAlignment, 4);
  put(exe, kOptionalAt + 36, kFileAlignment, 4);
  put(exe, kOptionalAt + 60, kHeadersBytes, 4);
  put(exe, kOptionalAt + 64, 0x1234, 4);  // a checksum, which a patched program no longer has
  put(exe, kOptionalAt + 108, static_cast<std::uint32_t>(layout.directory_count), 4);
}

// The program, its one section holding its resources, and where in the file the data entries of
// its pictures and its groups are.
struct Program {
  Bytes exe;
  std::vector<std::size_t> picture_entries;
  std::vector<std::size_t> groups;
};

Program built(const Layout& layout) {
  Program made;
  Bytes& exe = made.exe;
  headers(exe, layout);
  const Resources laid = resources(layout);
  const Bytes& rsrc = laid.rsrc;
  for (const std::size_t entry : laid.picture_entries) made.picture_entries.push_back(layout.first_section + entry);
  for (const std::size_t group : laid.groups) made.groups.push_back(layout.first_section + group);
  put(exe, directory_at(kResources), kResourcesAddress, 4);
  put(exe, directory_at(kResources) + 4, static_cast<std::uint32_t>(rsrc.size()), 4);
  put(exe, kOptionalAt + 56, aligned(kResourcesAddress + rsrc.size(), kSectionAlignment), 4);
  put(exe, kSectionsAt + 8, static_cast<std::uint32_t>(rsrc.size()), 4);
  put(exe, kSectionsAt + 12, kResourcesAddress, 4);
  put(exe, kSectionsAt + 16, aligned(rsrc.size(), kFileAlignment), 4);
  put(exe, kSectionsAt + 20, layout.first_section, 4);
  exe.resize(layout.first_section, 0);
  exe.insert(exe.end(), rsrc.begin(), rsrc.end());
  exe.resize(layout.first_section + aligned(rsrc.size(), kFileAlignment), 0);
  return made;
}

Bytes program(const Layout& layout) { return built(layout).exe; }
Bytes program(const std::vector<int>& sides) { return program(Layout{sides}); }

// The same program signed: a certificate table at the end of the file, which its directory names.
Bytes signed_program(const std::vector<int>& sides) {
  Bytes exe = program(sides);
  const std::size_t at = exe.size();
  exe.insert(exe.end(), kCertificateBytes, 0x5A);
  put(exe, directory_at(kCertificates), static_cast<std::uint32_t>(at), 4);
  put(exe, directory_at(kCertificates) + 4, static_cast<std::uint32_t>(kCertificateBytes), 4);
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
  Layout no_room{kPlayerSides};
  no_room.first_section = kSectionsAt + kSectionHeaderBytes;
  Bytes full = program(no_room);
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

TEST_CASE("every language of every picture, and every icon group, take the game's icon") {
  Layout layout{kPlayerSides};
  layout.languages = {kLanguage, 1046, 1031};  // US English, Brazilian Portuguese, German
  layout.groups = 2;
  Program made = built(layout);
  std::string error;
  const IconSet icons = new_icons();
  REQUIRE_MESSAGE(replace_exe_icon(made.exe, icons, error), error);
  REQUIRE(made.picture_entries.size() == kPlayerSides.size() * layout.languages.size());
  const std::uint32_t section = read_little(made.exe, kSectionsAt + kSectionHeaderBytes + 12, 4);
  for (const std::size_t entry : made.picture_entries) {
    CHECK(read_little(made.exe, entry, 4) >= section);  // in the new section
    const std::uint32_t size = read_little(made.exe, entry + 4, 4);
    CHECK(std::any_of(icons.begin(), icons.end(), [size](const auto& icon) { return icon.second.size() == size; }));
  }
  for (const std::size_t group : made.groups) {
    for (std::size_t i = 0; i < kPlayerSides.size(); ++i) {
      CHECK(read_little(made.exe, group + 6 + i * kGroupEntryBytes + 8, 4) == icons.at(kPlayerSides[i]).size());
    }
  }
}

TEST_CASE("a signed player loses its signature with its icon: the game's executable is signed anew") {
  Bytes exe = signed_program(kPlayerSides);
  const std::size_t unsigned_size = exe.size() - kCertificateBytes;
  std::string error;
  REQUIRE_MESSAGE(replace_exe_icon(exe, new_icons(), error), error);
  CHECK(read_little(exe, directory_at(kCertificates), 4) == 0);
  CHECK(read_little(exe, directory_at(kCertificates) + 4, 4) == 0);
  // The new section starts where the signature was: none of it is left.
  CHECK(read_little(exe, kSectionsAt + kSectionHeaderBytes + 20, 4) == unsigned_size);
  CHECK(exe_icons(exe) == new_icons());
}

TEST_CASE("a section header goes only on bytes nothing else uses") {
  std::string error;
  CHECK(exe_has_room_for_icon(program(kPlayerSides)));
  // Something already after the last header (a linker's bound imports, say) is not written over.
  Bytes used = program(kPlayerSides);
  put(used, kSectionsAt + kSectionHeaderBytes + 3, 0x77, 1);
  CHECK_FALSE(exe_has_room_for_icon(used));
  const Bytes before = used;
  CHECK_FALSE(replace_exe_icon(used, new_icons(), error));
  CHECK(error == tr("project.icon.no_room"));
  CHECK(used == before);
  // Bound imports named by their directory there, even on zeros, are no room either.
  Bytes bound = program(kPlayerSides);
  put(bound, directory_at(kBoundImports), static_cast<std::uint32_t>(kSectionsAt + kSectionHeaderBytes + 8), 4);
  put(bound, directory_at(kBoundImports) + 4, 16, 4);
  CHECK_FALSE(exe_has_room_for_icon(bound));
}

TEST_CASE("a program whose optional header lacks the resource directory is no program to patch") {
  std::string error;
  Layout few{kPlayerSides};
  few.directory_count = 2;  // directories 0 and 1 only: no resources
  Bytes exe = program(few);
  CHECK(exe_icon_sides(exe).empty());
  CHECK_FALSE(replace_exe_icon(exe, new_icons(), error));
  Layout short_header{kPlayerSides};
  short_header.optional_bytes = 112 + 2 * 8;  // ends before directory 2
  CHECK(exe_icon_sides(program(short_header)).empty());
}
