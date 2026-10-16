#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>

#include "editor/scripts/script_editor.hpp"
#include "editor/scripts/script_workspace.hpp"
#include "core/files.hpp"
#include "editor/scripts/script_files.hpp"

using namespace redgem;

namespace {

std::filesystem::path fresh_dir(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem_scripts_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

}  // namespace

TEST_CASE("script names are snake_case file names") {
  CHECK(valid_script_name("door"));
  CHECK(valid_script_name("enemy_ai2"));
  CHECK(valid_script_name("enemy_ai.rb"));
  CHECK_FALSE(valid_script_name(""));
  CHECK_FALSE(valid_script_name("Door"));
  CHECK_FALSE(valid_script_name("2door"));
  CHECK_FALSE(valid_script_name("my door"));
  CHECK_FALSE(valid_script_name("../evil"));
  CHECK(script_file_name("door") == "door.rb");
  CHECK(script_file_name("door.rb") == "door.rb");
}

TEST_CASE("the template is valid Ruby defining the matching Behaviour class") {
  const std::string code = script_template("enemy_ai.rb");
  CHECK(code.find("class EnemyAi < Behaviour") != std::string::npos);
  RubyVm vm;
  CHECK(vm.check_syntax(code, "enemy_ai.rb").ok);
}

TEST_CASE("creating a script writes the template and never overwrites") {
  const auto dir = fresh_dir("create");
  std::string error;
  const auto created = create_script(dir, "spinner", error);
  REQUIRE_MESSAGE(created.has_value(), error);
  CHECK(*created == "spinner.rb");
  CHECK(std::filesystem::exists(dir / "spinner.rb"));
  CHECK_FALSE(create_script(dir, "spinner", error).has_value());
  CHECK(error.find("already exists") != std::string::npos);
  CHECK_FALSE(create_script(dir, "Bad Name", error).has_value());
  std::filesystem::remove_all(dir);
}

TEST_CASE("the script editor saves and reports syntax errors with their line") {
  const auto dir = fresh_dir("editor");
  std::string error;
  REQUIRE(create_script(dir, "mover", error));
  ScriptEditor editor(dir);
  REQUIRE(editor.open("mover.rb", error));
  CHECK_FALSE(editor.modified());
  CHECK(editor.syntax_error().empty());

  editor.text() = "class Mover < Behaviour\n  def update(dt)\n    if true\n  end\nend\n";
  CHECK(editor.modified());
  REQUIRE(editor.save(error));  // saved anyway, so no work is lost
  CHECK_FALSE(editor.modified());
  CHECK_FALSE(editor.syntax_error().empty());
  CHECK(editor.syntax_error().find("mover.rb") != std::string::npos);
  CHECK(editor.syntax_error().find("line") != std::string::npos);

  editor.text() = "class Mover < Behaviour\nend\n";
  REQUIRE(editor.save(error));
  CHECK(editor.syntax_error().empty());
  const auto on_disk = read_text_file(dir / "mover.rb", error);
  REQUIRE(on_disk);
  CHECK(*on_disk == editor.text());

  editor.text() += "# scratch\n";
  editor.revert();
  CHECK_FALSE(editor.modified());
  std::filesystem::remove_all(dir);
}

TEST_CASE("several scripts stay open in tabs; opening again brings a tab to the front") {
  const auto dir = fresh_dir("workspace");
  std::string error;
  REQUIRE(create_script(dir, "player", error));
  REQUIRE(create_script(dir, "enemy", error));
  ScriptWorkspace workspace(dir);
  REQUIRE(workspace.open("player.rb", error) != nullptr);
  REQUIRE(workspace.open("enemy.rb", error) != nullptr);
  CHECK(workspace.take_focus_request() == "enemy.rb");
  CHECK_FALSE(workspace.take_focus_request().has_value());  // taken once
  CHECK(workspace.open("player.rb", error) == workspace.find("player.rb"));
  CHECK(workspace.tabs().size() == 2);
  CHECK(workspace.take_focus_request() == "player.rb");
  CHECK(workspace.open("missing.rb", error) == nullptr);
  CHECK_FALSE(error.empty());

  workspace.find("player.rb")->text() += "# edited\n";
  workspace.find("enemy.rb")->text() += "# edited too\n";
  CHECK(workspace.any_modified());
  REQUIRE(workspace.save_all(error));
  CHECK_FALSE(workspace.any_modified());
  workspace.close("enemy.rb");
  CHECK(workspace.find("enemy.rb") == nullptr);
  CHECK(workspace.tabs().size() == 1);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a script changed by another program is reloaded, unless the tab has unsaved edits") {
  constexpr std::chrono::seconds kLater{5};
  const auto dir = fresh_dir("disk_sync");
  std::string error;
  REQUIRE(create_script(dir, "door", error));
  ScriptEditor editor(dir);
  REQUIRE(editor.open("door.rb", error));
  const std::filesystem::path file = dir / "door.rb";
  const auto write_outside = [&](const std::string& text, std::chrono::seconds later) {
    REQUIRE(write_text_file(file, text, error));
    std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + later);
  };

  write_outside("# edited in another editor\n", kLater);
  editor.sync_with_disk();
  CHECK(editor.text() == "# edited in another editor\n");
  CHECK_FALSE(editor.modified());
  CHECK_FALSE(editor.changed_on_disk());

  // With unsaved typing the tab keeps its text and warns; Revert takes the file's version.
  editor.text() += "# typed here\n";
  write_outside("# edited outside again\n", kLater * 2);
  editor.sync_with_disk();
  CHECK(editor.changed_on_disk());
  CHECK(editor.text().find("# typed here") != std::string::npos);
  editor.revert();
  CHECK(editor.text() == "# edited outside again\n");
  CHECK_FALSE(editor.changed_on_disk());

  // Saving settles it the other way.
  editor.text() = "# mine\n";
  write_outside("# theirs\n", kLater * 3);
  editor.sync_with_disk();
  REQUIRE(editor.changed_on_disk());
  REQUIRE(editor.save(error));
  CHECK_FALSE(editor.changed_on_disk());
  CHECK(*read_text_file(file, error) == "# mine\n");
  std::filesystem::remove_all(dir);
}
