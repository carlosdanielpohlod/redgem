#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "scene/property.hpp"

namespace redgem {

struct ExtrasParseResult {
  PropertyMap properties;
  // Keys skipped because their value is an array/object/null (not supported as props).
  std::vector<std::string> skipped_keys;
  bool ok = true;
  std::string error;
};

// Parses the JSON object stored in a glTF "extras" field into flat properties.
// Empty input yields an empty, successful result.
ExtrasParseResult parse_extras(std::string_view json);

}  // namespace redgem
