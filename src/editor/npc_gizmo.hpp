#pragma once

// What the Scene view draws of a selected object's NPC parts: an Interactable's reach (a ring of
// its range, the edges of its angle) and prompt, a Perception's sight cone and hearing ring, a
// Schedule's plan, and the path a NavAgent walks to where it was sent.

#include <string>
#include <vector>

#include "scene/transform.hpp"
#include "script/npc_settings.hpp"

namespace redgem {

inline constexpr int kNpcRingSegments = 32;  // of a full circle
inline constexpr int kNpcArcSegments = 16;   // of a sight cone's far edge
inline constexpr float kNpcFullTurn = 360.0f;
inline constexpr float kNpcMarkSize = 0.25f;     // meters: the cross where a NavAgent goes
inline constexpr float kNpcPromptLift = 2.0f;    // meters above the origin the prompt shows
inline constexpr float kNpcScheduleLift = 2.4f;  // ... and the plan

// Which part a line shows, for its color.
enum class NpcGizmoPart { Interactable, Sight, Hearing, Route };

struct NpcGizmoLine {
  Vec3 from;
  Vec3 to;
  NpcGizmoPart part = NpcGizmoPart::Interactable;
};

struct NpcGizmoLabel {
  Vec3 at;
  std::string text;
};

struct NpcGizmo {
  std::vector<NpcGizmoLine> lines;
  std::vector<NpcGizmoLabel> labels;
};

// The I18n key of the engine's own prompt, shown for an Interactable without one.
inline constexpr const char* kDefaultPromptKey = "game.interaction.interact";

// The gizmo of an object with these settings, where `world` has it.
NpcGizmo npc_gizmo(const NpcSettings& settings, const Transform& world);

}  // namespace redgem
