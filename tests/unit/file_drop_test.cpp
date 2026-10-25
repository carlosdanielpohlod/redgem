#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "core/config.hpp"
#include "editor/file_drop.hpp"

using namespace redgem;

namespace {

struct Desk {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-file-drop-test";
  std::filesystem::path game = root / "game";

  Desk() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(game);
  }
  ~Desk() { std::filesystem::remove_all(root); }

  std::filesystem::path file(const std::string& name, const std::string& contents = "data") const {
    std::ofstream(root / name) << contents;
    return root / name;
  }
};

std::string text_of(const std::filesystem::path& file) {
  std::ifstream in(file);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("a dropped file is a model or a sound by its extension, whatever its case") {
  CHECK(kind_of_dropped("tree.glb") == DroppedKind::Model);
  CHECK(kind_of_dropped("TREE.GLB") == DroppedKind::Model);
  for (const char* sound : {"jump.wav", "theme.ogg", "hit.MP3", "wind.flac"}) {
    CHECK(kind_of_dropped(sound) == DroppedKind::Sound);
  }
  for (const char* picture : {"wood.png", "bark.JPG", "leaf.jpeg", "moss.webp"}) {
    CHECK(kind_of_dropped(picture) == DroppedKind::Image);
  }
  CHECK_FALSE(kind_of_dropped("tree.fbx").has_value());
  CHECK_FALSE(kind_of_dropped("notes").has_value());
}

TEST_CASE("dropped models and sounds are copied into the game's folders, which are made when missing") {
  Desk desk;
  const DropReport report =
      add_dropped_files(desk.game, {desk.file("tree.glb", "a tree"), desk.file("jump.wav"), desk.file("notes.txt")});
  CHECK(report.models == std::vector<std::string>{"tree.glb"});
  CHECK(report.sounds == std::vector<std::string>{"jump.wav"});
  REQUIRE(report.refused.size() == 1);
  CHECK(report.refused.front().find("notes.txt") == 0);
  CHECK(text_of(desk.game / config::kModelsDirName / "tree.glb") == "a tree");
  CHECK(std::filesystem::exists(desk.game / config::kSoundsDirName / "jump.wav"));
  CHECK(std::filesystem::exists(desk.root / "tree.glb"));  // copied, not moved
}

TEST_CASE("dropped pictures are copied into the game's images folder") {
  Desk desk;
  const DropReport report = add_dropped_files(desk.game, {desk.file("wood.png", "a picture"), desk.file("bark.webp")});
  CHECK(report.images == std::vector<std::string>{"wood.png", "bark.webp"});
  CHECK(report.refused.empty());
  CHECK(text_of(desk.game / config::kImagesDirName / "wood.png") == "a picture");
  CHECK(std::filesystem::exists(desk.game / config::kImagesDirName / "bark.webp"));
}

TEST_CASE("a file the game already has under that name is left as it is") {
  Desk desk;
  add_dropped_files(desk.game, {desk.file("tree.glb", "the first tree")});
  const DropReport again = add_dropped_files(desk.game, {desk.file("tree.glb", "another tree")});
  CHECK(again.models.empty());
  REQUIRE(again.refused.size() == 1);
  CHECK(again.refused.front().find("already") != std::string::npos);
  CHECK(text_of(desk.game / config::kModelsDirName / "tree.glb") == "the first tree");
}

TEST_CASE("a file that is not there is reported, not copied") {
  Desk desk;
  const DropReport report = add_dropped_files(desk.game, {desk.root / "missing.glb"});
  CHECK(report.models.empty());
  CHECK(report.refused.size() == 1);
}
