// Loading a demo game's scene: its .glb read and parsed (meshes, materials, the objects' custom
// properties), and the scene the engine runs made of it.

#include <string>

#include "bench/bench_case.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene.hpp"

namespace redgem::bench {

namespace {

constexpr int kLoadsPerSample = 5;

void load_once() {
  const LoadResult loaded = load_gltf_scene(REDGEM_BENCH_SCENE);
  require(loaded.ok, "loading " + std::string(REDGEM_BENCH_SCENE) + " (" + loaded.error + ")");
  keep(Scene::from_desc(loaded.scene).size());
}

}  // namespace

BenchCase scene_case() {
  return {"scene.load_glb", "reading, parsing and building the platformer's scene.glb",
          [] { return Sampler([] { return microseconds_each(kLoadsPerSample, load_once); }); }};
}

}  // namespace redgem::bench
