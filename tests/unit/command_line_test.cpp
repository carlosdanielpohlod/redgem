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

namespace {

// What a program of the test's reads its line into.
struct Settings {
  int players = 0;
};

constexpr command_line::NumberOption<Settings> kOptions[] = {
    {"--players", 1, 15, [](Settings& settings, int players) { settings.players = players; }},
};

// What reading the option at the start of a line says, and how far it read.
std::string read(const std::vector<std::string>& line, Settings& settings, std::size_t& next) {
  next = 0;
  return command_line::read_number_option(kOptions, line, next, settings);
}

}  // namespace

TEST_CASE("an option given a number reads it, within its range and in digits only") {
  Settings settings;
  std::size_t next = 0;
  CHECK(read({"--players", "15", "game"}, settings, next).empty());
  CHECK(settings.players == 15);
  CHECK(next == 2);
  CHECK(read({"--players", "00004"}, settings, next).empty());  // five digits at most, zeros too
  CHECK(settings.players == 4);
  for (const char* wrong : {"0", "16", "000004", "-4", "4a", "many", "", "--play"}) {
    CAPTURE(wrong);
    CHECK(read({"--players", wrong}, settings, next) == "--players needs a number from 1 to 15");
  }
  CHECK(read({"--players"}, settings, next) == "--players needs a number from 1 to 15");
  CHECK(read({"--bogus", "4"}, settings, next) == "unexpected argument --bogus");
  CHECK(settings.players == 4);  // what was refused changed nothing
}
