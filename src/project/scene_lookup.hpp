#pragma once

// Which object of a scene a command line means: by its id ("#12"), by its path in the hierarchy
// ("Level/Walls/Wall") or by its name ("Wall"). A name several objects share is refused, with
// each of them listed, so that the next try can name one by id or path.

#include <string>

#include "scene/scene_desc.hpp"

namespace redgem::tools {

// The object found, or why none was: kNoIndex with `error` saying so.
struct FoundObject {
  int index = kNoIndex;
  std::string error;
};

FoundObject find_object(const SceneDesc& scene, const std::string& reference);

// "#12" as the 12 it names; kNoIndex for anything else.
int id_number(const std::string& reference);

}  // namespace redgem::tools
