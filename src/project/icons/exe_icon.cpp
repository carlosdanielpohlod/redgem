// The icon inside a Windows program (a PE file): read, and replaced by a game's.

#include "project/icons/exe_icon.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <optional>
#include <set>
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
constexpr std::size_t kDirectoryCountBytes = 4;  // NumberOfRvaAndSizes, just before the directories
// The data directories read and written here, by their place in the list.
constexpr std::size_t kResourceDirectory = 2;       // the resources: an address once loaded
constexpr std::size_t kCertificateDirectory = 4;    // the signature: an offset in the file
constexpr std::size_t kBoundImportDirectory = 11;   // bound imports: kept among the headers
constexpr std::uint32_t kCertificateAlignment = 8;  // what the signature at the end is padded to
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
  std::size_t file_size = 0;
  std::size_t coff = 0;
  std::size_t optional = 0;
  std::size_t directories = 0;  // where the data directories start
  std::size_t directory_count = 0;
  std::size_t sections_at = 0;
  std::uint32_t section_alignment = 0;
  std::uint32_t file_alignment = 0;
  std::uint32_t headers_size = 0;
  std::vector<Section> sections;
};

// A data directory: where something is (an address or a file offset, by directory) and its size.
struct Directory {
  std::uint32_t at = 0;
  std::uint32_t size = 0;
};

// One picture of an icon group: where its entry in the group is, its side and its id.
struct GroupPicture {
  std::size_t entry = 0;
  int side = 0;
  std::uint32_t id = 0;
};

// One variant of a resource (one language of it): its id, and where its data entry is.
struct Leaf {
  std::uint32_t id = 0;
  std::size_t entry = 0;
};

// The program's icon: the pictures of all its icon groups, and every variant of every picture
// they name.
struct ExeIcon {
  std::vector<GroupPicture> pictures;
  std::vector<Leaf> leaves;
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

// Whether data directory `index` is there: counted by the header, inside the optional header.
bool has_directory(const PeFile& pe, std::size_t index, std::size_t optional_size) {
  const std::size_t end = pe.directories - pe.optional + (index + 1) * kDirectoryBytes;
  return index < pe.directory_count && end <= optional_size;
}

Directory directory(const Bytes& exe, const PeFile& pe, std::size_t index) {
  const std::size_t at = pe.directories + index * kDirectoryBytes;
  if (index >= pe.directory_count || !inside(exe.size(), at, kDirectoryBytes)) return {};
  return {read_little(exe, at, kWord), read_little(exe, at + kWord, kWord)};
}

bool usable_alignments(const PeFile& pe) { return pe.section_alignment != 0 && pe.file_alignment != 0; }

// Where the data directories start in an optional header of a known kind; 0 for any other.
std::size_t directories_in(const Bytes& exe, std::size_t optional) {
  if (!inside(exe.size(), optional, kShort)) return 0;
  return directories_at(read_little(exe, optional, kShort));
}

// Whether the optional header is of a known kind and inside the file, with its directories.
bool readable_optional_header(const Bytes& exe, const PeFile& pe, std::size_t directories, std::size_t size) {
  return directories != 0 && directories <= size && inside(exe.size(), pe.optional, size);
}

// The headers of a known kind, whose optional header counts and holds the resource directory.
bool read_optional_header(const Bytes& exe, PeFile& pe) {
  const std::size_t optional_size = read_little(exe, pe.coff + kOptionalHeaderSizeAt, kShort);
  const std::size_t directories = directories_in(exe, pe.optional);
  if (!readable_optional_header(exe, pe, directories, optional_size)) return false;
  pe.directories = pe.optional + directories;
  pe.directory_count = read_little(exe, pe.directories - kDirectoryCountBytes, kDirectoryCountBytes);
  pe.section_alignment = read_little(exe, pe.optional + kSectionAlignmentAt, kWord);
  pe.file_alignment = read_little(exe, pe.optional + kFileAlignmentAt, kWord);
  pe.headers_size = read_little(exe, pe.optional + kHeadersSizeAt, kWord);
  return has_directory(pe, kResourceDirectory, optional_size) && usable_alignments(pe);
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
  pe.file_size = exe.size();
  pe.coff = signature + kWord;
  pe.optional = pe.coff + kCoffHeaderBytes;
  pe.sections_at = pe.optional + read_little(exe, pe.coff + kOptionalHeaderSizeAt, kShort);
  const bool readable = read_optional_header(exe, pe) && read_sections(exe, pe);
  if (!readable) return std::nullopt;
  return pe;
}

bool holds(const Section& section, std::uint32_t address, std::size_t count) {
  return address >= section.virtual_address && inside(section.raw_size, address - section.virtual_address, count);
}

// Where in the file `count` bytes at `address` (once loaded) are; nullopt when no section has
// them, or the one that says it does reaches past the end of the file.
std::optional<std::size_t> file_offset(const PeFile& pe, std::uint32_t address, std::size_t count) {
  for (const Section& section : pe.sections) {
    if (!holds(section, address, count)) continue;
    const std::size_t at = std::size_t{section.raw_offset} + (address - section.virtual_address);
    if (inside(pe.file_size, at, count)) return at;
  }
  return std::nullopt;
}

// An entry of a resource directory: its name (an id), what it points at (from the root of the
// tree) and whether that is another directory.
struct Entry {
  std::uint32_t name = 0;
  std::size_t at = 0;
  bool directory = false;
};

std::size_t entry_count(const Bytes& exe, std::size_t dir) {
  return read_little(exe, dir + kNamedCountAt, kShort) + read_little(exe, dir + kIdCountAt, kShort);
}

// The entries of the resource directory at `dir`; none for one that is not inside the file.
std::vector<Entry> entries_of(const Bytes& exe, std::size_t root, std::size_t dir) {
  std::vector<Entry> entries;
  const std::size_t count = inside(exe.size(), dir, kResourceDirectoryBytes) ? entry_count(exe, dir) : 0;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = dir + kResourceDirectoryBytes + i * kResourceEntryBytes;
    if (!inside(exe.size(), entry, kResourceEntryBytes)) break;
    const std::uint32_t points_at = read_little(exe, entry + kWord, kWord);
    entries.push_back(
        {read_little(exe, entry, kWord), root + (points_at & ~kSubdirectoryBit), (points_at & kSubdirectoryBit) != 0});
  }
  return entries;
}

