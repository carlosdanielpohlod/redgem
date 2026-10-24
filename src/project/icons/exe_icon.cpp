// The icon inside a Windows program (a PE file): read, and replaced by a game's.

#include "project/icons/exe_icon.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include "core/translations.hpp"

namespace redgem {
namespace {

using bytes::Bytes;
using bytes::inside;
using bytes::read_little;
using bytes::write_little;

constexpr std::size_t kShort = 2;
constexpr std::size_t kWord = 4;

// Where a PE file keeps what is read and written here (Microsoft's "PE Format").
constexpr std::size_t kPeHeaderPointerAt = 0x3C;    // in the DOS header: where the PE header is
constexpr std::uint32_t kPeSignature = 0x00004550;  // "PE\0\0"
constexpr std::size_t kCoffHeaderBytes = 20;
constexpr std::size_t kSectionCountAt = 2;  // in the COFF header
constexpr std::size_t kOptionalHeaderSizeAt = 16;
constexpr std::uint32_t kPe32Magic = 0x10B;      // a 32-bit program's optional header
constexpr std::uint32_t kPe32PlusMagic = 0x20B;  // a 64-bit program's
// In the optional header.
constexpr std::size_t kInitializedDataAt = 8;
constexpr std::size_t kSectionAlignmentAt = 32;
constexpr std::size_t kFileAlignmentAt = 36;
constexpr std::size_t kImageSizeAt = 56;
constexpr std::size_t kHeadersSizeAt = 60;
constexpr std::size_t kChecksumAt = 64;
constexpr std::size_t kPe32DirectoriesAt = 96;
constexpr std::size_t kPe32PlusDirectoriesAt = 112;
constexpr std::size_t kDirectoryBytes = 8;
constexpr std::size_t kResourceDirectory = 2;  // which data directory says where the resources are
// A section's header.
constexpr std::size_t kSectionHeaderBytes = 40;
constexpr std::size_t kSectionNameBytes = 8;
constexpr std::size_t kVirtualSizeAt = 8;
constexpr std::size_t kVirtualAddressAt = 12;
constexpr std::size_t kRawSizeAt = 16;
constexpr std::size_t kRawOffsetAt = 20;
constexpr std::size_t kCharacteristicsAt = 36;
constexpr const char* kIconSectionName = ".icons";
constexpr std::uint32_t kReadOnlyData = 0x40000040;  // initialized data, readable
// The resource tree: directories of entries (type, then id, then language) down to data entries.
constexpr std::size_t kNamedCountAt = 12;
constexpr std::size_t kIdCountAt = 14;
constexpr std::size_t kResourceDirectoryBytes = 16;
constexpr std::size_t kResourceEntryBytes = 8;
constexpr std::size_t kDataEntryBytes = 16;  // the data's address, its size, a code page, reserved
constexpr std::uint32_t kSubdirectoryBit = 0x80000000;
constexpr std::uint32_t kIconType = 3;        // RT_ICON: one picture
constexpr std::uint32_t kIconGroupType = 14;  // RT_GROUP_ICON: which pictures make one icon
// An icon group: a header, then an entry for each picture.
constexpr std::size_t kGroupHeaderBytes = 6;
constexpr std::size_t kGroupCountAt = 4;
constexpr std::size_t kGroupEntryBytes = 14;
constexpr std::size_t kGroupHeightAt = 1;
constexpr std::size_t kGroupColorsAt = 2;
constexpr std::size_t kGroupPlanesAt = 4;
constexpr std::size_t kGroupBitsAt = 6;
constexpr std::size_t kGroupBytesAt = 8;
constexpr std::size_t kGroupIdAt = 12;
constexpr int kZeroSide = 256;        // the side a group writes as 0: it does not fit a byte
constexpr std::uint32_t kPlanes = 1;  // what a PNG picture's entry says
constexpr std::uint32_t kBitsPerPixel = 32;
constexpr std::size_t kPictureAlignment = 8;

struct Section {
  std::uint32_t virtual_size = 0;
  std::uint32_t virtual_address = 0;
  std::uint32_t raw_size = 0;
  std::uint32_t raw_offset = 0;
};

// What is known of a PE file once its headers are read: offsets are into the file.
struct PeFile {
  std::size_t coff = 0;
  std::size_t optional = 0;
  std::size_t sections_at = 0;
  std::uint32_t section_alignment = 0;
  std::uint32_t file_alignment = 0;
  std::uint32_t headers_size = 0;
  std::uint32_t resources = 0;  // the address of the resource tree; 0 for none
  std::vector<Section> sections;
};

// One picture of an icon group: where its entry in the group is, its side and its id.
struct GroupPicture {
  std::size_t entry = 0;
  int side = 0;
  std::uint32_t id = 0;
};

struct IconGroup {
  std::size_t root = 0;  // where the resource tree starts in the file
  std::vector<GroupPicture> pictures;
};

std::uint32_t aligned(std::size_t value, std::uint32_t alignment) {
  return static_cast<std::uint32_t>((value + alignment - 1) / alignment * alignment);
}

bool has_pe_signature(const Bytes& exe, std::size_t at) {
  return inside(exe.size(), at, kWord + kCoffHeaderBytes) && read_little(exe, at, kWord) == kPeSignature;
}

// Where the optional header's data directories start; 0 for a header of no kind known.
std::size_t directories_at(std::uint32_t magic) {
  if (magic == kPe32PlusMagic) return kPe32PlusDirectoriesAt;
  return magic == kPe32Magic ? kPe32DirectoriesAt : 0;
}

// Whether the optional header is of a known kind, with its resource directory inside the file.
bool known_header(std::size_t directories, const Bytes& exe, std::size_t resource) {
  return directories != 0 && inside(exe.size(), resource, kDirectoryBytes);
}

bool usable_alignments(const PeFile& pe) { return pe.section_alignment != 0 && pe.file_alignment != 0; }

bool read_optional_header(const Bytes& exe, PeFile& pe) {
  if (!inside(exe.size(), pe.optional, kShort)) return false;
  const std::size_t directories = directories_at(read_little(exe, pe.optional, kShort));
  const std::size_t resource = pe.optional + directories + kResourceDirectory * kDirectoryBytes;
  if (!known_header(directories, exe, resource)) return false;
  pe.section_alignment = read_little(exe, pe.optional + kSectionAlignmentAt, kWord);
  pe.file_alignment = read_little(exe, pe.optional + kFileAlignmentAt, kWord);
  pe.headers_size = read_little(exe, pe.optional + kHeadersSizeAt, kWord);
  pe.resources = read_little(exe, resource, kWord);
  return usable_alignments(pe);
}

bool read_sections(const Bytes& exe, PeFile& pe) {
  const std::size_t count = read_little(exe, pe.coff + kSectionCountAt, kShort);
  if (!inside(exe.size(), pe.sections_at, count * kSectionHeaderBytes)) return false;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t at = pe.sections_at + i * kSectionHeaderBytes;
    pe.sections.push_back({read_little(exe, at + kVirtualSizeAt, kWord),
                           read_little(exe, at + kVirtualAddressAt, kWord), read_little(exe, at + kRawSizeAt, kWord),
                           read_little(exe, at + kRawOffsetAt, kWord)});
  }
  return true;
}

std::optional<PeFile> pe_file(const Bytes& exe) {
  if (!inside(exe.size(), kPeHeaderPointerAt, kWord)) return std::nullopt;
  const std::size_t signature = read_little(exe, kPeHeaderPointerAt, kWord);
  if (!has_pe_signature(exe, signature)) return std::nullopt;
  PeFile pe;
  pe.coff = signature + kWord;
  pe.optional = pe.coff + kCoffHeaderBytes;
  pe.sections_at = pe.optional + read_little(exe, pe.coff + kOptionalHeaderSizeAt, kShort);
  if (!read_optional_header(exe, pe) || !read_sections(exe, pe)) return std::nullopt;
  return pe;
}

bool holds(const Section& section, std::uint32_t address, std::size_t count) {
  return address >= section.virtual_address && inside(section.raw_size, address - section.virtual_address, count);
}

// Where in the file `count` bytes at `address` (once loaded) are; nullopt when no section has them.
std::optional<std::size_t> file_offset(const PeFile& pe, std::uint32_t address, std::size_t count) {
  for (const Section& section : pe.sections) {
    if (holds(section, address, count)) return std::size_t{section.raw_offset} + (address - section.virtual_address);
  }
  return std::nullopt;
}

// What the entry of the resource directory at `dir` with `id` (the first one, with none) points
// at, from the root of the tree, and whether that is another directory.
struct Target {
  std::size_t at = 0;
  bool directory = false;
};

bool names(std::uint32_t entry_name, const std::optional<std::uint32_t>& id) { return !id || entry_name == *id; }

std::optional<Target> entry_in(const Bytes& exe, std::size_t root, std::size_t dir, std::optional<std::uint32_t> id) {
  if (!inside(exe.size(), dir, kResourceDirectoryBytes)) return std::nullopt;
  const std::size_t count = read_little(exe, dir + kNamedCountAt, kShort) + read_little(exe, dir + kIdCountAt, kShort);
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = dir + kResourceDirectoryBytes + i * kResourceEntryBytes;
    if (!inside(exe.size(), entry, kResourceEntryBytes)) return std::nullopt;
    if (!names(read_little(exe, entry, kWord), id)) continue;
    const std::uint32_t points_at = read_little(exe, entry + kWord, kWord);
    return Target{root + (points_at & ~kSubdirectoryBit), (points_at & kSubdirectoryBit) != 0};
  }
  return std::nullopt;
}

