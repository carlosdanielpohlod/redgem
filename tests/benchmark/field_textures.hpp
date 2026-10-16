#pragma once

// The benchmark's two cut-out pictures, drawn in code and encoded as a scene file holds them.

#include "scene/scene_desc.hpp"

namespace redgem::benchmark {

ImageData grass_picture();   // thin upright blades on nothing
ImageData leaves_picture();  // a leafy blob with holes

}  // namespace redgem::benchmark
