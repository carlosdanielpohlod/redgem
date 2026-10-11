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

// Returns nullopt and fills `error` when the file is not a valid .glb.
std::optional<GlbFile> read_glb(const std::filesystem::path& path, std::string& error);
// Writes atomically (temp file + rename). Returns false and fills `error` on failure.
bool write_glb(const std::filesystem::path& path, const GlbFile& glb, std::string& error);

}  // namespace redgem
