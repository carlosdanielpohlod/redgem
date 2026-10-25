// The engine's own JSON reader (JsonScanner), through what reads a glTF node's custom properties
// (extras) with it: any text a scene file may hold there.
#include <cstdint>
#include <string_view>

#include "fuzz/fuzz_target.hpp"
#include "scene/gltf/extras_parser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  redgem::parse_extras(std::string_view(reinterpret_cast<const char*>(data), size));
  return redgem::fuzz::kKeepInput;
}
