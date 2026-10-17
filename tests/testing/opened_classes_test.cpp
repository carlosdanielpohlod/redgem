#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "testing/opened_classes.hpp"

using redgem::testing::opened_classes;

TEST_CASE("the classes a test file opens are the constants of its unindented class and module lines") {
  const std::string source = R"(# A test that adds a trace to the game's Fighter.
class Fighter
  alias_method :untraced_strike, :strike
  class Move; end
end

module Scores; end
class Fighter::Combo; end
class FighterTest < RedGemTest::Case
  class << self; end
end
)";
  CHECK(opened_classes(source) == std::vector<std::string>{"Fighter", "Scores", "FighterTest"});
}

TEST_CASE("a file that opens no class opens nothing, whatever it says about classes") {
  CHECK(opened_classes("# class Fighter is the game's\nx = Fighter.new\nputs 'class Hidden'\n").empty());
  CHECK(opened_classes("").empty());
}
