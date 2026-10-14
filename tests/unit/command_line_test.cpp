#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/command_line.hpp"

using namespace redgem;

TEST_CASE("a word of a command line is an option when it starts with a dash") {
  CHECK(command_line::is_option("--play"));
  CHECK(command_line::is_option("-h"));
  CHECK_FALSE(command_line::is_option("games/platformer"));
  CHECK_FALSE(command_line::is_option(""));
}

TEST_CASE("the usage is asked for by --help and by -h, and by nothing else") {
  CHECK(command_line::asks_for_help("--help"));
  CHECK(command_line::asks_for_help("-h"));
  CHECK_FALSE(command_line::asks_for_help("help"));
  CHECK_FALSE(command_line::asks_for_help("--helpful"));
}

TEST_CASE("an option is given the word after it, which is then read") {
  const std::vector<std::string> line = {"--template", "platformer", "--play"};
  std::size_t next = 1;
  const auto given = command_line::option_value(line, next);
  REQUIRE(given.has_value());
  CHECK(*given == "platformer");
  CHECK(next == 2);
}

TEST_CASE("an option is given nothing by another option or by the end of the line") {
  const std::vector<std::string> line = {"--template", "--play"};
  std::size_t next = 1;
  CHECK_FALSE(command_line::option_value(line, next).has_value());
  CHECK(next == 1);  // the option that was there is still to be read
  next = line.size();
  CHECK_FALSE(command_line::option_value(line, next).has_value());
  CHECK(next == line.size());
}