std::optional<std::size_t> subdirectory(const Bytes& exe, std::size_t root, std::size_t dir,
                                        std::optional<std::uint32_t> id) {
  const auto target = entry_in(exe, root, dir, id);
  if (!target || !target->directory) return std::nullopt;
  return target->at;
}

bool is_data_entry(const Bytes& exe, const Target& target) {
  return !target.directory && inside(exe.size(), target.at, kDataEntryBytes);
}

// Where the data entry of the resource of `type` and `id` (the first, with none) is in the file:
// its first language's.
std::optional<std::size_t> data_entry(const Bytes& exe, std::size_t root, std::uint32_t type,
                                      std::optional<std::uint32_t> id) {
  const auto of_type = subdirectory(exe, root, root, type);
  const auto named = of_type ? subdirectory(exe, root, *of_type, id) : std::nullopt;
  const auto language = named ? entry_in(exe, root, *named, std::nullopt) : std::nullopt;
  if (!language || !is_data_entry(exe, *language)) return std::nullopt;
  return language->at;
}

// Where the data a data entry points at is in the file.
std::optional<std::size_t> data_of(const Bytes& exe, const PeFile& pe, std::size_t entry) {
  return file_offset(pe, read_little(exe, entry, kWord), read_little(exe, entry + kWord, kWord));
}

