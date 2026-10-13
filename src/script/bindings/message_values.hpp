#pragma once

#include <mruby.h>

#include <nlohmann/json.hpp>

namespace redgem {

// What a message carries, between Ruby and the wire: nil, true/false, numbers, texts, symbols (as
// texts), Vector3, arrays and hashes of those. json_from raises a Ruby ArgumentError for anything
// else, so only call it from a native method.
// value_from gives hash keys back as symbols when a script of this game already uses that symbol,
// and as texts when none does: unknown keys arrive as strings. (A symbol is kept for as long as
// the game runs, so another machine must not be able to make new ones by sending new keys.)
// It makes Ruby values, which can raise: only call it where Ruby errors are caught.
nlohmann::json json_from(mrb_state* mrb, mrb_value value);
mrb_value value_from(mrb_state* mrb, const nlohmann::json& json);

}  // namespace redgem
