#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "editor/editor_config.hpp"
#include "editor/editor_fly_bindings.hpp"

namespace redgem {

inline constexpr float kMinFlySpeed = 0.1f;
inline constexpr float kMaxFlySpeed = 100.0f;
inline constexpr float kMinSnapStep = 0.01f;
inline constexpr float kMaxSnapStep = 100.0f;
inline constexpr float kMaxRotateSnap = 180.0f;
inline constexpr float kMinEditorScale = 0.75f;
inline constexpr float kMaxEditorScale = 2.0f;

struct EditorPreferences {
  float fly_speed = editor_config::kDefaultFlySpeed;
  float translate_snap = editor_config::kTranslateSnap;
  float rotate_snap = editor_config::kRotateSnapDegrees;
  float scale_snap = editor_config::kScaleSnap;
  FlyBindings fly_bindings{};
  std::optional<float> interface_scale = std::nullopt;
};

std::filesystem::path editor_preferences_file();
EditorPreferences read_editor_preferences(const std::filesystem::path& file);
bool write_editor_preferences(const std::filesystem::path& file, const EditorPreferences& previous,
                              const EditorPreferences& values, std::string& error);

}  // namespace redgem
