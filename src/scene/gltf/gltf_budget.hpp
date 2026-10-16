#pragma once

// How much memory a scene needs as a whole, checked before it is loaded. A file can claim any
// sizes, and every part of it may be within its own limit (see gltf_checks) while all of them
// together are not: such a scene is refused instead of running out of memory.

#include <cgltf.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace redgem::gltf_budget {

// The most memory a scene may need as a whole while it is loaded (see scene_bytes). The biggest
// demo, the showcase, needs about 720 MB.
inline constexpr std::uint64_t kMaxSceneBytes = std::uint64_t{2048} * 1024 * 1024;

// Why a scene's file alone is more than a scene may need; empty when it is not. Run before the
// file is read.
std::string file_problem(const std::filesystem::path& file);

// What a parsed scene needs while it loads, in bytes: its file, the data files and the pictures
// it names, the pictures copied out of its own data, and its numbers as the engine keeps them
// (four bytes for each component of every element of every accessor).
std::uint64_t scene_bytes(const cgltf_data& data, const std::filesystem::path& file);

// Why a parsed scene would need more than kMaxSceneBytes; empty when it would not. Run before its
// data files are read.
std::string scene_problem(const cgltf_data& data, const std::filesystem::path& file);

}  // namespace redgem::gltf_budget
