#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

// Values as every save keeps them, whatever it keeps them in.
namespace redgem::saved {

// Arrays in hashes in arrays...: deeper than anything Save is given (a script's values nest at
// most net::kMaxDataDepth deep). What is read back nested deeper is no save's, and is left out:
// it keeps a damaged save from going deeper than the stack can.
inline constexpr int kMaxDepth = 32;

// A value as it is kept: whole numbers as integers; nullopt for one nested too deep.
std::optional<nlohmann::json> kept(const nlohmann::json& value);
// What a key read back from a save holds, as it is kept; nothing for null (which stands for
// nothing), for what was no value (text that did not parse) or for what nests too deep.
std::optional<nlohmann::json> entry(const nlohmann::json& value);
// A value as it is written out. (Text that is not valid UTF-8 is written with the broken parts
// replaced, instead of not at all.)
std::string written(const nlohmann::json& value, int indent = -1);

}  // namespace redgem::saved
