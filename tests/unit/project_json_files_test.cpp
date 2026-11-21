#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/files.hpp"
#include "project/project_json_files.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {
// A folder of the test's own, gone with it.
struct Folder {
  Folder() { fs::remove_all(path); }
  ~Folder() { fs::remove_all(path); }
  Folder(const Folder&) = delete;
  Folder& operator=(const Folder&) = delete;
  fs::path path = fs::temp_directory_path() / "redgem_project_json_files";
};

std::string text(const fs::path& file) {
  std::string error;
  return read_text_file(file, error).value_or("");
}

const project_files::Problems kNone = [] { return std::vector<std::string>{}; };
}  // namespace

TEST_CASE("file names are small words in folders, and languages are like en or pt-BR") {
  CHECK(project_files::valid_file_name("baker"));
  CHECK(project_files::valid_file_name("npcs/old_baker-2"));
  for (const char* name : {"", "../x", "/abs", "a//b", "Baker", "a b", "a.json"})
    CHECK_FALSE(project_files::valid_file_name(name));
  CHECK(project_files::valid_language("en"));
  CHECK(project_files::valid_language("pt-BR"));
  for (const char* language : {"", "e", "english", "en/../x", "EN"})
    CHECK_FALSE(project_files::valid_language(language));
}

TEST_CASE("read_object reads a missing file as {} and refuses one that is not an object") {
  const Folder folder;
  CHECK(project_files::read_object(folder.path / "none.json") == Json::object());
  project_files::write_checked(folder.path / "list.json", Json::array({1}), kNone);
  CHECK_THROWS_AS(project_files::read_object(folder.path / "list.json"), std::invalid_argument);
}

TEST_CASE("write_checked keeps a write that brings no new problem and puts back one that does") {
  const Folder folder;
  const fs::path file = folder.path / "sub" / "a.json";
  project_files::write_checked(file, {{"a", 1}}, kNone);
  CHECK(Json::parse(text(file))["a"] == 1);
  int calls = 0;
  const project_files::Problems second_breaks = [&] {
    return ++calls == 1 ? std::vector<std::string>{"old"} : std::vector<std::string>{"old", "new"};
  };
  CHECK_THROWS_WITH_AS(project_files::write_checked(file, {{"a", 2}}, second_breaks), doctest::Contains("new"),
                       std::invalid_argument);
  CHECK(Json::parse(text(file))["a"] == 1);
  calls = 0;
  const fs::path fresh = folder.path / "b.json";
  CHECK_THROWS_AS(project_files::write_checked(fresh, {{"b", 1}}, second_breaks), std::invalid_argument);
  CHECK_FALSE(fs::exists(fresh));
}
