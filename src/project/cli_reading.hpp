#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "project/cli.hpp"

// redgem's command line while it is being read: what cli.cpp, which walks the line, shares with
// cli_options.cpp, which knows what each option does.
namespace redgem::cli {

// A command line being read: what was understood so far, and how far the reading got.
struct Reading {
  explicit Reading(const std::vector<std::string>& line) : args(line) {}

  // Whether there is another argument to read.
  bool more() const { return !done && next < args.size(); }

  const std::vector<std::string>& args;
  std::size_t next = 0;  // the argument to read next
  CliResult result;
  bool mode_chosen = false;
  bool conflict = false;  // two different things were asked for
  bool done = false;      // an error or --help ended the reading
};

// Ends the reading with what is wrong with the line.
void fail(Reading& reading, const std::string& message);
// What to do is said once: by a subcommand or by one of the mode flags.
void choose(Reading& reading, RunMode mode);

// An option of the line, and the reading of it (and of what it is given).
struct Option {
  const char* name;
  void (*read)(Reading&);
};
// The option a word names; nullptr when it names none.
const Option* find_option(const std::string& word);

}  // namespace redgem::cli
