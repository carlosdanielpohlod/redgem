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

const project_files::Check kNone = [] { return project_files::Findings{}; };
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
  const project_files::Check second_breaks = [&] {
    return ++calls == 1 ? project_files::Findings{{"old"}, {}} : project_files::Findings{{"old", "new"}, {}};
  };
  CHECK_THROWS_WITH_AS(project_files::write_checked(file, {{"a", 2}}, second_breaks), doctest::Contains("new"),
                       std::invalid_argument);
  CHECK(Json::parse(text(file))["a"] == 1);
  calls = 0;
  const fs::path fresh = folder.path / "b.json";
  CHECK_THROWS_AS(project_files::write_checked(fresh, {{"b", 1}}, second_breaks), std::invalid_argument);
  CHECK_FALSE(fs::exists(fresh));
}

TEST_CASE("write_checked keeps a write that only brings warnings, and tells them") {
  const Folder folder;
  const fs::path file = folder.path / "a.json";
  int calls = 0;
  const project_files::Check warns = [&] {
    return ++calls == 1 ? project_files::Findings{{}, {"old"}} : project_files::Findings{{}, {"old", "lacks pt"}};
  };
  CHECK(project_files::write_checked(file, {{"a", 1}}, warns) == project_files::Lines{"lacks pt"});
  CHECK(fs::exists(file));
}

TEST_CASE("write_checked puts the file back when its check throws") {
  const Folder folder;
  const fs::path file = folder.path / "a.json";
  project_files::write_checked(file, {{"a", 1}}, kNone);
  int calls = 0;
  const project_files::Check throws_after = [&]() -> project_files::Findings {
    if (++calls == 2) throw std::runtime_error("unreadable");
    return {};
  };
  CHECK_THROWS_AS(project_files::write_checked(file, {{"a", 2}}, throws_after), std::runtime_error);
  CHECK(Json::parse(text(file))["a"] == 1);
}
