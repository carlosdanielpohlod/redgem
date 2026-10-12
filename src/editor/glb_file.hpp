#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem {

// A binary glTF split into its JSON document and its binary buffer (meshes, images...).
struct GlbFile {
  nlohmann::json json;
  std::vector<std::uint8_t> bin;
};

// Chunks of a .glb start on multiples of 4 bytes: `size` rounded up to the next one.
std::size_t glb_padded_size(std::size_t size);

// A .glb says its own length, and each chunk's, in 32 bits. Whether a file with a JSON chunk and
// a binary chunk of these sizes (padded) can say them.
bool fits_in_glb(std::uint64_t json_bytes, std::uint64_t bin_bytes);

// Returns nullopt and fills `error` when the file is not a valid .glb.
std::optional<GlbFile> read_glb(const std::filesystem::path& path, std::string& error);
// Writes atomically (temp file + rename). Returns false and fills `error` on failure: also for a
// scene too big for the format (see fits_in_glb), of which nothing is written.
bool write_glb(const std::filesystem::path& path, const GlbFile& glb, std::string& error);

}  // namespace redgem
