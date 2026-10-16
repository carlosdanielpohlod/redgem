#include "app/game_texts.hpp"

#include "core/translations.hpp"
#include "generated/game_texts.hpp"

namespace redgem {

void add_game_texts() {
  for (const generated::FolderFile& file : generated::game_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem
