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

constexpr double kStepsPerSecond = 1.0 / config::kFixedDeltaSeconds;

// Whether a number of steps is one a tool plays: from `least` to kMostSteps.
bool within_steps(double steps, long long least) {
  return std::isfinite(steps) && steps >= static_cast<double>(least) && steps <= static_cast<double>(kMostSteps);
}

// How many steps `length` is, at `steps_per_unit`; nothing when that is fewer than `least` or
// more than a tool plays (or no number at all).
std::optional<long long> steps_from(std::optional<double> length, double steps_per_unit, long long least) {
  if (!length) return std::nullopt;
  const double steps = std::round(*length * steps_per_unit);
  if (!within_steps(steps, least)) return std::nullopt;
  return static_cast<long long>(steps);
}

// What `--frames`, else `seconds_option`, ask for; `otherwise` seconds when neither is given.
std::optional<long long> steps_asked(const ToolLine& line, const char* seconds_option, double otherwise,
                                     long long least) {
  if (const auto frames = line.value(kFramesOption)) return steps_from(number_from(*frames), 1.0, least);
  const auto seconds = line.value(seconds_option);
  return steps_from(seconds ? number_from(*seconds) : otherwise, kStepsPerSecond, least);
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
  options.random_seed = request.seed;
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
  return steps_asked(line, kSecondsOption, otherwise, 1);
}

std::optional<long long> moment_asked(const ToolLine& line, const char* seconds_option, double otherwise) {
  return steps_asked(line, seconds_option, otherwise, 0);
}

}  // namespace redgem::tools
