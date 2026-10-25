// The glTF loader (cgltf and the engine's checks and readers), on scene files made up by the
// fuzzer: a .glb, or a .gltf whose buffers are embedded or name files beside it.
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

#include "fuzz/fuzz_target.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace fs = std::filesystem;

namespace {

// The loader reads a file: each input is written to the same one, in a folder of this run's own,
// removed when the run ends.
class SceneFolder {
 public:
  SceneFolder() : folder_(fs::temp_directory_path() / ("redgem_fuzz_gltf_" + std::to_string(std::random_device()()))) {
    fs::create_directories(folder_);
  }
  ~SceneFolder() {
    std::error_code ignored;
    fs::remove_all(folder_, ignored);
  }
  SceneFolder(const SceneFolder&) = delete;
  SceneFolder& operator=(const SceneFolder&) = delete;

  fs::path file() const { return folder_ / "scene.glb"; }

 private:
  fs::path folder_;
};

const fs::path& scene_file() {
  static const SceneFolder folder;
  static const fs::path file = folder.file();
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
