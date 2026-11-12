#include "core/build_id.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace redgem {
namespace {

// An ELF file (64-bit, little-endian): its header, its program headers, and the notes one of them
// points at. The build id is the GNU note of type 3.
constexpr std::array<unsigned char, 4> kElfMagic = {0x7f, 'E', 'L', 'F'};
constexpr std::size_t kElfIdentBytes = 16;
constexpr std::size_t kElfClassAt = 4;
constexpr unsigned char kElfClass64 = 2;
constexpr std::size_t kElfDataAt = 5;
constexpr unsigned char kElfLittleEndian = 1;
constexpr std::uint64_t kElfProgramTableAt = 32;  // e_phoff
constexpr std::uint64_t kElfProgramEntryAt = 54;  // e_phentsize
constexpr std::uint64_t kElfProgramCountAt = 56;  // e_phnum
constexpr std::uint32_t kProgramLoad = 1;         // PT_LOAD
constexpr std::uint32_t kProgramNote = 4;         // PT_NOTE
constexpr std::uint64_t kProgramOffsetAt = 8;     // p_offset
constexpr std::uint64_t kProgramAddressAt = 16;   // p_vaddr
constexpr std::uint64_t kProgramSizeAt = 32;      // p_filesz
constexpr std::uint32_t kGnuBuildIdNote = 3;      // NT_GNU_BUILD_ID
constexpr std::array<char, 4> kGnuNoteName = {'G', 'N', 'U', '\0'};
constexpr std::uint64_t kNoteAlignment = 4;

// A PE32+ file (64-bit Windows): the DOS header points at the PE header, whose optional header
// lists the debug directory, whose CodeView entry holds the build id (a GUID and an age).
constexpr std::uint16_t kDosMagic = 0x5a4d;         // "MZ"
constexpr std::uint64_t kPeHeaderPointerAt = 0x3c;  // e_lfanew
constexpr std::uint32_t kPeSignature = 0x00004550;  // "PE\0\0"
constexpr std::uint64_t kSectionCountAt = 6;        // from the signature on
constexpr std::uint64_t kOptionalSizeAt = 20;
constexpr std::uint64_t kOptionalHeaderAt = 24;
constexpr std::uint16_t kPe32PlusMagic = 0x20b;
constexpr std::uint64_t kImageBaseAt = 24;  // in the optional header
constexpr std::uint64_t kDirectoryCountAt = 108;
constexpr std::uint64_t kDirectoriesAt = 112;
constexpr std::uint32_t kDebugDirectory = 6;
constexpr std::uint64_t kDirectoryBytes = 8;
constexpr std::uint64_t kSectionBytes = 40;
constexpr std::uint64_t kSectionFieldsAt = 8;  // VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData
constexpr std::uint64_t kDebugEntryBytes = 28;
constexpr std::uint64_t kDebugFieldsAt = 12;  // Type, SizeOfData, AddressOfRawData, PointerToRawData
constexpr std::uint64_t kMaxDebugEntries = 16;
constexpr std::uint32_t kCodeViewDebug = 2;
constexpr std::uint32_t kCodeViewSignature = 0x53445352;  // "RSDS"
constexpr std::uint64_t kCodeViewIdAt = 4;
constexpr std::size_t kCodeViewIdBytes = 20;  // the GUID and the age

constexpr const char* kHexDigits = "0123456789abcdef";
constexpr int kBitsPerHexDigit = 4;
constexpr unsigned kHexDigitMask = 0xf;

template <typename Value>
bool read_value(const ByteReader& read, std::uint64_t at, Value& value) {
  return read(at, &value, sizeof value);
}

std::uint64_t note_aligned(std::uint64_t size) { return (size + kNoteAlignment - 1) / kNoteAlignment * kNoteAlignment; }

bool fits_a_build_id(std::uint64_t bytes) { return bytes > 0 && bytes <= kMaxBuildIdBytes; }

// `count` bytes at `at`, in hexadecimal; empty when they are no build id, or not all there.
std::string hex_at(const ByteReader& read, std::uint64_t at, std::uint64_t count) {
  std::array<unsigned char, kMaxBuildIdBytes> bytes{};
  if (!fits_a_build_id(count)) return {};
  if (!read(at, bytes.data(), count)) return {};
  std::string text;
  for (std::size_t i = 0; i < count; ++i) {
    text.push_back(kHexDigits[bytes[i] >> kBitsPerHexDigit]);
    text.push_back(kHexDigits[bytes[i] & kHexDigitMask]);
  }
  return text;
}

// --- ELF

bool is_elf64(const ByteReader& read) {
  std::array<unsigned char, kElfIdentBytes> ident{};
  if (!read(0, ident.data(), ident.size())) return false;
  const bool magic = std::equal(kElfMagic.begin(), kElfMagic.end(), ident.begin());
  return magic && ident[kElfClassAt] == kElfClass64 && ident[kElfDataAt] == kElfLittleEndian;
}

struct NoteHeader {
  std::uint32_t name_bytes = 0;
  std::uint32_t id_bytes = 0;
  std::uint32_t type = 0;
};

// Whether another note starts at `at`, before `end`: then it is read into `note`.
bool next_note(const ByteReader& read, std::uint64_t at, std::uint64_t end, NoteHeader& note) {
  return at + sizeof note <= end && read_value(read, at, note);
}

bool is_build_id_note(const ByteReader& read, const NoteHeader& note, std::uint64_t name_at) {
  std::array<char, kGnuNoteName.size()> name{};
  const bool gnu_sized = note.type == kGnuBuildIdNote && note.name_bytes == kGnuNoteName.size();
  return gnu_sized && read(name_at, name.data(), name.size()) && name == kGnuNoteName;
}

std::string build_id_in_notes(const ByteReader& read, std::uint64_t at, std::uint64_t size) {
  const std::uint64_t end = at + size;
  NoteHeader note;
  while (next_note(read, at, end, note)) {
    const std::uint64_t name_at = at + sizeof note;
    const std::uint64_t id_at = name_at + note_aligned(note.name_bytes);
    if (is_build_id_note(read, note, name_at)) return hex_at(read, id_at, note.id_bytes);
    at = id_at + note_aligned(note.id_bytes);
  }
  return {};
}

struct ProgramHeader {
  std::uint32_t type = 0;
  std::uint64_t offset = 0;   // in the file
  std::uint64_t address = 0;  // where it is linked to load
  std::uint64_t size = 0;
};

std::optional<ProgramHeader> program_header(const ByteReader& read, std::uint64_t at) {
  ProgramHeader header;
  const bool whole = read_value(read, at, header.type) && read_value(read, at + kProgramOffsetAt, header.offset) &&
                     read_value(read, at + kProgramAddressAt, header.address) &&
                     read_value(read, at + kProgramSizeAt, header.size);
  if (!whole) return std::nullopt;
  return header;
}

std::vector<ProgramHeader> program_headers(const ByteReader& read) {
  std::uint64_t table = 0;
  std::uint16_t entry_bytes = 0;
  std::uint16_t count = 0;
  std::vector<ProgramHeader> headers;
  const bool told = read_value(read, kElfProgramTableAt, table) && read_value(read, kElfProgramEntryAt, entry_bytes) &&
                    read_value(read, kElfProgramCountAt, count);
  for (std::uint16_t i = 0; told && i < count; ++i) {
    const auto header = program_header(read, table + std::uint64_t{i} * entry_bytes);
    if (!header) break;
    headers.push_back(*header);
  }
  return headers;
}

// Where the file's start is linked to load: what its first loaded part's address says.
std::uint64_t elf_preferred_base(const std::vector<ProgramHeader>& headers) {
  for (const ProgramHeader& header : headers) {
    if (header.type == kProgramLoad) return header.address - header.offset;
  }
  return 0;
}

std::uint64_t place_of_part(const ProgramHeader& header, BinaryLayout layout, std::uint64_t base) {
  return layout == BinaryLayout::File ? header.offset : header.address - base;
}

std::optional<BinaryIdentity> elf_identity(const ByteReader& read, BinaryLayout layout) {
  const std::vector<ProgramHeader> headers = program_headers(read);
  BinaryIdentity identity;
  identity.preferred_base = elf_preferred_base(headers);
  for (const ProgramHeader& header : headers) {
    if (header.type != kProgramNote) continue;
    identity.build_id = build_id_in_notes(read, place_of_part(header, layout, identity.preferred_base), header.size);
    if (!identity.build_id.empty()) break;
  }
  return identity;
}

// --- PE

struct PeHeaders {
  std::uint64_t optional_at = 0;
  std::uint64_t sections_at = 0;
  std::uint16_t section_count = 0;
};

std::optional<std::uint64_t> pe_signature_at(const ByteReader& read) {
  std::uint16_t dos = 0;
  std::uint32_t pointer = 0;
  std::uint32_t signature = 0;
  const bool found = read_value(read, 0, dos) && dos == kDosMagic && read_value(read, kPeHeaderPointerAt, pointer) &&
                     read_value(read, pointer, signature) && signature == kPeSignature;
  if (!found) return std::nullopt;
  return pointer;
}

std::optional<PeHeaders> pe_headers(const ByteReader& read) {
  const auto signature = pe_signature_at(read);
  if (!signature) return std::nullopt;
  PeHeaders headers;
  std::uint16_t optional_bytes = 0;
  std::uint16_t magic = 0;
  headers.optional_at = *signature + kOptionalHeaderAt;
  const bool pe32_plus = read_value(read, *signature + kSectionCountAt, headers.section_count) &&
                         read_value(read, *signature + kOptionalSizeAt, optional_bytes) &&
                         read_value(read, headers.optional_at, magic) && magic == kPe32PlusMagic;
  if (!pe32_plus) return std::nullopt;
  headers.sections_at = headers.optional_at + optional_bytes;
  return headers;
}

struct DataDirectory {
  std::uint32_t address = 0;
  std::uint32_t size = 0;
};

std::optional<DataDirectory> debug_directory(const ByteReader& read, const PeHeaders& headers) {
  std::uint32_t count = 0;
  DataDirectory directory;
  const std::uint64_t at = headers.optional_at + kDirectoriesAt + kDebugDirectory * kDirectoryBytes;
  const bool listed = read_value(read, headers.optional_at + kDirectoryCountAt, count) && count > kDebugDirectory &&
                      read_value(read, at, directory);
  if (!listed) return std::nullopt;
  return directory;
}

struct Section {
  std::uint32_t size = 0;
  std::uint32_t address = 0;
  std::uint32_t raw_size = 0;
  std::uint32_t raw_at = 0;
};

bool section_holds(const Section& section, std::uint32_t address) {
  return address >= section.address && address - section.address < section.raw_size;
}

// Where in the file an address of the loaded program is.
std::optional<std::uint64_t> file_offset_of(const ByteReader& read, const PeHeaders& headers, std::uint32_t address) {
  for (std::uint16_t i = 0; i < headers.section_count; ++i) {
    Section section;
    if (!read_value(read, headers.sections_at + i * kSectionBytes + kSectionFieldsAt, section)) return std::nullopt;
    if (section_holds(section, address)) return std::uint64_t{section.raw_at} + (address - section.address);
  }
  return std::nullopt;
}

std::optional<std::uint64_t> place_of_address(const ByteReader& read, const PeHeaders& headers, std::uint32_t address,
                                              BinaryLayout layout) {
  if (layout == BinaryLayout::Loaded) return address;
  return file_offset_of(read, headers, address);
}

std::string codeview_id(const ByteReader& read, std::uint64_t at) {
  std::uint32_t signature = 0;
  if (!read_value(read, at, signature)) return {};
  if (signature != kCodeViewSignature) return {};
  return hex_at(read, at + kCodeViewIdAt, kCodeViewIdBytes);
}

struct DebugEntry {
  std::uint32_t type = 0;
  std::uint32_t size = 0;
  std::uint32_t address = 0;  // where it is in the loaded program
  std::uint32_t raw_at = 0;   // where it is in the file
};

std::string pe_build_id(const ByteReader& read, std::uint64_t at, std::uint32_t size, BinaryLayout layout) {
  const std::uint64_t count = std::min<std::uint64_t>(size / kDebugEntryBytes, kMaxDebugEntries);
  for (std::uint64_t entry = 0; entry < count; ++entry) {
    DebugEntry debug;
    if (!read_value(read, at + entry * kDebugEntryBytes + kDebugFieldsAt, debug)) return {};
    if (debug.type != kCodeViewDebug) continue;
    return codeview_id(read, layout == BinaryLayout::File ? debug.raw_at : debug.address);
  }
  return {};
}

std::optional<BinaryIdentity> pe_identity(const ByteReader& read, BinaryLayout layout) {
  const auto headers = pe_headers(read);
  if (!headers) return std::nullopt;
  BinaryIdentity identity;
  read_value(read, headers->optional_at + kImageBaseAt, identity.preferred_base);
  const auto directory = debug_directory(read, *headers);
  const auto at = directory ? place_of_address(read, *headers, directory->address, layout) : std::nullopt;
  if (at) identity.build_id = pe_build_id(read, *at, directory->size, layout);
  return identity;
}

}  // namespace

std::optional<BinaryIdentity> read_binary_identity(const ByteReader& read, BinaryLayout layout) {
  if (is_elf64(read)) return elf_identity(read, layout);
  return pe_identity(read, layout);
}

std::optional<BinaryIdentity> binary_identity_of_file(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  const ByteReader read = [&in](std::uint64_t at, void* into, std::size_t bytes) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(at));
    in.read(static_cast<char*>(into), static_cast<std::streamsize>(bytes));
    return static_cast<std::size_t>(in.gcount()) == bytes;
  };
  return read_binary_identity(read, BinaryLayout::File);
}

std::optional<BinaryIdentity> binary_identity_in_memory(const void* base) {
  const auto* start = static_cast<const unsigned char*>(base);
  const ByteReader read = [start](std::uint64_t at, void* into, std::size_t bytes) {
    std::memcpy(into, start + at, bytes);
    return true;
  };
  return read_binary_identity(read, BinaryLayout::Loaded);
}

}  // namespace redgem