std::vector<GroupPicture> group_pictures(const Bytes& exe, std::size_t group) {
  std::vector<GroupPicture> pictures;
  const std::size_t count = read_little(exe, group + kGroupCountAt, kShort);
  if (!inside(exe.size(), group, kGroupHeaderBytes + count * kGroupEntryBytes)) return pictures;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = group + kGroupHeaderBytes + i * kGroupEntryBytes;
    const int side = exe[entry] == 0 ? kZeroSide : exe[entry];
    pictures.push_back({entry, side, read_little(exe, entry + kGroupIdAt, kShort)});
  }
  return pictures;
}

// The program's first icon group; nullopt when it has none.
std::optional<IconGroup> icon_group(const Bytes& exe, const PeFile& pe) {
  const auto root = file_offset(pe, pe.resources, kResourceDirectoryBytes);
  const auto entry = root ? data_entry(exe, *root, kIconGroupType, std::nullopt) : std::nullopt;
  const auto group = entry ? data_of(exe, pe, *entry) : std::nullopt;
  if (!group || !inside(exe.size(), *group, kGroupHeaderBytes)) return std::nullopt;
  IconGroup found{*root, group_pictures(exe, *group)};
  if (found.pictures.empty()) return std::nullopt;
  return found;
}

std::optional<IconGroup> icon_group_of(const Bytes& exe) {
  const auto pe = pe_file(exe);
  return pe ? icon_group(exe, *pe) : std::nullopt;
}