bool is_data_entry(const Bytes& exe, const Entry& entry) {
  return !entry.directory && inside(exe.size(), entry.at, kDataEntryBytes);
}

// The directory of `type` under the root; nullopt when the program has no resource of that type.
std::optional<std::size_t> type_directory(const Bytes& exe, std::size_t root, std::uint32_t type) {
  for (const Entry& entry : entries_of(exe, root, root)) {
    if (entry.directory && entry.name == type) return entry.at;
  }
  return std::nullopt;
}

// Every variant (every language) of every resource of `type`, with its id.
void add_variants(const Bytes& exe, std::size_t root, const Entry& named, std::vector<Leaf>& leaves) {
  if (!named.directory) return;
  for (const Entry& language : entries_of(exe, root, named.at)) {
    if (is_data_entry(exe, language)) leaves.push_back({named.name, language.at});
  }
}

std::vector<Leaf> leaves_of(const Bytes& exe, std::size_t root, std::uint32_t type) {
  std::vector<Leaf> leaves;
  const auto of_type = type_directory(exe, root, type);
  for (const Entry& named : of_type ? entries_of(exe, root, *of_type) : std::vector<Entry>()) {
    add_variants(exe, root, named, leaves);
  }
  return leaves;
}

// Where the data a data entry points at is in the file.
std::optional<std::size_t> data_of(const Bytes& exe, const PeFile& pe, std::size_t entry) {
  return file_offset(pe, read_little(exe, entry, kWord), read_little(exe, entry + kWord, kWord));
}

int group_side(const Bytes& exe, std::size_t entry) { return exe[entry] == 0 ? kZeroSide : exe[entry]; }

void add_group_pictures(const Bytes& exe, std::size_t group, std::vector<GroupPicture>& pictures) {
  if (!inside(exe.size(), group, kGroupHeaderBytes)) return;
  const std::size_t count = read_little(exe, group + kGroupCountAt, kShort);
  if (!inside(exe.size(), group, kGroupHeaderBytes + count * kGroupEntryBytes)) return;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = group + kGroupHeaderBytes + i * kGroupEntryBytes;
    pictures.push_back({entry, group_side(exe, entry), read_little(exe, entry + kGroupIdAt, kShort)});
  }
}

bool names_picture(const std::vector<GroupPicture>& pictures, std::uint32_t id) {
  return std::any_of(pictures.begin(), pictures.end(), [id](const GroupPicture& picture) { return picture.id == id; });
}

bool has_variant(const std::vector<Leaf>& leaves, std::uint32_t id) {
  return std::any_of(leaves.begin(), leaves.end(), [id](const Leaf& leaf) { return leaf.id == id; });
}

// Whether every picture the groups name has a variant to point at a new picture.
bool every_picture_found(const ExeIcon& icon) {
  return !icon.pictures.empty() &&
         std::all_of(icon.pictures.begin(), icon.pictures.end(),
                     [&icon](const GroupPicture& picture) { return has_variant(icon.leaves, picture.id); });
}

