#pragma once

#include <string>
#include <vector>

namespace redgem {

// "camera_follow.rb" -> "CameraFollow", "enemies/slime.rb" -> "Slime".
std::string class_name_for_script(const std::string& file_name);

// The reverse: "CameraFollow" -> "camera_follow.rb".
std::string script_for_class_name(const std::string& class_name);
// The script that sets a scene up: "main" -> "main_scene.rb".
std::string scene_script_for(const std::string& scene_name);

// The `script` property may list several scripts: "player.rb, health.rb".
std::vector<std::string> split_script_list(const std::string& value);
// The reverse: {"player.rb", "health.rb"} -> "player.rb, health.rb".
std::string join_script_list(const std::vector<std::string>& scripts);

}  // namespace redgem
