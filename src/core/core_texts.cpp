#include "core/core_texts.hpp"

#include "core/translations.hpp"
#include "generated/core_texts.hpp"

namespace redgem {

void add_core_texts() { translations::add_files(generated::core_texts()); }

}  // namespace redgem
