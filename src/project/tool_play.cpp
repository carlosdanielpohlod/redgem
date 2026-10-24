#include "project/tool_play.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "project/scene_command.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kScratchPrefix = "redgem-run-";
constexpr const char* kSaveFile = "save.json";

std::vector<std::string> camera_names(const SceneDesc& scene) {
  std::vector<std::string> names;
  for (const NodeDesc& node : scene.nodes) {
    if (node.camera) names.push_back(node.name);
  }
  return names;
}

bool is_camera_named(const NodeDesc& node, const std::string& name) { return node.camera && node.name == name; }

std::variant<SceneDesc, std::string> scene_of(const PlayRequest& request) {
  std::string error;
  if (!named_scene_file(request.game_dir, request.scene, error)) return error;
  std::optional<SceneDesc> scene = read_scene(request.game_dir, request.scene);
  if (!scene) return tr("project.run.scene_unreadable", {{"scene", request.scene}});
  if (!request.camera) return std::move(*scene);
  if (const auto problem = keep_camera(*scene, *request.camera)) return *problem;
  return std::move(*scene);
}

// A length that is one: above nothing, and not endless.
bool is_length(double value) { return value > 0 && std::isfinite(value); }

constexpr double kStepsPerSecond = 1.0 / config::kFixedDeltaSeconds;

// How many steps `length` is, at `steps_per_unit`: one at least.
std::optional<long long> steps_from(std::optional<double> length, double steps_per_unit) {
  if (!length || !is_length(*length)) return std::nullopt;
  return std::max(1LL, static_cast<long long>(std::llround(*length * steps_per_unit)));
}

// A name no other run picks, all but surely.
std::string random_digits() {
  std::random_device random;
  return std::to_string(random());
}

}  // namespace

ScratchFolder::ScratchFolder() : path_(std::filesystem::temp_directory_path() / (kScratchPrefix + random_digits())) {
  std::filesystem::remove_all(path_);
  std::filesystem::create_directories(path_);
}

ScratchFolder::~ScratchFolder() {
  std::error_code ignored;
  std::filesystem::remove_all(path_, ignored);
}

std::variant<std::unique_ptr<Engine>, std::string> engine_for(const PlayRequest& request, Presenter* presenter,
                                                              const ScratchFolder& scratch) {
  auto scene = scene_of(request);
  if (auto* problem = std::get_if<std::string>(&scene)) return *problem;
  EngineOptions options;
  options.game_dir = request.game_dir;
  options.presenter = presenter;
  options.scene_name = request.scene;
  options.fixed_step = true;
  options.save_file = scratch.path() / kSaveFile;
  return Engine::create(options, std::move(std::get<SceneDesc>(scene)));
}

std::optional<std::string> keep_camera(SceneDesc& scene, const std::string& name) {
  const auto found = std::find_if(scene.nodes.begin(), scene.nodes.end(),
                                  [&name](const NodeDesc& node) { return is_camera_named(node, name); });
  if (found == scene.nodes.end()) {
    return tr("project.run.no_camera", {{"camera", name}, {"cameras", joined(camera_names(scene))}});
  }
  for (NodeDesc& node : scene.nodes) {
    if (!is_camera_named(node, name)) node.camera.reset();
  }
  return std::nullopt;
}

std::optional<long long> frames_asked(const ToolLine& line, double otherwise) {
  if (const auto frames = line.value(kFramesOption)) return steps_from(number_from(*frames), 1.0);
  const auto seconds = line.value(kSecondsOption);
  return steps_from(seconds ? number_from(*seconds) : otherwise, kStepsPerSecond);
}

}  // namespace redgem::tools
