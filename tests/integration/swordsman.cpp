#include "integration/swordsman.hpp"

#include <string>

namespace redgem::test {
namespace {

constexpr float kGuardSeconds = 2.0f;
constexpr std::size_t kX = 0;
constexpr std::size_t kY = 1;
constexpr std::size_t kEndAt = 3;  // where a two-key translation's second key starts

AnimationDesc sword_clip(const char* name, int sword, float seconds, std::size_t axis) {
  AnimationChannel channel;
  channel.node = sword;
  channel.times = {0.0f, seconds};
  channel.values = {0, 0, 0, 0, 0, 0};
  channel.values[kEndAt + axis] = kSwordReach;
  return AnimationDesc{name, seconds, {channel}};
}

}  // namespace

void add_swordsman(SceneDesc& desc) {
  const int swordsman = static_cast<int>(desc.nodes.size());
  const int sword = swordsman + 1;
  NodeDesc body;
  body.name = kSwordsman;
  body.children = {sword};
  NodeDesc blade;
  blade.name = "Sword";
  blade.parent = swordsman;
  desc.nodes.push_back(body);
  desc.nodes.push_back(blade);
  desc.animations.push_back(sword_clip("Attack", sword, kAttackSeconds, kX));
  desc.animations.push_back(sword_clip("Guard", sword, kGuardSeconds, kY));
}

}  // namespace redgem::test
