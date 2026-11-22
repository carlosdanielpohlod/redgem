#pragma once

// A model's import settings (core/import_settings.hpp) put on the copy the editor imports into a
// scene: its scale and up axis on the model's own top-level nodes (the object it sits under keeps
// the placement it is given), colliders on its meshes, and its pictures extracted into images/.

#include <filesystem>
#include <string>

#include "core/import_settings.hpp"
#include "scene/transform.hpp"

namespace redgem {
class SceneDocument;
}

namespace redgem::model_settings {

// One of the model's top-level nodes, scaled and turned so that the model's up axis is Y.
Transform settled(const Transform& local, const import_settings::ModelSettings& settings);

// Puts the settings on what import_model just added under `root`. Unrecorded: it joins the
// import's own undo step.
void apply(SceneDocument& document, int root, const import_settings::ModelSettings& settings);

// The folder a model's pictures are extracted into: images/<its name>/.
std::filesystem::path pictures_dir(const std::filesystem::path& images_dir, const std::filesystem::path& model);

// Writes each picture of the model into pictures_dir as <name>_<number>.<kind>; a file already
// there is left as it is. The count written; -1 with `error` when the model cannot be read or a
// picture cannot be written.
int extract_pictures(const std::filesystem::path& model, const std::filesystem::path& images_dir, std::string& error);

}  // namespace redgem::model_settings
