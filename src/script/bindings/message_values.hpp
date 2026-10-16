#pragma once

#include <cstddef>
#include <limits>

#include <mruby.h>

#include <nlohmann/json.hpp>

#include "net/wire.hpp"

namespace redgem {

// What a message carries (and what Save keeps, and HTTP sends as JSON), between Ruby and the wire: nil, true/false,
// numbers, texts, symbols (as texts), Vector3, arrays and hashes of those. json_from raises a Ruby ArgumentError for
// anything else, so only call it from a native method. value_from gives hash keys back as symbols when a script of this
// game already uses that symbol, and as texts when none does: unknown keys arrive as strings. (A symbol is kept for as
// long as the game runs, so another machine must not be able to make new ones by sending new keys.) It makes Ruby
// values, which can raise: only call it where Ruby errors are caught. Whose values json_from makes, for what it says
// when it refuses one ("a message cannot carry infinity or NaN", "Save cannot keep ..."), and how
// many things one of its arrays or hashes may hold. A message's are limited because each becomes a
// value in the memory of the machine it arrives at; a saved value is already in the game's memory
// when it is saved, so its arrays and hashes are not (how deep they nest still is, for every holder:
// it keeps the stack within bounds).
struct ValueHolder {
  const char* name;   // "a message"
  const char* owner;  // "a message's" (hash keys, data)
  const char* verb;   // "carry"
  std::size_t most_members;
};
inline constexpr std::size_t kAnyNumber = std::numeric_limits<std::size_t>::max();
inline constexpr ValueHolder kMessageHolder{"a message", "a message's", "carry", net::kMaxDataMembers};
inline constexpr ValueHolder kSaveHolder{"Save", "a saved value's", "keep", kAnyNumber};
inline constexpr ValueHolder kJsonHolder{"json:", "json:'s", "send", net::kMaxDataMembers};

nlohmann::json json_from(mrb_state* mrb, mrb_value value, const ValueHolder& holder = kMessageHolder);
// What the keys of hashes come back as: the symbols scripts use (strings for the others), or
// always strings (Save: a key that names a player must not turn into a symbol a script happens to use).
enum class HashKeys { KnownSymbols, Strings };
mrb_value value_from(mrb_state* mrb, const nlohmann::json& json, HashKeys keys = HashKeys::KnownSymbols);

}  // namespace redgem
