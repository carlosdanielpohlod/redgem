// How long the renderer takes to draw a frame of a scene, in a window, as fast as the graphics
// card goes (no vertical sync). A tool for whoever works on the renderer: not a test, not shipped.
//
//   redgem_field_benchmark objects              the field, every tuft and tree an object of its own
//   redgem_field_benchmark scatter              the same field, every tuft and tree a scattered copy
//   redgem_field_benchmark file games/city/scene.glb
//   redgem_field_benchmark terrain 8 [stochastic] [triplanar]
//                                               a large terrain with 8 ground layers (0: none), every
//                                               one under every pixel, at 1920 x 1080
//
// It prints the average and the median frame time over the frames it measures.

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "benchmark/field_scene.hpp"
#include "benchmark/terrain_scene.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace {

using namespace redgem;

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kWarmupFrames = 60;
constexpr int kMeasuredFrames = 600;
constexpr int kUnlimitedFps = 0;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr int kUsage = 2;

constexpr int kTerrainWidth = 1920;
constexpr int kTerrainHeight = 1080;

constexpr const char* kScatterMode = "scatter";
constexpr const char* kTerrainMode = "terrain";

bool asks_for(const std::vector<std::string>& args, const char* option) {
  return std::find(args.begin(), args.end(), option) != args.end();
}

// A count of layers (0 to 8); a word that is no number (an option) asks for all of them.
int layer_count(const std::string& arg) {
  const bool number = !arg.empty() && std::all_of(arg.begin(), arg.end(), [](char c) { return std::isdigit(c) != 0; });
  return number ? std::clamp(std::stoi(arg), 0, kMaxTerrainLayers) : kMaxTerrainLayers;
}

benchmark::TerrainOptions terrain_options(const std::vector<std::string>& args) {
  benchmark::TerrainOptions options;
  options.layers = args.size() > 1 ? layer_count(args[1]) : kMaxTerrainLayers;
  options.stochastic = asks_for(args, "stochastic");
  options.triplanar = asks_for(args, "triplanar");
  return options;
}

SceneDesc scene_for(const std::vector<std::string>& args) {
  if (args.size() > 1 && args[0] == "file") return load_gltf_scene(args[1]).scene;
  if (args[0] == kTerrainMode) return benchmark::terrain_scene(terrain_options(args));
  if (args[0] == kScatterMode) return benchmark::field_templates();
  return benchmark::field_of_objects();
}

// Seconds each frame took, from one shown frame to the next.
std::vector<double> frame_times(Renderer& renderer, const Scene& scene) {
  std::vector<double> times;
  double last = GetTime();
  for (int frame = 0; frame < kWarmupFrames + kMeasuredFrames; ++frame) {
    BeginDrawing();
    renderer.draw_world(scene, renderer.scene_camera(scene));
    EndDrawing();
    const double now = GetTime();
    if (frame >= kWarmupFrames) times.push_back(now - last);
    last = now;
  }
  return times;
}

void report(std::vector<double> times) {
  double total = 0.0;
  for (const double time : times) total += time;
  std::sort(times.begin(), times.end());
  std::printf("frames %zu  average %.3f ms  median %.3f ms\n", times.size(),
              total / static_cast<double>(times.size()) * kMillisecondsPerSecond,
              times[times.size() / 2] * kMillisecondsPerSecond);
}

// A window as large as the mode measures at, drawing as fast as the card goes.
WindowSettings settings_for(const std::vector<std::string>& args) {
  const bool terrain = args[0] == kTerrainMode;
  WindowSettings settings{terrain ? kTerrainWidth : kWidth, terrain ? kTerrainHeight : kHeight,
                          "RedGem field benchmark"};
  settings.frames_per_second = kUnlimitedFps;
  return settings;
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    std::puts(
        "usage: redgem_field_benchmark objects | scatter | file <scene.glb> | terrain <layers> [stochastic] "
        "[triplanar]");
    return kUsage;
  }
  Window window(settings_for(args));
  ClearWindowState(FLAG_VSYNC_HINT);
  const SceneDesc desc = scene_for(args);
  Renderer renderer(window);
  renderer.upload(desc);
  Scene scene = Scene::from_desc(desc);
  if (args[0] == kScatterMode) benchmark::scatter_field(scene);
  report(frame_times(renderer, scene));
  return 0;
}
