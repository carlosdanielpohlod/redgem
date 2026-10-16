#include "project/project_texts.hpp"

#include "core/translations.hpp"
#include "generated/project_texts.hpp"

namespace redgem {

void add_project_texts() {
  for (const generated::FolderFile& file : generated::project_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem
