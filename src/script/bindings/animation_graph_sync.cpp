#include "script/bindings/animation_sync.hpp"

#include <mruby/array.h>

#include "script/bindings/binding_helpers.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kGraphParts = 5;  // blend, remaining, playing, source clips, destination clips
constexpr mrb_int kBlendAt = 0;
constexpr mrb_int kRemainingAt = 1;
constexpr mrb_int kPlayingAt = 2;
constexpr mrb_int kFromAt = 3;
constexpr mrb_int kToAt = 4;
constexpr mrb_int kWeightedParts = 2;
constexpr mrb_int kClipAt = 0;
constexpr mrb_int kWeightAt = 1;

void require_size(mrb_state* mrb, mrb_value list, mrb_int size) {
  const bool sized = mrb_array_p(list) && RARRAY_LEN(list) == size;
  if (!sized) mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid synchronized animation graph");
}

float number_at(mrb_state* mrb, mrb_value list, mrb_int at) {
  return static_cast<float>(finite(mrb, mrb_as_float(mrb, mrb_ary_ref(mrb, list, at))));
}

std::vector<WeightedClipState> side_arg(mrb_state* mrb, mrb_value value) {
  if (mrb_type(value) != MRB_TT_ARRAY) mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid synchronized animation clips");
  if (RARRAY_LEN(value) > static_cast<mrb_int>(kMaxGraphSideClips))
    mrb_raise(mrb, E_ARGUMENT_ERROR, "too many synchronized animation clips");
  std::vector<WeightedClipState> result;
  for (mrb_int index = 0; index < RARRAY_LEN(value); ++index) {
    const auto entry = mrb_ary_ref(mrb, value, index);
    require_size(mrb, entry, kWeightedParts);
    const auto clip = animation_clip_arg(mrb, mrb_ary_ref(mrb, entry, kClipAt));
    if (!clip) mrb_raise(mrb, E_ARGUMENT_ERROR, "a graph clip cannot be nil");
    result.push_back({*clip, number_at(mrb, entry, kWeightAt)});
  }
  return result;
}

std::vector<net::WeightedClipPart> part_side(const std::vector<WeightedClipState>& side) {
  std::vector<net::WeightedClipPart> result;
  for (const auto& entry : side) {
    const auto& clip = entry.clip;
    result.push_back({{clip.clip, clip.time, clip.speed, clip.loop, clip.playing}, entry.weight});
  }
  return result;
}

}  // namespace

net::GraphPart animation_graph_part(const GraphAnimation& graph) {
  return {part_side(graph.from), part_side(graph.to), graph.blend, graph.remaining, graph.playing};
}

std::optional<GraphAnimation> animation_graph_arg(mrb_state* mrb, mrb_value value) {
  if (mrb_nil_p(value)) return std::nullopt;
  require_size(mrb, value, kGraphParts);
  const auto playing = mrb_ary_ref(mrb, value, kPlayingAt);
  if (!mrb_true_p(playing) && !mrb_false_p(playing)) mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid graph playing flag");
  GraphAnimation graph;
  graph.blend = number_at(mrb, value, kBlendAt);
  graph.remaining = number_at(mrb, value, kRemainingAt);
  graph.playing = mrb_test(playing);
  graph.from = side_arg(mrb, mrb_ary_ref(mrb, value, kFromAt));
  graph.to = side_arg(mrb, mrb_ary_ref(mrb, value, kToAt));
  if (!net::valid_graph(animation_graph_part(graph))) mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid graph weights or fade");
  return graph;
}

}  // namespace redgem
