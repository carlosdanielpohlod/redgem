#pragma once

#include <mruby.h>

#include <nlohmann/json.hpp>

namespace redgem {

// What a message carries, between Ruby and the wire: nil, true/false, numbers, texts, symbols (as
// texts), Vector3, arrays and hashes of those (hash keys come back as symbols).
// json_from raises a Ruby ArgumentError for anything else, so only call it from a native method.
nlohmann::json json_from(mrb_state* mrb, mrb_value value);
mrb_value value_from(mrb_state* mrb, const nlohmann::json& json);

}  // namespace redgem
