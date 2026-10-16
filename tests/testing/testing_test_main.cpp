// The tests of what games are tested with, with the texts of redgem-test and of the games it
// plays (in English, whatever the machine's language: the tests read them).
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "app/game_texts.hpp"
#include "testing/testing_texts.hpp"

int main(int argc, char** argv) {
  redgem::add_game_texts();
  redgem::testing::add_testing_texts();
  return doctest::Context(argc, argv).run();
}
