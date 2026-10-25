#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "core/config.hpp"
#include "editor/file_drop.hpp"
#include "test_support.hpp"

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

TEST_CASE("dropped pictures are copied into the game's images folder, what only looks like one is not") {
  Desk desk;
  std::filesystem::copy_file(redgem::test::fixture("images/sniff.png"), desk.root / "wood.png");
  std::filesystem::copy_file(redgem::test::fixture("images/lossy.webp"), desk.root / "bark.webp");
  const DropReport report = add_dropped_files(
      desk.game, {desk.root / "wood.png", desk.root / "bark.webp", desk.file("fake.png", "not a picture"),
                  redgem::test::fixture("images/animated.webp")});
  CHECK(report.images == std::vector<std::string>{"wood.png", "bark.webp"});
  REQUIRE(report.refused.size() == 2);
  CHECK(report.refused[0].find("fake.png") == 0);
  CHECK(report.refused[1].find("animated.webp") == 0);  // a moving picture is not shown either
  CHECK(std::filesystem::exists(desk.game / config::kImagesDirName / "wood.png"));
  CHECK_FALSE(std::filesystem::exists(desk.game / config::kImagesDirName / "fake.png"));
}

TEST_CASE("a link of the dropped file's name, even one pointing nowhere, is not written through") {
  Desk desk;
  const std::filesystem::path images = desk.game / config::kImagesDirName;
  std::filesystem::create_directories(images);
  const std::filesystem::path outside = desk.root / "outside.png";
  std::filesystem::create_symlink(outside, images / "wood.png");  // dangling: outside.png is not there
  std::filesystem::copy_file(redgem::test::fixture("images/sniff.png"), desk.root / "wood.png");
  const DropReport report = add_dropped_files(desk.game, {desk.root / "wood.png"});
  CHECK(report.images.empty());
  REQUIRE(report.refused.size() == 1);
  CHECK_FALSE(std::filesystem::exists(outside));
}

TEST_CASE("a game folder that is a link is not copied into") {
  Desk desk;
  const std::filesystem::path elsewhere = desk.root / "elsewhere";
  std::filesystem::create_directories(elsewhere);
  std::filesystem::create_symlink(elsewhere, desk.game / config::kImagesDirName);
  std::filesystem::copy_file(redgem::test::fixture("images/sniff.png"), desk.root / "wood.png");
  const DropReport report = add_dropped_files(desk.game, {desk.root / "wood.png"});
  CHECK(report.images.empty());
  CHECK_FALSE(std::filesystem::exists(elsewhere / "wood.png"));
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
