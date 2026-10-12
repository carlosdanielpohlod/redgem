#include <doctest/doctest.h>

#include "render/gpu_select.hpp"
#include "render/window.hpp"

using namespace redgem;

TEST_CASE("the dedicated graphics card is requested unless the user prefers the integrated one") {
  for (const GpuChoice wanted : {GpuChoice::Auto, GpuChoice::Dedicated}) {
    const auto nvidia = discrete_gpu_environment(true, wanted);
    CHECK(nvidia.at("__NV_PRIME_RENDER_OFFLOAD") == "1");
    CHECK(nvidia.at("__GLX_VENDOR_LIBRARY_NAME") == "nvidia");
    CHECK(nvidia.count("DRI_PRIME") == 0);
    const auto open_source = discrete_gpu_environment(false, wanted);
    CHECK(open_source.at("DRI_PRIME") == "1");
    CHECK(open_source.size() == 1);
  }
  CHECK(discrete_gpu_environment(true, GpuChoice::Integrated).empty());
  CHECK(discrete_gpu_environment(false, GpuChoice::Integrated).empty());
}

TEST_CASE("graphics card choices have stable names") {
  for (const GpuChoice choice : {GpuChoice::Auto, GpuChoice::Dedicated, GpuChoice::Integrated}) {
    CHECK(gpu_choice_from_name(gpu_choice_name(choice)) == choice);
  }
  CHECK_FALSE(gpu_choice_from_name("fastest").has_value());
  CHECK_FALSE(gpu_choice_from_name("").has_value());
}

TEST_CASE("whoever opens a window is asked which card, and says the dedicated one unless told otherwise") {
  CHECK(WindowSettings{}.gpu == GpuChoice::Auto);
}
