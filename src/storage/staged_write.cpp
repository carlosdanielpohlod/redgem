#include "storage/staged_write.hpp"

#include <cerrno>
#include <random>
#include <sstream>

namespace redgem::durable::staging {
namespace {

constexpr char kNameSeparator = '.';      // before the random part of a write's own name
constexpr const char* kThisFolder = ".";  // where a file named with no folder is

bool wrote_all(std::FILE* out, std::string_view text) {
  return std::fwrite(text.data(), 1, text.size(), out) == text.size();
}

}  // namespace

std::filesystem::path staged_name(const std::filesystem::path& file, const char* suffix) {
  thread_local std::mt19937_64 random{std::random_device{}()};
  std::ostringstream part;
  part << kNameSeparator << std::hex << random();
  std::filesystem::path name = file.filename();
  name += part.str();
  name += suffix;
  return name;
}

std::filesystem::path folder_of(const std::filesystem::path& file) {
  return file.has_parent_path() ? file.parent_path() : std::filesystem::path(kThisFolder);
}

bool write_and_close(std::FILE* out, std::string_view text, const Flush& flush) {
  const bool flushed = wrote_all(out, text) ? flush(out) : false;
  // A full disk can show only here: the file being replaced must not be lost to half a one.
  if (std::fclose(out) != 0) return false;
  return flushed;
}

std::error_code write_error() {
  const int said = errno;
  return said != 0 ? std::error_code(said, std::generic_category()) : std::make_error_code(std::errc::io_error);
}

}  // namespace redgem::durable::staging
