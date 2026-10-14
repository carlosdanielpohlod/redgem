#pragma once

#include <string>
#include <string_view>

// What a game's name is made into where a name may hold less than people type: the ids of games
// and of their apps, the files of their saves.
namespace redgem {

// The letters every such id may hold: 'a' to 'z' and '0' to '9'.
bool small_letter_or_digit(unsigned char letter);

// Eight hex digits made from a text (FNV-1a): the same for the same text on every machine, and
// different enough between two texts to tell apart the names that lost what set them apart.
std::string name_digits(std::string_view text);

}  // namespace redgem
