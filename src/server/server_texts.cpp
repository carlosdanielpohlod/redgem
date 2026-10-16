#include "server/server_texts.hpp"

#include "core/translations.hpp"
#include "generated/server_texts.hpp"

namespace redgem::server {

void add_server_texts() {
  for (const generated::FolderFile& file : generated::server_texts()) translations::add_file(file.path, file.file);
}

}  // namespace redgem::server
