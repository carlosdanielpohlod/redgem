#include "net/rooms/relay_texts.hpp"

#include "core/translations.hpp"
#include "generated/relay_texts.hpp"

namespace redgem::net {

void add_relay_texts() {
  for (const generated::FolderFile& file : generated::relay_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem::net