// The data entry of each picture of the group, in its order; nullopt when one has none.
std::optional<std::vector<std::size_t>> picture_entries(const Bytes& exe, const IconGroup& group) {
  std::vector<std::size_t> entries;
  for (const GroupPicture& picture : group.pictures) {
    const auto entry = data_entry(exe, group.root, kIconType, picture.id);
    if (!entry) return std::nullopt;
    entries.push_back(*entry);
  }
  return entries;
}

bool has_every_side(const IconGroup& group, const IconSet& icons) {
  return std::all_of(group.pictures.begin(), group.pictures.end(),
                     [&icons](const GroupPicture& picture) { return icons.count(picture.side) != 0; });
}

// The pictures of `icons` the group asks for, one after the other, and where each starts.
struct Pictures {
  Bytes data;
  std::vector<std::size_t> starts;
};

Pictures pictures_for(const IconGroup& group, const IconSet& icons) {
  Pictures pictures;
  for (const GroupPicture& picture : group.pictures) {
    pictures.data.resize(aligned(pictures.data.size(), kPictureAlignment), 0);
    pictures.starts.push_back(pictures.data.size());
    const Bytes& png = icons.at(picture.side);
    pictures.data.insert(pictures.data.end(), png.begin(), png.end());
  }
  return pictures;
}

std::uint32_t first_raw_offset(const PeFile& pe) {
  std::uint32_t first = pe.headers_size;
  for (const Section& section : pe.sections) {
    if (section.raw_offset != 0) first = std::min(first, section.raw_offset);
  }
  return first;
}

// Whether one more section header fits between the last one and what follows the headers.
bool room_for_section(const PeFile& pe, std::size_t header) {
  return header + kSectionHeaderBytes <= first_raw_offset(pe);
}

// Where the program ends once loaded: the next section starts after it.
std::uint32_t loaded_end(const PeFile& pe) {
  std::uint32_t end = 0;
  for (const Section& section : pe.sections) {
    end = std::max(end, section.virtual_address + std::max(section.virtual_size, section.raw_size));
  }
  return end;
}

void write_section_header(Bytes& exe, std::size_t header, const Section& section) {
  std::fill(exe.begin() + static_cast<std::ptrdiff_t>(header),
            exe.begin() + static_cast<std::ptrdiff_t>(header + kSectionHeaderBytes), 0);
  std::memcpy(&exe[header], kIconSectionName, std::min(std::strlen(kIconSectionName), kSectionNameBytes));
  write_little(exe, header + kVirtualSizeAt, section.virtual_size, kWord);
  write_little(exe, header + kVirtualAddressAt, section.virtual_address, kWord);
  write_little(exe, header + kRawSizeAt, section.raw_size, kWord);
  write_little(exe, header + kRawOffsetAt, section.raw_offset, kWord);
  write_little(exe, header + kCharacteristicsAt, kReadOnlyData, kWord);
}

// The headers count the new section: one more of them, a bigger program once loaded, no checksum
// (Windows checks none for programs).
void count_section(Bytes& exe, const PeFile& pe, const Section& section) {
  write_little(exe, pe.coff + kSectionCountAt, static_cast<std::uint32_t>(pe.sections.size() + 1), kShort);
  const std::uint32_t data = read_little(exe, pe.optional + kInitializedDataAt, kWord);
  write_little(exe, pe.optional + kInitializedDataAt, data + section.raw_size, kWord);
  write_little(exe, pe.optional + kImageSizeAt,
               aligned(section.virtual_address + section.virtual_size, pe.section_alignment), kWord);
  write_little(exe, pe.optional + kChecksumAt, 0, kWord);
}

