#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>

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
