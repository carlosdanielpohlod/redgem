#pragma once

// What a file of Ruby tests opens of a game's classes.

#include <string>
#include <vector>

namespace redgem::testing {

// The classes and modules a file of Ruby opens at its top level (`class Fighter`, `module Scores`,
// `class Fighter::Move` opens Fighter), each once, in the order it opens them first. A test that
// reopens one of the game's classes to add to it needs that class loaded before: `class Fighter`
// makes a new, empty class when no Fighter exists yet, where a use of the name would load it.
std::vector<std::string> opened_classes(const std::string& source);

}  // namespace redgem::testing
