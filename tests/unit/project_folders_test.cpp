#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/config.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/project_folders.hpp"

using namespace redgem;
using namespace redgem::ui;
using namespace redgem::ui::project;

namespace {

ProjectFiles some_files() {
  ProjectFiles files;
  files.scripts = {"enemy.rb"};
  files.models = {"crate.glb", "tree.glb"};
  files.sounds = {"jump.wav"};
  files.scenes = {"main", "level2"};
  files.images = {"wood.png"};
  return files;
}

}  // namespace

TEST_CASE("each folder of the Project panel lists its own files") {
  const ProjectFiles files = some_files();
  CHECK(files_of(files, ProjectFolder::Scripts) == files.scripts);
  CHECK(files_of(files, ProjectFolder::Models) == files.models);
  CHECK(files_of(files, ProjectFolder::Sounds) == files.sounds);
  CHECK(files_of(files, ProjectFolder::Scenes) == files.scenes);
  CHECK(files_of(files, ProjectFolder::Images) == files.images);
}

TEST_CASE("scripts, models and pictures can be dragged out of the Project panel, sounds and scenes cannot") {
  CHECK(std::string(payload_of(ProjectFolder::Scripts)) == editor_config::kPayloadScript);
  CHECK(std::string(payload_of(ProjectFolder::Models)) == editor_config::kPayloadModel);
  CHECK(std::string(payload_of(ProjectFolder::Images)) == editor_config::kPayloadImage);
  CHECK(payload_of(ProjectFolder::Sounds) == nullptr);
  CHECK(payload_of(ProjectFolder::Scenes) == nullptr);
}

TEST_CASE("every folder is described: where it is in the game, and what opening a file does") {
  const std::vector<std::string> dirs = {config::kScriptsDirName, config::kModelsDirName, config::kSoundsDirName,
                                         config::kScenesDirName, config::kImagesDirName};
  REQUIRE(folders().size() == dirs.size());
  for (std::size_t i = 0; i < dirs.size(); ++i) {
    const FolderInfo& info = folders()[i];
    CHECK(info.dir == dirs[i]);
    CHECK(&info_of(info.folder) == &info);
    CHECK(info.open != nullptr);
    CHECK_FALSE(std::string(info.hint).empty());
  }
}
