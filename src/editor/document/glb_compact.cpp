#include "editor/document/glb_compact.hpp"

#include <algorithm>
#include <array>
#include <exception>

#include "editor/document/glb_usage.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {
namespace {

using namespace gltf_json;
using namespace glb_usage;
using Json = nlohmann::json;

bool all_used(const Usage& usage) {
  return std::all_of(usage.begin(), usage.end(), [](const std::vector<bool>& used) {
    return std::all_of(used.begin(), used.end(), [](bool one) { return one; });
  });
}

// The number each used entry gets once the unused ones are gone.
std::vector<int> new_numbers(const std::vector<bool>& used) {
  std::vector<int> numbers(used.size(), kNoIndex);
  int next = 0;
  for (std::size_t i = 0; i < used.size(); ++i) {
    if (used[i]) numbers[i] = next++;
  }
  return numbers;
}

// Points every reference at its entry's new number.
void renumber(Json& doc, const Usage& usage) {
  std::array<std::vector<int>, kKinds> numbers;
  for (std::size_t kind = 0; kind < kKinds; ++kind) numbers[kind] = new_numbers(usage[kind]);
  walk(doc, usage, [&numbers](Json& reference, Kind kind) {
    reference = numbers[kind][entry_of(reference, numbers[kind].size())];
  });
}

// Takes the unused entries out of one of the file's lists (and the list, once it is empty).
void keep_used(Json& doc, Kind kind, const std::vector<bool>& used) {
  Json* holder = holder_of(doc, kind);
  if (holder == nullptr) return;
  Json kept = Json::array();
  std::size_t index = 0;
  each(*holder, kListNames[kind], [&](Json& entry) {
    if (used[index++]) kept.push_back(std::move(entry));
  });
  set_unless_empty(*holder, kListNames[kind], kept);
}

// A buffer view kept the plain way: no extension, in the file's only buffer.
bool plain_view(const Json& view) {
  return !view.contains(kExtensions) && view.value(kBuffer, kOnlyBuffer) == kOnlyBuffer;
}

bool inside(std::size_t offset, std::size_t length, std::size_t whole) {
  return offset <= whole && length <= whole - offset;
}

// Where a buffer view's bytes are in a binary chunk of `bin_size` bytes: first and one past last.
std::pair<std::size_t, std::size_t> bytes_of(const Json& view, std::size_t bin_size) {
  if (!plain_view(view)) throw NotFollowed();
  const auto offset = view.value(kByteOffset, std::size_t{0});
  const auto length = view.at(kByteLength).get<std::size_t>();
  if (!inside(offset, length, bin_size)) throw NotFollowed();
  return {offset, offset + length};
}

// The bytes the buffer views left in the file point at, one view after the other; each view is
// pointed at where its bytes went.
std::vector<std::uint8_t> packed_bytes(Json& doc, const std::vector<std::uint8_t>& bin) {
  std::vector<std::uint8_t> packed;
  each(doc, kBufferViews, [&](Json& view) {
    const auto [first, last] = bytes_of(view, bin.size());
    packed.resize(glb_padded_size(packed.size()));  // data starts on multiples of 4 bytes
    view[kByteOffset] = packed.size();
    packed.insert(packed.end(), bin.begin() + static_cast<std::ptrdiff_t>(first),
                  bin.begin() + static_cast<std::ptrdiff_t>(last));
  });
  return packed;
}

void describe_buffer(Json& doc, std::size_t bytes) {
  if (bytes == 0) {
    doc.erase(kBuffers);
  } else {
    doc[kBuffers] = Json::array({{{kByteLength, bytes}}});
  }
}

std::optional<GlbFile> lean_copy(const GlbFile& glb) {
  GlbFile lean;
  lean.json = glb.json;
  const Usage usage = usage_of(lean.json);
  if (all_used(usage)) return std::nullopt;
  renumber(lean.json, usage);
  for (std::size_t kind = 0; kind < kKinds; ++kind) keep_used(lean.json, static_cast<Kind>(kind), usage[kind]);
  lean.bin = packed_bytes(lean.json, glb.bin);
  describe_buffer(lean.json, lean.bin.size());
  return lean;
}

}  // namespace

std::optional<GlbFile> without_unused(const GlbFile& glb) {
  try {
    return lean_copy(glb);
  } catch (const std::exception&) {
    // (Also what the JSON library throws when the file holds something else than glTF says.)
    return std::nullopt;
  }
}

}  // namespace redgem
