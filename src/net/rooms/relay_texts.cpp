#include "net/rooms/relay_texts.hpp"

#include "core/translations.hpp"
#include "generated/relay_texts.hpp"

namespace redgem::net {

void add_relay_texts() { translations::add_files(generated::relay_texts()); }

}  // namespace redgem::net
