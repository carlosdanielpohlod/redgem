#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "app/json_file.hpp"

using namespace redgem;

namespace {

// A file of the test's own, removed when the test ends.
struct ScratchFile {
  std::filesystem::path path = std::filesystem::temp_directory_path() / "redgem-json-file-test.json";
  explicit ScratchFile(const std::string& text) { std::ofstream(path) << text; }
  ~ScratchFile() { std::filesystem::remove(path); }
};

}  // namespace

TEST_CASE("a JSON file is read as what it holds") {
  const ScratchFile file(R"({"title": "Kart Night", "window_width": 1280})");
  const nlohmann::json json = read_json_file(file.path);
  REQUIRE(json.is_object());
  CHECK(json["title"] == "Kart Night");
}

TEST_CASE("a file that is missing or is not JSON holds nothing to use") {
  const ScratchFile broken("{ not json");
  CHECK(read_json_file(broken.path).is_discarded());
  CHECK(read_json_file(broken.path.string() + ".missing").is_discarded());
}

TEST_CASE("what a file says under a key is used only when it is of its kind") {
  const nlohmann::json json = {{"title", "Kart Night"}, {"width", 1280}, {"scale", 1.5}};
  CHECK(holds_text(json, "title"));
  CHECK_FALSE(holds_text(json, "width"));
  CHECK_FALSE(holds_text(json, "missing"));
  CHECK(holds_whole_number(json, "width"));
  CHECK_FALSE(holds_whole_number(json, "scale"));
  CHECK_FALSE(holds_whole_number(json, "title"));
  CHECK_FALSE(holds_whole_number(json, "missing"));
}
