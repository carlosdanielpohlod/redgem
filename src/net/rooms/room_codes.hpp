#pragma once

#include <functional>
#include <string>

// The codes rooms are known by, on a relay and on a dedicated server: a few letters and digits
// (kRoomCodeLetters), easy to read out and type.
namespace redgem::net {

// A code for a new room, every letter of it drawn from the operating system's own source of
// randomness: the code is all that keeps strangers out of a room, so nobody must be able to work
// out the next ones from the ones they were given.
std::string random_room_code();

// A new code that `taken` says no room has yet; empty when a few tries found none.
std::string unused_room_code(const std::function<bool(const std::string&)>& taken);

}  // namespace redgem::net
