#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/engine.hpp"
#include "core/safe_path.hpp"
#include "testing/recording_audio.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::LogCapture;

namespace {

constexpr const char* kExtensions[] = {"", ".png", ".jpg"};
constexpr std::uintmax_t kSmallLimit = 8;  // bytes

// A folder of the test's own, with a game's assets folder in it and a file beside that folder.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-safe-path-test";
  std::filesystem::path assets = dir / "assets";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(assets / "ui");
    std::ofstream(assets / "heart.png") << "heart";
    std::ofstream(assets / "ui" / "frame.png") << "frame";
    std::ofstream(assets / "jump.wav") << "jump";
    std::ofstream(dir / "secret.png") << "secret";
    std::ofstream(dir / "secret.wav") << "secret";
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;
};

}  // namespace

TEST_CASE("an asset name stays inside the folder it is looked for in") {
  CHECK(safe_asset_name("jump"));
  CHECK(safe_asset_name("ui/heart.png"));
  CHECK(safe_asset_name("level..2"));  // dots inside a name are just dots
  CHECK_FALSE(safe_asset_name(""));
  CHECK_FALSE(safe_asset_name("/etc/passwd"));
  CHECK_FALSE(safe_asset_name(".."));
  CHECK_FALSE(safe_asset_name("../secret"));
  CHECK_FALSE(safe_asset_name("ui/../../secret"));
  CHECK_FALSE(safe_asset_name("ui/.."));
  CHECK_FALSE(safe_asset_name("..\\secret"));
  CHECK_FALSE(safe_asset_name("C:secret"));
  CHECK_FALSE(safe_asset_name(std::string_view("jump\0.wav", 9)));
}

TEST_CASE("an asset is found by a safe name only, and a refused name is said in the log") {
  const Scratch scratch;
  CHECK(find_asset_file(scratch.assets, "heart", kExtensions) == scratch.assets / "heart.png");
  CHECK(find_asset_file(scratch.assets, "ui/frame.png", kExtensions) == scratch.assets / "ui" / "frame.png");
  CHECK_FALSE(find_asset_file(scratch.assets, "nothing", kExtensions).has_value());
  CHECK_FALSE(find_asset_file(scratch.assets, "ui", kExtensions).has_value());  // a folder is not a file
  const LogCapture log;
  CHECK_FALSE(find_asset_file(scratch.assets, "../secret", kExtensions).has_value());
  CHECK_FALSE(find_asset_file(scratch.assets, (scratch.dir / "secret").string(), kExtensions).has_value());
  CHECK(log.contains("refused"));
  CHECK(log.count(log::Level::Warning) == 2);
}

TEST_CASE("a sound outside the sounds folder is not played") {
  const Scratch scratch;
  testing::RecordingAudio audio;
  audio.set_sounds_dir(scratch.assets);
  CHECK(audio.play_sound("jump", 1.0f, 1.0f));
  CHECK_FALSE(audio.play_sound("../secret", 1.0f, 1.0f));
  CHECK_FALSE(audio.play_music("../secret", false, 1.0f));
  CHECK(audio.played() == std::vector<std::string>{"jump"});
  CHECK_FALSE(find_sound_file(scratch.assets, "../secret").has_value());
}

TEST_CASE("a screenshot is written in the screenshots folder or not at all") {
  const std::filesystem::path folder = "/shots";
  const LogCapture log;
  const std::vector<std::filesystem::path> files =
      screenshot_files(folder, {"title", "../../home/me/.bashrc", "levels/first", "/tmp/x"});
  CHECK(files == std::vector<std::filesystem::path>{folder / "title.png", folder / "levels/first.png"});
  CHECK(log.count(log::Level::Warning) == 2);
}

TEST_CASE("a file is inside a folder only when it really is, links and '..' followed") {
  const Scratch scratch;
  std::filesystem::create_symlink(scratch.dir / "secret.png", scratch.assets / "link.png");
  CHECK(file_inside(scratch.assets, "heart.png").has_value());
  CHECK(file_inside(scratch.assets, "ui/../heart.png").has_value());
  CHECK(file_inside(scratch.assets, scratch.assets / "heart.png").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "../secret.png").has_value());
  CHECK_FALSE(file_inside(scratch.assets, scratch.dir / "secret.png").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "link.png").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "missing.png").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "ui").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "").has_value());
  CHECK_FALSE(file_inside(scratch.assets, "/dev/null").has_value());
}

TEST_CASE("a file is read whole up to a size, and what is bigger or is not a file is not read") {
  const Scratch scratch;
  const auto heart = read_file_up_to(scratch.assets / "heart.png", kSmallLimit);
  REQUIRE(heart.has_value());
  CHECK(std::string(heart->begin(), heart->end()) == "heart");
  std::ofstream(scratch.assets / "big.png") << "more than eight bytes";
  CHECK_FALSE(read_file_up_to(scratch.assets / "big.png", kSmallLimit).has_value());
  CHECK_FALSE(read_file_up_to(scratch.assets / "missing.png", kSmallLimit).has_value());
  CHECK_FALSE(read_file_up_to(scratch.assets / "ui", kSmallLimit).has_value());
  // A device has no end to read up to.
  CHECK_FALSE(read_file_up_to("/dev/zero", kSmallLimit).has_value());
}

TEST_CASE("text from a file is shown without control characters, other letters untouched") {
  CHECK(printable("Crate") == "Crate");
  CHECK(printable("caf\xc3\xa9") == "caf\xc3\xa9");
  CHECK(printable("a\nb\tc\x1b[31m\x7f") == "a?b?c?[31m?");
}
