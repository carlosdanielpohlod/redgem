#pragma once

#include <filesystem>
#include <string>

#include "editor/editor_config.hpp"

namespace redgem {

inline constexpr float kDefaultFlySpeed = 8.0f;
inline constexpr float kMinFlySpeed = 0.1f;
inline constexpr float kMaxFlySpeed = 100.0f;
inline constexpr float kMinSnapStep = 0.01f;
inline constexpr float kMaxSnapStep = 100.0f;
inline constexpr float kMaxRotateSnap = 180.0f;

struct EditorPreferences {
  float fly_speed = kDefaultFlySpeed;
  float translate_snap = editor_config::kTranslateSnap;
  float rotate_snap = editor_config::kRotateSnapDegrees;
  float scale_snap = editor_config::kScaleSnap;
};

std::filesystem::path editor_preferences_file();
EditorPreferences read_editor_preferences(const std::filesystem::path& file);
bool write_editor_preferences(const std::filesystem::path& file, const EditorPreferences& values, std::string& error);

}  // namespace redgem
