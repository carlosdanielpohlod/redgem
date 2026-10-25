// The glTF loader (cgltf and the engine's checks and readers), on scene files made up by the
// fuzzer: a .glb, or a .gltf whose buffers are embedded or name files beside it.
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "fuzz/fuzz_target.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace fs = std::filesystem;

namespace {

// The loader reads a file: each input is written to the same one, in a folder of this run's own.
const fs::path& scene_file() {
  static const fs::path file = [] {
    const fs::path folder = fs::temp_directory_path() / ("redgem_fuzz_gltf_" + std::to_string(std::random_device()()));
    fs::create_directories(folder);
    return folder / "scene.glb";
  }();
  return file;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  {
    std::ofstream out(scene_file(), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  }
  redgem::load_gltf_scene(scene_file().string());
  return redgem::fuzz::kKeepInput;
}
