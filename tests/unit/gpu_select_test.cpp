#include <doctest/doctest.h>

#include "render/gpu_select.hpp"

using namespace redgem;

TEST_CASE("the dedicated graphics card is requested unless the user prefers the integrated one") {
  const auto nvidia = discrete_gpu_environment(true, "");
  CHECK(nvidia.at("__NV_PRIME_RENDER_OFFLOAD") == "1");
  CHECK(nvidia.at("__GLX_VENDOR_LIBRARY_NAME") == "nvidia");
  CHECK(nvidia.count("DRI_PRIME") == 0);
  const auto open_source = discrete_gpu_environment(false, "");
  CHECK(open_source.at("DRI_PRIME") == "1");
  CHECK(open_source.size() == 1);
  CHECK(discrete_gpu_environment(true, "integrated").empty());
  CHECK(discrete_gpu_environment(false, "integrated").empty());
}