// The program's icon: every group, and every variant of the pictures they name; nullopt when it
// has none, or a group names a picture it lacks.
// The pictures of every icon group of the tree at `root`.
std::vector<GroupPicture> pictures_of_groups(const Bytes& exe, const PeFile& pe, std::size_t root) {
  std::vector<GroupPicture> pictures;
  for (const Leaf& group : leaves_of(exe, root, kIconGroupType)) {
    if (const auto data = data_of(exe, pe, group.entry)) add_group_pictures(exe, *data, pictures);
  }
  return pictures;
}

// Every variant of the pictures the groups name.
std::vector<Leaf> variants_named(const Bytes& exe, std::size_t root, const std::vector<GroupPicture>& pictures) {
  std::vector<Leaf> leaves;
  for (const Leaf& leaf : leaves_of(exe, root, kIconType)) {
    if (names_picture(pictures, leaf.id)) leaves.push_back(leaf);
  }
  return leaves;
}

std::optional<ExeIcon> exe_icon(const Bytes& exe, const PeFile& pe) {
  const Directory resources = directory(exe, pe, kResourceDirectory);
  const auto root = file_offset(pe, resources.at, kResourceDirectoryBytes);
  if (!root) return std::nullopt;
  ExeIcon icon;
  icon.pictures = pictures_of_groups(exe, pe, *root);
  icon.leaves = variants_named(exe, *root, icon.pictures);
  if (!every_picture_found(icon)) return std::nullopt;
  return icon;
}

std::optional<ExeIcon> exe_icon_of(const Bytes& exe) {
  const auto pe = pe_file(exe);
  return pe ? exe_icon(exe, *pe) : std::nullopt;
}

bool has_every_side(const ExeIcon& icon, const IconSet& icons) {
  return std::all_of(icon.pictures.begin(), icon.pictures.end(),
                     [&icons](const GroupPicture& picture) { return icons.count(picture.side) != 0; });
}

// The side of the picture `id` names (the first group naming it says).
int side_of(const ExeIcon& icon, std::uint32_t id) {
  const auto picture = std::find_if(icon.pictures.begin(), icon.pictures.end(),
                                    [id](const GroupPicture& known) { return known.id == id; });
  return picture->side;
}

// The new pictures, one of each side the groups ask for, one after the other, and where each starts.
struct Pictures {
  Bytes data;
  std::map<int, std::size_t> starts;
};

