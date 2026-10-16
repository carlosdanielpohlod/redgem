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
