#include <doctest/doctest.h>

#include <string>

#include "editor/ui/labels.hpp"

using namespace redgem;

TEST_CASE("an editor label puts its text after an icon, or an id after its text") {
  CHECK(std::string(ui::icon_label("@", "editor.menu.file")) == "@  File");
  CHECK(std::string(ui::label_with_id("editor.menu.file", "menu")) == "File##menu");
  CHECK(std::string(ui::icon_label_with_id("@", "editor.menu.file", "menu")) == "@  File##menu");
}

TEST_CASE("an editor label is made once, and stays where it is") {
  const char* first = ui::icon_label("@", "editor.menu.edit");
  CHECK(ui::icon_label("@", "editor.menu.edit") == first);
  CHECK(ui::icon_label("#", "editor.menu.edit") != first);
}

TEST_CASE("a window's or a dialog's name is its title, then an id no title changes") {
  CHECK(std::string(ui::window_title("editor.menu.file", "###first")) == "File###first");
  CHECK(ui::window_title("editor.menu.file", "###second") != ui::window_title("editor.menu.file", "###first"));
  CHECK(ui::window_title("editor.menu.file", "###first") == ui::window_title("editor.menu.file", "###first"));
}
