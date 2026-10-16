#include "core/core_texts.hpp"

#include "core/translations.hpp"
#include "generated/core_texts.hpp"

namespace redgem {

void add_core_texts() {
  for (const generated::FolderFile& file : generated::core_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem
