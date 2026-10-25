#pragma once

// "Did you mean": the names nearest to one that was mistyped, for messages that say what to type
// instead of only what was wrong.

#include <cstddef>
#include <string>
#include <vector>

namespace redgem::tools {

// How many single-letter changes (a letter added, left out or replaced) turn `a` into `b`,
// ignoring case.
std::size_t edit_distance(const std::string& a, const std::string& b);

// The `most` names of `known` nearest to `word`, nearest first: those within a few changes of it
// (about a third of its letters), and those that hold it or that it holds. None when nothing is
// near.
std::vector<std::string> closest_words(const std::string& word, const std::vector<std::string>& known,
                                       std::size_t most);

// The text in small letters (ASCII ones).
std::string lowered(const std::string& text);

// Names as a message lists them: "a, b, c".
std::string joined(const std::vector<std::string>& names, const std::string& separator = ", ");

}  // namespace redgem::tools
