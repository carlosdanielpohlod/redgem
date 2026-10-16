#pragma once

// How much memory a scene needs as a whole, checked before it is loaded. A file can claim any
// sizes, and every part of it may be within its own limit (see gltf_checks) while all of them
// together are not: such a scene is refused instead of running out of memory.

#include <cgltf.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "scene/gltf/gltf_checks.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::gltf_budget {

// The most memory a scene may need as a whole while it is loaded (see scene_bytes and
// pictures_problem). It is an estimate of what the program holds, not a cap on the graphics
// card's memory. The biggest demo, the showcase, needs about 1.8 GB by this count (720 MB, and
// 1 GB of pictures once decoded).
inline constexpr std::uint64_t kMaxSceneBytes = std::uint64_t{4096} * 1024 * 1024;

// Why a scene's file alone is more than a scene may need; empty when it is not. Run before the
// file is read. (The same for a file of `bytes` bytes.)
std::string file_problem(const std::filesystem::path& file);
std::string file_size_problem(std::uint64_t bytes);

// What a parsed scene needs while it loads, in bytes: its file, the data files and the pictures
// it names (`files`, found once for the load), the pictures copied out of its own data, and its
// numbers as the engine keeps them (four bytes for each component of every element of every accessor).
std::uint64_t scene_bytes(const cgltf_data& data, const std::filesystem::path& file,
                          const gltf_checks::NamedFiles& files);

// Why a parsed scene would need more than kMaxSceneBytes; empty when it would not. Run before its
// data files are read.
std::string scene_problem(const cgltf_data& data, const std::filesystem::path& file,
                          const gltf_checks::NamedFiles& files);

// What a scene's pictures take once decoded, in bytes: four for each pixel their headers say they
// have (a picture that will not be decoded takes none).
std::uint64_t decoded_bytes(const std::vector<ImageData>& images);

// Why a scene that was read would need more than kMaxSceneBytes once its pictures are decoded
// too; empty when it would not. Run before any picture is decoded.
std::string pictures_problem(const cgltf_data& data, const std::filesystem::path& file,
                             const gltf_checks::NamedFiles& files, const std::vector<ImageData>& images);

}  // namespace redgem::gltf_budget
