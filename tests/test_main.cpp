// The engine's tests, with the texts of every program they test (in English, whatever the
// machine's language: the tests read them).
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "app/game_texts.hpp"
#include "core/core_texts.hpp"
#include "editor/editor_texts.hpp"
#include "net/rooms/relay_texts.hpp"
#include "project/project_texts.hpp"
#include "server/server_texts.hpp"

int main(int argc, char** argv) {
  redgem::add_core_texts();
  redgem::add_game_texts();
  redgem::add_editor_texts();
  redgem::add_project_texts();
  redgem::server::add_server_texts();
  redgem::net::add_relay_texts();
  return doctest::Context(argc, argv).run();
}
