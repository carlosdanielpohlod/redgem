#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "editor/project_files.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

bool has(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}  // namespace

TEST_CASE("a game folder is listed by kind, with the names scripts use") {
  const ProjectFiles files = ProjectFiles::scan(fixture("games/sandbox"));
  CHECK(has(files.scripts, "lift.rb"));
  CHECK(std::is_sorted(files.scripts.begin(), files.scripts.end()));
  CHECK(files.sounds == std::vector<std::string>{"beep"});
  CHECK(files.scenes == std::vector<std::string>{"main", "level2"});  // scene.glb first
  CHECK(files.models.empty());
  CHECK(files == ProjectFiles::scan(fixture("games/sandbox")));
  CHECK(ProjectFiles::scan(fixture("missing_game")).scripts.empty());
}

TEST_CASE("the pictures of images/ are listed by file name, whatever the case of their extension") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem_project_files_images";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "images");
  for (const char* file : {"wood.png", "Bark.JPG", "moss.webp", "notes.txt"}) {
    std::ofstream(game / "images" / file) << "data";
  }
  const ProjectFiles files = ProjectFiles::scan(game);
  CHECK(files.images == std::vector<std::string>{"Bark.JPG", "moss.webp", "wood.png"});
  std::filesystem::remove_all(game);
}

TEST_CASE("files reached through links are not listed: built games leave them out") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem_project_files_links";
  std::filesystem::remove_all(game);
  std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
  std::filesystem::create_symlink(game / "scripts" / "lift.rb", game / "scripts" / "alias.rb");
  std::filesystem::create_symlink(game / "sounds" / "beep.wav", game / "sounds" / "alias.wav");
  // A sound whose .wav is a link and whose .ogg is not is listed: it plays the .ogg.
  std::filesystem::create_symlink(game / "sounds" / "beep.wav", game / "sounds" / "jump.wav");
  std::filesystem::copy_file(game / "sounds" / "beep.wav", game / "sounds" / "jump.ogg");
  const ProjectFiles files = ProjectFiles::scan(game);
  CHECK(has(files.scripts, "lift.rb"));
  CHECK_FALSE(has(files.scripts, "alias.rb"));
  CHECK(files.sounds == std::vector<std::string>{"beep", "jump"});
  std::filesystem::remove_all(game);
}

TEST_CASE("prefab files are listed separately from imported models and affect project rescans") {
  const auto game = std::filesystem::temp_directory_path() / "redgem_project_files_prefabs";
  std::filesystem::remove_all(game);
  std::filesystem::create_directories(game / "prefabs");
  const auto before = ProjectFiles::scan(game);
  for (const char* file : {"zebra.glb", "arch.glb", "notes.txt"}) std::ofstream(game / "prefabs" / file) << "data";
  const auto after = ProjectFiles::scan(game);
  CHECK(after.prefabs == std::vector<std::string>{"arch.glb", "zebra.glb"});
  CHECK(after.models.empty());
  CHECK_FALSE(before == after);
  std::filesystem::create_symlink(game / "prefabs/arch.glb", game / "prefabs/link.glb");
  CHECK(ProjectFiles::scan(game) == after);
  std::filesystem::rename(game / "prefabs", game / "elsewhere");
  std::filesystem::create_directory_symlink(game / "elsewhere", game / "prefabs");
  CHECK(ProjectFiles::scan(game).prefabs.empty());
  std::filesystem::remove_all(game);
}
