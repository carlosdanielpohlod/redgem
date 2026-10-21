#include "scene/animation/animation_blend.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "scene/animation/animation.hpp"

namespace redgem {
namespace {

constexpr std::size_t kPositionWidth = 3;
constexpr std::size_t kRotationWidth = 4;
using ChannelKey = AnimationChannelKey;

ChannelValue rest_value(const Transform& rest, AnimationPath path) {
  if (path == AnimationPath::Rotation) return {rest.rotation.x, rest.rotation.y, rest.rotation.z, rest.rotation.w};
  const Vec3 value = path == AnimationPath::Translation ? rest.position : rest.scale;
  return {value.x, value.y, value.z, 0.0f};
}

ChannelValue value_at(const AnimationDesc& clip, const AnimationGraphSample& sample, const AnimationChannel* channel,
                      const ChannelValue& fallback) {
  if (channel == nullptr) return fallback;
  const float time = sample.loop && clip.duration > 0.0f ? std::fmod(sample.time, clip.duration) : sample.time;
  return sample_channel(*channel, time);
}

double alignment(const ChannelValue& left, const ChannelValue& right) {
  double dot = 0.0;
  for (std::size_t index = 0; index < kRotationWidth; ++index) dot += static_cast<double>(left[index]) * right[index];
  return dot;
}

ChannelValue normalized_rotation(ChannelValue value, const ChannelValue& fallback) {
  const double length = std::sqrt(alignment(value, value));
  if (length <= 0.0) return fallback;
  for (float& component : value) component = static_cast<float>(component / length);
  return value;
}

struct ChannelBlend {
  std::array<double, kMaxChannelFloats> total{};
  ChannelValue reference{};
  double weight = 0.0;
};

bool valid_sample(const AnimationGraphSample& sample, std::size_t clips) {
  return sample.clip < clips && std::isfinite(sample.weight) && sample.weight > 0.0f && std::isfinite(sample.time) &&
         sample.time >= 0.0f;
}

void add_value(ChannelBlend& blend, const ChannelValue& value, float weight, AnimationPath path) {
  if (blend.weight == 0.0) blend.reference = value;
  const bool opposite_rotation = path == AnimationPath::Rotation && alignment(blend.reference, value) < 0.0;
  const double signed_weight = opposite_rotation ? -static_cast<double>(weight) : weight;
  for (std::size_t index = 0; index < value.size(); ++index) blend.total[index] += value[index] * signed_weight;
  blend.weight += weight;
}

ChannelValue blended_value(const std::vector<AnimationDesc>& clips, const std::vector<AnimationGraphSample>& samples,
                           const AnimationBlendLayout::value_type& entry, const ChannelValue& fallback) {
  ChannelBlend blend;
  for (const auto& sample : samples) {
    if (!valid_sample(sample, clips.size())) continue;
    add_value(blend, value_at(clips[sample.clip], sample, entry.second[sample.clip], fallback), sample.weight,
              entry.first.second);
  }
  if (blend.weight <= 0.0) return fallback;
  ChannelValue value{};
  for (std::size_t index = 0; index < value.size(); ++index)
    value[index] = static_cast<float>(blend.total[index] / blend.weight);
  return entry.first.second == AnimationPath::Rotation ? normalized_rotation(value, fallback) : value;
}

AnimationChannel merged_channel(ChannelKey key, const ChannelValue& value) {
  AnimationChannel channel;
  channel.node = key.first;
  channel.path = key.second;
  channel.times = {0.0f};
  const std::size_t width = key.second == AnimationPath::Rotation ? kRotationWidth : kPositionWidth;
  channel.values.assign(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(width));
  return channel;
}

void add_layout_channel(AnimationBlendLayout& layout, const AnimationChannel& channel, std::size_t index,
                        std::size_t clips) {
  auto& channels = layout[{channel.node, channel.path}];
  if (channels.empty()) channels.resize(clips);
  channels[index] = &channel;
}

}  // namespace

AnimationBlendLayout animation_blend_layout(const std::vector<AnimationDesc>& clips, std::size_t nodes) {
  AnimationBlendLayout layout;
  for (std::size_t index = 0; index < clips.size(); ++index) {
    for (const auto& channel : clips[index].channels) {
      if (!is_index_of(channel.node, nodes)) continue;
      add_layout_channel(layout, channel, index, clips.size());
    }
  }
  return layout;
}

AnimationDesc blended_animation(const std::vector<AnimationDesc>& clips,
                                const std::vector<AnimationGraphSample>& samples, const std::vector<Transform>& rest,
                                const AnimationBlendLayout& layout) {
  AnimationDesc pose;
  for (const auto& entry : layout) {
    const auto& key = entry.first;
    const auto fallback = rest_value(rest[static_cast<std::size_t>(key.first)], key.second);
    pose.channels.push_back(merged_channel(key, blended_value(clips, samples, entry, fallback)));
  }
  return pose;
}

}  // namespace redgem
