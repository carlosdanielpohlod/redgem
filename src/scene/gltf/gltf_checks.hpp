#pragma once

// What the glTF loader checks about a file's own numbers before it believes them. A scene or a
// model may come from anywhere: its counts, offsets and file names are claims, not facts.

#include <cgltf.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace redgem::gltf_checks {

// The most elements one accessor may hold (a mesh of five million triangles has fifteen million
// indices). More than that is refused instead of allocated.
inline constexpr std::size_t kMaxAccessorElements = 16 * 1024 * 1024;
// How deep objects may be nested. Walking a scene's tree takes stack for every level.
inline constexpr int kMaxNodeDepth = 256;
// The most nodes a file may hold.
inline constexpr std::size_t kMaxSceneNodes = 100000;
// The biggest picture file, and the biggest data file beside a .gltf, that are read.
inline constexpr std::uintmax_t kMaxImageFileBytes = 256ull * 1024 * 1024;
inline constexpr std::uintmax_t kMaxBufferFileBytes = 1024ull * 1024 * 1024;

// Whether `count` elements of `element_size` bytes, placed `stride` bytes apart from `offset` on,
// all lie within `size` bytes. No sum or product in it can wrap around.
bool elements_fit(std::size_t offset, std::size_t stride, std::size_t count, std::size_t element_size,
                  std::size_t size);

// What is wrong with what an accessor says about itself (too many elements, data reaching past
// its buffer view or the view past its buffer); empty when nothing is.
std::string accessor_problem(const cgltf_accessor& accessor);

// Whether an accessor's elements can be read: it is there, its data is loaded, it has no problem,
// and it is not sparse or without a buffer view (those are treated as absent).
bool readable(const cgltf_accessor* accessor);

// The first thing wrong with the file's buffer views and accessors; empty when nothing is. Run
// before anything walks them.
std::string data_problem(const cgltf_data& data);

// The files a parsed scene's buffers and pictures name, by their numbers in the scene: each
// found once for a load (see file_of_uri), however many of its steps need it. An empty path for
// one that names no file, or none it may read.
struct NamedFiles {
  std::vector<std::filesystem::path> buffers;
  std::vector<std::filesystem::path> images;
};
NamedFiles named_files(const cgltf_data& data, const std::filesystem::path& base_dir);

// Why a buffer's file must not be opened (it is outside the scene's folder, not a regular file,
// or too big); empty for the buffers that can be loaded. Run before the buffers are read.
std::string buffer_file_problem(const cgltf_data& data, const NamedFiles& files);

// The regular file a glTF "uri" names inside `base_dir` (percent escapes decoded); empty when it
// names none there. A "data:" uri names no file.
std::filesystem::path file_of_uri(const char* uri, const std::filesystem::path& base_dir);
// Whether a uri names a file at all (not data written into the JSON itself, not a web address).
bool names_a_file(const char* uri);

}  // namespace redgem::gltf_checks
