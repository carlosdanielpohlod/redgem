// The editor's system test, with the texts of the editor, the project hub and the games they play
// (in English, whatever the machine's language: the test reads them).
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "app/game_texts.hpp"
#include "editor/editor_texts.hpp"
#include "project/project_texts.hpp"

int main(int argc, char** argv) {
  redgem::add_game_texts();
  redgem::add_editor_texts();
  redgem::add_project_texts();
  return doctest::Context(argc, argv).run();
}
