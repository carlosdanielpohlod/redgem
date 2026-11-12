#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

// The id the linker gives each build of a program, read from the program itself: on Linux and
// Android the GNU build id note of an ELF file, on Windows the CodeView record of a PE file's
// debug directory. A crash report names the build that crashed by it, and `redgem symbolize`
// finds the symbols of that very build by it (see app/crash_report.hpp, project/crash_symbols.hpp).
//
// The same bytes are read from a program's file or from the memory it was loaded into: only where
// a part of it lies differs. (Both kinds of file are read as little-endian, as every system the
// engine runs on is.)
namespace redgem {

// What a program says of itself.
struct BinaryIdentity {
  std::string build_id;              // in hexadecimal; empty when it carries none
  std::uint64_t preferred_base = 0;  // the address it is linked to load at: what an offset into it counts from
};

// Where a program's bytes are read from.
enum class BinaryLayout {
  File,    // its file
  Loaded,  // the memory the system loaded it into (an offset from the address it was loaded at)
};

// Reads `bytes` bytes at offset `at` into `into`; false when they are not all there.
using ByteReader = std::function<bool(std::uint64_t at, void* into, std::size_t bytes)>;

// What a 64-bit ELF or PE program says of itself; nothing for anything else (or a file cut short).
std::optional<BinaryIdentity> read_binary_identity(const ByteReader& read, BinaryLayout layout);
// The same for a program's file; nothing when it cannot be read.
std::optional<BinaryIdentity> binary_identity_of_file(const std::filesystem::path& file);
// The same for a program the system loaded at `base` (this one, or a library of it).
std::optional<BinaryIdentity> binary_identity_in_memory(const void* base);

// The longest build id read (a SHA-1 is 20 bytes, an MD5 or a UUID 16).
inline constexpr std::size_t kMaxBuildIdBytes = 64;

}  // namespace redgem