Pictures pictures_for(const ExeIcon& icon, const IconSet& icons) {
  Pictures pictures;
  for (const GroupPicture& picture : icon.pictures) {
    if (pictures.starts.count(picture.side) != 0) continue;
    pictures.data.resize(aligned(pictures.data.size(), kPictureAlignment), 0);
    pictures.starts[picture.side] = pictures.data.size();
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

bool all_zero(const Bytes& exe, std::size_t at, std::size_t count) {
  return std::all_of(exe.begin() + static_cast<std::ptrdiff_t>(at),
                     exe.begin() + static_cast<std::ptrdiff_t>(at + count),
                     [](std::uint8_t byte) { return byte == 0; });
}

// Whether what a directory says is in the file overlaps `count` bytes at `at`.
bool overlaps(const Directory& directory, std::size_t at, std::size_t count) {
  return directory.at < at + count && at < std::size_t{directory.at} + directory.size;
}

// Whether a header at `header` ends before the first section's data starts, inside the file.
bool before_first_section(const Bytes& exe, const PeFile& pe, std::size_t header) {
  return header + kSectionHeaderBytes <= first_raw_offset(pe) && inside(exe.size(), header, kSectionHeaderBytes);
}

// Whether the bound imports, which linkers may keep right after the section headers, are where a
// new header would go.
bool bound_imports_at(const Bytes& exe, const PeFile& pe, std::size_t header) {
  const Directory bound = directory(exe, pe, kBoundImportDirectory);
  return bound.size != 0 && overlaps(bound, header, kSectionHeaderBytes);
}

// Whether one more section header fits after the last one: before the first section's data,
// on bytes nothing uses.
bool room_for_section(const Bytes& exe, const PeFile& pe, std::size_t header) {
  return before_first_section(exe, pe, header) && all_zero(exe, header, kSectionHeaderBytes) &&
         !bound_imports_at(exe, pe, header);
}

std::size_t next_header(const PeFile& pe) { return pe.sections_at + pe.sections.size() * kSectionHeaderBytes; }

// Where the program ends once loaded: the next section starts after it.
std::uint32_t loaded_end(const PeFile& pe) {
  std::uint32_t end = 0;
  for (const Section& section : pe.sections) {
    end = std::max(end, section.virtual_address + std::max(section.virtual_size, section.raw_size));
  }
  return end;
}

void write_section_header(Bytes& exe, std::size_t header, const Section& section) {
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
  const std::size_t header = next_header(pe);
  if (!room_for_section(exe, pe, header)) return std::nullopt;
  const Section section{static_cast<std::uint32_t>(data.size()), aligned(loaded_end(pe), pe.section_alignment),
                        aligned(data.size(), pe.file_alignment), aligned(exe.size(), pe.file_alignment)};
  write_section_header(exe, header, section);
  exe.resize(section.raw_offset, 0);
  exe.insert(exe.end(), data.begin(), data.end());
  exe.resize(std::size_t{section.raw_offset} + section.raw_size, 0);
  count_section(exe, pe, section);
  return section.virtual_address;
}

// Whether the signature is the last thing in the file (padded), as signing tools put it.
bool signature_at_end(const Directory& certificate, std::size_t file_size) {
  return certificate.at <= file_size &&
         aligned(std::size_t{certificate.at} + certificate.size, kCertificateAlignment) >= file_size;
}

// The program without its signature (Authenticode), which would no longer match it once its icon
// changes: the signature's directory emptied, and the signature cut off the end of the file.
void drop_signature(Bytes& exe, const PeFile& pe) {
  const Directory certificate = directory(exe, pe, kCertificateDirectory);
  if (certificate.size == 0) return;
  write_little(exe, pe.directories + kCertificateDirectory * kDirectoryBytes, 0, kWord);
  write_little(exe, pe.directories + kCertificateDirectory * kDirectoryBytes + kWord, 0, kWord);
  if (signature_at_end(certificate, exe.size())) exe.resize(certificate.at);
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

// Every variant of every picture points at the new picture of its side, and every group says
// what that picture is.
void point_at_pictures(Bytes& exe, const ExeIcon& icon, const Pictures& pictures, const IconSet& icons,
                       std::uint32_t section) {
  for (const Leaf& leaf : icon.leaves) {
    const int side = side_of(icon, leaf.id);
    write_little(exe, leaf.entry, section + static_cast<std::uint32_t>(pictures.starts.at(side)), kWord);
    write_little(exe, leaf.entry + kWord, static_cast<std::uint32_t>(icons.at(side).size()), kWord);
  }
  for (const GroupPicture& picture : icon.pictures) describe_picture(exe, picture, icons.at(picture.side).size());
}

// A program about to be given an icon: a copy without its signature, read, and its icon.
struct Patching {
  Bytes exe;
  PeFile pe;
  ExeIcon icon;
};

std::optional<Patching> patching(const Bytes& exe) {
  Bytes copy = exe;
  if (const auto signed_pe = pe_file(copy)) drop_signature(copy, *signed_pe);
  const auto pe = pe_file(copy);
  if (!pe) return std::nullopt;
  auto icon = exe_icon(copy, *pe);
  if (!icon) return std::nullopt;
  return Patching{std::move(copy), *pe, std::move(*icon)};
}

bool fail(std::string& error, const char* text) {
  error = tr(text);
  return false;
}

}  // namespace

std::vector<int> exe_icon_sides(const bytes::Bytes& exe) {
  std::set<int> sides;
  if (const auto icon = exe_icon_of(exe)) {
    for (const GroupPicture& picture : icon->pictures) sides.insert(picture.side);
  }
  return {sides.begin(), sides.end()};
}

bool exe_has_room_for_icon(const bytes::Bytes& exe) {
  const auto pe = pe_file(exe);
  return pe && room_for_section(exe, *pe, next_header(*pe));
}

bool replace_exe_icon(bytes::Bytes& exe, const IconSet& icons, std::string& error) {
  auto patch = patching(exe);
  if (!patch) return fail(error, "project.icon.no_exe_icon");
  if (!has_every_side(patch->icon, icons)) return fail(error, "project.icon.missing_side");
  const Pictures pictures = pictures_for(patch->icon, icons);
  const auto section = add_section(patch->exe, patch->pe, pictures.data);
  if (!section) return fail(error, "project.icon.no_room");
  point_at_pictures(patch->exe, patch->icon, pictures, icons, *section);
  exe = std::move(patch->exe);
  return true;
}

IconSet exe_icons(const bytes::Bytes& exe) {
  IconSet icons;
  const auto pe = pe_file(exe);
  if (!pe) return icons;
  const auto icon = exe_icon(exe, *pe);
  if (!icon) return icons;
  for (const Leaf& leaf : icon->leaves) {
    const auto data = data_of(exe, *pe, leaf.entry);
    const std::size_t size = read_little(exe, leaf.entry + kWord, kWord);
    if (data) icons.emplace(side_of(*icon, leaf.id), Bytes(exe.begin() + *data, exe.begin() + *data + size));
  }
  return icons;
}

}  // namespace redgem
