// The render system tests run a game and nothing of the tools: only the texts a game shows.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "app/game_texts.hpp"
#include "core/core_texts.hpp"

int main(int argc, char** argv) {
  redgem::add_core_texts();
  redgem::add_game_texts();
  return doctest::Context(argc, argv).run();
}
