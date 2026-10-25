#include "editor/ui/project_folders.hpp"

#include <algorithm>

#include "core/config.hpp"
#include "core/platform.hpp"
#include "editor/editor_config.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui::project {
namespace {

using namespace editor_config;
using Folder = ProjectFolder;

// What a double click on a file of each folder does.
void edit_script(PanelContext& context, const std::string& name) { open_script(context.scripts, name); }

void add_model(PanelContext& context, const std::string& name) {
  import_model(context.session, name, context.spawn_point);
}

void play_sound(PanelContext& context, const std::string& name) {
  if (context.audio == nullptr) return;
  context.audio->set_sounds_dir(context.session.game_dir() / config::kSoundsDirName);
  context.audio->play_sound(name, kFullVolume, kAsRecorded);  // sound previews play as recorded
}

void open_scene(PanelContext& context, const std::string& name) { context.scene_to_open = name; }

// A picture opens in the system's viewer (it is put on a material from the Inspector).
void show_image(PanelContext& context, const std::string& name) {
  platform::open_in_system(context.session.game_dir() / config::kImagesDirName / name);
}

}  // namespace

const std::vector<FolderInfo>& folders() {
  static const std::vector<FolderInfo> all = {
      {Folder::Scripts, ICON_FA_GEM, "editor.project.folder.scripts", config::kScriptsDirName,
       "editor.project.folder.scripts_hint", edit_script},
      {Folder::Models, ICON_FA_CUBES, "editor.project.folder.models", config::kModelsDirName,
       "editor.project.folder.models_hint", add_model},
      {Folder::Sounds, ICON_FA_VOLUME_HIGH, "editor.project.folder.sounds", config::kSoundsDirName,
       "editor.project.folder.sounds_hint", play_sound},
      {Folder::Scenes, ICON_FA_MAP, "editor.project.folder.scenes", config::kScenesDirName,
       "editor.project.folder.scenes_hint", open_scene},
      {Folder::Images, ICON_FA_IMAGE, "editor.project.folder.images", config::kImagesDirName,
       "editor.project.folder.images_hint", show_image},
  };
  return all;
}

const FolderInfo& info_of(Folder folder) {
  const std::vector<FolderInfo>& all = folders();
  return *std::find_if(all.begin(), all.end(), [folder](const FolderInfo& info) { return info.folder == folder; });
}

const std::vector<std::string>& files_of(const ProjectFiles& files, Folder folder) {
  switch (folder) {
    case Folder::Models:
      return files.models;
    case Folder::Sounds:
      return files.sounds;
    case Folder::Scenes:
      return files.scenes;
    case Folder::Images:
      return files.images;
    case Folder::Scripts:
      break;
  }
  return files.scripts;
}

const char* payload_of(Folder folder) {
  if (folder == Folder::Scripts) return kPayloadScript;
  if (folder == Folder::Images) return kPayloadImage;
  return folder == Folder::Models ? kPayloadModel : nullptr;
}

}  // namespace redgem::ui::project
