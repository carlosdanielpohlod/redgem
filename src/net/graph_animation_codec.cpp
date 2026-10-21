#include "net/animation_codec.hpp"

#include <cmath>

namespace redgem::net {
namespace {

constexpr float kWeightTolerance = 0.0001f;  // rounding error of two normalized float weights

bool unit_weight(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }

bool valid_side(const std::vector<WeightedClipPart>& side, bool required) {
  if (side.size() > kMaxBlendClips) return false;
  if (side.empty()) return !required;
  double total = 0.0;
  for (const auto& entry : side) {
    if (!valid_clip(entry.clip) || !unit_weight(entry.weight)) return false;
    total += entry.weight;
  }
  return std::abs(total - 1.0) <= kWeightTolerance;
}

void write_side(PoseWriter& writer, const std::vector<WeightedClipPart>& side) {
  for (const auto& entry : side) {
    write_animation_clip(writer, entry.clip);
    writer.number(entry.weight);
  }
}

bool read_side(PoseReader& reader, std::size_t count, std::vector<WeightedClipPart>& side) {
  if (count > kMaxBlendClips) return false;
  for (std::size_t index = 0; index < count; ++index) {
    std::optional<ClipPart> clip;
    if (!read_animation_clip(reader, clip) || !clip) return false;
    if (reader.left() < kFloatBytes) return false;
    side.push_back({*clip, reader.number()});
  }
  return true;
}

}  // namespace

bool valid_graph(const GraphPart& graph) {
  if (!valid_side(graph.from, false) || !valid_side(graph.to, true)) return false;
  if (!unit_weight(graph.blend) || !std::isfinite(graph.remaining)) return false;
  if (graph.from.empty()) return graph.blend == 1.0f && graph.remaining == 0.0f;
  return graph.blend < 1.0f && graph.remaining > 0.0f;
}

void write_graph(PoseWriter& writer, const GraphPart& graph) {
  writer.byte(static_cast<std::uint8_t>(graph.from.size()));
  writer.byte(static_cast<std::uint8_t>(graph.to.size()));
  writer.number(graph.blend);
  writer.number(graph.remaining);
  writer.byte(graph.playing ? 1 : 0);
  write_side(writer, graph.from);
  write_side(writer, graph.to);
}

std::optional<GraphPart> read_graph(PoseReader& reader) {
  if (reader.left() < kGraphHeadBytes) return std::nullopt;
  const std::size_t from = reader.word(kByte);
  const std::size_t to = reader.word(kByte);
  GraphPart graph;
  graph.blend = reader.number();
  graph.remaining = reader.number();
  const auto playing = reader.word(kByte);
  if (playing > 1) return std::nullopt;
  graph.playing = playing != 0;
  if (!read_side(reader, from, graph.from) || !read_side(reader, to, graph.to)) return std::nullopt;
  if (!valid_graph(graph)) return std::nullopt;
  return graph;
}

}  // namespace redgem::net