// Adds a section holding `data` at the end of the program; its address once loaded, or nullopt
// when the headers have no room for it.
std::optional<std::uint32_t> add_section(Bytes& exe, const PeFile& pe, const Bytes& data) {
  const std::size_t header = pe.sections_at + pe.sections.size() * kSectionHeaderBytes;
  if (!room_for_section(pe, header)) return std::nullopt;
  const Section section{static_cast<std::uint32_t>(data.size()), aligned(loaded_end(pe), pe.section_alignment),
                        aligned(data.size(), pe.file_alignment), aligned(exe.size(), pe.file_alignment)};
  write_section_header(exe, header, section);
  exe.resize(section.raw_offset, 0);
  exe.insert(exe.end(), data.begin(), data.end());
  exe.resize(std::size_t{section.raw_offset} + section.raw_size, 0);
  count_section(exe, pe, section);
  return section.virtual_address;
}

void describe_picture(Bytes& exe, const GroupPicture& picture, std::size_t size) {
  const std::uint32_t side = picture.side >= kZeroSide ? 0 : static_cast<std::uint32_t>(picture.side);
  write_little(exe, picture.entry, side, 1);
  write_little(exe, picture.entry + kGroupHeightAt, side, 1);
  write_little(exe, picture.entry + kGroupColorsAt, 0, kShort);  // no palette, and reserved
  write_little(exe, picture.entry + kGroupPlanesAt, kPlanes, kShort);
  write_little(exe, picture.entry + kGroupBitsAt, kBitsPerPixel, kShort);
  write_little(exe, picture.entry + kGroupBytesAt, static_cast<std::uint32_t>(size), kWord);
}

// Each picture's data entry points at its new picture, and the group says what that picture is.
void point_at_pictures(Bytes& exe, const IconGroup& group, const std::vector<std::size_t>& entries,
                       const Pictures& pictures, const IconSet& icons, std::uint32_t section) {
  for (std::size_t i = 0; i < group.pictures.size(); ++i) {
    const std::size_t size = icons.at(group.pictures[i].side).size();
    write_little(exe, entries[i], section + static_cast<std::uint32_t>(pictures.starts[i]), kWord);
    write_little(exe, entries[i] + kWord, static_cast<std::uint32_t>(size), kWord);
    describe_picture(exe, group.pictures[i], size);
  }
}

bool fail(std::string& error, const char* text) {
  error = tr(text);
  return false;
}

}  // namespace

std::vector<int> exe_icon_sides(const bytes::Bytes& exe) {
  std::vector<int> sides;
  if (const auto group = icon_group_of(exe)) {
    for (const GroupPicture& picture : group->pictures) sides.push_back(picture.side);
  }
  std::sort(sides.begin(), sides.end());
  return sides;
}

bool replace_exe_icon(bytes::Bytes& exe, const IconSet& icons, std::string& error) {
  const auto pe = pe_file(exe);
  const auto group = pe ? icon_group(exe, *pe) : std::nullopt;
  const auto entries = group ? picture_entries(exe, *group) : std::nullopt;
  if (!entries) return fail(error, "project.icon.no_exe_icon");
  if (!has_every_side(*group, icons)) return fail(error, "project.icon.missing_side");
  const Pictures pictures = pictures_for(*group, icons);
  Bytes patched = exe;
  const auto section = add_section(patched, *pe, pictures.data);
  if (!section) return fail(error, "project.icon.no_room");
  point_at_pictures(patched, *group, *entries, pictures, icons, *section);
  exe = std::move(patched);
  return true;
}

IconSet exe_icons(const bytes::Bytes& exe) {
  IconSet icons;
  const auto pe = pe_file(exe);
  const auto group = pe ? icon_group(exe, *pe) : std::nullopt;
  const auto entries = group ? picture_entries(exe, *group) : std::nullopt;
  for (std::size_t i = 0; entries && i < entries->size(); ++i) {
    const auto data = data_of(exe, *pe, (*entries)[i]);
    const std::size_t size = read_little(exe, (*entries)[i] + kWord, kWord);
    if (data) icons[group->pictures[i].side] = Bytes(exe.begin() + *data, exe.begin() + *data + size);
  }
  return icons;
}

}  // namespace redgem
