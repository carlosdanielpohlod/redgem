#include <doctest/doctest.h>

#include <algorithm>

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
  CHECK(files.scenes == std::vector<std::string>{"level2"});
  CHECK(files.models.empty());
  CHECK(files == ProjectFiles::scan(fixture("games/sandbox")));
  CHECK(ProjectFiles::scan(fixture("missing_game")).scripts.empty());
}
