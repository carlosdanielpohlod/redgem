// How long the renderer takes to draw a frame of a scene, in a window, as fast as the graphics
// card goes (no vertical sync). A tool for whoever works on the renderer: not a test, not shipped.
//
//   redgem_field_benchmark objects              the field, every tuft and tree an object of its own
//   redgem_field_benchmark file games/city/scene.glb
//
// It prints the average and the median frame time over the frames it measures.

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "benchmark/field_scene.hpp"
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

SceneDesc scene_for(const std::vector<std::string>& args) {
  if (args.size() > 1 && args[0] == "file") return load_gltf_scene(args[1]).scene;
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

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) {
    std::puts("usage: redgem_field_benchmark objects | file <scene.glb>");
    return kUsage;
  }
  WindowSettings settings{kWidth, kHeight, "RedGem field benchmark"};
  settings.frames_per_second = kUnlimitedFps;
  Window window(settings);
  ClearWindowState(FLAG_VSYNC_HINT);
  const SceneDesc desc = scene_for(args);
  Renderer renderer(window);
  renderer.upload(desc);
  const Scene scene = Scene::from_desc(desc);
  report(frame_times(renderer, scene));
  return 0;
}
