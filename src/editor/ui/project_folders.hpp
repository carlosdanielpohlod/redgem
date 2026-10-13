#pragma once

// The folders of a game the Project panel lists: what each holds, how it is shown, and what a
// double click on one of its files does.

#include <string>
#include <vector>

#include "editor/ui/panels.hpp"

namespace redgem::ui::project {

struct FolderInfo {
  ProjectFolder folder;
  const char* icon;
  const char* title;
  const char* dir;   // the folder's name inside the game
  const char* hint;  // shown when the folder is empty
  void (*open)(PanelContext&, const std::string& name);
};

// Every folder, in the order the panel lists them.
const std::vector<FolderInfo>& folders();
const FolderInfo& info_of(ProjectFolder folder);
// The files of a folder, as the last scan of the game found them.
const std::vector<std::string>& files_of(const ProjectFiles& files, ProjectFolder folder);
// What can be dragged out of a folder's tiles (nullptr: nothing).
const char* payload_of(ProjectFolder folder);

}  // namespace redgem::ui::project
