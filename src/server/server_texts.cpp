#include "server/server_texts.hpp"

#include "core/translations.hpp"
#include "generated/server_texts.hpp"

namespace redgem::server {

void add_server_texts() { translations::add_files(generated::server_texts()); }

}  // namespace redgem::server
