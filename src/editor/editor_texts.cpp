#include "editor/editor_texts.hpp"

#include "core/translations.hpp"
#include "generated/editor_texts.hpp"

namespace redgem {

void add_editor_texts() {
  for (const generated::FolderFile& file : generated::editor_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem
